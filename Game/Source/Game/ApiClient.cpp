#include "ApiClient.h"

#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void UApiClient::Init(const FString& InBaseUrl, float InTimeoutSec)
{
    BaseUrl = InBaseUrl;
    BaseUrl.TrimStartAndEndInline();
    while (BaseUrl.EndsWith(TEXT("/")))
    {
        BaseUrl.LeftChopInline(1);
    }

    TimeoutSeconds = FMath::Max(0.0f, InTimeoutSec);
}

void UApiClient::SetLocalPlayerIdentity(const FString& InPlayerId, const FString& InNickname)
{
    PlayerId = InPlayerId;
    PlayerId.TrimStartAndEndInline();

    PlayerNickname = InNickname;
    PlayerNickname.TrimStartAndEndInline();
}

void UApiClient::ClearLocalPlayerIdentity()
{
    PlayerId.Reset();
    PlayerNickname.Reset();
}

void UApiClient::Get(const FString& Path, const TMap<FString, FString>& QueryParams, FOnApiResponse Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("GET"), Path, &QueryParams);
    ProcessRequest(Request, MoveTemp(Callback));
}

void UApiClient::Get(const FString& Path, FOnApiResponse Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("GET"), Path, nullptr);
    ProcessRequest(Request, MoveTemp(Callback));
}

void UApiClient::PostJson(const FString& Path, const FString& Body, FOnApiResponse Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("POST"), Path, nullptr);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Body);
    ProcessRequest(Request, MoveTemp(Callback));
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UApiClient::CreateRequest(const FString& Verb, const FString& Path, const TMap<FString, FString>* QueryParams)
{
    FHttpModule& Module = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Module.CreateRequest();

    Request->SetVerb(Verb);
    Request->SetURL(BuildUrl(Path, QueryParams));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetTimeout(TimeoutSeconds);

    ApplyIdentityHeaders(Request);

    return Request;
}

void UApiClient::ProcessRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request, FOnApiResponse Callback)
{
    Request->OnProcessRequestComplete().BindLambda([Callback = MoveTemp(Callback)](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bConnectedSuccessfully) mutable
        {
            if (!Callback.IsBound())
            {
                return;
            }

            if (!bConnectedSuccessfully || !Response.IsValid())
            {
                Callback.Execute(false, TEXT("NETWORK_ERROR"));
                return;
            }

            const int32 StatusCode = Response->GetResponseCode();
            if (StatusCode >= 200 && StatusCode < 300)
            {
                Callback.Execute(true, Response->GetContentAsString());
            }
            else
            {
                const FString ErrorMessage = FString::Printf(TEXT("HTTP_%d:%s"), StatusCode, *Response->GetContentAsString());
                Callback.Execute(false, ErrorMessage);
            }
        });

    Request->ProcessRequest();
}

FString UApiClient::BuildUrl(const FString& Path, const TMap<FString, FString>* QueryParams) const
{
    FString Url;
    if (Path.StartsWith(TEXT("http")))
    {
        Url = Path;
    }
    else
    {
        Url = BaseUrl;
        if (!Path.IsEmpty())
        {
            const bool bBaseHasSlash = Url.EndsWith(TEXT("/"));
            const bool bPathHasSlash = Path.StartsWith(TEXT("/"));
            if (!bBaseHasSlash && !bPathHasSlash)
            {
                Url += TEXT("/");
            }
            else if (bBaseHasSlash && bPathHasSlash)
            {
                Url.LeftChopInline(1);
            }
            Url += Path;
        }
    }

    if (QueryParams && QueryParams->Num() > 0)
    {
        TArray<FString> Pairs;
        Pairs.Reserve(QueryParams->Num());

        for (const TPair<FString, FString>& Kvp : *QueryParams)
        {
            const FString EncodedKey = FGenericPlatformHttp::UrlEncode(Kvp.Key);
            const FString EncodedValue = FGenericPlatformHttp::UrlEncode(Kvp.Value);
            Pairs.Add(FString::Printf(TEXT("%s=%s"), *EncodedKey, *EncodedValue));
        }

        Url += TEXT("?");
        Url += FString::Join(Pairs, TEXT("&"));
    }

    return Url;
}

void UApiClient::ApplyIdentityHeaders(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request) const
{
    if (!PlayerId.IsEmpty())
    {
        Request->SetHeader(TEXT("X-Player-Id"), PlayerId);
    }

    const FString NickToSend = PlayerNickname.IsEmpty() ? PlayerId : PlayerNickname;
    if (!NickToSend.IsEmpty())
    {
        Request->SetHeader(TEXT("X-Player-Nickname"), NickToSend);
    }
}