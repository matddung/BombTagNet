package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.common.PlayerRequestUtils;
import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.dao.Room;
import com.BombTagNet.Backend.dto.RoomDto.*;
import com.BombTagNet.Backend.service.RoomService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/rooms")
public class RoomController {
    private static final int MIN_PLAYERS = 2;

    private final RoomService rooms;

    public RoomController(RoomService rooms) {
        this.rooms = rooms;
    }

    @PostMapping
    public ResponseEntity<RoomSummary> create(HttpServletRequest request, @RequestBody CreateRoomReq req) {
        String playerId = PlayerRequestUtils.requirePlayerId(request);
        Room room = rooms.create(playerId, req.name(), req.maxPlayers() == null ? 4 : req.maxPlayers(), req.password(),
                request == null ? null : request.getRemoteAddr());
        Player host = new Player(playerId, PlayerRequestUtils.resolveNickname(request));
        room.add(host);
        return ResponseEntity.ok(toSummary(room));
    }

    @PostMapping("/{roomId}/join")
    public ResponseEntity<JoinRoomRes> join(HttpServletRequest request, @PathVariable String roomId, @RequestBody(required = false) JoinRoomReq req) {
        Room room = requireRoom(roomId);
        String playerId = PlayerRequestUtils.requirePlayerId(request);
        String nick = PlayerRequestUtils.resolveNickname(request);
        rooms.join(room, new Player(playerId, nick), req == null ? null : req.password());
        int slot = room.size();
        return ResponseEntity.ok(new JoinRoomRes(room.roomId(), slot, snapshotPlayers(room)));
    }

    @PostMapping("/{roomId}/leave")
    public ResponseEntity<?> leave(HttpServletRequest request, @PathVariable String roomId) {
        Room room = requireRoom(roomId);
        String playerId = PlayerRequestUtils.requirePlayerId(request);
        rooms.leave(room, playerId);
        return ResponseEntity.ok().build();
    }

    @GetMapping("/{roomId}")
    public ResponseEntity<RoomDetail> get(HttpServletRequest request, @PathVariable String roomId) {
        PlayerRequestUtils.requirePlayerId(request);
        return ResponseEntity.ok(toDetail(requireRoom(roomId)));
    }

    @PostMapping("/{roomId}/start")
    public ResponseEntity<?> start(HttpServletRequest request, @PathVariable String roomId) {
        Room room = requireRoom(roomId);
        rooms.start(room, PlayerRequestUtils.requirePlayerId(request), MIN_PLAYERS);
        if (request != null) {
            room.updateHostEndpoint(request.getRemoteAddr(), room.hostPort());
        }
        return ResponseEntity.ok().body(java.util.Map.of(
                "matchId", "m_" + System.currentTimeMillis(),
                "map", "MainMap",
                "seed", 123456,
                "hostPlayerId", room.hostId(),
                "hostAddress", room.hostAddress(),
                "hostPort", room.hostPort()
        ));
    }

    private Room requireRoom(String roomId) {
        return rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
    }

    private List<Player> snapshotPlayers(Room room) {
        return List.copyOf(room.players());
    }

    private RoomSummary toSummary(Room room) {
        List<Player> players = snapshotPlayers(room);
        return new RoomSummary(room.roomId(), room.name(), room.hostId(), room.status(), MIN_PLAYERS, room.maxPlayers(),
                room.size(), players, room.hostAddress(), room.hostPort());
    }

    private RoomDetail toDetail(Room room) {
        List<Player> players = snapshotPlayers(room);
        return new RoomDetail(room.roomId(), room.name(), room.status(), MIN_PLAYERS, room.maxPlayers(), room.size(),
                players, room.hostId(), room.hostAddress(), room.hostPort());
    }
}