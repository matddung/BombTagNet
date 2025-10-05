#pragma once

#include "CoreMinimal.h"
#include "RoomService.h"
#include "MatchService.generated.h"

class UApiClient;

UENUM(BlueprintType)
enum class EMatchTicketStatus : uint8
{
    Unknown,
    Queued,
    Forming,
    Matched,
    Cancelled
};

USTRUCT(BlueprintType)
struct GAME_API FMatchQueueStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    FString TicketId;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    EMatchTicketStatus Status = EMatchTicketStatus::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    int32 Position = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    int32 ReadyInSeconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    int32 WaitForFourthSeconds = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    int32 MinPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    int32 MaxPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    FString MatchId;

    UPROPERTY(BlueprintReadOnly, Category = "Matchmaking")
    TArray<FRoomPlayer> Players;
};

UCLASS()
class GAME_API UMatchService : public UObject
{
    GENERATED_BODY()

public:
    void Init(UApiClient* InApi);

    void JoinQueue(TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback);
    void GetQueueStatus(const FString& TicketId, TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback);
    void CancelQueue(const FString& TicketId, TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback);

private:
    bool ParseMatchQueueStatus(const TSharedPtr<class FJsonObject>& JsonObject, FMatchQueueStatus& OutStatus) const;
    EMatchTicketStatus ParseTicketStatus(const FString& StatusString) const;

private:
    UPROPERTY()
    TObjectPtr<UApiClient> ApiClient;
};