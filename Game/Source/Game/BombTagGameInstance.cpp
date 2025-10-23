#include "BombTagGameInstance.h"
#include "BombTagSaveGame.h"
#include "ApiClient.h"
#include "RoomService.h"
#include "BombTagPlayerController.h"
#include "Game.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/LocalPlayer.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"

namespace
{
    constexpr int32 PlayerSaveSlotIndex = 0;
    const TCHAR* PlayerSaveSlotName = TEXT("PlayerProfile");
    const TCHAR* DefaultBackendBaseUrl = TEXT("http://34.64.149.81:8080/api");
    const TCHAR* DefaultMatchHostAddress = TEXT("34.64.149.81");
    constexpr int32 DefaultMatchPort = 7777;
    const FName SessionSettingOwnerKey(TEXT("SETTING_OWNER"));

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
    if (Api)
    {
        if (!PlayerNickname.IsEmpty())
        {
            Api->SetLocalPlayerIdentity(PlayerNickname, PlayerNickname);
        }
        else
        {
            Api->ClearLocalPlayerIdentity();
        }
    }
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

    IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
    if (!OnlineSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] OnlineSubsystem not available; session metadata not published."));
        return;
    }

    IOnlineSessionPtr SessionInterface = OnlineSubsystem->GetSessionInterface();
    if (!SessionInterface.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Session interface invalid; session metadata not published."));
        return;
    }

    FOnlineSessionSettings SessionSettings;
    SessionSettings.NumPublicConnections = CurrentMaxPlayers;
    SessionSettings.NumPrivateConnections = 0;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bIsLANMatch = bCurrentIsLan;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bAllowJoinViaPresence = true;
    SessionSettings.bAllowJoinViaPresenceFriendsOnly = false;

    const FString MapNameValue = TEXT("/Game/Maps/MainMap");
    const FString GameModeValue = TEXT("BP_BombTagGameMode");
    const FString OwnerValue = PlayerId.IsEmpty() ? FString(TEXT("UNKNOWN_HOST")) : PlayerId;

    // 온라인 세션 브라우징 시 서버측 메타데이터가 노출되도록 ViaOnlineService로 광고한다.
    SessionSettings.Set(SETTING_MAPNAME, MapNameValue, EOnlineDataAdvertisementType::ViaOnlineService);
    SessionSettings.Set(SETTING_GAMEMODE, GameModeValue, EOnlineDataAdvertisementType::ViaOnlineService);
    // 엔진 기본 키에 존재하지 않는 호스트 식별자는 별도 키를 명시적으로 정의해 사용한다.
    SessionSettings.Set(SessionSettingOwnerKey, OwnerValue, EOnlineDataAdvertisementType::ViaOnlineService);
    // 매치 로그에서도 동일 정보를 남겨 추적 가능하게 한다.
    UE_LOG(LogTemp, Log, TEXT("[Match] Session metadata map=%s mode=%s owner=%s"), *MapNameValue, *GameModeValue, *OwnerValue);

    if (!CurrentSessionPassword.IsEmpty())
    {
        SessionSettings.Set(TEXT("SESSION_PASSWORD"), CurrentSessionPassword, EOnlineDataAdvertisementType::DontAdvertise);
    }

    const ULocalPlayer* LocalPlayer = GetFirstGamePlayer();
    FUniqueNetIdPtr UserId = LocalPlayer ? LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId() : nullptr;

    if (UserId.IsValid())
    {
        SessionInterface->CreateSession(*UserId, NAME_GameSession, SessionSettings);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] Unable to resolve unique net id for host; session not created."));
    }
}

void UBombTagGameInstance::FindAndJoinSession(const FString& SessionName, const FString& SessionPassword, bool /*bIsLanQuery*/)
{
    UE_LOG(LogTemp, Log, TEXT("[Join] name='%s' pw='%s'"), *SessionName, *SessionPassword);
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
    FString NickToUse = InNickname;
    NickToUse.TrimStartAndEndInline();

    if (NickToUse.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Backend_Login failed: nickname required"));
        OnBackendLogin.Broadcast(false, TEXT("NICKNAME_REQUIRED"));
        return;
    }

    FString SavedNickname = GetPlayerNickname();
    if (SavedNickname.IsEmpty() || !SavedNickname.Equals(NickToUse, ESearchCase::CaseSensitive))
    {
        SetPlayerNickname(NickToUse);
        SavedNickname = GetPlayerNickname();
    }

    PlayerNickname = SavedNickname;
    PlayerId = PlayerNickname;

    if (Api)
    {
        Api->SetLocalPlayerIdentity(PlayerId, PlayerNickname);
    }

    UE_LOG(LogTemp, Log, TEXT("Using local profile for login: %s"), *PlayerNickname);
    OnBackendLogin.Broadcast(true, FString());
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
                PendingMatchRoomId = RoomSummary.RoomId;
                PrepareMatchLaunch(RoomSummary.HostId, RoomSummary.HostAddress, RoomSummary.HostPort, RoomSummary.StartToken);
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
    PendingMatchStartToken.Reset();
    PendingMatchRoomId.Reset();
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
    PendingMatchStartToken.Reset();
    PendingMatchRoomId.Reset();
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
            PendingMatchRoomId = Status.MatchId;
            PrepareMatchLaunch(Status.HostPlayerId, Status.HostAddress, Status.HostPort, Status.StartToken);
            RequestServerMatchStart();
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

void UBombTagGameInstance::PrepareMatchLaunch(const FString& HostPlayer, const FString& HostAddress, int32 HostPort, const FString& StartToken)
{
    PendingMatchHostPlayerId = HostPlayer;
    PendingMatchHostAddress = HostAddress.IsEmpty() ? FString(DefaultMatchHostAddress) : HostAddress;
    PendingMatchHostPort = HostPort > 0 ? HostPort : DefaultMatchPort;
    PendingMatchStartToken = StartToken;
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
            PendingMatchRoomId = Info.MatchId.IsEmpty() ? CurrentRoomId : Info.MatchId;
            PrepareMatchLaunch(Info.HostPlayerId, Info.HostAddress, Info.HostPort, Info.StartToken);
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

void UBombTagGameInstance::RequestServerMatchStart()
{
    const FString RoomIdentifier = !PendingMatchRoomId.IsEmpty() ? PendingMatchRoomId : CurrentRoomId;

    if (RoomIdentifier.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] RequestServerMatchStart skipped: no room or match identifier available."));
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (ABombTagPlayerController* BTPC = Cast<ABombTagPlayerController>(PC))
            {
                const FString HostId = GetEffectiveHostPlayerId();
                UE_LOG(LogTemp, Log, TEXT("[Match] Requesting server match start via RPC (room=%s host=%s)."), *RoomIdentifier, *HostId);
                // 클라이언트 트래블 경로가 다시 호출되지 않도록 개발 단계에서 감시한다.
                BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(RequestServerMatchStart);
                BTPC->ServerRequestStartMatch(RoomIdentifier, PendingMatchStartToken);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] First player controller is not ABombTagPlayerController."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Match][Warn] No player controller available for match start request."));
        }
    }
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

FString UBombTagGameInstance::GetEffectiveHostPlayerId() const
{
    if (!PendingMatchHostPlayerId.IsEmpty())
    {
        return PendingMatchHostPlayerId;
    }

    if (!PlayerId.IsEmpty())
    {
        return PlayerId;
    }

    if (!PlayerNickname.IsEmpty())
    {
        return PlayerNickname;
    }

    return FString(TEXT("UNKNOWN_HOST"));
}

void UBombTagGameInstance::Deprecated_ClientTravelToMatch()
{
    // 클라이언트 트래블 호출이 남아 있는지 감시용으로만 존재한다.
    BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Deprecated_ClientTravelToMatch);
}

void UBombTagGameInstance::Deprecated_ClientReturnToMenu()
{
    // 서버 주도 복귀를 강제하기 위한 감시용 스텁.
    BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Deprecated_ClientReturnToMenu);
}