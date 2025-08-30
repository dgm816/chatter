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
#include <buffer.h>
#include <ctype.h>
#include <globals.h>

#define RECV_BUFFER_INITIAL_CAPACITY 4096

void irc_init(Irc *irc) {
    memset(irc, 0, sizeof(Irc));
    irc->state = IRC_STATE_DISCONNECTED;
}

int irc_connect(Irc *irc, const char *host, int port, const char *nick, const char *user, const char *realname, const char *channel, int use_ssl) {
    irc_init(irc);

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

    irc->state = IRC_STATE_CONNECTING;

    freeaddrinfo(res);

    irc->recv_buffer = (char *)malloc(RECV_BUFFER_INITIAL_CAPACITY);
    if (!irc->recv_buffer) {
        log_message("ERROR: Failed to allocate receive buffer");
        close(irc->sock);
        return -1;
    }
    irc->recv_buffer_capacity = RECV_BUFFER_INITIAL_CAPACITY;
    irc->recv_buffer_len = 0;

    irc->nickname = strdup(nick);
    irc->username = strdup(user);
    irc->realname = strdup(realname);
    irc->channel = strdup(channel);
    irc->server = strdup(host);

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

    irc->state = IRC_STATE_CONNECTED;
    return 0;
}

void irc_disconnect(Irc *irc) {
    if (irc->nickname) free(irc->nickname);
    if (irc->username) free(irc->username);
    if (irc->realname) free(irc->realname);
    if (irc->channel) free(irc->channel);
    if (irc->server) free(irc->server);

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
    if (irc->recv_buffer) {
        free(irc->recv_buffer);
        irc->recv_buffer = NULL;
    }
}

int irc_send(Irc *irc, const char *data) {
    log_message("SEND: %s", data);
    buffer_node_t *status_buf = get_buffer_by_name("status");
    if (status_buf) {
        char formatted_msg[MAX_MSG_LEN];
        snprintf(formatted_msg, sizeof(formatted_msg), "-> %s", data);
        // Remove trailing \r\n for display
        char *newline = strstr(formatted_msg, "\r\n");
        if (newline) {
            *newline = '\0';
        }
        buffer_append_message(status_buf, formatted_msg);
    }

    if (irc->ssl) {
        return SSL_write(irc->ssl, data, strlen(data));
    } else {
        return send(irc->sock, data, strlen(data), 0);
    }
}

int irc_process_buffer(Irc *irc, bool *needs_refresh, char *out_command_buf, int out_command_buf_size) {
    char *current_pos = irc->recv_buffer;
    char *line_end;
    int lines_processed = 0;
    *needs_refresh = false; // Initialize output parameter

    while ((line_end = strstr(current_pos, "\r\n")) != NULL) {
        lines_processed++;
        *line_end = '\0'; // Null-terminate the current line
        char *parse_line = current_pos;

        // Always append the raw incoming line to the status buffer
        buffer_node_t *status_buf = get_buffer_by_name("status");
        if (status_buf) {
            buffer_append_message(status_buf, parse_line);
            if (status_buf == active_buffer) {
                *needs_refresh = true;
            }
        }

        // Handle PING requests
        if (strncmp(parse_line, "PING :", 6) == 0) {
            char pong_buf[512];
            snprintf(pong_buf, sizeof(pong_buf), "PONG :%s\r\n", parse_line + 6);
            irc_send(irc, pong_buf);
        } else {
            // Parse IRC message to determine target buffer
            char *prefix = NULL; // Will point to the start of the prefix in parse_line (if any)
            char *command_start_ptr = NULL; // Will point to the start of the command in parse_line
            char *params_start_ptr = NULL; // Will point to the start of the parameters in parse_line

            // 1. Check if the message starts with a prefix (i.e., the first character is a colon).
            if (parse_line[0] == ':') {
                prefix = parse_line + 1; // Prefix starts after the colon
                char *space_after_prefix = strchr(prefix, ' ');
                if (space_after_prefix) {
                    *space_after_prefix = '\0'; // Null-terminate the prefix in parse_line for later use
                    // 2. If a prefix exists, find the first space after the prefix to locate the beginning of the command.
                    command_start_ptr = space_after_prefix + 1;
                    // Skip any additional spaces after the first one
                    while (*command_start_ptr == ' ') {
                        command_start_ptr++;
                    }
                } else {
                    // Malformed message: prefix but no space, so no command or params.
                    // The raw line is already appended to status_buf.
                    current_pos = line_end + 2;
                    continue;
                }
            } else {
                // 3. If there's no prefix, the command starts at the beginning of the message.
                command_start_ptr = parse_line;
                // Skip any leading spaces before the command
                while (*command_start_ptr == ' ') {
                    command_start_ptr++;
                }
            }

            // Ensure command_start_ptr is valid (not pointing to null terminator or empty string)
            if (*command_start_ptr == '\0') {
                current_pos = line_end + 2;
                continue;
            }

            // 4. Once the command is located, find the next space to determine its end.
            char *space_after_command = strchr(command_start_ptr, ' ');
            size_t command_len;

            if (space_after_command) {
                command_len = space_after_command - command_start_ptr;
                params_start_ptr = space_after_command + 1;
                // Skip leading spaces for params
                while (*params_start_ptr == ' ') {
                    params_start_ptr++;
                }
            } else {
                command_len = strlen(command_start_ptr);
                params_start_ptr = NULL; // No parameters
            }

            // 5. Copy the command into out_command_buf.
            // This out_command_buf will be used for strcmp checks later.
            if (out_command_buf && command_start_ptr) {
                if (command_len >= out_command_buf_size) {
                    command_len = out_command_buf_size - 1; // Truncate if too long
                }
                strncpy(out_command_buf, command_start_ptr, command_len);
                out_command_buf[command_len] = '\0'; // Null-terminate the command
                log_message("DEBUG: Parsed command: [%s]", out_command_buf);
            }

            // Set the 'command' and 'params' pointers for the rest of the irc_recv logic.
            char *command = out_command_buf; // Use the null-terminated extracted command
            char *params = params_start_ptr; // Use the determined params_start_ptr
            if (space_after_command) {
                params = space_after_command + 1;
            }

            if (irc->state == IRC_STATE_REGISTERING && (strcmp(command, "001") == 0 || strcmp(command, "376") == 0)) {
                char buf[512];
                snprintf(buf, sizeof(buf), "JOIN %s\r\n", irc->channel);
                if (irc_send(irc, buf) < 0) {
                    log_message("ERROR: Failed to send JOIN");
                }
                irc->state = IRC_STATE_REGISTERED;
            }

            buffer_node_t *target_buffer = NULL;

            if (strcmp(command, "PRIVMSG") == 0 && params) {
                char *target = params;
                char *message_start = strchr(params, ':');
                if (message_start) {
                    *message_start = '\0';
                    message_start++;
                    
                    // Trim target whitespace
                    while (isspace((unsigned char)*target)) target++;
                    char *end = target + strlen(target) - 1;
                    while (end > target && isspace((unsigned char)*end)) end--;
                    *(end + 1) = '\0';

                    // Determine target buffer
                    if (target[0] == '#') { // Channel message
                        target_buffer = get_buffer_by_name(target);
                        if (!target_buffer) {
                            target_buffer = create_buffer(target);
                            add_buffer(target_buffer);
                        }
                    } else if (strcmp(target, irc->nickname) == 0) { // Private message to us
                        // Use sender's nick as buffer name
                        char *sender_nick = prefix;
                        if (sender_nick) {
                            char *excl = strchr(sender_nick, '!');
                            if (excl) *excl = '\0'; // Trim hostname
                            target_buffer = get_buffer_by_name(sender_nick);
                            if (!target_buffer) {
                                target_buffer = create_buffer(sender_nick);
                                add_buffer(target_buffer);
                            }
                        }
                    } else { // Fallback to status buffer for unknown targets
                        target_buffer = get_buffer_by_name("status");
                    }
                    
                    if (target_buffer) {
                        char formatted_msg[MAX_MSG_LEN];
                        if (prefix) {
                            char *nick_only = prefix;
                            char *excl = strchr(nick_only, '!');
                            if (excl) *excl = '\0';
                            snprintf(formatted_msg, sizeof(formatted_msg), "<%s> %s", nick_only, message_start);
                        } else {
                            snprintf(formatted_msg, sizeof(formatted_msg), "%s", message_start);
                        }
                        // Only append to target buffer if it's not the status buffer
                        // as the raw line is already appended to status buffer
                        if (target_buffer != status_buf) {
                            buffer_append_message(target_buffer, formatted_msg);
                            if (target_buffer == active_buffer) {
                                *needs_refresh = true;
                            }
                        }
                    }
                }
            } else if (strcmp(command, "JOIN") == 0 && params) {
                char *joined_channel_param = params;
                if (joined_channel_param[0] == ':') {
                    joined_channel_param++;
                }

                char *sender_nick = prefix;
                if (sender_nick) {
                    char *excl = strchr(sender_nick, '!');
                    if (excl) *excl = '\0';
                    if (strcmp(sender_nick, irc->nickname) == 0) {
                        buffer_node_t *channel_buffer = get_buffer_by_name(joined_channel_param);
                        if (!channel_buffer) {
                            channel_buffer = create_buffer(joined_channel_param);
                            add_buffer(channel_buffer);
                            set_active_buffer(channel_buffer);
                        }
                    }

                    buffer_node_t *channel_buffer = get_buffer_by_name(joined_channel_param);
                    if (channel_buffer) {
                        char join_msg[MAX_MSG_LEN];
                        snprintf(join_msg, sizeof(join_msg), "%s has joined %s", sender_nick, joined_channel_param);
                        buffer_append_message(channel_buffer, join_msg);
                        if (channel_buffer == active_buffer) {
                            *needs_refresh = true;
                        }
                    }
                }
            } else if (strcmp(command, "NOTICE") == 0 && params) {
                // Notices are typically sent to the server buffer
                buffer_node_t *server_buffer = get_buffer_by_name("status");
                if (server_buffer) {
                    char formatted_msg[MAX_MSG_LEN];
                    snprintf(formatted_msg, sizeof(formatted_msg), "-!- %s", params);
                    buffer_append_message(server_buffer, formatted_msg);
                    if (server_buffer == active_buffer) {
                        *needs_refresh = true;
                    }
                }
            }
        }
        current_pos = line_end + 2; // Move past the \r\n
    }

    // Move any remaining partial message to the beginning of the buffer
    size_t remaining_len = irc->recv_buffer_len - (current_pos - irc->recv_buffer);
    if (remaining_len > 0) {
        memmove(irc->recv_buffer, current_pos, remaining_len);
    }
    irc->recv_buffer_len = remaining_len;
    irc->recv_buffer[irc->recv_buffer_len] = '\0'; // Null-terminate the new end

    return lines_processed;
}

int irc_recv(Irc *irc) {
    // Ensure there's enough space in the buffer for new data + null terminator
    // We try to read up to a reasonable chunk size.
    size_t read_size = irc->recv_buffer_capacity - irc->recv_buffer_len -1;
    if (read_size < 1024) {
        size_t new_capacity = irc->recv_buffer_capacity * 2;
        char *new_buffer = (char *)realloc(irc->recv_buffer, new_capacity);
        if (!new_buffer) {
            log_message("ERROR: Failed to reallocate receive buffer");
            return -1;
        }
        irc->recv_buffer = new_buffer;
        irc->recv_buffer_capacity = new_capacity;
    }
    read_size = irc->recv_buffer_capacity - irc->recv_buffer_len - 1;


    int bytes_received;
    if (irc->ssl) {
        bytes_received = SSL_read(irc->ssl, irc->recv_buffer + irc->recv_buffer_len, read_size);
    } else {
        bytes_received = recv(irc->sock, irc->recv_buffer + irc->recv_buffer_len, read_size, 0);
    }

    if (bytes_received > 0) {
        irc->recv_buffer_len += bytes_received;
        irc->recv_buffer[irc->recv_buffer_len] = '\0'; // Null-terminate the accumulated buffer
    }

    return bytes_received;
}