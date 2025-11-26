#include "BombTagGameInstance.h"
#include "BombTagSaveGame.h"
#include "ApiClient.h"
#include "RoomService.h"
#include "BombTagPlayerController.h"
#include "Game.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CString.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    constexpr int32 PlayerSaveSlotIndex = 0;
    const TCHAR* PlayerSaveSlotName = TEXT("PlayerProfile");
    const TCHAR* DefaultBackendBaseUrl = TEXT("https://api.studyjun.net/api");

    bool IsBackendBaseUrlValid(FString& Url)
    {
        Url.TrimStartAndEndInline();

        if (Url.IsEmpty())
        {
            return false;
        }

        const FString SchemeDelimiter = TEXT("://");
        const int32 SchemeIndex = Url.Find(SchemeDelimiter, ESearchCase::IgnoreCase, ESearchDir::FromStart);

        if (SchemeIndex <= 0)
        {
            return false;
        }

        const FString Scheme = Url.Left(SchemeIndex);
        if (!Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase) && !Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        const int32 HostStartIndex = SchemeIndex + SchemeDelimiter.Len();
        if (HostStartIndex >= Url.Len())
        {
            return false;
        }

        if (Url.Mid(HostStartIndex, 1) == TEXT("/"))
        {
            return false;
        }

        return true;
    }

    FString NormalizeBackendBaseUrl(const FString& RawUrl, bool& bOutRewrote)
    {
        FString Url = RawUrl;
        Url.TrimStartAndEndInline();
        bOutRewrote = false;

        if (Url.IsEmpty())
        {
            return Url;
        }

        const FString SchemeDelimiter = TEXT("://");
        const int32 SchemeIndex = Url.Find(SchemeDelimiter, ESearchCase::IgnoreCase, ESearchDir::FromStart);
        if (SchemeIndex <= 0)
        {
            return Url;
        }

        const FString Scheme = Url.Left(SchemeIndex);
        const FString AfterScheme = Url.Mid(SchemeIndex + SchemeDelimiter.Len());

        FString Authority = AfterScheme;
        FString Path;
        const int32 SlashIndex = AfterScheme.Find(TEXT("/"));
        if (SlashIndex != INDEX_NONE)
        {
            Authority = AfterScheme.Left(SlashIndex);
            Path = AfterScheme.Mid(SlashIndex);
        }

        FString Host = Authority;
        FString Port;

        if (!Authority.IsEmpty())
        {
            const int32 ColonIndex = Authority.Find(TEXT(":"));
            if (ColonIndex != INDEX_NONE)
            {
                Host = Authority.Left(ColonIndex);
                Port = Authority.Mid(ColonIndex + 1);
            }
        }

        if (Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase))
        {
            bOutRewrote = true;
            return DefaultBackendBaseUrl;
        }

        if (Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
        {
            if (Port.Equals(TEXT("8080")))
            {
                bOutRewrote = true;
                if (!Host.IsEmpty())
                {
                    return FString::Printf(TEXT("https://%s%s"), *Host, *Path);
                }
                return DefaultBackendBaseUrl;
            }

            if (Host.Equals(TEXT("34.64.149.81"), ESearchCase::IgnoreCase))
            {
                bOutRewrote = true;
                return DefaultBackendBaseUrl;
            }
        }

        return Url;
    }
}

void UBombTagGameInstance::Init()
{
    Super::Init();

    const bool bIsDedicatedServer = IsRunningDedicatedServer();

    FString BackendBaseUrl;
    FString ConfigBackendBaseUrl;
    const bool bHasConfiguredUrl = GConfig->GetString(TEXT("Game.Net"), TEXT("BackendBaseUrl"), ConfigBackendBaseUrl, GGameIni);
    bool bRewroteConfiguredUrl = false;

    if (!bHasConfiguredUrl)
    {
        BackendBaseUrl = DefaultBackendBaseUrl;
    }
    else if (!IsBackendBaseUrlValid(ConfigBackendBaseUrl))
    {
        BackendBaseUrl = DefaultBackendBaseUrl;
    }
    else
    {
        const FString NormalizedUrl = NormalizeBackendBaseUrl(ConfigBackendBaseUrl, bRewroteConfiguredUrl);
        FString UrlToUse = NormalizedUrl;
        if (!IsBackendBaseUrlValid(UrlToUse))
        {
            BackendBaseUrl = DefaultBackendBaseUrl;
            bRewroteConfiguredUrl = false;
        }
        else
        {
            BackendBaseUrl = UrlToUse;
        }
    }

    if (bRewroteConfiguredUrl)
    {
        if (GConfig)
        {
            GConfig->SetString(TEXT("Game.Net"), TEXT("BackendBaseUrl"), *BackendBaseUrl, GGameIni);
            GConfig->Flush(false, GGameIni);
        }
    }

    float TimeoutSec = 10.0f;
    GConfig->GetFloat(TEXT("Game.Net"), TEXT("HttpTimeoutSec"), TimeoutSec, GGameIni);

    GConfig->GetString(TEXT("Game.DedicatedServer"), TEXT("DedicatedServerId"), DedicatedServerId, GGameIni);
    GConfig->GetString(TEXT("Game.DedicatedServer"), TEXT("PublicAddress"), DedicatedServerPublicAddress, GGameIni);
    GConfig->GetString(TEXT("Game.DedicatedServer"), TEXT("InternalAddress"), DedicatedServerInternalAddress, GGameIni);
    GConfig->GetInt(TEXT("Game.DedicatedServer"), TEXT("GamePort"), DedicatedServerGamePort, GGameIni);

    auto ApplyEnvOverride = [](const TCHAR* EnvVar, FString& Target, bool bTrim)
        {
            FString Value = FPlatformMisc::GetEnvironmentVariable(EnvVar);
            if (!Value.IsEmpty())
            {
                if (bTrim)
                {
                    Value.TrimStartAndEndInline();
                }
                Target = Value;
            }
        };

    ApplyEnvOverride(TEXT("BOMBTAG_DS_ID"), DedicatedServerId, true);
    ApplyEnvOverride(TEXT("BOMBTAG_DS_PUBLIC_ADDRESS"), DedicatedServerPublicAddress, true);
    ApplyEnvOverride(TEXT("BOMBTAG_DS_INTERNAL_ADDRESS"), DedicatedServerInternalAddress, true);

    auto ApplyEnvPort = [](const TCHAR* EnvVar, int32& Target)
        {
            FString Value = FPlatformMisc::GetEnvironmentVariable(EnvVar);
            if (!Value.IsEmpty())
            {
                Target = FCString::Atoi(*Value);
            }
        };

    ApplyEnvPort(TEXT("BOMBTAG_DS_GAME_PORT"), DedicatedServerGamePort);

    DedicatedServerId.TrimStartAndEndInline();
    DedicatedServerPublicAddress.TrimStartAndEndInline();
    DedicatedServerInternalAddress.TrimStartAndEndInline();

    if (bIsDedicatedServer && !DedicatedServerId.IsEmpty())
    {
        PlayerId = DedicatedServerId;
    }
    
    Api = NewObject<UApiClient>(this);
    if (Api)
    {
        Api->Init(BackendBaseUrl, TimeoutSec);
        TWeakObjectPtr<UBombTagGameInstance> WeakThis(this);
        Api->SetTrafficSink([WeakThis](const FString& Message)
            {
                if (WeakThis.IsValid())
                {
                    FTrafficMsg Msg;
                    Msg.Text = FText::FromString(Message);
                    WeakThis->HandleBackendTraffic(Msg);
                }
            });
    }

    Room = NewObject<URoomService>(this);
    if (Room && Api)
    {
        Room->Init(Api);
    }

    Match = NewObject<UMatchService>(this);
    if (Match && Api)
    {
        Match->Init(Api);
    }

    LoadOrCreatePlayerData();
    PlayerNickname = GetPlayerNickname();
    if (Api)
    {
        if (bIsDedicatedServer)
        {
            FString Identity = DedicatedServerId;
            if (Identity.IsEmpty())
            {
                Api->ClearLocalPlayerIdentity();
            }
            else
            {
                Api->SetLocalPlayerIdentity(Identity, Identity);
            }
        }
        else if (!PlayerNickname.IsEmpty())
        {
            Api->SetLocalPlayerIdentity(PlayerNickname, PlayerNickname);
        }
        else
        {
            Api->ClearLocalPlayerIdentity();
        }
    }

    if (bIsDedicatedServer)
    {
        NotifyBackendDedicatedServerReady();
    }

    BroadcastPlayerRecord();
}

void UBombTagGameInstance::SetPlayerNickname(const FString& NewNickname)
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    FString Name = NewNickname;
    Name.TrimStartAndEndInline();
    if (!IsValidNickname(Name))
    {
        return;
    }

    if (PlayerSaveGame->Nickname != Name)
    {
        PlayerSaveGame->Nickname = Name;
        SavePlayerData();
    }

    PlayerNickname = Name;
}

FString UBombTagGameInstance::GetPlayerNickname() const
{
    if (!PlayerSaveGame)
    {
        return FString();
    }

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    return Name;
}

bool UBombTagGameInstance::HasPlayerNickname() const
{
    if (!PlayerSaveGame)
    {
        return false;
    }

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    return !Name.IsEmpty();
}

void UBombTagGameInstance::RecordMatchResult(EBombTagMatchResult MatchResult)
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    if (MatchResult == EBombTagMatchResult::Win)
    {
        PlayerSaveGame->Win++;
    }
    else
    {
        PlayerSaveGame->Lose++;
    }

    SavePlayerData();
    BroadcastPlayerRecord();
}

void UBombTagGameInstance::GetPlayerRecord(int32& OutWin, int32& OutLose, int32& OutTotalMatches) const
{
    if (!PlayerSaveGame)
    {
        OutWin = OutLose = OutTotalMatches = 0;
        return;
    }

    OutWin = PlayerSaveGame->Win;
    OutLose = PlayerSaveGame->Lose;
    OutTotalMatches = OutWin + OutLose;
}

void UBombTagGameInstance::ResetPlayerRecord()
{
    LoadOrCreatePlayerData();
    if (!PlayerSaveGame) return;

    PlayerSaveGame->Win = 0;
    PlayerSaveGame->Lose = 0;
    SavePlayerData();
    BroadcastPlayerRecord();
}

void UBombTagGameInstance::LeaveSession()
{
    if (Room && !CurrentRoomId.IsEmpty())
    {
        const FString RoomIdToLeave = CurrentRoomId;
    }

    ResetCurrentSessionState();
}

void UBombTagGameInstance::Backend_Login(const FString& InNickname)
{
    FString NickToUse = InNickname;
    NickToUse.TrimStartAndEndInline();

    if (NickToUse.IsEmpty())
    {
        OnBackendLogin.Broadcast(false, TEXT("NICKNAME_REQUIRED"));
        return;
    }

    FString SavedNickname = GetPlayerNickname();
    if (SavedNickname.IsEmpty() || !SavedNickname.Equals(NickToUse, ESearchCase::CaseSensitive))
    {
        SetPlayerNickname(NickToUse);
        SavedNickname = GetPlayerNickname();
    }

    PlayerNickname = SavedNickname;
    PlayerId = PlayerNickname;

    if (Api)
    {
        Api->SetLocalPlayerIdentity(PlayerId, PlayerNickname);
    }

    OnBackendLogin.Broadcast(true, FString());
}

void UBombTagGameInstance::Backend_CreateRoom(const FString& Name, int32 MaxPlayers, const FString& Password)
{
    if (!Room)
    {
        OnRoomJoined.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    Room->CreateRoom(Name, MaxPlayers, Password, [this](bool bSuccess, const FRoomSummary& RoomSummary, const FString& Error)
        {
            if (!bSuccess)
            {
                OnRoomJoined.Broadcast(false, Error);
                return;
            }

            CurrentRoomId = RoomSummary.RoomId;

            bRoomHasStarted = false;

            OnRoomJoined.Broadcast(true, FString());
            OnRoomUpdated.Broadcast(RoomSummary);
        });
}

void UBombTagGameInstance::Backend_JoinRoom(const FString& RoomId, const FString& Password)
{
    if (!Room)
    {
        OnRoomJoined.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    Room->JoinRoom(RoomId, Password, [this](bool bSuccess, const FJoinRes& Result, const FString& Error)
        {
            if (!bSuccess)
            {
                OnRoomJoined.Broadcast(false, Error);
                return;
            }

            CurrentRoomId = Result.RoomId;

            bRoomHasStarted = false;

            OnRoomJoined.Broadcast(true, FString());

            FRoomSummary Summary;
            Summary.RoomId = Result.RoomId;
            Summary.Status = TEXT("WAITING");
            Summary.MinPlayers = 2;
            Summary.MaxPlayers = 4;
            Summary.CurrentPlayers = Result.Players.Num();
            Summary.Players = Result.Players;
            OnRoomUpdated.Broadcast(Summary);
        });
}

void UBombTagGameInstance::Backend_GetRoom()
{
    if (!Room)
    {
        return;
    }

    if (CurrentRoomId.IsEmpty())
    {
        return;
    }

    Room->GetRoom(CurrentRoomId, [this](bool bSuccess, const FRoomSummary& RoomSummary, const FString& Error)
        {
            if (!bSuccess)
            {
                if (Error.Contains(TEXT("ROOM_NOT_FOUND")))
                {
                    const FString LostRoomId = CurrentRoomId;
                    ResetCurrentSessionState();
                    OnRoomClosed.Broadcast(TEXT("ROOM_NOT_FOUND"));
                }
                return;
            }

            OnRoomUpdated.Broadcast(RoomSummary);

            if (RoomSummary.Status.Equals(TEXT("STARTED"), ESearchCase::IgnoreCase) && !bRoomHasStarted)
            {
                bRoomHasStarted = true;
                PrepareMatchLaunch(RoomSummary.DedicatedServerAddress, RoomSummary.DedicatedServerPort, RoomSummary.StartToken, RoomSummary.DedicatedServerId, RoomSummary.StartTokenExpiresAt);
                OnRoomStarted.Broadcast(true, RoomSummary.RoomId);
            }
        });
}

void UBombTagGameInstance::ResetCurrentSessionState()
{
    ResetMatchQueueState();
    CurrentRoomId.Reset();
    bRoomHasStarted = false;
}

void UBombTagGameInstance::ResetMatchQueueState(bool bPreservePendingMatchData)
{
    StopMatchQueuePolling();
    CurrentMatchTicketId.Reset();
    bHasMatchQueueStatus = false;
    CachedMatchQueueStatus = FMatchQueueStatus();
    bMatchQueueLaunched = false;

    if (!bPreservePendingMatchData)
    {
        ClearPendingMatchData();
    }
}

void UBombTagGameInstance::ClearPendingMatchData()
{
    StopMatchQueuePolling();
    CurrentMatchTicketId.Reset();
    bHasMatchQueueStatus = false;
    CachedMatchQueueStatus = FMatchQueueStatus();
    bMatchQueueLaunched = false;
    PendingMatchServerAddress.Reset();
    PendingMatchServerPort = 0;
    PendingMatchStartToken.Reset();
    PendingMatchDedicatedServerId.Reset();
    PendingMatchStartTokenExpiresAt.Reset();
    PendingMatchId.Reset();
}

void UBombTagGameInstance::StartMatchQueuePolling()
{
    if (UWorld* World = GetWorld())
    {
        if (!World->GetTimerManager().IsTimerActive(MatchQueuePollTimerHandle))
        {
            World->GetTimerManager().SetTimer(MatchQueuePollTimerHandle, this, &UBombTagGameInstance::Backend_QueryMatchQueueStatus, 1.0f, true, 1.0f);
        }
    }
}

void UBombTagGameInstance::StopMatchQueuePolling()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchQueuePollTimerHandle);
    }
}

void UBombTagGameInstance::HandleMatchQueueStatusResult(bool bSuccess, const FMatchQueueStatus& Status, const FString& ErrorMessage)
{
    if (!bSuccess)
    {
        StopMatchQueuePolling();
        OnMatchQueueStatus.Broadcast(false, Status, ErrorMessage);
        return;
    }

    OnMatchQueueStatus.Broadcast(true, Status, FString());

    if (!Status.TicketId.IsEmpty())
    {
        CurrentMatchTicketId = Status.TicketId;
    }

    CachedMatchQueueStatus = Status;
    bHasMatchQueueStatus = true;

    if (Status.Status == EMatchTicketStatus::Matched)
    {
        StopMatchQueuePolling();
        CurrentMatchTicketId.Reset();
        if (!bMatchQueueLaunched)
        {
            bMatchQueueLaunched = true;
            PendingMatchId = Status.MatchId;
            PrepareMatchLaunch(Status.DedicatedServerAddress, Status.DedicatedServerPort, Status.StartToken, Status.DedicatedServerId, Status.StartTokenExpiresAt);
            RequestServerMatchStart();
        }
        return;
    }

    if (Status.Status == EMatchTicketStatus::Cancelled)
    {
        StopMatchQueuePolling();
        ResetMatchQueueState();
        return;
    }

    if (Status.Status == EMatchTicketStatus::Unknown)
    {
        return;
    }

    StartMatchQueuePolling();
}

void UBombTagGameInstance::PrepareMatchLaunch(const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& StartToken, const FString& DSId, const FString& StartTokenExpiresAt)
{
    FString ResolvedAddress = DedicatedServerAddress;
    ResolvedAddress.TrimStartAndEndInline();

    PendingMatchServerAddress = ResolvedAddress;
    PendingMatchServerPort = DedicatedServerPort > 0 ? DedicatedServerPort : 0;
    PendingMatchStartToken = StartToken;
    PendingMatchDedicatedServerId = DSId;
    PendingMatchStartTokenExpiresAt = StartTokenExpiresAt;

#if !UE_BUILD_SHIPPING
    const FString AddressLabel = BombTag::Logging::DescribeOptionalForLog(ResolvedAddress);
    const FString DsLabel = BombTag::Logging::DescribeOptionalForLog(DSId);
    const FString ExpirationLabel = BombTag::Logging::DescribeOptionalForLog(StartTokenExpiresAt, TEXT("<unspecified>"));
    const FString TokenPreview = BombTag::Logging::DescribeTokenForLog(StartToken);
#endif
}

FString UBombTagGameInstance::GetPendingMatchTravelURL() const
{
    FString ServerAddress = PendingMatchServerAddress;
    ServerAddress.TrimStartAndEndInline();

    if (ServerAddress.IsEmpty())
    {
        return FString();
    }

    FString TravelURL = ServerAddress;
    if (PendingMatchServerPort > 0)
    {
        TravelURL = FString::Printf(TEXT("%s:%d"), *ServerAddress, PendingMatchServerPort);
    }

    TArray<FString> Options;

    const FString MatchIdentifier = !PendingMatchId.IsEmpty() ? PendingMatchId : CurrentRoomId;
    if (!MatchIdentifier.IsEmpty())
    {
        Options.Add(FString::Printf(TEXT("matchId=%s"), *MatchIdentifier));
    }

    if (!PendingMatchStartToken.IsEmpty())
    {
        Options.Add(FString::Printf(TEXT("startToken=%s"), *PendingMatchStartToken));
    }

    if (!Options.IsEmpty())
    {
        TravelURL.AppendChar('?');
        TravelURL.Append(FString::Join(Options, TEXT("&")));
    }

    return TravelURL;
}

void UBombTagGameInstance::Backend_StartRoom()
{
    if (!Room)
    {
        OnRoomStarted.Broadcast(false, TEXT("NOT_INITIALIZED"));
        return;
    }

    if (CurrentRoomId.IsEmpty())
    {
        OnRoomStarted.Broadcast(false, TEXT("NO_ROOM"));
        return;
    }

    Room->StartRoom(CurrentRoomId, [this](bool bSuccess, const FMatchStartInfo& Info, const FString& Error)
        {
            if (!bSuccess)
            {
                OnRoomStarted.Broadcast(false, Error);
                return;
            }

            bRoomHasStarted = true;
            PendingMatchId = Info.MatchId;
            PrepareMatchLaunch(Info.DedicatedServerAddress, Info.DedicatedServerPort, Info.StartToken, Info.DedicatedServerId, Info.StartTokenExpiresAt);
            OnRoomStarted.Broadcast(true, Info.MatchId);
        });
}

void UBombTagGameInstance::Backend_JoinMatchQueue()
{
    if (!Match)
    {
        OnMatchQueueStatus.Broadcast(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        return;
    }

    if (bMatchQueueLaunched && CurrentMatchTicketId.IsEmpty())
    {
        ResetMatchQueueState(/*bPreservePendingMatchData=*/true);
    }

    if (!CurrentMatchTicketId.IsEmpty())
    {
        Backend_QueryMatchQueueStatus();
        return;
    }

    Match->JoinQueue([this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::Backend_LeaveMatchQueue()
{
    if (!Match)
    {
        OnMatchQueueStatus.Broadcast(false, FMatchQueueStatus(), TEXT("NOT_INITIALIZED"));
        return;
    }

    if (CurrentMatchTicketId.IsEmpty())
    {
        ResetMatchQueueState();
        return;
    }

    const FString TicketToCancel = CurrentMatchTicketId;
    Match->CancelQueue(TicketToCancel, [this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::Backend_QueryMatchQueueStatus()
{
    if (!Match || CurrentMatchTicketId.IsEmpty())
    {
        return;
    }

    const FString Ticket = CurrentMatchTicketId;
    Match->GetQueueStatus(Ticket, [this](bool bSuccess, const FMatchQueueStatus& Status, const FString& Error)
        {
            HandleMatchQueueStatusResult(bSuccess, Status, Error);
        });
}

void UBombTagGameInstance::RequestServerMatchStart()
{
    const FString RoomIdentifier = CurrentRoomId;

    if (RoomIdentifier.IsEmpty())
    {
        return;
    }

    const FString TravelURL = GetPendingMatchTravelURL();
    if (TravelURL.IsEmpty())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (ABombTagPlayerController* BTPC = Cast<ABombTagPlayerController>(PC))
            {
                OnBackendTraffic.Broadcast(FTrafficMsgFactory::MakeStartRequestInfo(RoomIdentifier, PendingMatchServerAddress, PendingMatchServerPort, TravelURL));
                const FString DsLabel = !PendingMatchDedicatedServerId.IsEmpty() ? PendingMatchDedicatedServerId : TEXT("<unknown-ds>");
                BTPC->ServerRequestStartMatch(RoomIdentifier, PendingMatchStartToken, PendingMatchServerAddress, PendingMatchServerPort, TravelURL);
            }
            else
            {
                return;
            }
        }
    }
}

void UBombTagGameInstance::LoadOrCreatePlayerData()
{
    if (PlayerSaveGame) return;

    if (UGameplayStatics::DoesSaveGameExist(PlayerSaveSlotName, PlayerSaveSlotIndex))
    {
        PlayerSaveGame = Cast<UBombTagSaveGame>(UGameplayStatics::LoadGameFromSlot(PlayerSaveSlotName, PlayerSaveSlotIndex));
    }

    if (!PlayerSaveGame)
    {
        PlayerSaveGame = Cast<UBombTagSaveGame>(UGameplayStatics::CreateSaveGameObject(UBombTagSaveGame::StaticClass()));
        if (PlayerSaveGame)
        {
            SavePlayerData();
        }
    }

    EnsureNicknameIsValid();
}

void UBombTagGameInstance::SavePlayerData()
{
    if (PlayerSaveGame)
    {
        UGameplayStatics::SaveGameToSlot(PlayerSaveGame, PlayerSaveSlotName, PlayerSaveSlotIndex);
    }
}

void UBombTagGameInstance::BroadcastPlayerRecord()
{
    if (!PlayerSaveGame)
    {
        return;
    }

    const int32 WinCount = PlayerSaveGame->Win;
    const int32 LoseCount = PlayerSaveGame->Lose;
    OnPlayerRecordUpdated.Broadcast(WinCount, LoseCount, WinCount + LoseCount);
}

void UBombTagGameInstance::HandleBackendTraffic(const FTrafficMsg& Message)
{
#if !UE_BUILD_SHIPPING
#endif
    OnBackendTraffic.Broadcast(Message);
}

bool UBombTagGameInstance::IsRunningDedicatedServer() const
{
    if (const UWorld* World = GetWorld())
    {
        return World->IsNetMode(NM_DedicatedServer);
    }

    return false;
}

void UBombTagGameInstance::NotifyBackendDedicatedServerReady()
{
    if (!IsRunningDedicatedServer())
    {
        return;
    }

    if (!Api)
    {
        return;
    }

    if (DedicatedServerId.IsEmpty())
    {
        return;
    }

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("dsId"), DedicatedServerId);

    if (!DedicatedServerPublicAddress.IsEmpty())
    {
        Payload->SetStringField(TEXT("publicAddress"), DedicatedServerPublicAddress);
    }

    if (!DedicatedServerInternalAddress.IsEmpty())
    {
        Payload->SetStringField(TEXT("internalAddress"), DedicatedServerInternalAddress);
    }

    if (DedicatedServerGamePort > 0)
    {
        Payload->SetNumberField(TEXT("gamePort"), DedicatedServerGamePort);
    }

    Payload->SetStringField(TEXT("status"), TEXT("READY"));

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

    Api->PostJson(TEXT("/ds/register"), Content, FOnApiResponse());
}

void UBombTagGameInstance::EnsureNicknameIsValid()
{
    if (!PlayerSaveGame) return;

    FString Name = PlayerSaveGame->Nickname;
    Name.TrimStartAndEndInline();
    if (!Name.IsEmpty() && !IsValidNickname(Name))
    {
        PlayerSaveGame->Nickname.Empty();
        SavePlayerData();
    }
    else if (PlayerSaveGame->Nickname != Name)
    {
        PlayerSaveGame->Nickname = Name;
        SavePlayerData();
    }
}

bool UBombTagGameInstance::IsValidNickname(const FString& Nickname) const
{
    int32 L = Nickname.Len();
    if (L < 4 || L > 10) return false;
    for (TCHAR C : Nickname)
    {
        if (!IsAsciiAlphanumeric(C)) return false;
    }
    return true;
}

bool UBombTagGameInstance::IsAsciiAlphanumeric(TCHAR Character) const
{
    return (Character >= '0' && Character <= '9') ||
        (Character >= 'A' && Character <= 'Z') ||
        (Character >= 'a' && Character <= 'z');
}

void UBombTagGameInstance::Deprecated_ClientTravelToMatch()
{
    BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Deprecated_ClientTravelToMatch);
}

void UBombTagGameInstance::Deprecated_ClientReturnToMenu()
{
    BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Deprecated_ClientReturnToMenu);
}