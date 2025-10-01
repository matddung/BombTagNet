#include "AuthService.h"
#include "ApiClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

void UAuthService::Init(UApiClient* InApi)
{
    ApiClient = InApi;
}

void UAuthService::GuestLogin(const FString& Nickname, TFunction<void(bool bSuccess, const FGuestLoginRes& Response, const FString& Error)> Callback)
{
    if (!ApiClient)
    {
        UE_LOG(LogTemp, Error, TEXT("GuestLogin failed: ApiClient not initialized"));
        if (Callback)
        {
            Callback(false, FGuestLoginRes(), TEXT("NOT_INITIALIZED"));
        }
        return;
    }

    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("nickname"), Nickname);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    if (!FJsonSerializer::Serialize(Payload, Writer))
    {
        UE_LOG(LogTemp, Error, TEXT("JSON_SERIALIZE_ERROR: GuestLogin request body"));
        if (Callback)
        {
            Callback(false, FGuestLoginRes(), TEXT("JSON_SERIALIZE_ERROR"));
        }
        return;
    }

    FOnApiResponse Response;
    Response.BindLambda([Callback](bool bOk, const FString& BodyOrError)
        {
            if (!bOk)
            {
                if (Callback)
                {
                    Callback(false, FGuestLoginRes(), BodyOrError);
                }
                return;
            }

            TSharedPtr<FJsonObject> RootObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyOrError);
            if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: GuestLogin response"));
                if (Callback)
                {
                    Callback(false, FGuestLoginRes(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            FGuestLoginRes Result;
            double ExpiresInValue = 0.0;
            if (!RootObject->TryGetStringField(TEXT("playerId"), Result.PlayerId) ||
                !RootObject->TryGetStringField(TEXT("nickname"), Result.Nickname) ||
                !RootObject->TryGetStringField(TEXT("accessToken"), Result.AccessToken) ||
                !RootObject->TryGetNumberField(TEXT("expiresIn"), ExpiresInValue))
            {
                UE_LOG(LogTemp, Error, TEXT("JSON_PARSE_ERROR: Missing fields in GuestLogin response"));
                if (Callback)
                {
                    Callback(false, FGuestLoginRes(), TEXT("JSON_PARSE_ERROR"));
                }
                return;
            }

            Result.ExpiresIn = static_cast<int32>(ExpiresInValue);

            if (Callback)
            {
                Callback(true, Result, FString());
            }
        });

    ApiClient->PostJson(TEXT("/auth/guest"), Content, MoveTemp(Response));
}