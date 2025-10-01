package com.BombTagNet.Backend.dto;

public class AuthDto {
    public record GuestLoginReq(String nickname) {
    }

    public record GuestLoginRes(String playerId, String nickname, String accessToken, int expiresIn) {
    }
}
