#include "MatchService.h"
#include "ApiClient.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString StatusToUpper(const FString& In)
    {
        FString Normalized = In;
        Normalized.TrimStartAndEndInline();
        Normalized = Normalized.ToUpper();
        return Normalized;
    }
}

void UMatchService::Init(UApiClient* InApi)
{
    ApiClient = InApi;
}

void UMatchService::JoinQueue(TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        if (Callback)
        {
            Callback(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!Callback)
            {
                return;
            }

            if (!bOk)
            {
                Callback(false, FMatchQueueStatus(), BodyOrError);
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            FMatchQueueStatus Status;
            if (!ParseMatchQueueStatus(RootObject, Status))
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            Callback(true, Status, FString());
        });

    ApiClient->PostJson(TEXT("/matches/queue"), TEXT("{}"), MoveTemp(Response));
}

void UMatchService::GetQueueStatus(const FString& TicketId, TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        if (Callback)
        {
            Callback(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    if (TicketId.IsEmpty())
    {
        if (Callback)
        {
            Callback(false, FMatchQueueStatus(), TEXT("INVALID_TICKET"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/matches/queue/%s"), *TicketId);

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!Callback)
            {
                return;
            }

            if (!bOk)
            {
                Callback(false, FMatchQueueStatus(), BodyOrError);
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            FMatchQueueStatus Status;
            if (!ParseMatchQueueStatus(RootObject, Status))
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            Callback(true, Status, FString());
        });

    ApiClient->Get(Path, MoveTemp(Response));
}

void UMatchService::CancelQueue(const FString& TicketId, TFunction<void(bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        if (Callback)
        {
            Callback(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    if (TicketId.IsEmpty())
    {
        if (Callback)
        {
            Callback(false, FMatchQueueStatus(), TEXT("INVALID_TICKET"));
        }
        return;
    }

    const FString Path = FString::Printf(TEXT("/matches/queue/%s/cancel"), *TicketId);

    FOnApiResponse Response;
    Response.BindLambda([this, Callback](bool bOk, const FString& BodyOrError)
        {
            if (!Callback)
            {
                return;
            }

            if (!bOk)
            {
                Callback(false, FMatchQueueStatus(), BodyOrError);
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            FMatchQueueStatus Status;
            if (!ParseMatchQueueStatus(RootObject, Status))
            {
                Callback(false, FMatchQueueStatus(), TEXT("JSON_PARSE_ERROR"));
                return;
            }

            Callback(true, Status, FString());
        });

    ApiClient->PostJson(Path, TEXT("{}"), MoveTemp(Response));
}

bool UMatchService::ParseMatchQueueStatus(const TSharedPtr<FJsonObject>& JsonObject, FMatchQueueStatus& OutStatus) const
{
    if (!JsonObject.IsValid())
    {
        return false;
    }

    FString TicketId;
    if (!JsonObject->TryGetStringField(TEXT("ticketId"), TicketId))
    {
        return false;
    }

    FString StatusString;
    if (!JsonObject->TryGetStringField(TEXT("status"), StatusString))
    {
        return false;
    }

    OutStatus = FMatchQueueStatus();
    OutStatus.TicketId = TicketId;
    OutStatus.Status = ParseTicketStatus(StatusToUpper(StatusString));

    double NumberValue = 0.0;
    if (JsonObject->TryGetNumberField(TEXT("position"), NumberValue))
    {
        OutStatus.Position = static_cast<int32>(NumberValue);
    }

    if (JsonObject->TryGetNumberField(TEXT("readyInSeconds"), NumberValue))
    {
        OutStatus.ReadyInSeconds = static_cast<int32>(NumberValue);
    }

    if (JsonObject->TryGetNumberField(TEXT("waitForFourthSeconds"), NumberValue))
    {
        OutStatus.WaitForFourthSeconds = static_cast<int32>(NumberValue);
    }

    if (JsonObject->TryGetNumberField(TEXT("minPlayers"), NumberValue))
    {
        OutStatus.MinPlayers = static_cast<int32>(NumberValue);
    }

    if (JsonObject->TryGetNumberField(TEXT("maxPlayers"), NumberValue))
    {
        OutStatus.MaxPlayers = static_cast<int32>(NumberValue);
    }

    FString MatchId;
    if (JsonObject->TryGetStringField(TEXT("matchId"), MatchId))
    {
        OutStatus.MatchId = MatchId;
    }

    const TArray<TSharedPtr<FJsonValue>>* PlayersArray = nullptr;
    if (JsonObject->TryGetArrayField(TEXT("players"), PlayersArray) && PlayersArray)
    {
        OutStatus.Players.Empty();
        for (const TSharedPtr<FJsonValue>& Value : *PlayersArray)
        {
            TSharedPtr<FJsonObject> PlayerObj = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!PlayerObj.IsValid())
            {
                continue;
            }

            FRoomPlayer Player;
            PlayerObj->TryGetStringField(TEXT("playerId"), Player.PlayerId);
            PlayerObj->TryGetStringField(TEXT("nickname"), Player.Nickname);
            OutStatus.Players.Add(Player);
        }
    }

    JsonObject->TryGetStringField(TEXT("hostPlayerId"), OutStatus.HostPlayerId);
    JsonObject->TryGetStringField(TEXT("hostAddress"), OutStatus.HostAddress);

    double HostPortValue = 0.0;
    if (JsonObject->TryGetNumberField(TEXT("hostPort"), HostPortValue))
    {
        OutStatus.HostPort = static_cast<int32>(HostPortValue);
    }

    return true;
}

EMatchTicketStatus UMatchService::ParseTicketStatus(const FString& StatusString) const
{
    const FString Normalized = StatusToUpper(StatusString);
    if (Normalized == TEXT("QUEUED"))
    {
        return EMatchTicketStatus::Queued;
    }
    if (Normalized == TEXT("FORMING"))
    {
        return EMatchTicketStatus::Forming;
    }
    if (Normalized == TEXT("MATCHED"))
    {
        return EMatchTicketStatus::Matched;
    }
    if (Normalized == TEXT("CANCELLED"))
    {
        return EMatchTicketStatus::Cancelled;
    }
    return EMatchTicketStatus::Unknown;
}