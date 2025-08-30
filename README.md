# Chatter

A simple IRC client written in C.

## Build Instructions

This project uses CMake for its build system. To build the project, follow these steps:

1. Create a build directory:

   ```bash
   mkdir build
   cd build
   ```

2. Run CMake to configure the project:

   ```bash
   cmake ..
   ```

3. Build the project:

   ```bash
   make
   ```

## Commands

*   `/join <channel>` - Joins the specified IRC channel.
*   `/part [channel] [reason]` - Leaves the current or specified IRC channel.
*   `/nick <new_nickname>` - Changes your nickname on the server.
*   `/quit` - Disconnects from the server and exits the application.

## License

This project is licensed under the [GNU General Public License v2](LICENSE).

## Keybindings

*   `Alt-j`: Move to the next buffer (down the list).
*   `Alt-k`: Move to the previous buffer (up the list).
*   `Page Up`: Scroll up by half a page in the main buffer.
*   `Page Down`: Scroll down by half a page in the main buffer.
*   `Shift+Page Up`: Scroll up by a full page in the main buffer.
*   `Shift+Page Down`: Scroll down by a full page in the main buffer.
