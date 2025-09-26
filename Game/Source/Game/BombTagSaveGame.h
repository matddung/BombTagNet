#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "BombTagSaveGame.generated.h"

UCLASS()
class GAME_API UBombTagSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
    UBombTagSaveGame();

    UPROPERTY(BlueprintReadWrite, Category = "Player Profile")
    FString Nickname = TEXT("");

    UPROPERTY(BlueprintReadWrite, Category = "Player Profile")
    int32 Win = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Player Profile")
    int32 Lose = 0;
};