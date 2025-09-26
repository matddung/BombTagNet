#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "BombTagGameInstance.generated.h"

class UBombTagSaveGame;

UENUM(BlueprintType)
enum class EBombTagMatchResult : uint8
{
    Win,
    Lose
};

DECLARE_MULTICAST_DELEGATE(FOnWaitingRoomJoinSucceeded);

UCLASS()
class GAME_API UBombTagGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
    virtual void Init() override;

    UFUNCTION(BlueprintCallable, Category = "Player Profile")
    void SetPlayerNickname(const FString& NewNickname);

    UFUNCTION(BlueprintPure, Category = "Player Profile")
    FString GetPlayerNickname() const;

    UFUNCTION(BlueprintCallable, Category = "Player Profile")
    void RecordMatchResult(EBombTagMatchResult MatchResult);

    UFUNCTION(BlueprintPure, Category = "Player Profile")
    void GetPlayerRecord(int32& OutWin, int32& OutLose, int32& OutTotalMatches) const;

    UFUNCTION(BlueprintCallable, Category = "Player Profile")
    void ResetPlayerRecord();

    FOnWaitingRoomJoinSucceeded& OnWaitingRoomJoinSucceeded() { return WaitingRoomJoinSucceededDelegate; }

    UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
    void HostOnlineSession(const FString& SessionName, const FString& SessionPassword, int32 MaxPublicConnections, bool bIsLanMatch);

    UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
    void FindAndJoinSession(const FString& SessionName, const FString& SessionPassword, bool bIsLanQuery);

    UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
    void StartHostedMatch();

    UFUNCTION(BlueprintCallable, Category = "Online|Sessions")
    void LeaveSession();

private:
    void LoadOrCreatePlayerData();
    void SavePlayerData();
    void EnsureNicknameIsValid();
    bool IsValidNickname(const FString& Nickname) const;
    bool IsAsciiAlphanumeric(TCHAR Character) const;

    void TravelToLobby();
    void ReturnToMenuMap();

private:
    UPROPERTY()
    UBombTagSaveGame* PlayerSaveGame = nullptr;

    FString CurrentSessionName;
    FString CurrentSessionPassword;
    int32   CurrentMaxPlayers = 4;
    bool    bCurrentIsLan = false;

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FName LobbyMapName = FName(TEXT("/Game/Maps/MenuMap"));

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FName MatchMapName = FName(TEXT("/Game/Maps/MainMap"));

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FString MenuReturnURL = TEXT("/Game/Maps/MenuMap");

    FOnWaitingRoomJoinSucceeded WaitingRoomJoinSucceededDelegate;
};