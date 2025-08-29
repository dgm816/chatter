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
#include <log.h>
#include <stdarg.h>
#include <stdlib.h>

static FILE *log_file = NULL;

void open_log(const char *filename) {
    log_file = fopen(filename, "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
}

void close_log(void) {
    if (log_file != NULL) {
        fclose(log_file);
    }
}

void log_message(const char *format, ...) {
    if (log_file != NULL) {
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        va_end(args);
        fflush(log_file);
    }
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (log_file != NULL) {
        va_start(args, format);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        va_end(args);
        fflush(log_file);
    }
}