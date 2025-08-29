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
#ifndef VERSION_H
#define VERSION_H

#define CHATTER_VERSION_MAJOR 0
#define CHATTER_VERSION_MINOR 0
#define CHATTER_VERSION_PATCH 1

// Function prototype for get_chatter_version
const char* get_chatter_version();

#endif // VERSION_H