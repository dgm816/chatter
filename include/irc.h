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
#ifndef IRC_H
#define IRC_H

#include <openssl/ssl.h>

typedef struct Irc {
    int sock;
    SSL *ssl;
    SSL_CTX *ctx;
    char *channel;
    char *nickname;
} Irc;

int irc_connect(Irc *irc, const char *host, int port, const char *nick, const char *user, const char *realname, const char *channel, int use_ssl);
void irc_disconnect(Irc *irc);
int irc_send(Irc *irc, const char *data);
int irc_recv(Irc *irc, char *buf, int size);

#endif // IRC_H