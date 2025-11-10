package com.BombTagNet.Backend.service;

import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.Comparator;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

@Service
public class DedicatedServerRegistry {
    public enum DedicatedServerStatus {
        REGISTERED,
        READY,
        BUSY
    }

    public record DedicatedServerRecord(
            String dsId,
            String publicAddress,
            String internalAddress,
            int gamePort,
            DedicatedServerStatus status,
            Instant lastUpdated
    ) {
    }

    private final Map<String, DedicatedServerRecord> servers = new ConcurrentHashMap<>();
    private final Object lock = new Object();

    public DedicatedServerRecord registerOrUpdate(String dsId, String publicAddress, String internalAddress,
                                                  Integer gamePort, DedicatedServerStatus status) {
        if (dsId == null || dsId.isBlank()) {
            throw new IllegalArgumentException("dsId required");
        }

        synchronized (lock) {
            DedicatedServerRecord existing = servers.get(dsId);
            DedicatedServerStatus effectiveStatus = status != null ? status :
                    (existing != null ? existing.status() : DedicatedServerStatus.REGISTERED);

            String resolvedPublic = normalize(publicAddress, existing == null ? null : existing.publicAddress());
            String resolvedInternal = normalize(internalAddress, existing == null ? null : existing.internalAddress());
            int resolvedGamePort = normalizePort(gamePort, existing == null ? null : existing.gamePort());

            DedicatedServerRecord updated = new DedicatedServerRecord(
                    dsId,
                    resolvedPublic,
                    resolvedInternal,
                    resolvedGamePort,
                    effectiveStatus,
                    Instant.now()
            );
            servers.put(dsId, updated);
            return updated;
        }
    }

    public Optional<DedicatedServerRecord> find(String dsId) {
        if (dsId == null || dsId.isBlank()) {
            return Optional.empty();
        }
        return Optional.ofNullable(servers.get(dsId));
    }

    public Optional<DedicatedServerRecord> allocateReadyServer() {
        synchronized (lock) {
            return servers.values().stream()
                    .filter(record -> record.status() == DedicatedServerStatus.READY)
                    .min(Comparator.comparing(DedicatedServerRecord::lastUpdated))
                    .map(record -> {
                        DedicatedServerRecord updated = new DedicatedServerRecord(
                                record.dsId(),
                                record.publicAddress(),
                                record.internalAddress(),
                                record.gamePort(),
                                DedicatedServerStatus.BUSY,
                                Instant.now()
                        );
                        servers.put(record.dsId(), updated);
                        return updated;
                    });
        }
    }

    public Optional<DedicatedServerRecord> updateStatus(String dsId, DedicatedServerStatus status) {
        if (dsId == null || dsId.isBlank() || status == null) {
            return Optional.empty();
        }

        synchronized (lock) {
            DedicatedServerRecord existing = servers.get(dsId);
            if (existing == null) {
                return Optional.empty();
            }

            DedicatedServerRecord updated = new DedicatedServerRecord(
                    existing.dsId(),
                    existing.publicAddress(),
                    existing.internalAddress(),
                    existing.gamePort(),
                    status,
                    Instant.now()
            );
            servers.put(dsId, updated);
            return Optional.of(updated);
        }
    }

    private String normalize(String candidate, String fallback) {
        if (candidate != null) {
            String trimmed = candidate.trim();
            if (!trimmed.isEmpty()) {
                return trimmed;
            }
        }
        return fallback;
    }

    private int normalizePort(Integer candidate, Integer fallback) {
        if (candidate != null && candidate > 0) {
            return candidate;
        }
        if (fallback != null && fallback > 0) {
            return fallback;
        }
        return 0;
    }
}