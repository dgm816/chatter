# IRC Connection Command-Line Argument Design

This document outlines the design for the command-line arguments required for the new IRC connection feature in the `chatter` application.

## Command-Line Arguments

| Argument | Description | Default Value |
|---|---|---|
| `--server` | The IRC server to connect to. | `irc.libera.chat` |
| `--port` | The port to use for the IRC connection. | `6667` |
| `--ssl` | A flag to indicate whether to use SSL for the connection. | `false` |
| `--nick` | The nickname to use in the IRC channel. | `chatter_user` |
| `--realname` | The real name to be associated with the user. | `Chatter User` |
| `--host` | The host to use for the connection. | `localhost` |

## Usage Example

Here is an example of how to run the `chatter` application with the new IRC connection arguments:

```bash
./chatter --server irc.example.com --port 6697 --ssl --nick my_awesome_nick --realname "My Awesome Name" --host my_host
