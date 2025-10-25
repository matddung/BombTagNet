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

        FString ResolveHostId(const UBombTagGameInstance* GameInstance)
        {
            if (!GameInstance)
            {
                return FString(TEXT("UNKNOWN_HOST"));
            }

            const FString HostId = GameInstance->GetEffectiveHostPlayerId();
            return HostId.IsEmpty() ? FString(TEXT("UNKNOWN_HOST")) : HostId;
        }
    }
}