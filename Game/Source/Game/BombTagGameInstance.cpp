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

    FString Sanitized = NewNickname;
    Sanitized.TrimStartAndEndInline();

    if (!IsValidNickname(Sanitized))
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid nickname: %s"), *Sanitized);
        return;
    }

    PlayerSaveGame->Nickname = Sanitized;
    SavePlayerData();
    CurrentPlayerNickname = Sanitized;
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
        PlayerSaveGame->Win++;
    else
        PlayerSaveGame->Lose++;

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

void UBombTagGameInstance::HostOnlineSession(const FString& SessionName, int32 MaxPlayers)
{
    CurrentSessionName = SessionName;
    UE_LOG(LogTemp, Log, TEXT("Hosting session: %s, MaxPlayers: %d"), *SessionName, MaxPlayers);

    // Todo : Backend API Call
}

void UBombTagGameInstance::FindAndJoinSession(const FString& SessionName)
{
    UE_LOG(LogTemp, Log, TEXT("Searching and joining session: %s"), *SessionName);

    // Todo : Backend API Call
}

void UBombTagGameInstance::LeaveSession()
{
    UE_LOG(LogTemp, Log, TEXT("Leaving session: %s"), *CurrentSessionName);

    // Todo : Backend API Call
    CurrentSessionName.Empty();
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
        PlayerSaveGame->Nickname = TEXT("Guest");
        SavePlayerData();
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

    if (!IsValidNickname(Name))
    {
        PlayerSaveGame->Nickname = TEXT("Guest");
        SavePlayerData();
    }
}

bool UBombTagGameInstance::IsValidNickname(const FString& Nickname) const
{
    int32 Length = Nickname.Len();
    if (Length < 3 || Length > 12) return false;

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