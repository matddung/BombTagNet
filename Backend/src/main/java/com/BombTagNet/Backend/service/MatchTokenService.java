package com.BombTagNet.Backend.service;

import com.BombTagNet.Backend.config.MatchProperties;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.node.ObjectNode;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Duration;
import java.time.Instant;
import java.util.Base64;
import java.util.HexFormat;
import java.util.Optional;

@Service
public class MatchTokenService {
    public record TokenPayload(String version, String dsId, String roomId, String matchId, Instant expiresAt) {
    }

    public record IssuedToken(String token, TokenPayload payload) {
    }

    private static final String VERSION = "v1";

    private final ObjectMapper mapper = new ObjectMapper();
    private final String secret;
    private final Duration ttl;

    public MatchTokenService(MatchProperties properties) {
        this.secret = properties.getTokenSecret();
        this.ttl = Duration.ofSeconds(Math.max(1L, properties.getTokenTtlSeconds()));
    }

    public IssuedToken issueToken(String dsId, String roomId, String matchId) {
        Instant expiresAt = Instant.now().plus(ttl);
        String token = encode(dsId, roomId, matchId, expiresAt);
        TokenPayload payload = new TokenPayload(VERSION, dsId, roomId, matchId, expiresAt);
        return new IssuedToken(token, payload);
    }

    public Optional<TokenPayload> verify(String token) {
        if (token == null || token.isBlank()) {
            return Optional.empty();
        }

        String[] segments = token.split("\\.");
        if (segments.length != 3) {
            return Optional.empty();
        }

        String header = segments[0];
        String payloadBase64 = segments[1];
        String signature = segments[2];

        if (!VERSION.equalsIgnoreCase(header)) {
            return Optional.empty();
        }

        String expectedSignature = sign(header + "." + payloadBase64);
        if (!expectedSignature.equalsIgnoreCase(signature)) {
            return Optional.empty();
        }

        byte[] payloadBytes;
        try {
            payloadBytes = Base64.getDecoder().decode(payloadBase64);
        } catch (IllegalArgumentException ex) {
            return Optional.empty();
        }

        try {
            ObjectNode node = (ObjectNode) mapper.readTree(payloadBytes);
            String dsId = optText(node, "dsId");
            String roomId = optText(node, "roomId");
            String matchId = optText(node, "matchId");
            String exp = optText(node, "exp");
            if (dsId == null || roomId == null || matchId == null || exp == null) {
                return Optional.empty();
            }

            Instant expiresAt = Instant.parse(exp);
            return Optional.of(new TokenPayload(header, dsId, roomId, matchId, expiresAt));
        } catch (Exception ex) {
            return Optional.empty();
        }
    }

    private String encode(String dsId, String roomId, String matchId, Instant expiresAt) {
        try {
            ObjectNode node = mapper.createObjectNode();
            node.put("dsId", dsId);
            node.put("roomId", roomId);
            node.put("matchId", matchId);
            node.put("exp", expiresAt.toString());

            byte[] payloadBytes = mapper.writeValueAsBytes(node);
            String payloadBase64 = Base64.getEncoder().withoutPadding().encodeToString(payloadBytes);
            String header = VERSION;
            String signature = sign(header + "." + payloadBase64);
            return header + "." + payloadBase64 + "." + signature;
        } catch (Exception ex) {
            throw new IllegalStateException("Failed to encode match token", ex);
        }
    }

    private String sign(String message) {
        try {
            MessageDigest md5 = MessageDigest.getInstance("MD5");
            md5.update((message + "." + secret).getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(md5.digest());
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("MD5 not available", e);
        }
    }

    private String optText(ObjectNode node, String field) {
        if (node.hasNonNull(field)) {
            String value = node.get(field).asText();
            if (value != null && !value.isBlank()) {
                return value;
            }
        }
        return null;
    }
}