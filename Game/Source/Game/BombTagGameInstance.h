#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "RoomService.h"
#include "MatchService.h"
#include "BombTagGameInstance.generated.h"

class UBombTagSaveGame;
class UApiClient;
class UAuthService;
class URoomService;
class UMatchService;

UENUM(BlueprintType)
enum class EBombTagMatchResult : uint8
{
    Win,
    Lose
};

DECLARE_MULTICAST_DELEGATE(FOnWaitingRoomJoinSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBackendLogin, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomJoined, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomUpdated, const FRoomSummary&, RoomSummary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomStarted, bool, bSuccess, const FString&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomClosed, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMatchQueueStatus, bool, bSuccess, const FMatchQueueStatus&, Status, const FString&, ErrorMessage);

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

    UFUNCTION(BlueprintPure, Category = "Player Profile")
    bool HasPlayerNickname() const;

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

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_Login(const FString& InNickname);

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_CreateRoom(const FString& Name, int32 MaxPlayers, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_JoinRoom(const FString& RoomId, const FString& Password);

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_GetRoom();

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_StartRoom();

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_JoinMatchQueue();

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_LeaveMatchQueue();

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void Backend_QueryMatchQueueStatus();

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnBackendLogin OnBackendLogin;

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnRoomJoined OnRoomJoined;

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnRoomUpdated OnRoomUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnRoomStarted OnRoomStarted;

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnRoomClosed OnRoomClosed;

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnMatchQueueStatus OnMatchQueueStatus;

private:
    void LoadOrCreatePlayerData();
    void SavePlayerData();
    void EnsureNicknameIsValid();
    bool IsValidNickname(const FString& Nickname) const;
    bool IsAsciiAlphanumeric(TCHAR Character) const;

    void TravelToLobby();
    void ReturnToMenuMap();
    void ResetCurrentSessionState();
    void ResetMatchQueueState();
    void StartMatchQueuePolling();
    void StopMatchQueuePolling();
    void HandleMatchQueueStatusResult(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage);
    void PrepareMatchLaunch(const FString& HostPlayer, const FString& HostAddress, int32 HostPort);

private:
    UPROPERTY()
    UBombTagSaveGame* PlayerSaveGame = nullptr;

    UPROPERTY()
    TObjectPtr<UApiClient> Api = nullptr;

    UPROPERTY()
    TObjectPtr<UAuthService> Auth = nullptr;

    UPROPERTY()
    TObjectPtr<URoomService> Room = nullptr;

    UPROPERTY()
    TObjectPtr<UMatchService> Match = nullptr;

    FString CurrentSessionName;
    FString CurrentSessionPassword;
    int32 CurrentMaxPlayers = 4;
    bool bCurrentIsLan = false;

    FString PlayerId;
    FString PlayerNickname;
    FString AccessToken;
    FString CurrentRoomId;
    bool bRoomHasStarted = false;

    FString CurrentMatchTicketId;
    bool bMatchQueueLaunched = false;
    bool bHasMatchQueueStatus = false;
    FMatchQueueStatus CachedMatchQueueStatus;
    FTimerHandle MatchQueuePollTimerHandle;

    FString PendingMatchHostPlayerId;
    FString PendingMatchHostAddress;
    int32 PendingMatchHostPort = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FName LobbyMapName = FName(TEXT("/Game/Maps/MenuMap"));

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FName MatchMapName = FName(TEXT("/Game/Maps/MainMap"));

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FString MenuReturnURL = TEXT("/Game/Maps/MenuMap");

    FOnWaitingRoomJoinSucceeded WaitingRoomJoinSucceededDelegate;
};