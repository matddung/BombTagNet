#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AuthService.generated.h"

class UApiClient;

USTRUCT(BlueprintType)
struct GAME_API FGuestLoginRes
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Auth")
    FString PlayerId;

    UPROPERTY(BlueprintReadOnly, Category = "Auth")
    FString Nickname;

    UPROPERTY(BlueprintReadOnly, Category = "Auth")
    FString AccessToken;

    UPROPERTY(BlueprintReadOnly, Category = "Auth")
    int32 ExpiresIn = 0;
};

UCLASS()
class GAME_API UAuthService : public UObject
{
    GENERATED_BODY()

public:
    void Init(UApiClient* InApi);

    void GuestLogin(const FString& Nickname, TFunction<void(bool bSuccess, const FGuestLoginRes& Response, const FString& Error)> Callback);

private:
    UPROPERTY()
    TObjectPtr<UApiClient> ApiClient;
};