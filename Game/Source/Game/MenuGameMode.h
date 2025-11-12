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
    void HandleStartMatchRequest(class ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL);

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;

    void SendClientsToMatch(const FString& TravelURL);
    void VerifyStartTokenWithBackend(class ABombTagPlayerController* RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL);
    void HandleVerifyStartTokenResponse(TWeakObjectPtr<class ABombTagPlayerController> RequestingController, const FString& RoomId, const FString& StartToken, const FString& DedicatedServerAddress, int32 DedicatedServerPort, const FString& TravelURL, bool bOk, const FString& BodyOrError);
    FString ResolveMatchIdentifierForVerification(const class UBombTagGameInstance* GameInstance, const FString& RoomId) const;

    UPROPERTY(EditAnywhere, Category = "Menu")
    TSubclassOf<UUserWidget> MenuClass;
};