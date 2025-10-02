#include "RoomService.h"
#include "ApiClient.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

void URoomService::Init(UApiClient* InApi)
{
    ApiClient = InApi;
}

void URoomService::CreateRoom(const FString& Name, int32 MaxPlayers, const FString& Password, TFunction<void(bool bSuccess, const FRoomSummary& Room, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("CreateRoom failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, FRoomSummary(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("name"), Name);
    Payload->SetNumberField(TEXT("maxPlayers"), MaxPlayers);
    Payload->SetStringField(TEXT("password"), Password);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    if (!FJsonSerializer::Serialize(Payload, Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("JSON_SERIALIZE_ERROR: CreateRoom request body"));
        if (Callback)
        {
            Callback(false, FRoomSummary(), TEXT("JSON_SERIALIZE_ERROR"));
        }
        return;
    }

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!bOk)
            {
                if (Callback)
                {
                    Callback(false, FRoomSummary(), BodyOrError);
                }
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: CreateRoom response"));
                if (Callback)
                {
                    Callback(false, FRoomSummary(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            FRoomSummary Summary;
            if (!ParseRoomSummary(RootObject, Summary))
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: Missing fields in CreateRoom response"));
                if (Callback)
                {
                    Callback(false, FRoomSummary(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            if (Callback)
            {
                Callback(true, Summary, FString());
            }
        });

    ApiClient->PostJson(TEXT("/rooms"), Content, MoveTemp(Response));
}

void URoomService::JoinRoom(const FString& RoomId, const FString& Password, TFunction<void(bool bSuccess, const FJoinRes& Result, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("JoinRoom failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, FJoinRes(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("password"), Password);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    if (!FJsonSerializer::Serialize(Payload, Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("JSON_SERIALIZE_ERROR: JoinRoom request body"));
        if (Callback)
        {
            Callback(false, FJoinRes(), TEXT("JSON_SERIALIZE_ERROR"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/rooms/%s/join"), *RoomId);

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!bOk)
            {
                if (Callback)
                {
                    Callback(false, FJoinRes(), BodyOrError);
                }
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: JoinRoom response"));
                if (Callback)
                {
                    Callback(false, FJoinRes(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            FJoinRes Result;
            double SlotValue = 0.0;
            if (!RootObject->TryGetStringField(TEXT("roomId"), Result.RoomId) ||
                !RootObject->TryGetNumberField(TEXT("slot"), SlotValue) ||
                !ParseRoomPlayers(RootObject, Result.Players))
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: Missing fields in JoinRoom response"));
                if (Callback)
                {
                    Callback(false, FJoinRes(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            Result.Slot = static_cast<int32>(SlotValue);

            if (Callback)
            {
                Callback(true, Result, FString());
            }
        });

    ApiClient->PostJson(Path, Content, MoveTemp(Response));
}

void URoomService::LeaveRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("LeaveRoom failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    if (RoomId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("LeaveRoom skipped: RoomId is empty"));
        if (Callback)
        {
            Callback(false, TEXT("INVALID_ROOM"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/rooms/%s/leave"), *RoomId);

    FOnApiResponse Response;
    Response.BindLambda([Callback](bool bOk, const FString& BodyOrError)
        {
            if (Callback)
            {
                if (!bOk)
                {
                    Callback(false, BodyOrError);
                    return;
                }

                Callback(true, FString());
            }
        });

    ApiClient->PostJson(Path, TEXT("{}"), MoveTemp(Response));
}

void URoomService::GetRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FRoomSummary& Room, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("GetRoom failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, FRoomSummary(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/rooms/%s"), *RoomId);

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!bOk)
            {
                if (Callback)
                {
                    Callback(false, FRoomSummary(), BodyOrError);
                }
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: GetRoom response"));
                if (Callback)
                {
                    Callback(false, FRoomSummary(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            FRoomSummary Summary;
            if (!ParseRoomSummary(RootObject, Summary))
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: Missing fields in GetRoom response"));
                if (Callback)
                {
                    Callback(false, FRoomSummary(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            if (Callback)
            {
                Callback(true, Summary, FString());
            }
        });

    ApiClient->Get(Path, MoveTemp(Response));
}

void URoomService::StartRoom(const FString& RoomId, TFunction<void(bool bSuccess, const FString& MatchId, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("StartRoom failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, FString(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/rooms/%s/start"), *RoomId);

    FOnApiResponse Response;
    Response.BindLambda([Callback](bool bOk, const FString& BodyOrError)
        {
            if (!bOk)
            {
                if (Callback)
                {
                    Callback(false, FString(), BodyOrError);
                }
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: StartRoom response"));
                if (Callback)
                {
                    Callback(false, FString(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            FString MatchId;
            if (!RootObject->TryGetStringField(TEXT("matchId"), MatchId))
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: Missing matchId in StartRoom response"));
                if (Callback)
                {
                    Callback(false, FString(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            if (Callback)
            {
                Callback(true, MatchId, FString());
            }
        });

    ApiClient->PostJson(Path, TEXT("{}"), MoveTemp(Response));
}

bool URoomService::ParseRoomSummary(const TSharedPtr<FJsonObject>& JsonObject, FRoomSummary& OutSummary) const
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    double MinPlayersValue = 0.0;
    double MaxPlayersValue = 0.0;
    double CurrentPlayersValue = 0.0;

    if (!JsonObject->TryGetStringField(TEXT("roomId"), OutSummary.RoomId) ||
        !JsonObject->TryGetStringField(TEXT("name"), OutSummary.Name) ||
        !JsonObject->TryGetStringField(TEXT("status"), OutSummary.Status) ||
        !JsonObject->TryGetNumberField(TEXT("minPlayers"), MinPlayersValue) ||
        !JsonObject->TryGetNumberField(TEXT("maxPlayers"), MaxPlayersValue))
    {
        return false;
    }

    OutSummary.MinPlayers = static_cast<int32>(MinPlayersValue);
    OutSummary.MaxPlayers = static_cast<int32>(MaxPlayersValue);

    ParseRoomPlayers(JsonObject, OutSummary.Players);

    if (JsonObject->TryGetNumberField(TEXT("currentPlayers"), CurrentPlayersValue))
    {
        OutSummary.CurrentPlayers = static_cast<int32>(CurrentPlayersValue);
    }
    else
    {
        const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
        if (JsonObject->TryGetArrayField(TEXT("players"), PlayersArray) && PlayersArray)
        {
            OutSummary.CurrentPlayers = PlayersArray->Num();
        }
        else
        {
            OutSummary.CurrentPlayers = 0;
        }
    }
    return true;
}

bool URoomService::ParseRoomPlayers(const TSharedPtr<FJsonObject>& JsonObject, TArray<FRoomPlayer>& OutPlayers) const
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
    if (!JsonObject->TryGetArrayField(TEXT("players"), PlayersArray))
    {
        return false;
    }

    OutPlayers.Reset();
    for (const TSharedPtr<FJsonValue>& Value : *PlayersArray)
    {
        if (!Value.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> PlayerObject = Value->AsObject();
        if (!PlayerObject.IsValid())
        {
            continue;
        }

        FRoomPlayer Player;
        if (!PlayerObject->TryGetStringField(TEXT("playerId"), Player.PlayerId) ||
            !PlayerObject->TryGetStringField(TEXT("nickname"), Player.Nickname))
        {
            UE_LOG(LogTemp, Warning, TEXT("JSON_PARSE_ERROR: Invalid player entry"));
            continue;
        }

        OutPlayers.Add(Player);
    }

    return true;
}