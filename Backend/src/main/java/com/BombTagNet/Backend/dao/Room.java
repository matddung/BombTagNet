package com.BombTagNet.Backend.dao;

import com.BombTagNet.Backend.common.RoomStatus;

import java.util.Collection;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class Room {
    private final String roomId;
    private volatile String hostId;
    private final String name;
    private final int maxPlayers;
    private final String password;
    private volatile RoomStatus status = RoomStatus.WAITING;
    private volatile String dedicatedServerAddress;
    private volatile int dedicatedServerPort = 0;
    private volatile String dedicatedServerInternalAddress;
    private volatile Integer dedicatedServerQueryPort;
    private volatile String dedicatedServerId;
    private volatile String startToken;
    private volatile java.time.Instant startTokenExpiresAt;

    private final Map<String, Player> players = new ConcurrentHashMap<>();

    public Room(String roomId, String hostId, String name, int maxPlayers, String password) {
        this.roomId = roomId;
        this.hostId = hostId;
        this.name = name;
        this.maxPlayers = maxPlayers;
        this.password = (password == null || password.isBlank()) ? null : password;
    }

    public String roomId() {
        return roomId;
    }

    public String hostId() {
        return hostId;
    }

    public String dedicatedServerAddress() {
        return dedicatedServerAddress;
    }

    public int dedicatedServerPort() {
        return dedicatedServerPort;
    }

    public void updateDedicatedServerEndpoint(String address, Integer port, String internalAddress, Integer queryPort) {
        if (address != null && !address.isBlank()) {
            this.dedicatedServerAddress = address.trim();
        }
        if (port != null && port > 0) {
            this.dedicatedServerPort = port;
        }
        if (internalAddress != null && !internalAddress.isBlank()) {
            this.dedicatedServerInternalAddress = internalAddress.trim();
        }
        if (queryPort != null && queryPort > 0) {
            this.dedicatedServerQueryPort = queryPort;
        }
    }

    public void clearDedicatedServerEndpoint() {
        this.dedicatedServerAddress = null;
        this.dedicatedServerPort = 0;
        this.dedicatedServerInternalAddress = null;
        this.dedicatedServerQueryPort = null;
    }

    public String dedicatedServerInternalAddress() {
        return dedicatedServerInternalAddress;
    }

    public Integer dedicatedServerQueryPort() {
        return dedicatedServerQueryPort;
    }

    public void setDedicatedServerId(String dedicatedServerId) {
        this.dedicatedServerId = dedicatedServerId;
    }

    public String dedicatedServerId() {
        return dedicatedServerId;
    }

    public void setStartToken(String token, java.time.Instant expiresAt) {
        this.startToken = token;
        this.startTokenExpiresAt = expiresAt;
    }

    public String startToken() {
        return startToken;
    }

    public java.time.Instant startTokenExpiresAt() {
        return startTokenExpiresAt;
    }

    public String name() {
        return name;
    }

    public int maxPlayers() {
        return maxPlayers;
    }

    public String password() {
        return password;
    }

    public RoomStatus status() {
        return status;
    }

    public void setStatus(RoomStatus s) {
        this.status = s;
    }

    public Collection<Player> players() {
        return players.values();
    }

    public boolean contains(String playerId) {
        return players.containsKey(playerId);
    }

    public int size() {
        return players.size();
    }

    public boolean canJoin() {
        return status == RoomStatus.WAITING && size() < maxPlayers;
    }

    public void add(Player p) {
        players.put(p.playerId(), p);
    }

    public void remove(String playerId) {
        players.remove(playerId);
    }

    public void clearPlayers() {
        players.clear();
    }
}
