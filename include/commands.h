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
#ifndef COMMANDS_H
#define COMMANDS_H

#include "buffer.h"

// Forward declaration for the Irc struct to avoid circular dependencies
typedef struct Irc Irc;

/**
 * @brief Enumeration of argument types for commands.
 */
typedef enum {
    ARG_TYPE_CHANNEL,
    ARG_TYPE_NICKNAME,
    ARG_TYPE_HOSTMASK,
    ARG_TYPE_STRING
} arg_type;

/**
 * @brief Enumeration of argument necessity for commands.
 */
typedef enum {
    ARG_NECESSITY_REQUIRED,
    ARG_NECESSITY_OPTIONAL,
    ARG_NECESSITY_CONTEXTUAL
} arg_necessity;

/**
 * @brief Structure to define a command argument.
 */
typedef struct {
    const char *name;
    arg_type type;
    arg_necessity necessity;
} command_arg;

/**
 * @brief Structure to define a command.
 */
typedef struct {
    const char *name;
    void (*handler)(Irc *irc, const char **args, buffer_node_t *active_buffer);
    const command_arg *args;
    int num_args;
} command_def;

/**
 * @brief Parses a command string and executes the corresponding command.
 *
 * @param irc The IRC connection object.
 * @param input The raw input string from the user.
 * @param active_buffer The currently active buffer, for context-aware parsing.
 */
void parse_command(Irc *irc, const char *input, buffer_node_t *active_buffer);

#endif /* COMMANDS_H */