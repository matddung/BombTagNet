#pragma once

#include "CoreMinimal.h"

#include "MatchResultTypes.generated.h"

USTRUCT(BlueprintType)
struct FBombTagPlayerMatchResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    FString PlayerName;

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    bool bIsWinner = false;
};

USTRUCT(BlueprintType)
struct FBombTagMatchResultSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Match")
    TArray<FBombTagPlayerMatchResult> PlayerResults;

    FString BuildCanonicalSignature() const;

    bool IsPlayerWinner(const FString& PlayerName) const;

    bool IsEmpty() const
    {
        return PlayerResults.Num() == 0;
    }

    int32 NumPlayers() const
    {
        return PlayerResults.Num();
    }
};

inline FString FBombTagMatchResultSnapshot::BuildCanonicalSignature() const
{
    TArray<FBombTagPlayerMatchResult> SortedResults = PlayerResults;
    SortedResults.Sort([](const FBombTagPlayerMatchResult& A, const FBombTagPlayerMatchResult& B)
        {
            return A.PlayerName < B.PlayerName;
        });

    FString Signature;
    Signature.Reserve(SortedResults.Num() * 16);

    for (const FBombTagPlayerMatchResult& Entry : SortedResults)
    {
        Signature += Entry.PlayerName;
        Signature += TEXT(":");
        Signature += Entry.bIsWinner ? TEXT("1") : TEXT("0");
        Signature += TEXT("|");
    }

    return Signature;
}

inline bool FBombTagMatchResultSnapshot::IsPlayerWinner(const FString& PlayerName) const
{
    for (const FBombTagPlayerMatchResult& Entry : PlayerResults)
    {
        if (Entry.PlayerName.Equals(PlayerName, ESearchCase::IgnoreCase))
        {
            return Entry.bIsWinner;
        }
    }

    return false;
}