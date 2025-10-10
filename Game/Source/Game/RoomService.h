#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RoomService.generated.h"

class UApiClient;

USTRUCT(BlueprintType)
struct GAME_API FRoomPlayer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString PlayerId;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString Nickname;
};

USTRUCT(BlueprintType)
struct GAME_API FRoomSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString RoomId;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString Status;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString HostId;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString HostAddress;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    int32 HostPort = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    int32 MinPlayers = 2;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    int32 MaxPlayers = 4;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    int32 CurrentPlayers = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    TArray<FRoomPlayer> Players;
};

USTRUCT(BlueprintType)
struct GAME_API FJoinRes
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    FString RoomId;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    int32 Slot = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Room")
    TArray<FRoomPlayer> Players;
};

USTRUCT(BlueprintType)
struct GAME_API FMatchStartInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Match")
    FString MatchId;

    UPROPERTY(BlueprintReadOnly, Category = "Match")
    FString HostPlayerId;

    UPROPERTY(BlueprintReadOnly, Category = "Match")
    FString HostAddress;

    UPROPERTY(BlueprintReadOnly, Category = "Match")
    int32 HostPort = 0;
};

UCLASS()
class GAME_API URoomService : public UObject
{
    GENERATED_BODY()

public:
    void Init(UApiClient* InApi);

    void CreateRoom(const FString& Name, int32 MaxPlayers, const FString& Password, TFunction<void(bool bSuccess, const FRoomSummary& Room, const FString& Error)> Callback);
    void JoinRoom(const FString& RoomId, const FString& Password, TFunction<void(bool bSuccess, const FJoinRes& Result, const FString& Error)> Callback);
    void LeaveRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FString& Error)> Callback);
    void GetRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FRoomSummary& Room, const FString& Error)> Callback);
    void StartRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FMatchStartInfo& Info, const FString& Error)> Callback);

private:
    bool ParseRoomSummary(const TSharedPtr<class FJsonObject>& JsonObject, FRoomSummary& OutSummary) const;
    bool ParseRoomPlayers(const TSharedPtr<class FJsonObject>& JsonObject, TArray<FRoomPlayer>& OutPlayers) const;

private:
    UPROPERTY()
    TObjectPtr<UApiClient> ApiClient;
};