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
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <log.h>
#include <irc.h>
#include <tui.h>
#include <signal.h>
#include <globals.h>

volatile int running = 1;

void handle_sigint(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    open_log("chatter.log");

    char *server = "irc.libera.chat";
    int port = 6697;
    int ssl = 1;
    char *nick = "chatter_user";
    char *user = "chatter_user";
    char *realname = "chatter_user";
    char *channel = "#chatter";

    static struct option long_options[] = {
        {"server", required_argument, 0, 's'},
        {"port", required_argument, 0, 'p'},
        {"ssl", no_argument, 0, 'l'},
        {"nick", required_argument, 0, 'n'},
        {"user", required_argument, 0, 'u'},
        {"realname", required_argument, 0, 'r'},
        {"channel", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "s:p:ln:u:r:c:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                server = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                ssl = 1;
                break;
            case 'n':
                nick = optarg;
                break;
            case 'u':
                user = optarg;
                break;
            case 'r':
                realname = optarg;
                break;
            case 'c':
                channel = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [--server <server>] [--port <port>] [--ssl] [--nick <nick>] [--user <user>] [--realname <realname>] [--channel <channel>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    log_message("Server: %s", server);
    log_message("Port: %d", port);
    log_message("SSL: %s", ssl ? "true" : "false");
    log_message("Nick: %s", nick);
    log_message("User: %s", user);
    log_message("Realname: %s", realname);
    log_message("Channel: %s", channel);

    Irc irc;
    if (irc_connect(&irc, server, port, nick, user, realname, channel, ssl) != 0) {
        log_message("ERROR: Failed to connect to IRC server");
        close_log();
        return 1;
    }

    tui_init();
    tui_run(&irc);
    tui_destroy();

    irc_disconnect(&irc);
    close_log();
    
    return 0;
}