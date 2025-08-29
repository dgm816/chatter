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

// Windows
static WINDOW *main_buffer_win;
static WINDOW *status_bar_win;
static WINDOW *input_line_win;

#define MAX_MESSAGES 100
#define MAX_MSG_LEN 512

static char message_buffer[MAX_MESSAGES][MAX_MSG_LEN];
static int message_count = 0;
static int message_start = 0;
static int scroll_offset = 0;

// Function prototypes
static void tui_draw_borders(void);
static void tui_refresh_all(const char *input_buffer, int input_pos);
static void add_message(const char *msg);
static void draw_main_buffer(void);
static void update_status_bar(const char *status);
static void tui_redraw(void);
static void sigwinch_handler(int signum);
static void process_irc_messages(char *buffer);
static int get_total_display_lines(int text_width);
static void handle_scroll(int page_size);

static char current_status[128] = "[Disconnected]";
static char partial_buffer[MAX_MSG_LEN] = "";

void tui_init(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!has_colors()) {
        endwin();
        log_error("Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);

    int height, width;
    getmaxyx(stdscr, height, width);

    main_buffer_win = newwin(height - 2, width, 0, 0);
    status_bar_win = newwin(1, width, height - 2, 0);
    input_line_win = newwin(1, width, height - 1, 0);

    scrollok(main_buffer_win, TRUE);
    wbkgd(status_bar_win, COLOR_PAIR(1));
    signal(SIGWINCH, sigwinch_handler);
}

void tui_destroy(void) {
    delwin(main_buffer_win);
    delwin(status_bar_win);
    delwin(input_line_win);
    endwin();
}

#include <sys/select.h>

void tui_run(struct Irc *irc) {
    char input_buffer[400];
    int input_pos = 0;
    memset(input_buffer, 0, sizeof(input_buffer));

    curs_set(1);
    timeout(100);

    snprintf(current_status, sizeof(current_status), "[Connected to %s]", irc->channel);
    snprintf(current_status, sizeof(current_status), "[Connected to %s]", irc->channel);

    char version_msg[64];
    snprintf(version_msg, sizeof(version_msg), "chatter v%s", get_chatter_version());
    add_message(version_msg);

    draw_main_buffer();
    tui_refresh_all(input_buffer, input_pos);

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        FD_SET(irc->sock, &fds);

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
                needs_refresh = true;
                if (ch == '\n') {
                    if (strcmp(input_buffer, "/quit") == 0) {
                        running = 0;
                    } else if (strlen(input_buffer) > 0) {
                        char send_buf[MAX_MSG_LEN];
                        snprintf(send_buf, sizeof(send_buf), "PRIVMSG %s :%s\r\n", irc->channel, input_buffer);
                        irc_send(irc, send_buf);

                        char display_buf[MAX_MSG_LEN];
                        snprintf(display_buf, sizeof(display_buf), "<You> %s", input_buffer);
                        add_message(display_buf);
                        scroll_offset = 0;
                        draw_main_buffer();
                    }
                    memset(input_buffer, 0, sizeof(input_buffer));
                    input_pos = 0;
                } else if (ch == KEY_BACKSPACE || ch == 127) {
                    if (input_pos > 0) {
                        input_pos--;
                        input_buffer[input_pos] = '\0';
                    }
                } else if (ch == KEY_PPAGE) {
                    int height, width;
                    getmaxyx(main_buffer_win, height, width);
                    handle_scroll((height - 2) / 2);
                } else if (ch == KEY_NPAGE) {
                    int height, width;
                    getmaxyx(main_buffer_win, height, width);
                    handle_scroll(-((height - 2) / 2));
                } else if (ch == KEY_SPPAGE) {
                    int height, width;
                    getmaxyx(main_buffer_win, height, width);
                    handle_scroll(height - 2);
                } else if (ch == KEY_SNPAGE) {
                    int height, width;
                    getmaxyx(main_buffer_win, height, width);
                    handle_scroll(-(height - 2));
                } else if (ch >= 32 && ch <= 126) {
                    if (input_pos < (sizeof(input_buffer) - 1)) {
                        input_buffer[input_pos++] = ch;
                    }
                } else if (ch == 3) { // Ctrl-C
                    running = 0;
                } else {
                    needs_refresh = false;
                }
            }
        }

        if (FD_ISSET(irc->sock, &fds)) {
            char recv_buf[MAX_MSG_LEN];
            int n = irc_recv(irc, recv_buf, sizeof(recv_buf) - 1);
            if (n > 0) {
                recv_buf[n] = '\0';
                
                char combined_buffer[MAX_MSG_LEN * 2];
                snprintf(combined_buffer, sizeof(combined_buffer), "%s%s", partial_buffer, recv_buf);
                
                process_irc_messages(combined_buffer);
                
                draw_main_buffer();
                needs_refresh = true;
            } else {
                log_message("Connection closed.");
                running = 0;
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

static void process_irc_messages(char *buffer) {
    char *buf_ptr = buffer;
    char *line_end;

    while ((line_end = strstr(buf_ptr, "\r\n")) != NULL) {
        *line_end = '\0';
        if (scroll_offset > 0) {
            int h, w;
            getmaxyx(main_buffer_win, h, w);
            int text_width = w - 2;
            if (text_width > 0) {
                scroll_offset += (strlen(buf_ptr) + text_width - 1) / text_width;
            }
        }
        add_message(buf_ptr);
        buf_ptr = line_end + 2; // Move past "\r\n"
    }

    // Whatever is left is the partial message
    strncpy(partial_buffer, buf_ptr, sizeof(partial_buffer) - 1);
    partial_buffer[sizeof(partial_buffer) - 1] = '\0';
}

static int get_total_display_lines(int text_width) {
    int total_lines = 0;
    for (int i = 0; i < message_count; i++) {
        const char *msg = message_buffer[(message_start + i) % MAX_MESSAGES];
        int msg_len = strlen(msg);
        if (msg_len == 0) continue;
        total_lines += (msg_len + text_width - 1) / text_width;
    }
    return total_lines;
}

static void handle_scroll(int page_size) {
    int height, width;
    getmaxyx(main_buffer_win, height, width);
    
    int text_width = width - 2;
    if (text_width <= 0) return;

    if (page_size > 0) { // Scrolling up
        int total_lines = get_total_display_lines(text_width);
        int view_height = height - 2;
        int max_scroll = total_lines > view_height ? total_lines - view_height : 0;

        scroll_offset += page_size;
        if (scroll_offset > max_scroll) {
            scroll_offset = max_scroll;
        }
    } else { // Scrolling down
        scroll_offset += page_size; // page_size is negative
        if (scroll_offset < 0) {
            scroll_offset = 0;
        }
    }
    draw_main_buffer();
}

static void add_message(const char *msg) {
    char clean_msg[MAX_MSG_LEN];
    int clean_idx = 0;

    // Copy only printable characters from the message
    for (int i = 0; msg[i] != '\0' && clean_idx < MAX_MSG_LEN - 1; i++) {
        if (isprint(msg[i])) {
            clean_msg[clean_idx++] = msg[i];
        }
    }
    clean_msg[clean_idx] = '\0';

    // Trim leading whitespace
    char *start = clean_msg;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    // Return if the message is empty after cleaning
    if (*start == 0) {
        return;
    }

    // Trim trailing whitespace
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    // Ignore if message is now empty
    if (*start == 0) {
        return;
    }

    strncpy(message_buffer[(message_start + message_count) % MAX_MESSAGES], start, MAX_MSG_LEN - 1);
    message_buffer[(message_start + message_count) % MAX_MESSAGES][MAX_MSG_LEN - 1] = '\0';

    if (message_count < MAX_MESSAGES) {
        message_count++;
    } else {
        message_start = (message_start + 1) % MAX_MESSAGES;
    }
}

static void draw_main_buffer(void) {
    wclear(main_buffer_win);
    int height, width;
    getmaxyx(main_buffer_win, height, width);

    int text_width = width - 2;
    if (text_width < 1) return;

    int y = height - 2;

    int lines_to_skip = scroll_offset;

    for (int i = message_count - 1; i >= 0 && y >= 1; i--) {
        const char *msg = message_buffer[(message_start + i) % MAX_MESSAGES];
        int msg_len = strlen(msg);
        
        if (msg_len == 0) {
            continue;
        }

        int lines_needed = (msg_len + text_width - 1) / text_width;
        
        int start_line_in_msg = lines_needed - 1;

        if (lines_to_skip > 0) {
            if (lines_to_skip >= lines_needed) {
                lines_to_skip -= lines_needed;
                continue;
            }
            start_line_in_msg -= lines_to_skip;
            lines_to_skip = 0;
        }

        for (int j = start_line_in_msg; j >= 0 && y >= 1; j--) {
            int offset = j * text_width;
            int len_to_print = msg_len - offset;
            if (len_to_print > text_width) {
                len_to_print = text_width;
            }
            mvwprintw(main_buffer_win, y, 1, "%.*s", len_to_print, msg + offset);
            y--;
        }
    }
}

static void update_status_bar(const char *status) {
    snprintf(current_status, sizeof(current_status), "%s", status);
}

static void tui_draw_borders(void) {
    int height, width;
    getmaxyx(stdscr, height, width);

    box(main_buffer_win, 0, 0);
    mvwprintw(main_buffer_win, 0, 2, " Main Buffer ");
}

static void tui_refresh_all(const char *input_buffer, int input_pos) {
    tui_draw_borders();
    
    wnoutrefresh(main_buffer_win);

    // Redraw status bar
    wclear(status_bar_win);
    mvwprintw(status_bar_win, 0, 1, "%s", current_status);
    wnoutrefresh(status_bar_win);

    // Redraw input line
    wclear(input_line_win);
    mvwprintw(input_line_win, 0, 0, "> %s", input_buffer);
    wmove(input_line_win, 0, input_pos + 2);
    wnoutrefresh(input_line_win);

    doupdate();
}

static void tui_redraw(void) {
    int height, width;
    endwin();
    refresh();
    getmaxyx(stdscr, height, width);

    wresize(main_buffer_win, height - 2, width);
    wresize(status_bar_win, 1, width);
    wresize(input_line_win, 1, width);

    mvwin(status_bar_win, height - 2, 0);
    mvwin(input_line_win, height - 1, 0);

    draw_main_buffer();
    tui_refresh_all("", 0);
}

static void sigwinch_handler(int signum) {
    (void)signum;
    tui_redraw();
}