#include "GameModeTravelUtils.h"
#include "BombTagGameInstance.h"

#include "Kismet/GameplayStatics.h"

namespace BombTag
{
    namespace GameMode
    {
        void ExtractTravelTargets(const FString& URL, FString& OutMap, FString& OutGameMode)
        {
            OutMap = URL;
            OutGameMode.Reset();

            FString MapPart;
            FString Options;
            if (URL.Split(TEXT("?"), &MapPart, &Options))
            {
                OutMap = MapPart;
                const FString GameModeOption = UGameplayStatics::ParseOption(Options, TEXT("game"));
                if (!GameModeOption.IsEmpty())
                {
                    OutGameMode = GameModeOption;
                }
            }
        }

        FString ResolveDedicatedServerLabel(const UBombTagGameInstance* GameInstance)
        {
            if (!GameInstance)
            {
                return FString(TEXT("UNKNOWN_DEDICATED_SERVER"));
            }

            FString DsId = GameInstance->GetPendingMatchDedicatedServerId();
            if (DsId.IsEmpty())
            {
                DsId = GameInstance->GetDedicatedServerId();
            }

            if (!DsId.IsEmpty())
            {
                return DsId;
            }

            FString Address = GameInstance->GetPendingMatchServerAddress();
            if (!Address.IsEmpty())
            {
                return Address;
            }

            FString LocalId = GameInstance->GetLocalPlayerId();
            return LocalId.IsEmpty() ? FString(TEXT("UNKNOWN_DEDICATED_SERVER")) : LocalId;
        }
    }
}