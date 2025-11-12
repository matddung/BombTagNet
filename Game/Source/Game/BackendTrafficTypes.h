#pragma once

#include "CoreMinimal.h"
#include "BackendTrafficTypes.generated.h"

UENUM(BlueprintType)
enum class ETrafficSeverity : uint8
{
    Info,
    Success,
    Warn,
    Error
};

USTRUCT(BlueprintType)
struct GAME_API FTrafficMsg
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETrafficSeverity Severity = ETrafficSeverity::Info;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHostOnly = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TTLSeconds = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Key;
};

struct GAME_API FTrafficMsgFactory
{
    static FString MaskStr(const FString& S)
    {
        if (S.Len() <= 8)
        {
            return TEXT("<hidden>");
        }

        return S.Left(4) + TEXT("…") + S.Right(3);
    }

    static FTrafficMsg MakeStartRequestInfo(const FString& RoomId, const FString& Addr, int32 Port, const FString& Url)
    {
        FTrafficMsg M;
        M.Severity = ETrafficSeverity::Info;
        M.bHostOnly = true;
        M.TTLSeconds = 6.f;
        M.Key = TEXT("start.request");
        M.Text = FText::FromString(FString::Printf(
            TEXT("매치 시작 요청 중…\nroom=%s addr=%s port=%d url=%s"),
            *MaskStr(RoomId),
            *MaskStr(Addr),
            Port,
            *MaskStr(Url)));
        return M;
    }

    static FTrafficMsg MakeStartApproved()
    {
        FTrafficMsg M;
        M.Severity = ETrafficSeverity::Success;
        M.bHostOnly = false;
        M.TTLSeconds = 4.f;
        M.Key = TEXT("start.approved");
        M.Text = FText::FromString(TEXT("매치 시작 승인됨. 서버로 이동 중…"));
        return M;
    }

    static FTrafficMsg MakeDenied7(const FString& FinalAddr, int32 FinalPort, const FString& FinalURL)
    {
        FTrafficMsg M;
        M.Severity = ETrafficSeverity::Error;
        M.bHostOnly = true;
        M.TTLSeconds = 0.f;
        M.Key = TEXT("start.denied.7");
        M.Text = FText::FromString(FString::Printf(
            TEXT("매치 시작 실패(코드 7)\n서버 정보가 불완전합니다.\naddr=%s port=%d url=%s"),
            *MaskStr(FinalAddr),
            FinalPort,
            *MaskStr(FinalURL)));
        return M;
    }
};