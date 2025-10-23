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

    void StartMatchTravel();
    bool HasHostAuthority(const class ABombTagPlayerController* RequestingController) const;
    bool VerifyWithBackend(const FString& RoomId, const FString& StartToken, FString& OutError) const;
    bool ValidateServerInstance(const FString& ExpectedAddress, int32 ExpectedPort, FString& OutActualEndpoint) const;
    void BroadcastServerEndpointAudit(const FString& ExpectedAddress, int32 ExpectedPort) const;

    UPROPERTY(EditAnywhere, Category = "Menu")
    TSubclassOf<UUserWidget> MenuClass;

    UPROPERTY(EditDefaultsOnly, Category = "Match")
    FString MatchTravelURL = TEXT("/Game/Maps/MainMap?game=/Game/Blueprints/BP_BombTagGameMode.BP_BombTagGameMode_C");
};