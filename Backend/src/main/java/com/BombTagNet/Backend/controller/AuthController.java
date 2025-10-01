package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dto.AuthDto.GuestLoginReq;
import com.BombTagNet.Backend.dto.AuthDto.GuestLoginRes;
import com.BombTagNet.Backend.jwt.JwtService;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.UUID;

@RestController
@RequestMapping("/api/auth")
public class AuthController {
    private final JwtService jwt;
    private final int expires;

    public AuthController(JwtService jwt, @Value("${app.jwt.expiresSeconds}") int expires) {
        this.jwt = jwt; this.expires = expires;
    }

    @PostMapping("/guest")
    public ResponseEntity<GuestLoginRes> guest(@RequestBody GuestLoginReq req) {
        String nickname = (req.nickname() == null || req.nickname().isBlank()) ? "Guest" : req.nickname().trim();
        String playerId = "p_" + UUID.randomUUID().toString().replace("-", "").substring(0, 8);
        String token = jwt.issue(playerId, nickname);
        return ResponseEntity.ok(new GuestLoginRes(playerId, nickname, token, expires));
    }
}