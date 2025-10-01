package com.BombTagNet.Backend.dto;

import java.util.List;

public class MatchDto {
    public record PlayerResult(String playerId, String result) {
    }

    public record MatchResultReq(String winnerId, List<PlayerResult> players) {
    }

    public record OkRes(boolean ok) {
    }
}
