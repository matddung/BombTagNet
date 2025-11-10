package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.service.DedicatedServerRegistry;
import com.BombTagNet.Backend.service.DedicatedServerRegistry.DedicatedServerRecord;
import com.BombTagNet.Backend.service.DedicatedServerRegistry.DedicatedServerStatus;
import com.BombTagNet.Backend.service.MatchTokenService;
import com.BombTagNet.Backend.service.MatchTokenService.TokenPayload;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;

@RestController
@RequestMapping("/api/ds")
public class DedicatedServerController {
    private final DedicatedServerRegistry registry;
    private final MatchTokenService tokens;

    public DedicatedServerController(DedicatedServerRegistry registry, MatchTokenService tokens) {
        this.registry = registry;
        this.tokens = tokens;
    }

    @PostMapping("/register")
    public ResponseEntity<DedicatedServerRes> register(@RequestBody RegisterDedicatedServerReq req) {
        DedicatedServerStatus status = req.status() == null ? null : DedicatedServerStatus.valueOf(req.status().toUpperCase());
        DedicatedServerRecord record = registry.registerOrUpdate(
                req.dsId(),
                req.publicAddress(),
                req.internalAddress(),
                req.gamePort(),
                status
        );
        return ResponseEntity.ok(toResponse(record));
    }

    @PostMapping("/{dsId}/status")
    public ResponseEntity<DedicatedServerRes> updateStatus(@PathVariable String dsId, @RequestBody UpdateDedicatedServerStatusReq req) {
        DedicatedServerStatus status = DedicatedServerStatus.valueOf(req.status().toUpperCase());
        DedicatedServerRecord record = registry.updateStatus(dsId, status)
                .orElseThrow(() -> new IllegalStateException("DEDICATED_SERVER_NOT_FOUND"));
        return ResponseEntity.ok(toResponse(record));
    }

    @GetMapping("/{dsId}")
    public ResponseEntity<DedicatedServerRes> get(@PathVariable String dsId) {
        DedicatedServerRecord record = registry.find(dsId)
                .orElseThrow(() -> new IllegalStateException("DEDICATED_SERVER_NOT_FOUND"));
        return ResponseEntity.ok(toResponse(record));
    }

    @PostMapping("/matches/verify-start")
    public ResponseEntity<VerifyStartTokenRes> verifyStart(@RequestBody VerifyStartTokenReq req) {
        if (req.startToken() == null || req.startToken().isBlank()) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("TOKEN_MISSING"));
        }

        return tokens.verify(req.startToken())
                .map(payload -> verifyAgainstRequest(payload, req))
                .orElseGet(() -> ResponseEntity.ok(VerifyStartTokenRes.failure("TOKEN_INVALID")));
    }

    private ResponseEntity<VerifyStartTokenRes> verifyAgainstRequest(TokenPayload payload, VerifyStartTokenReq req) {
        Instant expiresAt = payload.expiresAt();
        if (expiresAt == null || !expiresAt.isAfter(Instant.now())) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("TOKEN_EXPIRED"));
        }

        if (hasText(req.dsId()) && !payload.dsId().equals(req.dsId())) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("DEDICATED_SERVER_MISMATCH"));
        }

        if (hasText(req.roomId()) && !payload.roomId().equals(req.roomId())) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("ROOM_MISMATCH"));
        }

        if (hasText(req.matchId()) && !payload.matchId().equals(req.matchId())) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("MATCH_MISMATCH"));
        }

        if (registry.find(payload.dsId()).isEmpty()) {
            return ResponseEntity.ok(VerifyStartTokenRes.failure("DEDICATED_SERVER_NOT_REGISTERED"));
        }

        return ResponseEntity.ok(VerifyStartTokenRes.success(payload));
    }

    private boolean hasText(String value) {
        return value != null && !value.isBlank();
    }

    private DedicatedServerRes toResponse(DedicatedServerRecord record) {
        return new DedicatedServerRes(
                record.dsId(),
                record.publicAddress(),
                record.internalAddress(),
                record.gamePort(),
                record.status().name(),
                record.lastUpdated()
        );
    }

    public record RegisterDedicatedServerReq(
            String dsId,
            String publicAddress,
            String internalAddress,
            Integer gamePort,
            String status
    ) {
    }

    public record UpdateDedicatedServerStatusReq(String status) {
    }

    public record DedicatedServerRes(
            String dsId,
            String publicAddress,
            String internalAddress,
            Integer gamePort,
            String status,
            Instant lastUpdated
    ) {
    }

    public record VerifyStartTokenReq(
            String dsId,
            String roomId,
            String matchId,
            String startToken
    ) {
    }

    public record VerifyStartTokenRes(
            boolean success,
            String error,
            String roomId,
            String matchId,
            String dedicatedServerId,
            String expiresAt
    ) {
        static VerifyStartTokenRes success(TokenPayload payload) {
            return new VerifyStartTokenRes(
                    true,
                    null,
                    payload.roomId(),
                    payload.matchId(),
                    payload.dsId(),
                    payload.expiresAt() == null ? null : payload.expiresAt().toString()
            );
        }

        static VerifyStartTokenRes failure(String error) {
            return new VerifyStartTokenRes(false, error, null, null, null, null);
        }
    }
}