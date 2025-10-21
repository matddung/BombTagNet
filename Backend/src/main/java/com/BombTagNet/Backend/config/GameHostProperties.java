package com.BombTagNet.Backend.config;

import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.stereotype.Component;

@Component
@ConfigurationProperties(prefix = "game.host")
public class GameHostProperties {
    private String address = "34.64.149.81";

    private int port = 7777;

    public String getAddress() {
        return address;
    }

    public void setAddress(String address) {
        this.address = address;
    }

    public int getPort() {
        return port;
    }

    public void setPort(int port) {
        this.port = port;
    }

    public String resolveAddress(String candidate) {
        if (candidate != null) {
            String trimmed = candidate.trim();
            if (trimmed.equals(address)) {
                return trimmed;
            }
        }
        return address;
    }

    public int resolvePort(Integer candidate) {
        if (candidate != null && candidate > 0 && candidate == port) {
            return candidate;
        }
        return port;
    }
}