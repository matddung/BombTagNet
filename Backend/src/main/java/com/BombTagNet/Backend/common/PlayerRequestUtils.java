package com.BombTagNet.Backend.common;

import jakarta.servlet.http.HttpServletRequest;

public final class PlayerRequestUtils {
    private PlayerRequestUtils() {
    }

    public static String requirePlayerId(HttpServletRequest request) {
        if (request == null) {
            throw new IllegalStateException("PLAYER_ID_REQUIRED");
        }
        String id = request.getHeader("X-Player-Id");
        if (id == null || id.isBlank()) {
            throw new IllegalStateException("PLAYER_ID_REQUIRED");
        }
        return id.trim();
    }

    public static String resolveNickname(HttpServletRequest request) {
        if (request == null) {
            throw new IllegalStateException("PLAYER_NICKNAME_REQUIRED");
        }
        String nickname = request.getHeader("X-Player-Nickname");
        if (nickname == null || nickname.isBlank()) {
            return requirePlayerId(request);
        }
        return nickname.trim();
    }
}
