#include "ApiClient.h"

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

void UApiClient::Get(const FString& Path, FOnApiResponse Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("GET"), Path);
    EmitTraffic(FString::Printf(TEXT("-> GET %s"), *Request->GetURL()));
    ProcessRequest(Request, MoveTemp(Callback));
}

void UApiClient::PostJson(const FString& Path, const FString& Body, FOnApiResponse Callback)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("POST"), Path);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(Body);
    const FString BodyPreview = Body.Len() > 256 ? Body.Left(256) + TEXT("...") : Body;
    EmitTraffic(FString::Printf(TEXT("-> POST %s Body=%s"), *Request->GetURL(), *BodyPreview));
    ProcessRequest(Request, MoveTemp(Callback));
}

void UApiClient::SetTrafficSink(TFunction<void(const FString&)> InLogger)
{
    TrafficLogger = MoveTemp(InLogger);
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UApiClient::CreateRequest(const FString& Verb, const FString& Path)
{
    FHttpModule& Module = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Module.CreateRequest();

    Request->SetVerb(Verb);
    Request->SetURL(BuildUrl(Path));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetTimeout(TimeoutSeconds);

    ApplyIdentityHeaders(Request);

    return Request;
}

void UApiClient::ProcessRequest(TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request, FOnApiResponse Callback)
{
    const FString Verb = Request->GetVerb();
    const FString Url = Request->GetURL();

    Request->OnProcessRequestComplete().BindLambda([this, Callback = MoveTemp(Callback), Verb, Url](FHttpRequestPtr HttpRequest, FHttpResponsePtr Response, bool bConnectedSuccessfully) mutable
        {
            if (!Callback.IsBound())
            {
                return;
            }

            if (!bConnectedSuccessfully || !Response.IsValid())
            {
                EmitTraffic(FString::Printf(TEXT("<- %s %s NETWORK_ERROR"), *Verb, *Url));
                Callback.Execute(false, TEXT("NETWORK_ERROR"));
                return;
            }

            const int32 StatusCode = Response->GetResponseCode();
            const FString ResponseBody = Response->GetContentAsString();
            const FString BodyPreview = ResponseBody.Len() > 256 ? ResponseBody.Left(256) + TEXT("...") : ResponseBody;
            EmitTraffic(FString::Printf(TEXT("<- %s %s %d %s"), *Verb, *Url, StatusCode, *BodyPreview));
            if (StatusCode >= 200 && StatusCode < 300)
            {
                Callback.Execute(true, ResponseBody);
            }
            else
            {
                const FString ErrorMessage = FString::Printf(TEXT("HTTP_%d:%s"), StatusCode, *ResponseBody);
                Callback.Execute(false, ErrorMessage);
            }
        });

    Request->ProcessRequest();
}

FString UApiClient::BuildUrl(const FString& Path) const
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

void UApiClient::EmitTraffic(const FString& Message) const
{
    if (TrafficLogger)
    {
        TrafficLogger(Message);
    }
}