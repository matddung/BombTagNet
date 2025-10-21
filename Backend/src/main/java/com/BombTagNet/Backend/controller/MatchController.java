package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.common.PlayerRequestUtils;
import com.BombTagNet.Backend.common.RequestIpUtils;
import com.BombTagNet.Backend.dto.MatchDto.MatchQueueStatusRes;
import com.BombTagNet.Backend.dto.MatchDto.MatchResultReq;
import com.BombTagNet.Backend.dto.MatchDto.OkRes;
import com.BombTagNet.Backend.service.MatchService;
import com.BombTagNet.Backend.service.MatchService.MatchQueueStatus;
import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/matches")
public class MatchController {
    private final MatchService match;

    public MatchController(MatchService match) {
        this.match = match;
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
        String playerId = PlayerRequestUtils.requirePlayerId(request);
        MatchQueueStatus status = match.enqueue(playerId, PlayerRequestUtils.resolveNickname(request), RequestIpUtils.resolveRemoteAddress(request));
        return ResponseEntity.ok(toResponse(status));
    }

    @GetMapping("/queue/{ticketId}")
    public ResponseEntity<MatchQueueStatusRes> status(HttpServletRequest request, @PathVariable String ticketId) {
        return match.status(PlayerRequestUtils.requirePlayerId(request), ticketId)
                .map(this::toResponse)
                .map(ResponseEntity::ok)
                .orElseThrow(() -> new IllegalStateException("TICKET_NOT_FOUND"));
    }

    @PostMapping("/queue/{ticketId}/cancel")
    public ResponseEntity<MatchQueueStatusRes> cancel(HttpServletRequest request, @PathVariable String ticketId) {
        return match.cancel(PlayerRequestUtils.requirePlayerId(request), ticketId)
                .map(this::toResponse)
                .map(ResponseEntity::ok)
                .orElseThrow(() -> new IllegalStateException("TICKET_NOT_FOUND"));
    }

    @PostMapping("/{matchId}/result")
    public ResponseEntity<OkRes> result(@PathVariable String matchId, @RequestBody MatchResultReq req) {
        return ResponseEntity.ok(new OkRes(true));
    }
}