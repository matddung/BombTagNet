package com.BombTagNet.Backend.common;

import jakarta.servlet.http.HttpServletRequest;

public final class RequestIpUtils {
    private static final String[] FORWARDED_HEADERS = {
            "X-Forwarded-For",
            "X-Real-IP",
            "CF-Connecting-IP",
            "X-Client-IP",
            "X-Forwarded",
            "Forwarded",
            "Forwarded-For"
    };

    private RequestIpUtils() {
    }

    public static String resolveRemoteAddress(HttpServletRequest request) {
        if (request == null) {
            return null;
        }

        for (String header : FORWARDED_HEADERS) {
            String value = request.getHeader(header);
            if (value == null || value.isBlank()) {
                continue;
            }

            int commaIndex = value.indexOf(',');
            String address = commaIndex >= 0 ? value.substring(0, commaIndex) : value;
            address = address.trim();
            if (!address.isEmpty() && !"unknown".equalsIgnoreCase(address)) {
                return address;
            }
        }

        String remote = request.getRemoteAddr();
        if (remote == null || remote.isBlank()) {
            return null;
        }
        return remote.trim();
    }
}