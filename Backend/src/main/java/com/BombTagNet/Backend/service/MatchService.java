package com.BombTagNet.Backend.service;

import com.BombTagNet.Backend.service.DedicatedServerRegistry.*;
import com.BombTagNet.Backend.dao.Player;
import jakarta.annotation.PreDestroy;
import org.springframework.stereotype.Service;

import java.time.Instant;
import java.util.*;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

@Service
public class MatchService {
    public enum TicketStatus {
        QUEUED,
        FORMING,
        MATCHED,
        CANCELLED
    }

    public record MatchInfo(String matchId, List<Player> players, String hostPlayerId, String dedicatedServerId,
                            String hostAddress, String hostInternalAddress, int hostPort, Integer queryPort,
                            String startToken, Instant startTokenExpiresAt) {
    }

    public record MatchQueueStatus(
            String ticketId,
            TicketStatus status,
            Integer position,
            Integer readyInSeconds,
            Integer waitForFourthSeconds,
            Integer minPlayers,
            Integer maxPlayers,
            String matchId,
            List<Player> players,
            String hostPlayerId,
            String hostAddress,
            Integer hostPort,
            String hostInternalAddress,
            Integer queryPort,
            String dedicatedServerId,
            String startToken,
            Instant startTokenExpiresAt
    ) {
    }

    private static final int MIN_PLAYERS = 3;
    private static final int MAX_PLAYERS = 4;
    private static final int WAIT_FOR_FOURTH_SECONDS = 5;

    private final Object lock = new Object();
    private final Deque<MatchTicket> queue = new ArrayDeque<>();
    private final Map<String, MatchTicket> ticketsById = new HashMap<>();
    private final Map<String, MatchTicket> ticketsByPlayer = new HashMap<>();
    private final AtomicInteger ticketSeq = new AtomicInteger(1);
    private final AtomicInteger matchSeq = new AtomicInteger(1);
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private final DedicatedServerRegistry dedicatedServers;
    private final MatchTokenService tokens;

    private PendingMatch pendingMatch;

    public MatchService(DedicatedServerRegistry dedicatedServers, MatchTokenService tokens) {
        this.dedicatedServers = dedicatedServers;
        this.tokens = tokens;
    }

    public MatchQueueStatus enqueue(String playerId, String nickname, String address) {
        long now = System.currentTimeMillis();

        synchronized (lock) {
            MatchTicket existing = ticketsByPlayer.get(playerId);
            if (existing != null) {
                if (existing.status == TicketStatus.MATCHED || existing.status == TicketStatus.CANCELLED) {
                    removeTicket(existing);
                } else {
                    return statusFor(existing, now);
                }
            }

            MatchTicket ticket = new MatchTicket("t_" + ticketSeq.getAndIncrement(), new Player(playerId, nickname), address == null ? null : address.trim());
            ticketsById.put(ticket.ticketId, ticket);
            ticketsByPlayer.put(playerId, ticket);

            assignTicket(ticket, now);

            return statusFor(ticket, now);
        }
    }

    public Optional<MatchQueueStatus> status(String playerId, String ticketId) {
        long now = System.currentTimeMillis();
        synchronized (lock) {
            MatchTicket ticket = ticketsById.get(ticketId);
            if (ticket == null || !Objects.equals(ticket.player.playerId(), playerId)) {
                return Optional.empty();
            }

            return Optional.of(statusFor(ticket, now));
        }
    }

    public Optional<MatchQueueStatus> cancel(String playerId, String ticketId) {
        long now = System.currentTimeMillis();
        synchronized (lock) {
            MatchTicket ticket = ticketsById.get(ticketId);
            if (ticket == null || !Objects.equals(ticket.player.playerId(), playerId)) {
                return Optional.empty();
            }

            if (ticket.status == TicketStatus.MATCHED) {
                return Optional.of(statusFor(ticket, now));
            }

            if (ticket.status == TicketStatus.QUEUED) {
                queue.remove(ticket);
                ticket.status = TicketStatus.CANCELLED;
                removeTicket(ticket);
                return Optional.of(statusFor(ticket, now));
            }

            if (ticket.status == TicketStatus.FORMING) {
                PendingMatch match = ticket.pendingMatch;
                if (match != null) {
                    match.remove(ticket);
                    ticket.pendingMatch = null;
                    ticket.status = TicketStatus.CANCELLED;
                    if (match.countdown != null) {
                        match.countdown.cancel(false);
                    }

                    if (match.tickets.size() >= MIN_PLAYERS) {
                        match.deadline = Instant.now().plusSeconds(WAIT_FOR_FOURTH_SECONDS);
                        match.countdown = scheduler.schedule(() -> onCountdownFinished(match.matchId), WAIT_FOR_FOURTH_SECONDS, TimeUnit.SECONDS);
                    } else {
                        List<MatchTicket> remaining = new ArrayList<>(match.tickets);
                        match.tickets.clear();
                        pendingMatch = null;

                        for (int i = remaining.size() - 1; i >= 0; --i) {
                            MatchTicket other = remaining.get(i);
                            other.pendingMatch = null;
                            other.status = TicketStatus.QUEUED;
                            queue.addFirst(other);
                        }

                        tryPromote(now);
                    }
                }

                removeTicket(ticket);
                return Optional.of(statusFor(ticket, now));
            }

            ticket.status = TicketStatus.CANCELLED;
            removeTicket(ticket);
            return Optional.of(statusFor(ticket, now));
        }
    }

    private void assignTicket(MatchTicket ticket, long now) {
        if (pendingMatch != null && pendingMatch.tickets.size() < MAX_PLAYERS) {
            pendingMatch.add(ticket);
            if (pendingMatch.tickets.size() >= MAX_PLAYERS) {
                startMatch(pendingMatch);
            }
            return;
        }

        queue.addLast(ticket);
        tryPromote(now);
    }

    private void tryPromote(long now) {
        if (pendingMatch != null) {
            return;
        }

        if (queue.size() < MIN_PLAYERS) {
            return;
        }

        PendingMatch match = new PendingMatch("m_" + matchSeq.getAndIncrement());
        for (int i = 0; i < MIN_PLAYERS; ++i) {
            MatchTicket next = queue.pollFirst();
            if (next != null) {
                match.add(next);
            }
        }

        if (match.tickets.size() < MIN_PLAYERS) {
            for (int i = match.tickets.size() - 1; i >= 0; --i) {
                MatchTicket t = match.tickets.get(i);
                t.pendingMatch = null;
                t.status = TicketStatus.QUEUED;
                queue.addFirst(t);
            }
            return;
        }

        match.deadline = Instant.ofEpochMilli(now).plusSeconds(WAIT_FOR_FOURTH_SECONDS);
        match.countdown = scheduler.schedule(() -> onCountdownFinished(match.matchId), WAIT_FOR_FOURTH_SECONDS, TimeUnit.SECONDS);
        pendingMatch = match;
    }

    private void onCountdownFinished(String matchId) {
        synchronized (lock) {
            if (pendingMatch != null && Objects.equals(pendingMatch.matchId, matchId)) {
                startMatch(pendingMatch);
            }
        }
    }

    private void startMatch(PendingMatch match) {
        if (match.countdown != null) {
            match.countdown.cancel(false);
        }

        Optional<DedicatedServerRecord> serverOpt = dedicatedServers.allocateReadyServer();
        if (serverOpt.isEmpty()) {
            match.deadline = Instant.now().plusSeconds(1);
            match.countdown = scheduler.schedule(() -> onCountdownFinished(match.matchId), 1, TimeUnit.SECONDS);
            return;
        }

        DedicatedServerRecord server = serverOpt.get();

        List<Player> players = match.players();
        MatchTicket hostTicket = match.tickets.isEmpty() ? null : match.tickets.get(0);
        String hostPlayerId = hostTicket == null ? null : hostTicket.player.playerId();

        MatchTokenService.IssuedToken token = tokens.issueToken(server.dsId(), match.matchId, match.matchId);

        MatchInfo info = new MatchInfo(
                match.matchId,
                players,
                hostPlayerId,
                server.dsId(),
                server.publicAddress(),
                server.internalAddress(),
                server.gamePort(),
                server.queryPort(),
                token.token(),
                token.payload().expiresAt()
        );

        for (MatchTicket ticket : match.tickets) {
            ticket.pendingMatch = null;
            ticket.matchInfo = info;
            ticket.status = TicketStatus.MATCHED;
        }

        match.tickets.clear();
        pendingMatch = null;

        if (queue.size() >= MIN_PLAYERS) {
            tryPromote(System.currentTimeMillis());
        }
    }

    private MatchQueueStatus statusFor(MatchTicket ticket, long now) {
        Integer position = null;
        Integer readyIn = null;
        String matchId = null;
        List<Player> players = List.of();
        String hostPlayerId = null;
        String hostAddress = null;
        String hostInternalAddress = null;
        Integer hostPort = null;
        Integer queryPort = null;
        String dedicatedServerId = null;
        String startToken = null;
        Instant startTokenExpiresAt = null;

        if (ticket.status == TicketStatus.QUEUED) {
            position = queuePosition(ticket);
        } else if (ticket.status == TicketStatus.FORMING) {
            PendingMatch match = ticket.pendingMatch;
            if (match != null) {
                long millisRemaining = Math.max(0L, match.deadline.toEpochMilli() - now);
                readyIn = (int) Math.ceil(millisRemaining / 1000.0);
                players = match.players();
            }
        } else if (ticket.status == TicketStatus.MATCHED) {
            if (ticket.matchInfo != null) {
                matchId = ticket.matchInfo.matchId();
                players = ticket.matchInfo.players();
                hostPlayerId = ticket.matchInfo.hostPlayerId();
                hostAddress = ticket.matchInfo.hostAddress();
                hostInternalAddress = ticket.matchInfo.hostInternalAddress();
                hostPort = ticket.matchInfo.hostPort();
                queryPort = ticket.matchInfo.queryPort();
                dedicatedServerId = ticket.matchInfo.dedicatedServerId();
                startToken = ticket.matchInfo.startToken();
                startTokenExpiresAt = ticket.matchInfo.startTokenExpiresAt();
            }
        }

        return new MatchQueueStatus(
                ticket.ticketId,
                ticket.status,
                position,
                readyIn,
                WAIT_FOR_FOURTH_SECONDS,
                MIN_PLAYERS,
                MAX_PLAYERS,
                matchId,
                List.copyOf(players),
                hostPlayerId,
                hostAddress,
                hostPort,
                hostInternalAddress,
                queryPort,
                dedicatedServerId,
                startToken,
                startTokenExpiresAt
        );
    }

    private int queuePosition(MatchTicket ticket) {
        if (ticket.status != TicketStatus.QUEUED) {
            return -1;
        }

        int pos = 1;
        for (MatchTicket q : queue) {
            if (q == ticket) {
                return pos;
            }
            pos++;
        }

        return -1;
    }

    private void removeTicket(MatchTicket ticket) {
        ticketsById.remove(ticket.ticketId);
        ticketsByPlayer.remove(ticket.player.playerId());
    }

    @PreDestroy
    public void shutdown() {
        scheduler.shutdownNow();
    }

    private static class MatchTicket {
        private final String ticketId;
        private final Player player;
        private TicketStatus status = TicketStatus.QUEUED;
        private PendingMatch pendingMatch;
        private MatchInfo matchInfo;
        private final String address;

        private MatchTicket(String ticketId, Player player, String address) {
            this.ticketId = ticketId;
            this.player = player;
            this.address = (address == null || address.isBlank()) ? null : address;
        }
    }

    private static class PendingMatch {
        private final String matchId;
        private final List<MatchTicket> tickets = new ArrayList<>();
        private ScheduledFuture<?> countdown;
        private Instant deadline = Instant.now();

        private PendingMatch(String matchId) {
            this.matchId = matchId;
        }

        private void add(MatchTicket ticket) {
            tickets.add(ticket);
            ticket.pendingMatch = this;
            ticket.status = TicketStatus.FORMING;
        }

        private void remove(MatchTicket ticket) {
            tickets.remove(ticket);
        }

        private List<Player> players() {
            List<Player> list = new ArrayList<>(tickets.size());
            for (MatchTicket ticket : tickets) {
                list.add(ticket.player);
            }
            return list;
        }
    }
}