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
#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>

// Proposed for a new buffer.h header
typedef struct buffer_node {
    char *name;                 // e.g., "status", "#channel", "user"
    char **lines;               // Dynamically allocated array of strings for buffer content
    int line_count;             // Number of lines in the buffer
    int capacity;               // Current capacity of the lines array
    int active;                 // Flag (1 for active, 0 for inactive)
    int scroll_offset;          // Scroll offset for this buffer
    bool at_bottom;             // True if scrolled to the bottom
    struct buffer_node *prev;
    struct buffer_node *next;
} buffer_node_t;

// Global handle to the list of buffers
extern buffer_node_t *buffer_list_head;
extern buffer_node_t *active_buffer;

// Buffer management functions
void buffer_list_init(void);
buffer_node_t* create_buffer(const char *name);
void add_buffer(buffer_node_t *buffer);
void buffer_append_message(buffer_node_t *buffer, const char *message);
buffer_node_t* get_buffer_by_name(const char *name);
void set_active_buffer(buffer_node_t *buffer);
void buffer_free(buffer_node_t *buffer);
void remove_buffer(buffer_node_t *buffer);

#endif // BUFFER_H