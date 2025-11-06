#pragma once

#include "CoreMinimal.h"

class UBombTagGameInstance;

namespace BombTag
{
    namespace GameMode
    {
        void ExtractTravelTargets(const FString& URL, FString& OutMap, FString& OutGameMode);

        FString ResolveDedicatedServerLabel(const UBombTagGameInstance* GameInstance);
    }
}