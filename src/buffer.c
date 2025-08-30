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
#include "buffer.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <version.h>
#include <stdio.h> // For snprintf

// Global handle to the list of buffers
buffer_node_t *buffer_list_head = NULL;
buffer_node_t *active_buffer = NULL;

/**
 * @brief Initializes the buffer list.
 */
void buffer_list_init(void) {
    // Create the initial "status" buffer
    buffer_node_t *status_buffer = create_buffer("status");
    if (status_buffer) {
        add_buffer(status_buffer);
        set_active_buffer(status_buffer);

        char version_msg[64];
        snprintf(version_msg, sizeof(version_msg), "chatter v%s", get_chatter_version());
        buffer_append_message(status_buffer, version_msg);
    }
}

/**
 * @brief Creates a new buffer node.
 * @param name The name of the buffer.
 * @return A pointer to the newly created buffer_node_t, or NULL on failure.
 */
buffer_node_t* create_buffer(const char *name) {
    buffer_node_t *new_buffer = (buffer_node_t*) malloc(sizeof(buffer_node_t));
    if (!new_buffer) {
        return NULL; // Memory allocation failed
    }

    new_buffer->name = strdup(name);
    if (!new_buffer->name) {
        free(new_buffer);
        return NULL; // Memory allocation failed
    }

    new_buffer->lines = NULL;
    new_buffer->line_count = 0;
    new_buffer->capacity = 0;
    new_buffer->active = 0; // Not active by default
    new_buffer->scroll_offset = 0;
    new_buffer->at_bottom = true;
    new_buffer->prev = NULL;
    new_buffer->next = NULL;

    return new_buffer;
}

/**
 * @brief Adds a buffer to the global buffer list.
 * @param buffer The buffer to add.
 */
void add_buffer(buffer_node_t *buffer) {
    if (!buffer) {
        return;
    }

    if (!buffer_list_head) {
        buffer_list_head = buffer;
        buffer->next = buffer; // Circular list for single node
        buffer->prev = buffer;
    } else {
        buffer_node_t *tail = buffer_list_head->prev;
        tail->next = buffer;
        buffer->prev = tail;
        buffer->next = buffer_list_head;
        buffer_list_head->prev = buffer;
    }
}

/**
 * @brief Appends a message to a buffer.
 * @param buffer The buffer to append the message to.
 * @param message The message string to append.
 */
void buffer_append_message(buffer_node_t *buffer, const char *message) {
    if (!buffer || !message) {
        return;
    }

    // Reallocate if capacity is reached
    if (buffer->line_count >= buffer->capacity) {
        int new_capacity = buffer->capacity == 0 ? 10 : buffer->capacity * 2; // Start with 10, then double
        char **new_lines = (char**) realloc(buffer->lines, new_capacity * sizeof(char*));
        if (!new_lines) {
            // Handle realloc failure
            return;
        }
        buffer->lines = new_lines;
        buffer->capacity = new_capacity;
    }

    buffer->lines[buffer->line_count] = strdup(message);
    if (!buffer->lines[buffer->line_count]) {
        // Handle strdup failure
        return;
    }
    buffer->line_count++;
    if (buffer->at_bottom) {
        // This is a simplified calculation. A more accurate calculation would need to
        // account for line wrapping, which requires knowledge of the window width.
        // For now, we'll just scroll to the last line.
        if (buffer->line_count > 0) {
            buffer->scroll_offset = buffer->line_count - 1;
        }
    }
}

/**
 * @brief Gets a buffer by its name.
 * @param name The name of the buffer to find.
 * @return A pointer to the buffer_node_t if found, or NULL if not found.
 */
buffer_node_t* get_buffer_by_name(const char *name) {
    if (!buffer_list_head || !name) {
        return NULL;
    }

    buffer_node_t *current = buffer_list_head;
    do {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    } while (current != buffer_list_head);

    return NULL;
}

/**
 * @brief Sets the given buffer as the active buffer.
 * @param buffer The buffer to set as active.
 */
void set_active_buffer(buffer_node_t *buffer) {
    if (!buffer) {
        return;
    }

    if (active_buffer) {
        active_buffer->active = 0; // Deactivate current active buffer
    }
    active_buffer = buffer;
    active_buffer->active = 1; // Activate new buffer
}

/**
 * @brief Frees all resources used by a buffer.
 * @param buffer The buffer to free.
 */
void buffer_free(buffer_node_t *buffer) {
    if (!buffer) {
        return;
    }

    // Free each line
    for (int i = 0; i < buffer->line_count; i++) {
        free(buffer->lines[i]);
    }

    // Free the lines array
    free(buffer->lines);

    // Free the buffer name
    free(buffer->name);

    // Free the buffer struct itself
    free(buffer);
}