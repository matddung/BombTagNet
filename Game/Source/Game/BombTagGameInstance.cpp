#include "BombTagGameInstance.h"
#include "BombTagSaveGame.h"
#include "ApiClient.h"
#include "AuthService.h"
#include "RoomService.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    constexpr int32 PlayerSaveSlotIndex = 0;
    const TCHAR* PlayerSaveSlotName = TEXT("PlayerProfile");
}

void UBombTagGameInstance::Init()
{
    Super::Init();

    FString BackendBaseUrl;
    if (!GConfig->GetString(TEXT("Game.Net"), TEXT("BackendBaseUrl"), BackendBaseUrl, GGameIni))
    {
        BackendBaseUrl = TEXT("http://127.0.0.1:8080/api");
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

    LoadOrCreatePlayerData();
    PlayerNickname = GetPlayerNickname();
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
}

FString UBombTagGameInstance::GetPlayerNickname() const
{
    return PlayerSaveGame ? PlayerSaveGame->Nickname : TEXT("Guest");
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

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() != NM_Client)
        {
            UGameplayStatics::OpenLevel(World, MatchMapName, true, TEXT("listen"));
            return;
        }

        UGameplayStatics::OpenLevel(World, MatchMapName, true);
    }
}

void UBombTagGameInstance::LeaveSession()
{
    UE_LOG(LogTemp, Log, TEXT("LeaveSession"));

    CurrentSessionName.Reset();
    CurrentSessionPassword.Reset();
    CurrentMaxPlayers = 4;
    bCurrentIsLan = false;
}

void UBombTagGameInstance::Backend_GuestLogin(const FString& InNickname)
{
    if (!Auth)
    {
        UE_LOG(LogTemp, Error, TEXT("Backend_GuestLogin failed: Auth service not ready"));
        OnBackendLogin.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    const FString NickToUse = InNickname.IsEmpty() ? GetPlayerNickname() : InNickname;

    Auth->GuestLogin(NickToUse, [this](bool bSuccess, const FGuestLoginRes& Response, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("Guest login failed: %s"), *Error);
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
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("Room %s status=%s players=%d"), *RoomSummary.RoomId, *RoomSummary.Status, RoomSummary.CurrentPlayers);
            OnRoomUpdated.Broadcast(RoomSummary);
        });
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

    Room->StartRoom(CurrentRoomId, [this](bool bSuccess, const FString& MatchId, const FString& Error)
        {
            if (!bSuccess)
            {
                UE_LOG(LogTemp, Error, TEXT("StartRoom failed: %s"), *Error);
                OnRoomStarted.Broadcast(false, Error);
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("MatchId=%s"), *MatchId);
            OnRoomStarted.Broadcast(true, MatchId);
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
            if (PlayerSaveGame->Nickname.IsEmpty())
            {
                PlayerSaveGame->Nickname = TEXT("Guest");
            }
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

void UBombTagGameInstance::EnsureNicknameIsValid()
{
    if (!PlayerSaveGame) return;

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    if (!Name.IsEmpty() && !IsValidNickname(Name))
    {
        UE_LOG(LogTemp, Warning, TEXT("Loaded invalid nickname '%s' - reset to Guest"), *Name);
        PlayerSaveGame->Nickname = TEXT("Guest");
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
    UGameplayStatics::OpenLevel(this, LobbyMapName, true, TEXT("listen"));
}

void UBombTagGameInstance::ReturnToMenuMap()
{
    if (MenuReturnURL.IsEmpty()) return;

    if (UWorld* World = GetWorld())
    {
        if (World->GetNetMode() != NM_Client)
        {
            UGameplayStatics::OpenLevel(World, LobbyMapName, true);
        }
        else if (APlayerController* PC = GetFirstLocalPlayerController())
        {
            PC->ClientTravel(MenuReturnURL, TRAVEL_Absolute);
        }
    }
}