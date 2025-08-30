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

## License

This project is licensed under the [GNU General Public License v2](LICENSE).

## Keybindings

*   `Alt-k`: Move up the buffer list.
*   `Alt-j`: Move down the buffer list.
*   `Page Up`: Scroll up by half a page in the main buffer.
*   `Page Down`: Scroll down by half a page in the main buffer.
*   `Shift+Page Up`: Scroll up by a full page in the main buffer.
*   `Shift+Page Down`: Scroll down by a full page in the main buffer.
