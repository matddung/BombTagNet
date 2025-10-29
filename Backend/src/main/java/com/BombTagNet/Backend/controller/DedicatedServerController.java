package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.service.DedicatedServerRegistry;
import com.BombTagNet.Backend.service.DedicatedServerRegistry.DedicatedServerRecord;
import com.BombTagNet.Backend.service.DedicatedServerRegistry.DedicatedServerStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.time.Instant;

@RestController
@RequestMapping("/api/ds")
public class DedicatedServerController {
    private final DedicatedServerRegistry registry;

    public DedicatedServerController(DedicatedServerRegistry registry) {
        this.registry = registry;
    }

    @PostMapping("/register")
    public ResponseEntity<DedicatedServerRes> register(@RequestBody RegisterDedicatedServerReq req) {
        DedicatedServerStatus status = req.status() == null ? null : DedicatedServerStatus.valueOf(req.status().toUpperCase());
        DedicatedServerRecord record = registry.registerOrUpdate(
                req.dsId(),
                req.publicAddress(),
                req.internalAddress(),
                req.gamePort(),
                req.queryPort(),
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

    private DedicatedServerRes toResponse(DedicatedServerRecord record) {
        return new DedicatedServerRes(
                record.dsId(),
                record.publicAddress(),
                record.internalAddress(),
                record.gamePort(),
                record.queryPort(),
                record.status().name(),
                record.lastUpdated()
        );
    }

    public record RegisterDedicatedServerReq(
            String dsId,
            String publicAddress,
            String internalAddress,
            Integer gamePort,
            Integer queryPort,
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
            Integer queryPort,
            String status,
            Instant lastUpdated
    ) {
    }
}