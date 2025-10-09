#include "BombTagGameInstance.h"
#include "BombTagSaveGame.h"
#include "BombTagGameMode.h"
#include "MenuGameMode.h"
#include "ApiClient.h"
#include "AuthService.h"
#include "RoomService.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    constexpr int32 PlayerSaveSlotIndex = 0;
    const TCHAR* PlayerSaveSlotName = TEXT("PlayerProfile");
    const TCHAR* DefaultBackendBaseUrl = TEXT("http://127.0.0.1:8080/api");
    constexpr int32 DefaultMatchPort = 7777;

    FString BuildGameModeOptionString(const UClass* GameModeClass, bool bListen)
    {
        FString GameModeOption;

        if (GameModeClass)
        {
            const FString ClassPath = GameModeClass->GetPathName();
            if (!ClassPath.IsEmpty())
            {
                GameModeOption = FString::Printf(TEXT("game=%s"), *ClassPath);
            }
        }

        if (bListen)
        {
            if (GameModeOption.IsEmpty())
            {
                return TEXT("listen");
            }

            return FString::Printf(TEXT("listen?%s"), *GameModeOption);
        }

        return GameModeOption;
    }

    FString AppendGameModeQueryParam(const FString& BaseUrl, const FString& GameModePath)
    {
        if (BaseUrl.IsEmpty() || GameModePath.IsEmpty())
        {
            return BaseUrl;
        }

        if (BaseUrl.Contains(TEXT("game=")))
        {
            return BaseUrl;
        }

        const TCHAR Separator = BaseUrl.Contains(TEXT("?")) ? TEXT('&') : TEXT('?');
        return FString::Printf(TEXT("%s%cgame=%s"), *BaseUrl, Separator, *GameModePath);
    }

    bool IsBackendBaseUrlValid(FString& Url)
    {
        Url.TrimStartAndEndInline();

        if (Url.IsEmpty())
        {
            return false;
        }

        const FString SchemeDelimiter = TEXT("://");
        const int32 SchemeIndex = Url.Find(SchemeDelimiter, ESearchCase::IgnoreCase, ESearchDir::FromStart);

        if (SchemeIndex <= 0)
        {
            return false;
        }

        const int32 HostStartIndex = SchemeIndex + SchemeDelimiter.Len();
        if (HostStartIndex >= Url.Len())
        {
            return false;
        }

        if (Url.Mid(HostStartIndex, 1) == TEXT("/"))
        {
            return false;
        }

        return true;
    }
}

void UBombTagGameInstance::Init()
{
    Super::Init();

    FString BackendBaseUrl;
    if (!GConfig->GetString(TEXT("Game.Net"), TEXT("BackendBaseUrl"), BackendBaseUrl, GGameIni))
    {
        BackendBaseUrl = DefaultBackendBaseUrl;
    }
    else if (!IsBackendBaseUrlValid(BackendBaseUrl))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid BackendBaseUrl '%s' in config; using default '%s'"), *BackendBaseUrl, DefaultBackendBaseUrl);
        BackendBaseUrl = DefaultBackendBaseUrl;
    }

    float TimeoutSec = 10.0f;
    GConfig->GetFloat(TEXT("Game.Net"), TEXT("HttpTimeoutSec"), TimeoutSec, GGameIni);

    Api = NewObject<UApiClient>(this);
    if (Api)
    {
        Api->Init(BackendBaseUrl, TimeoutSec);
    }

    Auth = NewObject<UAuthService>(this);
    if (Auth && Api)
    {
        Auth->Init(Api);
    }

    Room = NewObject<URoomService>(this);
    if (Room && Api)
    {
        Room->Init(Api);
    }

    Match = NewObject<UMatchService>(this);
    if (Match && Api)
    {
        Match->Init(Api);
    }

    LoadOrCreatePlayerData();
    PlayerNickname = GetPlayerNickname();
    BroadcastPlayerRecord();
    UE_LOG(LogTemp, Log, TEXT("BombTag GameInstance initialized"));
}

void UBombTagGameInstance::SetPlayerNickname(const FString& NewNickname)
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    FString Name = NewNickname;
    Name.TrimStartAndEndInline();
    if (!IsValidNickname(Name))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid nickname: %s"), *Name);
        return;
    }

    if (PlayerSaveGame->Nickname != Name)
    {
        PlayerSaveGame->Nickname = Name;
        SavePlayerData();
    }

    PlayerNickname = Name;
}

FString UBombTagGameInstance::GetPlayerNickname() const
{
    if (!PlayerSaveGame)
    {
        return FString();
    }

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    return Name;
}

bool UBombTagGameInstance::HasPlayerNickname() const
{
    if (!PlayerSaveGame)
    {
        return false;
    }

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    return !Name.IsEmpty();
}

void UBombTagGameInstance::RecordMatchResult(EBombTagMatchResult MatchResult)
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    if (MatchResult == EBombTagMatchResult::Win)
    {
        PlayerSaveGame->Win++;
    }
    else
    {
        PlayerSaveGame->Lose++;
    }

    SavePlayerData();
    BroadcastPlayerRecord();
}

void UBombTagGameInstance::GetPlayerRecord(int32& OutWin, int32& OutLose, int32& OutTotalMatches) const
{
    if (!PlayerSaveGame)
    {
        OutWin = OutLose = OutTotalMatches = 0;
        return;
    }

    OutWin = PlayerSaveGame->Win;
    OutLose = PlayerSaveGame->Lose;
    OutTotalMatches = OutWin + OutLose;
}

void UBombTagGameInstance::ResetPlayerRecord()
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    PlayerSaveGame->Win = 0;
    PlayerSaveGame->Lose = 0;
    SavePlayerData();
    BroadcastPlayerRecord();
}


void UBombTagGameInstance::HostOnlineSession(const FString& SessionName, const FString& SessionPassword, int32 MaxPublicConnections, bool bIsLanMatch)
{
    CurrentSessionName = SessionName.IsEmpty() ? TEXT("BombTag Session") : SessionName;
    CurrentSessionPassword = SessionPassword;
    CurrentMaxPlayers = FMath::Clamp(MaxPublicConnections, 1, 64);
    bCurrentIsLan = bIsLanMatch;

    UE_LOG(LogTemp, Log, TEXT("[Host] name='%s' pw='%s' max=%d lan=%d"), *CurrentSessionName, *CurrentSessionPassword, CurrentMaxPlayers, bCurrentIsLan ? 1 : 0);
}

void UBombTagGameInstance::FindAndJoinSession(const FString& SessionName, const FString& SessionPassword, bool /*bIsLanQuery*/)
{
    UE_LOG(LogTemp, Log, TEXT("[Join] name='%s' pw='%s'"), *SessionName, *SessionPassword);
}

void UBombTagGameInstance::StartHostedMatch()
{
    if (MatchMapName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("MatchMapName is not set"));
        return;
    }

    const FString HostPlayerId = PendingMatchHostPlayerId;
    const FString HostAddress = PendingMatchHostAddress;
    const int32 HostPort = PendingMatchHostPort > 0 ? PendingMatchHostPort : DefaultMatchPort;

    PendingMatchHostPlayerId.Reset();
    PendingMatchHostAddress.Reset();
    PendingMatchHostPort = 0;

    const bool bShouldHost = HostPlayerId.IsEmpty() || PlayerId.IsEmpty() || PlayerId.Equals(HostPlayerId, ESearchCase::CaseSensitive);

    if (UWorld* World = GetWorld())
    {
        if (bShouldHost || World->GetNetMode() != NM_Client)
        {
            const FString Options = BuildGameModeOptionString(ABombTagGameMode::StaticClass(), true);
            UE_LOG(LogTemp, Log, TEXT("Starting match as host (listen server)."));
            UGameplayStatics::OpenLevel(World, MatchMapName, true, Options);
            return;
        }

        if (!HostAddress.IsEmpty())
        {
            FString ConnectionString = HostAddress;
            if (HostPort > 0)
            {
                ConnectionString = FString::Printf(TEXT("%s:%d"), *HostAddress, HostPort);
            }

            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                UE_LOG(LogTemp, Log, TEXT("Joining match at %s as client (host=%s)."), *ConnectionString, *HostPlayerId);
                PC->ClientTravel(ConnectionString, ETravelType::TRAVEL_Absolute);
                return;
            }

            UE_LOG(LogTemp, Warning, TEXT("No PlayerController for client travel; loading match map locally."));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Host address missing; loading match map locally."));
        }

        const FString Options = BuildGameModeOptionString(ABombTagGameMode::StaticClass(), false);
        UGameplayStatics::OpenLevel(World, MatchMapName, true, Options);
    }
}

void UBombTagGameInstance::LeaveSession()
{
    UE_LOG(LogTemp, Log, TEXT("LeaveSession"));

    if (Room && !CurrentRoomId.IsEmpty())
    {
        const FString RoomIdToLeave = CurrentRoomId;
        Room->LeaveRoom(RoomIdToLeave, [RoomIdToLeave](bool bSuccess, const FString& Error)
            {
                if (!bSuccess)
                {
                    UE_LOG(LogTemp, Warning, TEXT("LeaveRoom failed for %s: %s"), *RoomIdToLeave, *Error);
                }
            });
    }

    ResetCurrentSessionState();
}

void UBombTagGameInstance::Backend_Login(const FString& InNickname)
{
    if (!Auth)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_Login failed: Auth service not ready"));
        OnBackendLogin.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    FString NickToUse = InNickname;
    NickToUse.TrimStartAndEndInline();

    if (NickToUse.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Backend_Login failed: nickname required"));
        OnBackendLogin.Broadcast(false, TEXT("NICKNAME_REQUIRED"));
        return;
    }

    Auth->Login(NickToUse, [this](bool bSuccess, const FBackendLoginRes& Response, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("Backend login failed: %s"), *Error);
                OnBackendLogin.Broadcast(false, Error);
                return;
            }

            PlayerId = Response.PlayerId;
            PlayerNickname = Response.Nickname;
            AccessToken = Response.AccessToken;

            if (Api)
            {
                Api->SetAuthToken(FString::Printf(TEXT("Bearer %s"), *AccessToken));
            }

            PlayerNickname.TrimStartAndEndInline();

            UE_LOG(LogTemp, Log, TEXT("Logged in as %s (%s)"), *PlayerNickname, *PlayerId);
            OnBackendLogin.Broadcast(true, FString());
        });
}

void UBombTagGameInstance::Backend_CreateRoom(const FString& Name, int32 MaxPlayers, const FString& Password)
{
    if (!Room)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_CreateRoom failed: Room service not ready"));
        OnRoomJoined.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    CurrentSessionName = Name;
    CurrentSessionPassword = Password;
    CurrentMaxPlayers = MaxPlayers;

    Room->CreateRoom(Name, MaxPlayers, Password, [this](bool bSuccess, const FRoomSummary& RoomSummary, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("CreateRoom failed: %s"), *Error);
                OnRoomJoined.Broadcast(false, Error);
                return;
            }

            CurrentRoomId = RoomSummary.RoomId;
            UE_LOG(LogTemp, Log, TEXT("Room created: %s"), *CurrentRoomId);

            bRoomHasStarted = false;

            OnRoomJoined.Broadcast(true, FString());
            OnRoomUpdated.Broadcast(RoomSummary);
        });
}

void UBombTagGameInstance::Backend_JoinRoom(const FString& RoomId, const FString& Password)
{
    if (!Room)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_JoinRoom failed: Room service not ready"));
        OnRoomJoined.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    CurrentSessionName = RoomId;
    CurrentSessionPassword = Password;

    Room->JoinRoom(RoomId, Password, [this](bool bSuccess, const FJoinRes& Result, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("JoinRoom failed: %s"), *Error);
                OnRoomJoined.Broadcast(false, Error);
                return;
            }

            CurrentRoomId = Result.RoomId;
            UE_LOG(LogTemp, Log, TEXT("Joined room %s (slot %d)"), *Result.RoomId, Result.Slot);

            bRoomHasStarted = false;

            OnRoomJoined.Broadcast(true, FString());

            FRoomSummary Summary;
            Summary.RoomId = Result.RoomId;
            Summary.Status = TEXT("WAITING");
            Summary.MinPlayers = 2;
            Summary.MaxPlayers = CurrentMaxPlayers > 0 ? CurrentMaxPlayers : 4;
            Summary.CurrentPlayers = Result.Players.Num();
            Summary.Players = Result.Players;
            OnRoomUpdated.Broadcast(Summary);
        });
}

void UBombTagGameInstance::Backend_GetRoom()
{
    if (!Room)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_GetRoom failed: Room service not ready"));
        return;
    }

    if (CurrentRoomId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Backend_GetRoom skipped: CurrentRoomId is empty"));
        return;
    }

    Room->GetRoom(CurrentRoomId, [this](bool bSuccess, const FRoomSummary& RoomSummary, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("GetRoom failed: %s"), *Error);
                if (Error.Contains(TEXT("ROOM_NOT_FOUND")))
                {
                    const FString LostRoomId = CurrentRoomId;
                    UE_LOG(LogTemp, Warning, TEXT("Room %s no longer exists; clearing local room state"), *LostRoomId);
                    ResetCurrentSessionState();
                    OnRoomClosed.Broadcast(TEXT("ROOM_NOT_FOUND"));
                }
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("Room %s status=%s players=%d"), *RoomSummary.RoomId, *RoomSummary.Status, RoomSummary.CurrentPlayers);
            OnRoomUpdated.Broadcast(RoomSummary);

            if (RoomSummary.Status.Equals(TEXT("STARTED"), ESearchCase::IgnoreCase) && !bRoomHasStarted)
            {
                bRoomHasStarted = true;
                PrepareMatchLaunch(RoomSummary.HostId, RoomSummary.HostAddress, RoomSummary.HostPort);
                OnRoomStarted.Broadcast(true, RoomSummary.RoomId);
            }
        });
}

void UBombTagGameInstance::ResetCurrentSessionState()
{
    ResetMatchQueueState();
    CurrentSessionName.Reset();
    CurrentSessionPassword.Reset();
    CurrentMaxPlayers = 4;
    bCurrentIsLan = false;
    CurrentRoomId.Reset();
    bRoomHasStarted = false;
}

void UBombTagGameInstance::ResetMatchQueueState()
{
    StopMatchQueuePolling();
    CurrentMatchTicketId.Reset();
    bHasMatchQueueStatus = false;
    CachedMatchQueueStatus = FMatchQueueStatus();
    bMatchQueueLaunched = false;
    PendingMatchHostPlayerId.Reset();
    PendingMatchHostAddress.Reset();
    PendingMatchHostPort = 0;
}

void UBombTagGameInstance::StartMatchQueuePolling()
{
    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(MatchQueuePollTimerHandle))
        {
            World->GetTimerManager().SetTimer(MatchQueuePollTimerHandle, this, &UBombTagGameInstance::Backend_QueryMatchQueueStatus, 1.0f, true, 1.0f);
        }
    }
}

void UBombTagGameInstance::StopMatchQueuePolling()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchQueuePollTimerHandle);
    }
}

void UBombTagGameInstance::HandleMatchQueueStatusResult(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage)
{
    if (!bSuccess)
    {
        StopMatchQueuePolling();
        OnMatchQueueStatus.Broadcast(false, Status, ErrorMessage);
        return;
    }

    OnMatchQueueStatus.Broadcast(true, Status, FString());

    if (!Status.TicketId.IsEmpty())
    {
        CurrentMatchTicketId = Status.TicketId;
    }

    CachedMatchQueueStatus = Status;
    bHasMatchQueueStatus = true;

    if (Status.Status == EMatchTicketStatus::Matched)
    {
        StopMatchQueuePolling();
        CurrentMatchTicketId.Reset();
        if (!bMatchQueueLaunched)
        {
            bMatchQueueLaunched = true;
            PrepareMatchLaunch(Status.HostPlayerId, Status.HostAddress, Status.HostPort);
            StartHostedMatch();
        }
        return;
    }

    if (Status.Status == EMatchTicketStatus::Cancelled)
    {
        StopMatchQueuePolling();
        ResetMatchQueueState();
        return;
    }

    if (Status.Status == EMatchTicketStatus::Unknown)
    {
        return;
    }

    StartMatchQueuePolling();
}

void UBombTagGameInstance::PrepareMatchLaunch(const FString& HostPlayer, const FString& HostAddress, int32 HostPort)
{
    PendingMatchHostPlayerId = HostPlayer;
    PendingMatchHostAddress = HostAddress;
    PendingMatchHostPort = HostPort > 0 ? HostPort : DefaultMatchPort;
}

void UBombTagGameInstance::Backend_StartRoom()
{
    if (!Room)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_StartRoom failed: Room service not ready"));
        OnRoomStarted.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    if (CurrentRoomId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_StartRoom failed: CurrentRoomId is empty"));
        OnRoomStarted.Broadcast(false, TEXT("NO_ROOM"));
        return;
    }

    Room->StartRoom(CurrentRoomId, [this](bool bSuccess, const FMatchStartInfo& Info, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("StartRoom failed: %s"), *Error);
                OnRoomStarted.Broadcast(false, Error);
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("MatchId=%s host=%s address=%s port=%d"), *Info.MatchId, *Info.HostPlayerId, *Info.HostAddress, Info.HostPort);
            bRoomHasStarted = true;
            PrepareMatchLaunch(Info.HostPlayerId, Info.HostAddress, Info.HostPort);
            OnRoomStarted.Broadcast(true, Info.MatchId);
        });
}

void UBombTagGameInstance::Backend_JoinMatchQueue()
{
    if (!Match)
    {
        OnMatchQueueStatus.Broadcast(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        return;
    }

    if (bMatchQueueLaunched && CurrentMatchTicketId.IsEmpty())
    {
        ResetMatchQueueState();
    }

    if (!CurrentMatchTicketId.IsEmpty())
    {
        Backend_QueryMatchQueueStatus();
        return;
    }

    Match->JoinQueue([this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::Backend_LeaveMatchQueue()
{
    if (!Match)
    {
        OnMatchQueueStatus.Broadcast(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        return;
    }

    if (CurrentMatchTicketId.IsEmpty())
    {
        ResetMatchQueueState();
        return;
    }

    const FString TicketToCancel = CurrentMatchTicketId;
    Match->CancelQueue(TicketToCancel, [this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::Backend_QueryMatchQueueStatus()
{
    if (!Match || CurrentMatchTicketId.IsEmpty())
    {
        return;
    }

    const FString Ticket = CurrentMatchTicketId;
    Match->GetQueueStatus(Ticket, [this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::LoadOrCreatePlayerData()
{
    if (PlayerSaveGame) return;

    if (UGameplayStatics::DoesSaveGameExist(PlayerSaveSlotName, PlayerSaveSlotIndex))
    {
        PlayerSaveGame = Cast<UBombTagSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerSaveSlotName, PlayerSaveSlotIndex));
    }

    if (!PlayerSaveGame)
    {
        PlayerSaveGame = Cast<UBombTagSaveGame>(UGameplayStatics::CreateSaveGameObject(UBombTagSaveGame::StaticClass()));
        if (PlayerSaveGame)
        {
            SavePlayerData();
        }
    }

    EnsureNicknameIsValid();
}

void UBombTagGameInstance::SavePlayerData()
{
    if (PlayerSaveGame)
    {
        UGameplayStatics::SaveGameToSlot(PlayerSaveGame, PlayerSaveSlotName, PlayerSaveSlotIndex);
    }
}

void UBombTagGameInstance::BroadcastPlayerRecord()
{
    if (!PlayerSaveGame)
    {
        return;
    }

    const int32 WinCount = PlayerSaveGame->Win;
    const int32 LoseCount = PlayerSaveGame->Lose;
    OnPlayerRecordUpdated.Broadcast(WinCount, LoseCount, WinCount + LoseCount);
}

void UBombTagGameInstance::EnsureNicknameIsValid()
{
    if (!PlayerSaveGame) return;

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    if (!Name.IsEmpty() && !IsValidNickname(Name))
    {
        UE_LOG(LogTemp, Warning, TEXT("Loaded invalid nickname '%s' - clearing saved nickname"), *Name);
        PlayerSaveGame->Nickname.Empty();
        SavePlayerData();
    }
    else if (PlayerSaveGame->Nickname != Name)
    {
        PlayerSaveGame->Nickname = Name;
        SavePlayerData();
    }
}

bool UBombTagGameInstance::IsValidNickname(const FString& Nickname) const
{
    int32 L = Nickname.Len();
    if (L < 4 || L > 10) return false;
    for (TCHAR C : Nickname)
    {
        if (!IsAsciiAlphanumeric(C)) return false;
    }
    return true;
}

bool UBombTagGameInstance::IsAsciiAlphanumeric(TCHAR Character) const
{
    return (Character >= '0' && Character <= '9') ||
        (Character >= 'A' && Character <= 'Z') ||
        (Character >= 'a' && Character <= 'z');
}

void UBombTagGameInstance::TravelToLobby()
{
    if (LobbyMapName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("LobbyMapName not set"));
        return;
    }
    const FString Options = BuildGameModeOptionString(AMenuGameMode::StaticClass(), true);
    UGameplayStatics::OpenLevel(this, LobbyMapName, true, Options);
}

void UBombTagGameInstance::ReturnToMenuMap()
{
    if (MenuReturnURL.IsEmpty()) return;

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() != NM_Client)
        {
            const FString Options = BuildGameModeOptionString(AMenuGameMode::StaticClass(), false);
            UGameplayStatics::OpenLevel(World, LobbyMapName, true, Options);
        }
        else if (APlayerController* PC = GetFirstLocalPlayerController())
        {
            const FString MenuGameModePath = AMenuGameMode::StaticClass()->GetPathName();
            const FString TravelURL = AppendGameModeQueryParam(MenuReturnURL, MenuGameModePath);
            PC->ClientTravel(TravelURL, TRAVEL_Absolute);
        }
    }
}