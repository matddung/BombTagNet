// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#ifndef BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL
#define BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL (!UE_SERVER && !UE_BUILD_SHIPPING)
#endif

#if BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL
#define BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Context) \
    do \
    { \
        static bool bBombTagWarned##Context = false; \
        if (!bBombTagWarned##Context) \
        { \
            bBombTagWarned##Context = true; \
            ensureMsgf(false, TEXT("Client-side travel is forbidden in dedicated mode (%s)"), TEXT(#Context)); \
        } \
    } while (false)
#else
#define BOMB_TAG_ENSURE_NO_CLIENT_TRAVEL(Context) ((void)0)
#endif

namespace BombTag
{
    namespace Logging
    {
        inline FString DescribeOptionalForLog(const FString& Value, const TCHAR* EmptyLabel = TEXT("<empty>"))
        {
            FString Trimmed = Value;
            Trimmed.TrimStartAndEndInline();

            if (Trimmed.IsEmpty())
            {
                return FString(EmptyLabel);
            }

            return Trimmed;
        }

        inline FString DescribeTokenForLog(const FString& Token)
        {
            FString Trimmed = Token;
            Trimmed.TrimStartAndEndInline();

            if (Trimmed.IsEmpty())
            {
                return FString(TEXT("<empty>"));
            }

            const int32 Length = Trimmed.Len();
            if (Length <= 12)
            {
                return FString::Printf(TEXT("%s (len=%d)"), *Trimmed, Length);
            }

            return FString::Printf(TEXT("%s...%s (len=%d)"), *Trimmed.Left(6), *Trimmed.Right(4), Length);
        }
    }

    namespace Json
    {
        inline int32 ParseDedicatedServerPort(const TSharedPtr<FJsonObject>& JsonObject)
        {
            if (!JsonObject.IsValid())
            {
                return 0;
            }

            int32 Port = 0;

            auto ApplyNumberField = [&Port, &JsonObject](const TCHAR* FieldName)
                {
                    if (Port > 0)
                    {
                        return;
                    }

                    double NumberValue = 0.0;
                    if (JsonObject->TryGetNumberField(FieldName, NumberValue))
                    {
                        Port = static_cast<int32>(NumberValue);
                    }
                };

            ApplyNumberField(TEXT("dedicatedServerPort"));
            ApplyNumberField(TEXT("port"));
            ApplyNumberField(TEXT("gamePort"));
            ApplyNumberField(TEXT("serverPort"));

            auto ApplyStringField = [&Port, &JsonObject](const TCHAR* FieldName)
                {
                    if (Port > 0)
                    {
                        return;
                    }

                    FString PortString;
                    if (JsonObject->TryGetStringField(FieldName, PortString))
                    {
                        Port = FCString::Atoi(*PortString);
                    }
                };

            ApplyStringField(TEXT("port"));
            ApplyStringField(TEXT("dedicatedServerPort"));

            return Port;
        }
    }
}