package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dto.MatchDto.MatchQueueStatusRes;
import com.BombTagNet.Backend.dto.MatchDto.MatchResultReq;
import com.BombTagNet.Backend.dto.MatchDto.OkRes;
import com.BombTagNet.Backend.service.MatchService;
import com.BombTagNet.Backend.service.MatchService.MatchQueueStatus;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api/matches")
public class MatchController {
    private final MatchService match;

    public MatchController(MatchService match) {
        this.match = match;
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

    private MatchQueueStatusRes toResponse(MatchQueueStatus status) {
        return new MatchQueueStatusRes(
                status.ticketId(),
                status.status().name(),
                status.position(),
                status.readyInSeconds(),
                status.waitForFourthSeconds(),
                status.minPlayers(),
                status.maxPlayers(),
                status.matchId(),
                status.players(),
                status.hostPlayerId(),
                status.hostAddress(),
                status.hostPort()
        );
    }

    @PostMapping("/queue")
    public ResponseEntity<MatchQueueStatusRes> enqueue(HttpServletRequest request) {
        String playerId = pid(request);
        MatchQueueStatus status = match.enqueue(playerId, nickname(request), request == null ? null : request.getRemoteAddr());
        return ResponseEntity.ok(toResponse(status));
    }

    @GetMapping("/queue/{ticketId}")
    public ResponseEntity<MatchQueueStatusRes> status(HttpServletRequest request, @PathVariable String ticketId) {
        return match.status(pid(request), ticketId)
                .map(this::toResponse)
                .map(ResponseEntity::ok)
                .orElseThrow(() -> new IllegalStateException("TICKET_NOT_FOUND"));
    }

    @PostMapping("/queue/{ticketId}/cancel")
    public ResponseEntity<MatchQueueStatusRes> cancel(HttpServletRequest request, @PathVariable String ticketId) {
        return match.cancel(pid(request), ticketId)
                .map(this::toResponse)
                .map(ResponseEntity::ok)
                .orElseThrow(() -> new IllegalStateException("TICKET_NOT_FOUND"));
    }

    @PostMapping("/{matchId}/result")
    public ResponseEntity<OkRes> result(@PathVariable String matchId, @RequestBody MatchResultReq req) {
        return ResponseEntity.ok(new OkRes(true));
    }

    @ExceptionHandler(IllegalStateException.class)
    public ResponseEntity<?> bad(IllegalStateException e) {
        return ResponseEntity.status(HttpStatus.CONFLICT).body(Map.of("code", e.getMessage()));
    }
}