package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.dao.Room;
import com.BombTagNet.Backend.dto.RoomDto.*;
import com.BombTagNet.Backend.service.RoomService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/rooms")
public class RoomController {
    private final RoomService rooms;

    public RoomController(RoomService rooms) {
        this.rooms = rooms;
    }

    private String pid(HttpServletRequest request) {
        if (request == null) {
            throw new IllegalStateException("PLAYER_ID_REQUIRED");
        }
        String id = request.getHeader("X-Player-Id");
        if (id == null || id.isBlank()) {
            throw new IllegalStateException("PLAYER_ID_REQUIRED");
        }
        return id.trim();
    }

    private String nickname(HttpServletRequest request) {
        if (request == null) {
            throw new IllegalStateException("PLAYER_NICKNAME_REQUIRED");
        }
        String nickname = request.getHeader("X-Player-Nickname");
        if (nickname == null || nickname.isBlank()) {
            return pid(request);
        }
        return nickname.trim();
    }

    @PostMapping
    public ResponseEntity<RoomSummary> create(HttpServletRequest request, @RequestBody CreateRoomReq req) {
        String playerId = pid(request);
        Room r = rooms.create(playerId, req.name(), req.maxPlayers() == null ? 4 : req.maxPlayers(), req.password(),
                request == null ? null : request.getRemoteAddr());
        Player host = new Player(playerId, nickname(request));
        r.add(host);
        List<Player> players = List.copyOf(r.players());
        return ResponseEntity.ok(new RoomSummary(r.roomId(), r.name(), r.hostId(), r.status(), 2, r.maxPlayers(), r.size(), players,
                r.hostAddress(), r.hostPort()));
    }

    @PostMapping("/{roomId}/join")
    public ResponseEntity<JoinRoomRes> join(HttpServletRequest request, @PathVariable String roomId, @RequestBody(required = false) JoinRoomReq req) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        String playerId = pid(request);
        String nick = nickname(request);
        rooms.join(r, new Player(playerId, nick), req == null ? null : req.password());
        int slot = r.size();
        return ResponseEntity.ok(new JoinRoomRes(r.roomId(), slot, List.copyOf(r.players())));
    }

    @PostMapping("/{roomId}/leave")
    public ResponseEntity<?> leave(HttpServletRequest request, @PathVariable String roomId) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        String playerId = pid(request);
        rooms.leave(r, playerId);
        return ResponseEntity.ok().build();
    }

    @GetMapping("/{roomId}")
    public ResponseEntity<RoomDetail> get(HttpServletRequest request, @PathVariable String roomId) {
        pid(request);
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        return ResponseEntity.ok(new RoomDetail(r.roomId(), r.name(), r.status(), 2, r.maxPlayers(), r.size(), List.copyOf(r.players()),
                r.hostId(), r.hostAddress(), r.hostPort()));
    }

    @PostMapping("/{roomId}/start")
    public ResponseEntity<?> start(HttpServletRequest request, @PathVariable String roomId) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        rooms.start(r, pid(request), 2);
        if (request != null) {
            r.updateHostEndpoint(request.getRemoteAddr(), r.hostPort());
        }
        return ResponseEntity.ok().body(java.util.Map.of(
                "matchId", "m_" + System.currentTimeMillis(),
                "map", "MainMap",
                "seed", 123456,
                "hostPlayerId", r.hostId(),
                "hostAddress", r.hostAddress(),
                "hostPort", r.hostPort()
        ));
    }

    @ExceptionHandler(IllegalStateException.class)
    public ResponseEntity<?> bad(IllegalStateException e) {
        return ResponseEntity.status(HttpStatus.CONFLICT).body(java.util.Map.of("code", e.getMessage()));
    }
}