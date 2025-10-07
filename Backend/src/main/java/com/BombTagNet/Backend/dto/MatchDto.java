package com.BombTagNet.Backend.dto;

import com.BombTagNet.Backend.dao.Player;

import java.util.List;

public class MatchDto {
    public record PlayerResult(String playerId, String result) {
    }

    public record MatchResultReq(String winnerId, List<PlayerResult> players) {
    }

    public record OkRes(boolean ok) {
    }

    public record MatchQueueStatusRes(
            String ticketId,
            String status,
            Integer position,
            Integer readyInSeconds,
            Integer waitForFourthSeconds,
            Integer minPlayers,
            Integer maxPlayers,
            String matchId,
            List<Player> players,
            String hostPlayerId,
            String hostAddress,
            Integer hostPort
    ) {
    }
}
