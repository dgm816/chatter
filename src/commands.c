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
static void handle_nick(Irc *irc, const char **args, buffer_node_t *active_buffer);

// Example command definitions
const command_arg join_args[] = {
    {"channel", ARG_TYPE_CHANNEL, ARG_NECESSITY_REQUIRED}
};

const command_arg part_args[] = {
    {"channel", ARG_TYPE_CHANNEL, ARG_NECESSITY_CONTEXTUAL},
    {"message", ARG_TYPE_STRING, ARG_NECESSITY_OPTIONAL}
};

const command_arg nick_args[] = {
    {"nickname", ARG_TYPE_NICKNAME, ARG_NECESSITY_OPTIONAL}
};

const command_def command_defs[] = {
    {"join", (void (*)(Irc*, const char**, buffer_node_t*))handle_join_command, join_args, sizeof(join_args) / sizeof(command_arg)},
    {"part", (void (*)(Irc*, const char**, buffer_node_t*))handle_part_command, part_args, sizeof(part_args) / sizeof(command_arg)},
    {"nick", (void (*)(Irc*, const char**, buffer_node_t*))handle_nick, nick_args, sizeof(nick_args) / sizeof(command_arg)}
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
    char part_message_buf[MAX_MSG_LEN] = {0};
    const char *part_message = "";
    int message_arg_start_index = -1;

    if (args[0] && get_buffer_by_name(args[0]) != NULL) {
        // First argument is a channel
        channel_to_part = args[0];
        message_arg_start_index = 1;
    } else {
        // First argument is not a channel, or no arguments
        if (active_buffer && active_buffer->name[0] == '#') {
            channel_to_part = active_buffer->name;
            if (args[0]) {
                message_arg_start_index = 0;
            }
        } else {
            // /part used in non-channel buffer. A channel must be specified.
            if (args[0] == NULL) {
                buffer_append_message(get_buffer_by_name("status"), "Usage: /part [#channel] [message]");
                return;
            }

            // If we are here, it means args[0] was given, but it's not a valid channel
            // because the initial check `get_buffer_by_name(args[0])` failed.
            char error_msg[MAX_MSG_LEN];
            snprintf(error_msg, sizeof(error_msg), "Invalid channel: %s", args[0]);
            buffer_append_message(get_buffer_by_name("status"), error_msg);
            return;
        }
    }

    if (message_arg_start_index != -1) {
        int offset = 0;
        for (int i = message_arg_start_index; args[i] != NULL; i++) {
            int written = snprintf(part_message_buf + offset, sizeof(part_message_buf) - offset, "%s%s", (i > message_arg_start_index ? " " : ""), args[i]);
            if (written < 0 || (size_t)written >= sizeof(part_message_buf) - offset) {
                // Message truncated
                break;
            }
            offset += written;
        }
        part_message = part_message_buf;
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

static void handle_nick(Irc *irc, const char **args, buffer_node_t *active_buffer) {
    if (args[0] == NULL) {
        buffer_append_message(get_buffer_by_name("status"), "Usage: /nick <new_nickname>");
        return;
    }
    char send_buf[MAX_MSG_LEN];
    snprintf(send_buf, sizeof(send_buf), "NICK %s\r\n", args[0]);
    irc_send(irc, send_buf);
}

void parse_command(Irc *irc, const char *input, buffer_node_t *active_buffer) {
    if (input[0] != '/') {
        return;
    }

    if (input[1] == '/') {
        // Handle escaped slash command as a regular message
        if (active_buffer && active_buffer->name[0] == '#') {
            char send_buf[MAX_MSG_LEN];
            snprintf(send_buf, sizeof(send_buf), "PRIVMSG %s :%s\r\n", active_buffer->name, input + 1);
            irc_send(irc, send_buf);

            // Also append the message to the local buffer
            char formatted_msg[MAX_MSG_LEN];
            snprintf(formatted_msg, sizeof(formatted_msg), "<%s> %s", irc->nickname, input + 1);
            buffer_append_message(active_buffer, formatted_msg);
        } else {
            // Send as a raw command
            char send_buf[MAX_MSG_LEN];
            snprintf(send_buf, sizeof(send_buf), "%s\r\n", input + 1);
            irc_send(irc, send_buf);

            // Also append the command to the active buffer for local feedback
            buffer_append_message(active_buffer, input + 1);
        }
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
    int command_found = 0;
    for (int i = 0; i < num_command_defs; i++) {
        if (strcmp(command_name, command_defs[i].name) == 0) {
            // Argument validation would go here
            // Correctly cast the handler
            command_defs[i].handler(irc, args, active_buffer);
            command_found = 1;
            break;
        }
    }

    if (!command_found) {
        char error_msg[MAX_MSG_LEN];
        snprintf(error_msg, sizeof(error_msg), "Unknown command: /%s", command_name);
        buffer_append_message(get_buffer_by_name("status"), error_msg);
    }

    free(work_str);
}