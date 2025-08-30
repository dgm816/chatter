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
#include "commands.h"
#include "globals.h"
#include "irc.h"
#include "tui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations for command handlers
static void handle_join_command(Irc *irc, const char **args, buffer_node_t *active_buffer);
static void handle_part_command(Irc *irc, const char **args, buffer_node_t *active_buffer);

// Example command definitions
const command_arg join_args[] = {
    {"channel", ARG_TYPE_CHANNEL, ARG_NECESSITY_REQUIRED}
};

const command_arg part_args[] = {
    {"channel", ARG_TYPE_CHANNEL, ARG_NECESSITY_CONTEXTUAL},
    {"message", ARG_TYPE_STRING, ARG_NECESSITY_OPTIONAL}
};

const command_def command_defs[] = {
    {"join", (void (*)(Irc*, const char**, buffer_node_t*))handle_join_command, join_args, sizeof(join_args) / sizeof(command_arg)},
    {"part", (void (*)(Irc*, const char**, buffer_node_t*))handle_part_command, part_args, sizeof(part_args) / sizeof(command_arg)}
};

const int num_command_defs = sizeof(command_defs) / sizeof(command_def);

static void handle_join_command(Irc *irc, const char **args, buffer_node_t *active_buffer) {
    if (args[0] == NULL) {
        // In a real implementation, we would print an error to the server buffer
        return;
    }
    char send_buf[MAX_MSG_LEN];
    snprintf(send_buf, sizeof(send_buf), "JOIN %s\r\n", args[0]);
    irc_send(irc, send_buf);
}

static void handle_part_command(Irc *irc, const char **args, buffer_node_t *active_buffer) {
    const char *channel_to_part = NULL;
    const char *part_message = "";

    // Determine the channel to part
    if (args[0]) {
        channel_to_part = args[0];
        if (args[1]) {
            part_message = args[1];
        }
    } else {
        if (active_buffer && active_buffer->name[0] == '#') {
            channel_to_part = active_buffer->name;
        } else {
            // Error: /part used in non-channel buffer without specifying a channel
            buffer_append_message(get_buffer_by_name("status"), "Usage: /part [#channel] [message]");
            return;
        }
    }

    // Send the PART command
    char send_buf[MAX_MSG_LEN];
    snprintf(send_buf, sizeof(send_buf), "PART %s :%s\r\n", channel_to_part, part_message);
    irc_send(irc, send_buf);

    // Log the command to the server buffer
    char log_msg[MAX_MSG_LEN];
    snprintf(log_msg, sizeof(log_msg), "--> PART %s (%s)", channel_to_part, part_message);
    buffer_append_message(get_buffer_by_name("status"), log_msg);

    // Find and remove the buffer
    buffer_node_t *buffer_to_remove = get_buffer_by_name(channel_to_part);
    if (buffer_to_remove) {
        remove_buffer(buffer_to_remove);
    }
}

void parse_command(Irc *irc, const char *input, buffer_node_t *active_buffer) {
    if (input[0] != '/') {
        return;
    }

    if (input[1] == '/') {
        // Treat as a regular message
        char *message = strdup(input + 1);
        // This part will need to be integrated with the main message sending logic
        // For now, we'll just print it.
        printf("Message to send: %s\n", message);
        free(message);
        return;
    }

    char *work_str = strdup(input + 1);
    char *token;
    char *rest = work_str;
    const char *args[16]; // Max 16 arguments
    int arg_count = 0;

    // Get command
    token = strtok_r(rest, " ", &rest);
    if (!token) {
        free(work_str);
        return;
    }
    const char *command_name = token;

    // Get arguments
    while ((token = strtok_r(rest, " ", &rest)) != NULL && arg_count < 15) {
        args[arg_count++] = token;
    }
    args[arg_count] = NULL;

    // Find and execute command
    for (int i = 0; i < num_command_defs; i++) {
        if (strcmp(command_name, command_defs[i].name) == 0) {
            // Argument validation would go here
            // Correctly cast the handler
            command_defs[i].handler(irc, args, active_buffer);
            break;
        }
    }

    free(work_str);
}