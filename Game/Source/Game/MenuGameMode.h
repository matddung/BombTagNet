#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MenuGameMode.generated.h"

class APlayerController;
class UUserWidget;
class ABombTagPlayerController;

UCLASS()
class GAME_API AMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AMenuGameMode();
    void HandleStartMatchRequest(class ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken);

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    void SendClientsToMatch(const FString& TravelURL);
    bool HasHostAuthority(const class ABombTagPlayerController* RequestingController) const;
    bool VerifyWithBackend(const FString& RoomId, const FString& StartToken, FString& OutError) const;

    UPROPERTY(EditAnywhere, Category = "Menu")
    TSubclassOf<UUserWidget> MenuClass;
};