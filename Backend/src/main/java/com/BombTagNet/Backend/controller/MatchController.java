package com.BombTagNet.Backend.controller;

import com.BombTagNet.Backend.dto.MatchDto.*;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/matches")
public class MatchController {
    @PostMapping("/{matchId}/result")
    public ResponseEntity<OkRes> result(@PathVariable String matchId, @RequestBody MatchResultReq req) {
        return ResponseEntity.ok(new OkRes(true));
    }
}