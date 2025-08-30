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
#ifndef TUI_H
#define TUI_H

#include <stdbool.h>
#include <irc.h>

/**
 * @brief Initializes the terminal user interface.
 *
 * This function sets up the ncurses library, creates the main windows
 * (main buffer, status bar, and input line), and prepares the screen
 * for interaction.
 */
void tui_init(void);
void tui_draw_layout(void);
void tui_refresh_main_buffer(void);
void tui_handle_input(int ch, char *input_buffer, size_t buffer_size, int *input_pos, struct Irc *irc, bool *needs_refresh);

/**
 * @brief Tears down the terminal user interface.
 *
 * This function cleans up the ncurses library and restores the
 * terminal to its original state.
 */
void tui_destroy(void);

/**
 * @brief The main run loop for the TUI.
 *
 * This function handles user input, updates the screen, and manages
 * the overall flow of the user interface.
 */
void tui_run(struct Irc *irc, const char *channel);

#endif // TUI_H