*This project has been created as part of the 42 curriculum by ozemrani, aregragu, aait-laf.*

# ft_irc

## Description

**ft_irc** is an IRC (Internet Relay Chat) server written in C++98. It implements the
core of the IRC protocol as described in RFC 1459 and RFC 2812, so that any standard
IRC client (irssi, WeeChat, HexChat, or even `nc`) can connect to it, register with a
password, join channels, and exchange messages with other users in real time.

The goal of the project is to understand low-level network programming on Unix:
TCP sockets, non-blocking I/O, and multiplexing many simultaneous clients with a
single `poll()` loop, all without threads or forking.

### Overview

- A single `Server` object owns the listening socket and one `pollfd` entry per client.
- Incoming bytes are appended to a per-client input buffer and split on `\r\n`, so
  partial or aggregated commands (e.g. sent with `nc` and Ctrl+D) are handled correctly.
- Outgoing bytes are queued in a per-client output buffer and flushed only when
  `poll()` reports the socket as writable, so the server never blocks on `send()`.
- Every line is parsed into a `Command` (prefix, verb, arguments, trailing) and
  dispatched to a dedicated handler.
- Channels track their members, operators, pending invitations, topic, key, user
  limit, and mode flags.

### Supported commands

| Command   | Purpose                                                                 |
|-----------|-------------------------------------------------------------------------|
| `PASS`    | Provide the connection password (must be sent before `NICK`/`USER`).    |
| `NICK`    | Set or change the nickname.                                             |
| `USER`    | Set username and real name; completes registration.                    |
| `PING`    | Keep-alive; the server answers with `PONG`.                             |
| `JOIN`    | Join one or more channels (`JOIN #a,#b key1,key2`). Creates the channel if needed; the creator becomes operator. |
| `PRIVMSG` | Send a message to a user or to a channel.                               |
| `TOPIC`   | View or change a channel topic.                                         |
| `INVITE`  | Invite a user to a channel.                                             |
| `KICK`    | Remove a user from a channel (operators only).                          |
| `MODE`    | Change channel modes (operators only).                                  |

### Channel modes

| Mode | Effect                                                     |
|------|------------------------------------------------------------|
| `i`  | Invite-only channel.                                       |
| `t`  | Only operators may change the topic.                       |
| `k`  | Channel key (password) required to join.                   |
| `o`  | Give or take channel operator privilege (`+o nick` / `-o nick`). |
| `l`  | Maximum number of members.                                 |

## Instructions

### Requirements

- A Unix-like system (Linux or macOS).
- A C++ compiler supporting `-std=c++98` (`c++`, `g++`, or `clang++`).
- `make`.

### Compilation

```sh
make        # builds the ircserv binary
make clean  # removes object files
make fclean # removes object files and the binary
make re     # full rebuild
```

The code is compiled with `-Wall -Wextra -Werror -std=c++98`.

### Execution

```sh
./ircserv <port> <password>
```

- `port`: TCP port to listen on, between 1024 and 65535.
- `password`: the password clients must send with `PASS` to register.

Example:

```sh
./ircserv 6667 secret
```

### Connecting with a client

With **irssi**:

```sh
irssi -c 127.0.0.1 -p 6667 -w secret -n mynick
```

With **netcat** (raw protocol):

```sh
nc -C 127.0.0.1 6667
PASS secret
NICK alice
USER alice 0 * :Alice Example
JOIN #general
PRIVMSG #general :hello everyone
```

Stop the server with Ctrl+C (SIGINT) or SIGTERM. All sockets are closed cleanly.

### Project layout

```
inc/                 Headers (Server, Client, Channel, Parser, numeric replies)
src/main.cpp         Argument parsing, signal setup, server startup
src/Server.cpp       Socket setup, poll loop, I/O buffering, command dispatch
src/Client.cpp       Per-client state and input/output buffers
src/Channel.cpp      Channel state, membership, modes, broadcasting
src/Parser.cpp       IRC line parsing into a Command structure
src/commands/        Command handlers: Registration, Messaging, ChannelCmds
docs/                Detailed design notes and command walkthroughs (in French)
```

## Resources

### References

- [RFC 1459 – Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 – Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/) – an up-to-date, readable specification of the client protocol and numeric replies.
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) – sockets, `bind`, `listen`, `accept`, non-blocking I/O.
- `man 2 poll`, `man 2 socket`, `man 2 fcntl`, `man 7 ip`, `man 7 tcp`
- [irssi documentation](https://irssi.org/documentation/) – reference IRC client used for testing.

### Use of AI

AI assistants were used during this project for the following tasks:

- Explaining parts of RFC 1459 / RFC 2812 and the exact format of numeric replies.
- Reviewing the `poll()` loop and buffer handling for edge cases such as partial
  lines and clients disconnecting mid-write.

