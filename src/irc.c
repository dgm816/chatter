/*
 * chatter - A simple IRC client written in C.
 * Copyright (C) 2025 Doug Miller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <irc.h>
#include <log.h>

int irc_connect(Irc *irc, const char *host, int port, const char *nick, const char *user, const char *realname, const char *channel, int use_ssl) {
    struct addrinfo hints, *res;
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        log_message("ERROR: Failed to get address info");
        return -1;
    }

    if ((irc->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        log_message("ERROR: Failed to create socket");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(irc->sock, res->ai_addr, res->ai_addrlen) < 0) {
        log_message("ERROR: Failed to connect to server");
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);

    irc->channel = strdup(channel);

    if (use_ssl) {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();

        irc->ctx = SSL_CTX_new(TLS_client_method());
        if (irc->ctx == NULL) {
            log_message("ERROR: Failed to create SSL context");
            ERR_print_errors_fp(stderr);
            return -1;
        }

        irc->ssl = SSL_new(irc->ctx);
        SSL_set_fd(irc->ssl, irc->sock);

        if (SSL_connect(irc->ssl) <= 0) {
            log_message("ERROR: Failed to perform SSL handshake");
            ERR_print_errors_fp(stderr);
            return -1;
        }
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "NICK %s\r\n", nick);
    if (irc_send(irc, buf) < 0) return -1;

    snprintf(buf, sizeof(buf), "USER %s 0 * :%s\r\n", user, realname);
    if (irc_send(irc, buf) < 0) return -1;

    snprintf(buf, sizeof(buf), "JOIN %s\r\n", channel);
    if (irc_send(irc, buf) < 0) return -1;

    return 0;
}

void irc_disconnect(Irc *irc) {
    if (irc->channel) {
        free(irc->channel);
    }
    if (irc->ssl) {
        SSL_shutdown(irc->ssl);
        SSL_free(irc->ssl);
    }
    if (irc->ctx) {
        SSL_CTX_free(irc->ctx);
    }
    if (irc->sock > 0) {
        close(irc->sock);
    }
}

int irc_send(Irc *irc, const char *data) {
    log_message("SEND: %s", data);
    if (irc->ssl) {
        return SSL_write(irc->ssl, data, strlen(data));
    } else {
        return send(irc->sock, data, strlen(data), 0);
    }
}

int irc_recv(Irc *irc, char *buf, int size) {
    int bytes;
    if (irc->ssl) {
        bytes = SSL_read(irc->ssl, buf, size - 1);
    } else {
        bytes = recv(irc->sock, buf, size - 1, 0);
    }

    if (bytes > 0) {
        buf[bytes] = '\0';
        log_message("RECV: %s", buf);

        if (strncmp(buf, "PING :", 6) == 0) {
            char pong_buf[512];
            snprintf(pong_buf, sizeof(pong_buf), "PONG :%s\r\n", buf + 6);
            irc_send(irc, pong_buf);
        }
    }

    return bytes;
}