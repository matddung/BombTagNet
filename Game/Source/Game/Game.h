// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#ifndef BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL
// 개발/테스트 환경에서는 클라이언트 트래블 호출을 즉시 감지해 로그를 남기고,
// 서버 전용 또는 셰핑 빌드에서는 비활성화한다.
#define BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL (!UE_SERVER && !UE_BUILD_SHIPPING)
#endif

#if BOMB_TAG_ENSURE_FORBID_CLIENT_TRAVEL
// 특정 호출 지점(Context)에서 클라이언트 트래블 시도를 한 번만 ensure로 경고한다.
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