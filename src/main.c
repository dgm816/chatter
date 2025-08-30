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
#include <buffer.h>
#include <signal.h>
#include <globals.h>
#include <version.h>

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
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "s:p:ln:u:r:c:hv", long_options, &option_index)) != -1) {
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
            case 'h':
                printf("Usage: %s [OPTIONS]\n", argv[0]);
                printf("  --server <server>  IRC server to connect to (default: irc.libera.chat)\n");
                printf("  --port <port>      Port to connect to (default: 6697)\n");
                printf("  --ssl              Use SSL/TLS for connection (default: enabled)\n");
                printf("  --nick <nick>      Nickname to use (default: chatter_user)\n");
                printf("  --user <user>      Username to use (default: chatter_user)\n");
                printf("  --realname <name>  Real name to use (default: chatter_user)\n");
                printf("  --channel <channel> Channel to join (default: #chatter)\n");
                printf("  --help             Display this help message and exit\n");
                printf("  --version          Display version information and exit\n");
                printf("\n");
                printf("chatter is free software; you can redistribute it and/or modify\n");
                printf("it under the terms of the GNU General Public License as published by\n");
                printf("the Free Software Foundation; either version 2 of the License, or\n");
                printf("(at your option) any later version.\n");
                exit(EXIT_SUCCESS);
            case 'v':
                printf("chatter v%s\n", get_chatter_version());
                printf("\n");
                printf("chatter is free software; you can redistribute it and/or modify\n");
                printf("it under the terms of the GNU General Public License as published by\n");
                printf("the Free Software Foundation; either version 2 of the License, or\n");
                printf("(at your option) any later version.\n");
                exit(EXIT_SUCCESS);
            case '?': // getopt_long returns '?' for unknown options
            default:
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
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

    tui_init();

    Irc irc;
    irc.server = server;
    irc.nickname = nick;
    irc.username = user;
    irc.realname = realname;
    if (irc_connect(&irc, server, port, nick, user, realname, channel, ssl) != 0) {
        log_message("ERROR: Failed to connect to IRC server");
        tui_destroy(); // Clean up TUI resources before exiting
        close_log();
        return 1;
    }

    tui_run(&irc, channel);
    tui_destroy();

    irc_disconnect(&irc);
    close_log();
    
    return 0;
}