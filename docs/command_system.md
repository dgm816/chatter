# Command Parsing System

## 1. Overview

The command parsing system in `chatter` provides a structured way to handle user commands entered in the TUI. All commands are prefixed with a forward slash (`/`). The system is designed to be extensible, allowing new commands to be added with minimal effort.

Note: To send a message that begins with a `/`, prefix it with an additional `/`. For example, `//hello` will be sent as `/hello`.

The core of the system is a command definition table that maps command names to their respective handlers and defines the arguments they expect. When a user enters a command, the system parses the input, identifies the command, validates its arguments, and executes the corresponding handler function.

## 2. Core Components

The command system is defined in [`include/commands.h`](include/commands.h:1) and implemented in [`src/commands.c`](src/commands.c:1). The key data structures are:

### `command_def`

This structure defines a single command.

```c
typedef struct {
    const char *name;
    void (*handler)(Irc *irc, const char **args, buffer_node_t *active_buffer);
    const command_arg *args;
    int num_args;
} command_def;
```

-   `name`: The name of the command (e.g., "join", "part").
-   `handler`: A function pointer to the command's implementation.
-   `args`: An array of `command_arg` structures defining the command's arguments.
-   `num_args`: The number of arguments in the `args` array.

### `command_arg`

This structure defines a single argument for a command.

```c
typedef struct {
    const char *name;
    arg_type type;
    arg_necessity necessity;
} command_arg;
```

-   `name`: The name of the argument (e.g., "channel", "message").
-   `type`: The type of the argument, defined by the `arg_type` enum.
-   `necessity`: The necessity of the argument, defined by the `arg_necessity` enum.

### `arg_type`

This enum defines the type of an argument, which can be used for validation and context-aware parsing.

```c
typedef enum {
    ARG_TYPE_CHANNEL,
    ARG_TYPE_NICKNAME,
    ARG_TYPE_HOSTMASK,
    ARG_TYPE_STRING
} arg_type;
```

### `arg_necessity`

This enum defines whether an argument is required, optional, or contextual.

```c
typedef enum {
    ARG_NECESSITY_REQUIRED,
    ARG_NECESSITY_OPTIONAL,
    ARG_NECESSITY_CONTEXTUAL
} arg_necessity;
```

-   `ARG_NECESSITY_REQUIRED`: The argument must be provided.
-   `ARG_NECESSITY_OPTIONAL`: The argument may be omitted.
-   `ARG_NECESSITY_CONTEXTUAL`: The argument's necessity depends on the current context (e.g., the active buffer).

## 3. Implementation Details

The primary logic for command parsing is located in the `parse_command` function in [`src/commands.c`](src/commands.c:1).

### `parse_command`

```c
void parse_command(Irc *irc, const char *input, buffer_node_t *active_buffer);
```

This function takes the raw user input, the IRC connection object, and the active buffer as arguments. It performs the following steps:

1.  **Checks for Command Prefix**: It first verifies that the input string starts with a `/`. If not, it returns immediately. If the input starts with `//`, it is treated as a regular message with the first `/` stripped.
2.  **Tokenization**: The function tokenizes the input string to separate the command name from its arguments.
3.  **Command Lookup**: It then iterates through the `command_defs` array to find a matching command definition.
4.  **Execution**: If a match is found, it calls the associated handler function, passing the parsed arguments.

## 4. TUI Integration

The command system is integrated into the TUI in [`src/tui.c`](src/tui.c:1). The `tui_handle_input` function is responsible for capturing user input.

When the user presses `Enter`, `tui_handle_input` checks if the input buffer starts with a `/`. If it does, it calls `parse_command` to process the command. Otherwise, the input is treated as a regular message to be sent to the active channel or user.

## 5. Adding New Commands

To add a new command, follow these steps:

1.  **Define a Handler Function**: Create a new static function in [`src/commands.c`](src/commands.c:1) that will handle the command's logic. The function must have the following signature:

    ```c
    static void handle_my_command(Irc *irc, const char **args, buffer_node_t *active_buffer);
    ```

2.  **Define Arguments**: In [`src/commands.c`](src/commands.c:1), define a `command_arg` array for the new command's arguments.

    ```c
    const command_arg my_command_args[] = {
        {"arg1", ARG_TYPE_STRING, ARG_NECESSITY_REQUIRED},
        {"arg2", ARG_TYPE_STRING, ARG_NECESSITY_OPTIONAL}
    };
    ```

3.  **Add to `command_defs`**: Add a new `command_def` entry to the `command_defs` array in [`src/commands.c`](src/commands.c:1).

    ```c
    const command_def command_defs[] = {
        // ... existing commands
        {"my_command", (void (*)(Irc*, const char**, buffer_node_t*))handle_my_command, my_command_args, sizeof(my_command_args) / sizeof(command_arg)}
    };
    ```

4.  **Implement Handler Logic**: Implement the logic for `handle_my_command` in [`src/commands.c`](src/commands.c:1). You can use the provided arguments and the active buffer to perform the desired actions.

By following these steps, you can easily extend `chatter` with new commands while maintaining a clean and organized codebase.

## 6. Available Commands

### /join

*   **Usage**: `/join <channel>`
*   **Description**: Joins the specified IRC channel.
*   **Arguments**: `channel` (required): The name of the channel to join (e.g., `#chatter`).

### /part

*   **Usage**: `/part [channel]`
*   **Description**: Leaves the specified IRC channel. If no channel is provided, it will part the currently active channel.
*   **Arguments**: `channel` (optional): The name of the channel to leave. If omitted, the current channel is used.

### /nick

*   **Usage**: `/nick <new_nickname>`
*   **Description**: Changes your IRC nickname.
*   **Arguments**: `new_nickname` (required): The new nickname to use.

### /quit

*   **Usage**: `/quit [message]`
*   **Description**: Disconnects from the IRC server.
*   **Arguments**: `message` (optional): A quit message to be sent to the server.