package com.BombTagNet.Backend.dto;

import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.common.RoomStatus;

import java.util.List;

public class RoomDto {
    public record CreateRoomReq(String name, Integer maxPlayers, String password) {
    }

    public record RoomSummary(String roomId, String name, String hostId, RoomStatus status, Integer minPlayers,
                              Integer maxPlayers, Integer currentPlayers, List<Player> players,
                              String hostAddress, Integer hostPort) {
    }

    public record JoinRoomReq(String password) {
    }

    public record JoinRoomRes(String roomId, Integer slot, List<Player> players) {
    }

    public record RoomDetail(String roomId, String name, RoomStatus status, Integer minPlayers, Integer maxPlayers,
                             Integer currentPlayers, List<Player> players, String hostId, String hostAddress, Integer hostPort) {
    }
}