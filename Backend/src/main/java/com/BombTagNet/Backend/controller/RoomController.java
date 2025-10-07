package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.dao.Room;
import com.BombTagNet.Backend.dto.RoomDto.*;
import com.BombTagNet.Backend.jwt.JwtAuthFilter.PlayerPrincipal;
import com.BombTagNet.Backend.service.RoomService;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.*;

import java.util.List;

@RestController
@RequestMapping("/api/rooms")
public class RoomController {
    private final RoomService rooms;

    public RoomController(RoomService rooms) {
        this.rooms = rooms;
    }

    private String pid(Authentication auth) {
        if (auth == null) {
            throw new IllegalStateException("UNAUTHENTICATED");
        }
        Object principal = auth.getPrincipal();
        if (principal instanceof PlayerPrincipal pp) {
            return pp.playerId();
        }
        if (principal instanceof String s) {
            return s;
        }
        return principal == null ? null : principal.toString();
    }

    private String nicknameFromAuth(Authentication auth) {
        if (auth == null) {
            throw new IllegalStateException("UNAUTHENTICATED");
        }
        Object principal = auth.getPrincipal();
        if (principal instanceof PlayerPrincipal pp) {
            String nickname = pp.nickname();
            if (nickname != null && !nickname.isBlank()) {
                return nickname;
            }
            return pp.playerId();
        }
        Object details = auth.getDetails();
        if (details instanceof PlayerPrincipal pp) {
            String nickname = pp.nickname();
            if (nickname != null && !nickname.isBlank()) {
                return nickname;
            }
            return pp.playerId();
        }
        return pid(auth);
    }

    @PostMapping
    public ResponseEntity<RoomSummary> create(Authentication auth, HttpServletRequest request, @RequestBody CreateRoomReq req) {
        Room r = rooms.create(pid(auth), req.name(), req.maxPlayers() == null ? 4 : req.maxPlayers(), req.password(),
                request == null ? null : request.getRemoteAddr());
        Player host = new Player(pid(auth), nicknameFromAuth(auth));
        r.add(host);
        List<Player> players = List.copyOf(r.players());
        return ResponseEntity.ok(new RoomSummary(r.roomId(), r.name(), r.hostId(), r.status(), 2, r.maxPlayers(), r.size(), players,
                r.hostAddress(), r.hostPort()));
    }

    @PostMapping("/{roomId}/join")
    public ResponseEntity<JoinRoomRes> join(Authentication auth, @PathVariable String roomId, @RequestBody(required = false) JoinRoomReq req) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        String nick = nicknameFromAuth(auth);
        rooms.join(r, new Player(pid(auth), nick), req == null ? null : req.password());
        int slot = r.size();
        return ResponseEntity.ok(new JoinRoomRes(r.roomId(), slot, List.copyOf(r.players())));
    }

    @PostMapping("/{roomId}/leave")
    public ResponseEntity<?> leave(Authentication auth, @PathVariable String roomId) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        rooms.leave(r, pid(auth));
        return ResponseEntity.ok().build();
    }

    @GetMapping("/{roomId}")
    public ResponseEntity<RoomDetail> get(Authentication auth, @PathVariable String roomId) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        return ResponseEntity.ok(new RoomDetail(r.roomId(), r.name(), r.status(), 2, r.maxPlayers(), r.size(), List.copyOf(r.players()),
                r.hostId(), r.hostAddress(), r.hostPort()));
    }

    @PostMapping("/{roomId}/start")
    public ResponseEntity<?> start(Authentication auth, HttpServletRequest request, @PathVariable String roomId) {
        Room r = rooms.find(roomId).orElseThrow(() -> new IllegalStateException("ROOM_NOT_FOUND"));
        rooms.start(r, pid(auth), 2);
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