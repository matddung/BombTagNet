#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ApiClient.generated.h"

DECLARE_DELEGATE_TwoParams(FOnApiResponse, bool /*bOk*/, const FString& /*BodyOrError*/);

UCLASS()
class GAME_API UApiClient : public UObject
{
    GENERATED_BODY()

public:
    void Init(const FString& InBaseUrl, float InTimeoutSec);

    void SetLocalPlayerIdentity(const FString& InPlayerId, const FString& InNickname);
    void ClearLocalPlayerIdentity();

    void Get(const FString& Path, const TMap<FString, FString>& QueryParams, FOnApiResponse Callback);
    void Get(const FString& Path, FOnApiResponse Callback);

    void PostJson(const FString& Path, const FString& Body, FOnApiResponse Callback);

private:
    TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Verb, const FString& Path);
    void ProcessRequest(TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> Request, FOnApiResponse Callback);
    FString BuildUrl(const FString& Path) const;
    void ApplyIdentityHeaders(TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> Request) const;

private:
    FString BaseUrl;
    float TimeoutSeconds = 30.f;
    FString PlayerId;
    FString PlayerNickname;
};