package com.BombTagNet.Backend.service;

import com.BombTagNet.Backend.common.RoomStatus;
import com.BombTagNet.Backend.dao.Player;
import com.BombTagNet.Backend.dao.Room;
import org.springframework.stereotype.Service;

import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;

@Service
public class RoomService {
    private final Map<String, Room> rooms = new ConcurrentHashMap<>();
    private final AtomicInteger seq = new AtomicInteger(1);

    public Room create(String hostId, String name, int maxPlayers, String password, String hostAddress) {
        String id = "r_" + seq.getAndIncrement();
        Room r = new Room(id, hostId, name == null ? "Room" : name, Math.max(2, Math.min(4, maxPlayers)), password);
        r.updateHostEndpoint(hostAddress, 0);
        rooms.put(id, r);
        return r;
    }

    public Optional<Room> find(String roomId) {
        if (roomId == null) {
            return Optional.empty();
        }

        String normalized = roomId.trim();
        if (normalized.isEmpty()) {
            return Optional.empty();
        }

        return Optional.ofNullable(rooms.get(normalized));
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
            rooms.remove(r.roomId(), r);
            r.setStatus(RoomStatus.WAITING);
            return;
        }

        if (r.size() == 0) {
            rooms.remove(r.roomId(), r);
            r.setStatus(RoomStatus.WAITING);
            return;
        }

        if (r.status() == RoomStatus.STARTED && r.size() < 2) {
            r.setStatus(RoomStatus.WAITING);
        }
    }

    public void start(Room r, String requesterId, int minPlayersNeeded) {
        if (!Objects.equals(r.hostId(), requesterId)) throw new IllegalStateException("ONLY_HOST");
        if (r.size() < minPlayersNeeded) throw new IllegalStateException("NOT_ENOUGH_PLAYERS");
        r.setStatus(RoomStatus.STARTED);
    }
}