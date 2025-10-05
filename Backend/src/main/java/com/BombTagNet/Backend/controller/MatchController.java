package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dto.MatchDto.MatchQueueStatusRes;
import com.BombTagNet.Backend.dto.MatchDto.MatchResultReq;
import com.BombTagNet.Backend.dto.MatchDto.OkRes;
import com.BombTagNet.Backend.jwt.JwtAuthFilter.PlayerPrincipal;
import com.BombTagNet.Backend.service.MatchService;
import com.BombTagNet.Backend.service.MatchService.MatchQueueStatus;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.*;

import java.util.Map;

@RestController
@RequestMapping("/api/matches")
public class MatchController {
    private final MatchService match;

    public MatchController(MatchService match) {
        this.match = match;
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
                status.players()
        );
    }

    @PostMapping("/queue")
    public ResponseEntity<MatchQueueStatusRes> enqueue(Authentication auth) {
        MatchQueueStatus status = match.enqueue(pid(auth), nicknameFromAuth(auth));
        return ResponseEntity.ok(toResponse(status));
    }

    @GetMapping("/queue/{ticketId}")
    public ResponseEntity<MatchQueueStatusRes> status(Authentication auth, @PathVariable String ticketId) {
        return match.status(pid(auth), ticketId)
                .map(this::toResponse)
                .map(ResponseEntity::ok)
                .orElseThrow(() -> new IllegalStateException("TICKET_NOT_FOUND"));
    }

    @PostMapping("/queue/{ticketId}/cancel")
    public ResponseEntity<MatchQueueStatusRes> cancel(Authentication auth, @PathVariable String ticketId) {
        return match.cancel(pid(auth), ticketId)
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