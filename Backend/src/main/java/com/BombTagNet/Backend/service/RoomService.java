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

    public Room create(String hostId, String name, int maxPlayers, String password) {
        String id = "r_" + seq.getAndIncrement();
        Room r = new Room(id, hostId, name == null ? "Room" : name, Math.max(2, Math.min(4, maxPlayers)), password);
        rooms.put(id, r);
        return r;
    }

    public Optional<Room> find(String roomId){ return Optional.ofNullable(rooms.get(roomId)); }

    public Room join(Room r, Player p, String password) {
        if (r.password() != null && !Objects.equals(r.password(), password))
            throw new IllegalStateException("WRONG_PASSWORD");
        if (!r.canJoin())
            throw new IllegalStateException("ROOM_FULL_OR_STARTED");
        r.add(p);
        return r;
    }

    public void start(Room r, String requesterId, int minPlayersNeeded) {
        if (!Objects.equals(r.hostId(), requesterId)) throw new IllegalStateException("ONLY_HOST");
        if (r.size() < minPlayersNeeded) throw new IllegalStateException("NOT_ENOUGH_PLAYERS");
        r.setStatus(RoomStatus.STARTED);
    }
}