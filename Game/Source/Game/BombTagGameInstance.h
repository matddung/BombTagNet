#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "RoomService.h"
#include "MatchService.h"
#include "BombTagGameInstance.generated.h"

class UBombTagSaveGame;
class UApiClient;
class URoomService;
class UMatchService;

UENUM(BlueprintType)
enum class EBombTagMatchResult : uint8
{
    Win,
    Lose
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBackendLogin, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomJoined, bool, bSuccess, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomUpdated, const FRoomSummary&, RoomSummary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRoomStarted, bool, bSuccess, const FString&, Info);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomClosed, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnMatchQueueStatus, bool, bSuccess, const FMatchQueueStatus&, Status, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerRecordUpdated, int32, Win, int32, Lose, int32, TotalMatches);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBackendTraffic, const FString&, Message);

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

    UPROPERTY(BlueprintAssignable, Category = "Player Profile")
    FOnPlayerRecordUpdated OnPlayerRecordUpdated;

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

    UFUNCTION(BlueprintCallable, Category = "Backend")
    void RequestServerMatchStart();

    UFUNCTION(BlueprintCallable, Category = "Travel", meta = (DeprecatedFunction))
    void Deprecated_ClientTravelToMatch();

    UFUNCTION(BlueprintCallable, Category = "Travel", meta = (DeprecatedFunction))
    void Deprecated_ClientReturnToMenu();

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

    UPROPERTY(BlueprintAssignable, Category = "Backend")
    FOnBackendTraffic OnBackendTraffic;

public:
    FString GetCurrentRoomId() const { return CurrentRoomId; }
    FString GetPendingMatchStartToken() const { return PendingMatchStartToken; }
    FString GetPendingMatchServerAddress() const { return PendingMatchServerAddress; }
    int32 GetPendingMatchServerPort() const { return PendingMatchServerPort; }
    FString GetPendingMatchDedicatedServerId() const { return PendingMatchDedicatedServerId; }
    FString GetPendingMatchStartTokenExpiresAt() const { return PendingMatchStartTokenExpiresAt; }
    FString GetLocalPlayerId() const { return PlayerId; }
    FString GetPendingMatchId() const { return PendingMatchId; }
    FString GetPendingMatchTravelURL() const;
    FString GetDedicatedServerId() const { return DedicatedServerId; }
    FString GetDedicatedServerPublicAddress() const { return DedicatedServerPublicAddress; }
    FString GetDedicatedServerInternalAddress() const { return DedicatedServerInternalAddress; }
    int32 GetDedicatedServerGamePort() const { return DedicatedServerGamePort; }
    UApiClient* GetApiClient() const { return Api; }

private:
    void LoadOrCreatePlayerData();
    void SavePlayerData();
    void EnsureNicknameIsValid();
    bool IsValidNickname(const FString& Nickname) const;
    bool IsAsciiAlphanumeric(TCHAR Character) const;

    void ResetCurrentSessionState();
    void ResetMatchQueueState();
    void StartMatchQueuePolling();
    void StopMatchQueuePolling();
    void HandleMatchQueueStatusResult(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage);
    void PrepareMatchLaunch(const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& StartToken, const FString& DSId, const FString& StartTokenExpiresAt);
    void BroadcastPlayerRecord();
    void HandleBackendTraffic(const FString& Message);
    void NotifyBackendDedicatedServerReady();
    void OnNotifyDedicatedServerReadyResponse(bool bSuccess, const FString& BodyOrError);

private:
    UPROPERTY()
    UBombTagSaveGame* PlayerSaveGame = nullptr;

    UPROPERTY()
    TObjectPtr<UApiClient> Api = nullptr;

    UPROPERTY()
    TObjectPtr<URoomService> Room = nullptr;

    UPROPERTY()
    TObjectPtr<UMatchService> Match = nullptr;

    FString PlayerId;
    FString PlayerNickname;
    FString CurrentRoomId;
    bool bRoomHasStarted = false;

    FString CurrentMatchTicketId;
    bool bMatchQueueLaunched = false;
    bool bHasMatchQueueStatus = false;
    FMatchQueueStatus CachedMatchQueueStatus;
    FTimerHandle MatchQueuePollTimerHandle;

    FString PendingMatchServerAddress;
    int32 PendingMatchServerPort = 0;
    FString PendingMatchStartToken;
    FString PendingMatchDedicatedServerId;
    FString PendingMatchStartTokenExpiresAt;
    FString PendingMatchId;

    FString DedicatedServerId;
    FString DedicatedServerPublicAddress;
    FString DedicatedServerInternalAddress;
    int32 DedicatedServerGamePort = 0;

    UPROPERTY(EditDefaultsOnly, Category = "Online|Sessions")
    FName MatchMapName = FName(TEXT("/Game/Maps/MainMap"));
};