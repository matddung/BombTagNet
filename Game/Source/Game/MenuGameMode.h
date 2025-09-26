#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

class APlayerController;
class UUserWidget;

UCLASS()
class GAME_API AMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
    AMenuGameMode();

protected:
    virtual void PostLogin(APlayerController* NewPlayer) override;

    UPROPERTY(EditAnywhere, Category = "Menu")
    TSubclassOf<UUserWidget> MenuClass;
};