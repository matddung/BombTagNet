#include "BombTagGameInstance.h"
#include "BombTagSaveGame.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

namespace
{
    constexpr int32 PlayerSaveSlotIndex = 0;
    const TCHAR* PlayerSaveSlotName = TEXT("PlayerProfile");
}

void UBombTagGameInstance::Init()
{
    Super::Init();
    LoadOrCreatePlayerData();
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

    // TODO: Backend API Call

    WaitingRoomJoinSucceededDelegate.Broadcast();

    TravelToLobby();
}

void UBombTagGameInstance::FindAndJoinSession(const FString& SessionName, const FString& SessionPassword, bool /*bIsLanQuery*/)
{
    UE_LOG(LogTemp, Log, TEXT("[Join] name='%s' pw='%s'"), *SessionName, *SessionPassword);

    // TODO: Backend API Call

    WaitingRoomJoinSucceededDelegate.Broadcast();
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

    // TODO: Backend API Call

    CurrentSessionName.Reset();
    CurrentSessionPassword.Reset();
    CurrentMaxPlayers = 4;
    bCurrentIsLan = false;

    ReturnToMenuMap();
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