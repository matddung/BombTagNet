package com.BombTagNet.Backend.jwt;

import io.jsonwebtoken.Claims;
import io.jsonwebtoken.Jwts;
import io.jsonwebtoken.security.Keys;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import javax.crypto.SecretKey;
import java.time.Instant;
import java.util.Date;
import java.util.Map;

@Service
public class JwtService {
    private final SecretKey key;
    private final long expiresSec;

    public JwtService(@Value("${app.jwt.secret}") String secret,
                      @Value("${app.jwt.expiresSeconds}") long expiresSec) {
        this.key = Keys.hmacShaKeyFor(secret.getBytes());
        this.expiresSec = expiresSec;
    }

    public String issue(String playerId, String nickname) {
        Instant now = Instant.now();
        return Jwts.builder()
                .setSubject(playerId)
                .addClaims(Map.of("nickname", nickname))
                .setIssuedAt(Date.from(now))
                .setExpiration(Date.from(now.plusSeconds(expiresSec)))
                .signWith(key)
                .compact();
    }

    private Claims parseClaims(String token) {
        return Jwts.parserBuilder().setSigningKey(key).build()
                .parseClaimsJws(token).getBody();
    }

    public String parseSubject(String token) {
        return parseClaims(token).getSubject();
    }

    public String parseNickname(String token) {
        return parseClaims(token).get("nickname", String.class);
    }
}