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
#include <ncurses.h>

#ifndef KEY_SPPAGE
#define KEY_SPPAGE 398
#endif
#ifndef KEY_SNPAGE
#define KEY_SNPAGE 396
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <tui.h>
#include <irc.h>
#include <log.h>
#include <globals.h>
#include <version.h>
#include <buffer.h>
#include <commands.h>

// Windows
static WINDOW *buffer_list_win;
static WINDOW *main_buffer_win;
static WINDOW *status_bar_win;
static WINDOW *input_line_win;

// Pads for content that might exceed window size
static WINDOW *buffer_list_pad;
static WINDOW *main_buffer_pad;

#define BUFFER_LIST_WIDTH 16


// Function prototypes
static void tui_draw_borders(void);
static void tui_refresh_all(const char *input_buffer, int input_pos);
static void tui_refresh_input_line(const char *input_buffer, int input_pos);
static void update_status_bar(const char *status);
static void sigwinch_handler(int signum);
static void draw_buffer_list(void);
static void handle_scroll(int page_size);
static void tui_redraw(void);

static char current_status[128] = "[Disconnected]";
static volatile sig_atomic_t resize_pending = 0;

/**
 * @brief Initializes the terminal user interface.
 *
 * This function sets up the ncurses library, creates the main windows
 * (main buffer, status bar, and input line), and prepares the screen
 * for interaction.
 */
void tui_init(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    signal(SIGWINCH, sigwinch_handler);

    if (!has_colors()) {
        endwin();
        log_error("Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE); // Status bar color
    init_pair(2, COLOR_BLACK, COLOR_CYAN); // Active buffer color

    // Initialize buffer list
    buffer_list_init();

    // Draw initial layout
    tui_draw_layout();

}

/**
 * @brief Tears down the terminal user interface.
 *
 * This function cleans up the ncurses library and restores the
 * terminal to its original state.
 */
void tui_destroy(void) {
    // Free all buffers
    if (buffer_list_head) {
        buffer_node_t *current = buffer_list_head;
        buffer_node_t *last = buffer_list_head;

        // Find the tail of the circular list
        while (last->next != buffer_list_head) {
            last = last->next;
        }

        // Break the circle to treat it as a simple list
        last->next = NULL;

        // Traverse and free the now-linear list
        while (current != NULL) {
            buffer_node_t *next = current->next;
            buffer_free(current);
            current = next;
        }

        buffer_list_head = NULL;
    }

    delwin(buffer_list_win);
    delwin(main_buffer_win);
    delwin(status_bar_win);
    delwin(input_line_win);
    delwin(buffer_list_pad);
    delwin(main_buffer_pad);
    endwin();
}

/**
 * @brief Draws the TUI layout, creating and positioning all windows.
 */
void tui_draw_layout(void) {
    int height, width;
    getmaxyx(stdscr, height, width);

    // Buffer list window (left pane, full height)
    buffer_list_win = newwin(height, BUFFER_LIST_WIDTH, 0, 0);
    buffer_list_pad = newpad(height * 2, BUFFER_LIST_WIDTH); // Pad for potential scrolling

    // Main buffer window (right pane, top section)
    main_buffer_win = newwin(height - 2, width - BUFFER_LIST_WIDTH, 0, BUFFER_LIST_WIDTH);
    main_buffer_pad = newpad(MAX_MSG_LEN * 2, width - BUFFER_LIST_WIDTH); // Pad for messages

    // Status bar window (right pane, second to last line)
    status_bar_win = newwin(1, width - BUFFER_LIST_WIDTH, height - 2, BUFFER_LIST_WIDTH);

    // Input line window (right pane, bottom line)
    input_line_win = newwin(1, width - BUFFER_LIST_WIDTH, height - 1, BUFFER_LIST_WIDTH);

    scrollok(main_buffer_win, FALSE); // We'll manage scrolling with pads
    wbkgd(status_bar_win, COLOR_PAIR(1));
}

/**
 * @brief Refreshes the main buffer window with the content of the active buffer.
 */
void tui_refresh_main_buffer(void) {
    log_message("Drawing main buffer to pad");
    wclear(main_buffer_pad);
    if (!active_buffer) {
        return;
    }

    int pad_height, pad_width;
    getmaxyx(main_buffer_pad, pad_height, pad_width);

    int text_width = pad_width - 2; // Account for borders/padding
    if (text_width < 1) return;

    int current_y = 0;
    for (int i = 0; i < active_buffer->line_count; i++) {
        const char *msg = active_buffer->lines[i];
        int msg_len = strlen(msg);
        
        // Word-wrap long lines
        const char *line_start = msg;
        while (strlen(line_start) > text_width) {
            int break_pos = text_width;
            // Find the last space within the wrap width
            while (break_pos > 0 && line_start[break_pos] != ' ') {
                break_pos--;
            }
            // If no space is found, break the word
            if (break_pos == 0) {
                break_pos = text_width;
            }
            mvwprintw(main_buffer_pad, current_y++, 1, "%.*s", break_pos, line_start);
            line_start += break_pos;
            // Skip leading space on the next line
            if (*line_start == ' ') {
                line_start++;
            }
        }
        mvwprintw(main_buffer_pad, current_y++, 1, "%s", line_start);
    }

    int win_height, win_width;
    getmaxyx(main_buffer_win, win_height, win_width);

    // Calculate max scroll
    int total_display_lines = 0;
    for (int i = 0; i < active_buffer->line_count; i++) {
        int msg_len = strlen(active_buffer->lines[i]);
        total_display_lines += (msg_len + text_width - 1) / text_width;
    }

    int max_scroll = total_display_lines > win_height - 2 ? total_display_lines - (win_height - 2) : 0;
    if (active_buffer->scroll_offset > max_scroll) {
        active_buffer->scroll_offset = max_scroll;
    }
    if (active_buffer->scroll_offset < 0) {
        active_buffer->scroll_offset = 0;
    }

}

#include <sys/select.h>

/**
 * @brief The main run loop for the TUI.
 *
 * This function handles user input, updates the screen, and manages
 * the overall flow of the user interface.
 */
void tui_run(struct Irc *irc, const char *channel) {
    char input_buffer[400];
    int input_pos = 0;
    memset(input_buffer, 0, sizeof(input_buffer));

    curs_set(1);
    timeout(100);

    snprintf(current_status, sizeof(current_status), "[Connected to %s]", irc->server);

    tui_refresh_all(input_buffer, input_pos);

    while (running) {
        if (resize_pending) {
            resize_pending = 0;
            tui_redraw();
            tui_refresh_all(input_buffer, input_pos);
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (irc->state != IRC_STATE_DISCONNECTED) {
            FD_SET(irc->sock, &fds);
        }

        int max_fd = irc->sock;
        if (select(max_fd + 1, &fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            if (running) log_error("select() failed");
            break;
        }

        bool needs_refresh = false;

        if (FD_ISSET(0, &fds)) {
            int ch = getch();
            if (ch != ERR) {
                tui_handle_input(ch, input_buffer, sizeof(input_buffer), &input_pos, irc, &needs_refresh);
            }
        }

        if (irc->state != IRC_STATE_DISCONNECTED && FD_ISSET(irc->sock, &fds)) {
            if (irc_recv(irc) <= 0) {
                log_message("Connection closed.");
                irc->state = IRC_STATE_DISCONNECTED;
                running = 0;
                continue;
            }
            
            char command_buf[16];
            memset(command_buf, 0, sizeof(command_buf));
            irc_process_buffer(irc, &needs_refresh, command_buf, sizeof(command_buf));

            switch (irc->state) {
                case IRC_STATE_CONNECTED: {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "NICK %s\r\n", irc->nickname);
                    if (irc_send(irc, buf) < 0) {
                        log_message("ERROR: Failed to send NICK");
                    }
                    snprintf(buf, sizeof(buf), "USER %s 0 * :%s\r\n", irc->username, irc->realname);
                    if (irc_send(irc, buf) < 0) {
                        log_message("ERROR: Failed to send USER");
                    }
                    irc->state = IRC_STATE_REGISTERING;
                    break;
                }
                case IRC_STATE_REGISTERING:
                    // This state is now handled in irc_process_buffer
                    break;
                case IRC_STATE_REGISTERED:
                    // Normal operation
                    break;
                default:
                    // Do nothing
                    break;
            }
        }

        if (needs_refresh) {
            tui_refresh_all(input_buffer, input_pos);
        }
    }

    update_status_bar("[Disconnected]");
    tui_refresh_all("", 0);
    curs_set(0);
}

/**
 * @brief Handles user input from the TUI.
 * @param ch The character input.
 * @param input_buffer The current input buffer.
 * @param input_pos The current position in the input buffer.
 * @param irc The IRC connection struct.
 * @param needs_refresh Pointer to a boolean indicating if the screen needs refresh.
 */
void tui_handle_input(int ch, char *input_buffer, size_t buffer_size, int *input_pos, struct Irc *irc, bool *needs_refresh) {
    *needs_refresh = true; // Default to refreshing on any input
    if (ch == '\n') {
        if (strcmp(input_buffer, "/quit") == 0) {
            running = 0;
        } else if (input_buffer[0] == '/') {
            parse_command(irc, input_buffer, active_buffer);
        } else if (strlen(input_buffer) > 0) {
            char send_buf[MAX_MSG_LEN];
            char display_buf[MAX_MSG_LEN];

            if (active_buffer && strcmp(active_buffer->name, "status") == 0) {
                snprintf(send_buf, sizeof(send_buf), "%s\r\n", input_buffer);
                irc_send(irc, send_buf);
            } else if (active_buffer) {
                snprintf(send_buf, sizeof(send_buf), "PRIVMSG %s :%s\r\n", active_buffer->name, input_buffer);
                irc_send(irc, send_buf);
                snprintf(display_buf, sizeof(display_buf), "<%s> %s", irc->nickname, input_buffer);
                buffer_append_message(active_buffer, display_buf);
            }
        }
        memset(input_buffer, 0, buffer_size);
        *input_pos = 0;
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (*input_pos > 0) {
            (*input_pos)--;
            input_buffer[*input_pos] = '\0';
        }
    } else if (ch == KEY_PPAGE) {
        handle_scroll(-((getmaxy(main_buffer_win) - 2) / 2));
    } else if (ch == KEY_NPAGE) {
        handle_scroll((getmaxy(main_buffer_win) - 2) / 2);
    } else if (ch == KEY_SPPAGE) {
        handle_scroll(-(getmaxy(main_buffer_win) - 2));
    } else if (ch == KEY_SNPAGE) {
        handle_scroll(getmaxy(main_buffer_win) - 2);
    } else if (ch == 27) { // Alt key
        int next_ch = getch();
        if (next_ch == 'j') {
            if (active_buffer && active_buffer->next) set_active_buffer(active_buffer->next);
        } else if (next_ch == 'k') {
            if (active_buffer && active_buffer->prev) set_active_buffer(active_buffer->prev);
        }
    } else if (isprint(ch)) {
        if (*input_pos < (buffer_size - 1)) {
            input_buffer[(*input_pos)++] = ch;
            input_buffer[*input_pos] = '\0';
        }
    } else if (ch == 3) { // Ctrl-C
        running = 0;
    } else {
        *needs_refresh = false; // Don't refresh for unhandled characters
    }
}


static void handle_scroll(int page_size) {
    int win_height, win_width;
    getmaxyx(main_buffer_win, win_height, win_width);
    
    int text_width = win_width - 2;
    if (text_width <= 0) return;

    // Calculate total display lines for the active buffer
    int total_display_lines = 0;
    if (active_buffer) {
        for (int i = 0; i < active_buffer->line_count; i++) {
            int msg_len = strlen(active_buffer->lines[i]);
            total_display_lines += (msg_len + text_width - 1) / text_width;
        }
    }

    int view_height = win_height - 2; // Usable height of the main buffer window
    int max_scroll = total_display_lines > view_height ? total_display_lines - view_height : 0;

    active_buffer->scroll_offset += page_size;
    if (active_buffer->scroll_offset > max_scroll) {
        active_buffer->scroll_offset = max_scroll;
    }
    if (active_buffer->scroll_offset < 0) {
        active_buffer->scroll_offset = 0;
    }
    active_buffer->at_bottom = (active_buffer->scroll_offset == max_scroll);
    tui_refresh_main_buffer();
}

static void update_status_bar(const char *status) {
    snprintf(current_status, sizeof(current_status), "%s", status);
}

static void tui_draw_borders(void) {
    int height, width;
    getmaxyx(stdscr, height, width);

    // Buffer list border
    box(buffer_list_win, 0, 0);
    mvwprintw(buffer_list_win, 0, 2, " Buffers ");

    // Main buffer border
    box(main_buffer_win, 0, 0);
    mvwprintw(main_buffer_win, 0, 2, " Main Buffer ");
}

static void draw_buffer_list(void) {
    wclear(buffer_list_win);
    tui_draw_borders(); // Redraw borders for buffer list window

    if (!buffer_list_head) {
        wnoutrefresh(buffer_list_win);
        return;
    }

    int y = 1;
    buffer_node_t *current = buffer_list_head;
    do {
        if (y < LINES - 1) { // Ensure we don't write past the screen height
            if (current->active) {
                wattron(buffer_list_win, COLOR_PAIR(2) | A_BOLD);
                mvwprintw(buffer_list_win, y, 1, "> %s", current->name);
                wattroff(buffer_list_win, COLOR_PAIR(2) | A_BOLD);
            } else {
                mvwprintw(buffer_list_win, y, 1, "  %s", current->name);
            }
            y++;
        }
        current = current->next;
    } while (current != buffer_list_head);

    wnoutrefresh(buffer_list_win);
}

static void tui_refresh_all(const char *input_buffer, int input_pos) {
    log_message("Refreshing all windows");
    tui_refresh_main_buffer();
    tui_draw_borders();
    draw_buffer_list();
    
    int win_height, win_width;
    getmaxyx(main_buffer_win, win_height, win_width);
    prefresh(main_buffer_pad, active_buffer->scroll_offset, 1, 1, BUFFER_LIST_WIDTH + 1, win_height - 2, BUFFER_LIST_WIDTH + win_width - 2);

    // Redraw status bar
    wclear(status_bar_win);
    mvwprintw(status_bar_win, 0, 1, "%s", current_status);
    wnoutrefresh(status_bar_win);

    // Redraw input line
    tui_refresh_input_line(input_buffer, input_pos);

    doupdate();
}

static void sigwinch_handler(int signum) {
    if (signum == SIGWINCH) {
        resize_pending = 1;
    }
}

static void tui_redraw(void) {
    // The most reliable way to handle a resize is to completely
    // re-initialize the ncurses environment.
    endwin();
    refresh();

    // Re-initialize ncurses, which will query the terminal for its new size.
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1); // Set cursor for the input line

    // Re-initialize colors
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE); // Status bar color
        init_pair(2, COLOR_BLACK, COLOR_CYAN); // Active buffer color
    }

    // Re-create the entire layout from scratch.
    tui_draw_layout();
}

static void tui_refresh_input_line(const char *input_buffer, int input_pos) {
    wclear(input_line_win);
    mvwprintw(input_line_win, 0, 0, "> %s", input_buffer);
    wmove(input_line_win, 0, input_pos + 2);
    wnoutrefresh(input_line_win);
}
