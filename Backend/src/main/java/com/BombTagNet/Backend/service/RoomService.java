package com.BombTagNet.Backend.service;

import com.BombTagNet.Backend.service.DedicatedServerRegistry.*;
import com.BombTagNet.Backend.common.RoomStatus;
import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.dao.Room;
import org.springframework.stereotype.Service;

import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

@Service
public class RoomService {
    private final Map<String, Room> rooms = new ConcurrentHashMap<>();
    private final AtomicInteger seq = new AtomicInteger(1);
    private final DedicatedServerRegistry dedicatedServers;
    private final MatchTokenService tokens;

    public RoomService(DedicatedServerRegistry dedicatedServers, MatchTokenService tokens) {
        this.dedicatedServers = dedicatedServers;
        this.tokens = tokens;
    }

    public Room create(String hostId, String name, int maxPlayers, String password, String hostAddress) {
        String roomId = normalizeRoomKey(name);
        String canonicalKey = toCanonicalKey(roomId);
        Room r = new Room(roomId, hostId, roomId, Math.max(2, Math.min(4, maxPlayers)), password);
        r.updateHostEndpoint(hostAddress, null, null, null);
        Room existing = rooms.putIfAbsent(canonicalKey, r);
        if (existing != null) {
            throw new IllegalStateException("ROOM_ALREADY_EXISTS");
        }
        return r;
    }

    public void updateHostEndpoint(Room room, String address, Integer port) {
        if (room == null) {
            return;
        }
        room.updateHostEndpoint(address, port, null, null);
    }

    private String normalizeRoomKey(String name) {
        if (name == null) {
            throw new IllegalStateException("ROOM_NAME_REQUIRED");
        }

        String normalized = name.trim();
        if (normalized.isEmpty()) {
            throw new IllegalStateException("ROOM_NAME_REQUIRED");
        }

        return normalized;
    }

    private String toCanonicalKey(String roomId) {
        return roomId.trim().toLowerCase(Locale.ROOT);
    }

    public Optional<Room> find(String roomIdOrName) {
        if (roomIdOrName == null) {
            return Optional.empty();
        }

        String normalized = roomIdOrName.trim();
        if (normalized.isEmpty()) {
            return Optional.empty();
        }

        Room byId = rooms.get(toCanonicalKey(normalized));
        if (byId != null) {
            return Optional.of(byId);
        }

        return rooms.values().stream()
                .filter(r -> r.roomId().equalsIgnoreCase(normalized) || r.name().equalsIgnoreCase(normalized))
                .findFirst();
    }

    public Room join(Room r, Player p, String password) {
        if (r.contains(p.playerId())) {
            return r;
        }
        if (r.password() != null && !Objects.equals(r.password(), password))
            throw new IllegalStateException("WRONG_PASSWORD");
        if (!r.canJoin())
            throw new IllegalStateException("ROOM_FULL_OR_STARTED");
        r.add(p);
        return r;
    }

    public void leave(Room r, String playerId) {
        if (r == null || playerId == null || playerId.isBlank()) {
            return;
        }

        boolean wasHost = Objects.equals(r.hostId(), playerId);
        if (!r.contains(playerId)) {
            return;
        }

        r.remove(playerId);

        if (wasHost) {
            r.clearPlayers();
            rooms.remove(toCanonicalKey(r.roomId()), r);
            r.setStatus(RoomStatus.WAITING);
            return;
        }

        if (r.size() == 0) {
            rooms.remove(toCanonicalKey(r.roomId()), r);
            r.setStatus(RoomStatus.WAITING);
            return;
        }

        if (r.status() == RoomStatus.STARTED && r.size() < 2) {
            r.setStatus(RoomStatus.WAITING);
        }
    }

    public MatchLaunch start(Room r, String requesterId, int minPlayersNeeded) {
        if (!Objects.equals(r.hostId(), requesterId)) throw new IllegalStateException("ONLY_HOST");
        if (r.size() < minPlayersNeeded) throw new IllegalStateException("NOT_ENOUGH_PLAYERS");
        DedicatedServerRecord server = dedicatedServers.allocateReadyServer()
                .orElseThrow(() -> new IllegalStateException("NO_DEDICATED_SERVER_AVAILABLE"));

        String matchId = "m_" + seq.getAndIncrement();
        MatchTokenService.IssuedToken token = tokens.issueToken(server.dsId(), r.roomId(), matchId);

        r.updateHostEndpoint(server.publicAddress(), server.gamePort(), server.internalAddress(), server.queryPort());
        r.setDedicatedServerId(server.dsId());
        r.setStatus(RoomStatus.STARTED);
        r.setStartToken(token.token(), token.payload().expiresAt());

        return new MatchLaunch(matchId, r.hostId(), server, token.token(), token.payload().expiresAt());
    }

    public record MatchLaunch(String matchId, String hostPlayerId, DedicatedServerRecord server,
                              String startToken, java.time.Instant expiresAt) {
    }
}