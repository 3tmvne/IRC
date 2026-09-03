# Mini Server — the skeleton of ft_irc, explained

This folder contains a ~250-line TCP **chat server** in C++98. It is not an IRC
server yet: it has no commands, no channels, no password. But it contains **all
the hard parts of ft_irc** — the parts the subject can grade you 0 on if you get
them wrong:

- one single `poll()` that manages *everything* (listening, reading, writing),
- non-blocking sockets,
- per-client **read buffer** (rebuild commands from partial TCP data),
- per-client **write buffer** (never `send()` without poll's permission),
- clean handling of connect / disconnect, without ever crashing.

If you understand every line of this file, adding the IRC logic on top
(PASS/NICK/USER, JOIN, PRIVMSG, KICK...) is "just" parsing and bookkeeping.

---

## 1. Build and try it

```bash
make
./mini_serv 6667
```

In a **second** terminal:

```bash
nc 127.0.0.1 6667
```

In a **third** terminal:

```bash
nc 127.0.0.1 6667
```

Type a line in one `nc` → it appears in the other one as `client 5: your text`.
That is the same mechanism as an IRC channel: the server reads a full line from
one client and forwards it to the others.

### The subject's partial-data test

The ft_irc subject insists on this: TCP gives you a **stream of bytes**, not
messages. A "command" can arrive in several pieces. Test it:

```bash
nc -C 127.0.0.1 6667
com        <- type "com" then press ctrl+D (sends WITHOUT newline)
man        <- "man" then ctrl+D again
d          <- "d" then Enter
```

The other client receives **one** line: `client 5: command`. That works because
the server stores the pieces in `inBuf` and only reacts when a `\n` arrives.
A naive server that treats "1 recv() = 1 command" fails this test — and fails
the evaluation.

---

## 2. The big picture

The whole server is one loop around one `poll()` call:

```
                        ┌────────────────────────────────┐
                        │  update events[]:              │
                        │  every client: POLLIN          │
                        │  + POLLOUT if outBuf not empty │
                        └───────────────┬────────────────┘
                                        │
                                        v
                     ┌─────────  poll(fds, ...)  ─────────┐
                     │   sleeps until SOMETHING happens   │
                     └───────────────┬────────────────────┘
                                     │
          ┌──────────────────────────┼──────────────────────────┐
          v                          v                          v
  listen socket POLLIN       client POLLIN               client POLLOUT
  = someone knocks           = bytes arrived             = socket writable
          │                          │                          │
     accept()                   recv() → inBuf            send() from outBuf
     add to fds[]               cut complete lines        keep the unsent rest
     add to clients             broadcast → outBufs
          │                          │                          │
          └──────────────────────────┴──────────────────────────┘
                                     │
                                back to top
```

Key idea: **the server never waits on a client**. It waits only inside
`poll()`, which watches all sockets at once. When `poll()` returns, it tells
us exactly which sockets are ready, so every `recv()`/`send()` we then do
returns immediately. One slow or malicious client can never freeze the
server for the others.

Be precise about *why* it can't block — this is a classic defense question.
poll()'s report is an excellent hint, but the actual **guarantee** comes from
`O_NONBLOCK`: because every fd is non-blocking, even a socket that turns out
not to be ready just returns `-1` instead of freezing the whole server.
That is why the subject insists on both: poll() for efficiency, non-blocking
fds as the safety net. ("Why non-blocking if you already use poll()?" — now
you can answer.)

---

## 3. The data structures

```cpp
struct Client
{
    std::string inBuf;  // bytes received, waiting for a '\n'
    std::string outBuf; // bytes queued, waiting for POLLOUT
};

std::vector<pollfd>   fds;     // what poll() watches. fds[0] = listen socket
std::map<int, Client> clients; // fd -> that client's two buffers
```

- `pollfd` is the little struct poll() works with:
  `fd` (which socket), `events` (what I want to know: POLLIN = "readable",
  POLLOUT = "writable"), `revents` (what poll actually reports back).
- `fds[0]` is always the **listening** socket — the one clients connect to.
  Every entry after it is one connected client.
- The `map` lets us find a client's buffers from its fd in one lookup.

**Why two buffers per client?** Because both directions of TCP are streams:

- *inBuf*: a command may arrive in pieces → collect until `\n`.
- *outBuf*: `send()` may accept only part of your data (slow client, full
  kernel buffer) → keep the rest and retry when poll says POLLOUT.

In the real ft_irc you will replace this `struct` with a `Client` class
(nickname, username, registered-or-not, channels...) — but it will still
contain these two strings.

---

## 4. Walkthrough, function by function

### `createListenSocket(port)` — opening the door

```cpp
socket(AF_INET, SOCK_STREAM, 0)   // new TCP (SOCK_STREAM) IPv4 (AF_INET) socket
setsockopt(SO_REUSEADDR)          // allow instant restart of the server
bind(fd, addr)                    // attach it to <port> on all interfaces
listen(fd, SOMAXCONN)             // start accepting connection queues
fcntl(fd, F_SETFL, O_NONBLOCK)    // make even accept() non-blocking
```

Two details that matter:

- `htons(port)` / `htonl(...)`: the network byte order is big-endian, your CPU
  is probably little-endian. Forget `htons` and you listen on the wrong port.
- `SO_REUSEADDR`: without it, after you kill the server the port stays blocked
  for ~1 minute (TIME_WAIT) and `bind()` fails on restart. Annoying during the
  defense.

### `acceptNewClient()` — someone knocks

When `poll()` reports POLLIN **on the listening socket**, it means a client is
waiting. `accept()` creates a *new* dedicated socket for that client (the
listening socket keeps listening). We:

1. make the new fd non-blocking,
2. push it into `fds` so poll() watches it from now on,
3. create its `Client` entry,
4. queue a welcome message in its `outBuf` (note: *queue*, not send —
   even the welcome respects the poll rule).

### `readFromClient()` — bytes arrived

```cpp
ssize_t r = recv(fd, buf, sizeof(buf), 0);
if (r <= 0) { disconnectClient(...); return false; }
c.inBuf.append(buf, r);
```

- `r > 0`: we got `r` bytes — append them to `inBuf`.
- `r == 0`: the client closed the connection (that is how TCP says goodbye).
- `r == -1`: an error. We treat it like a disconnect — closing the connection
  is always a safe reaction. Note that we do **not** check `errno` here: we
  only called `recv()` because poll() said the socket was readable, and thanks
  to `O_NONBLOCK` even a rare "not ready after all" returns `-1` instead of
  blocking. (The ft_irc evaluation explicitly forbids using `errno` to
  compensate for a missing poll().)

Then the loop:

```cpp
while ((pos = c.inBuf.find('\n')) != std::string::npos)
```

cuts **complete lines** out of the buffer, strips the `\r` (IRC messages end
with `\r\n`), and gives them to `broadcast()`. If no `\n` is found, nothing
happens — the bytes simply wait for the next `recv()`. This is the whole
secret of the partial-data test.

### `broadcast()` — the heart of a chat/IRC server

For every *other* client, append the message to their `outBuf`. **No send()
here.** At the top of the next loop turn, those clients get `POLLOUT` added to
their `events`, poll() reports when each socket is writable, and only then do
we send. In ft_irc, "send a PRIVMSG to a channel" is exactly this function
plus a membership check.

### `writeToClient()` — poll says we may write

```cpp
ssize_t s = send(fd, c.outBuf.c_str(), c.outBuf.size(), 0);
c.outBuf.erase(0, s);
```

`send()` returns how many bytes the kernel actually accepted — possibly fewer
than we offered. We erase only what was sent; the rest stays queued for the
next POLLOUT. This is the **partial write** handling evaluators look for.

### `disconnectClient()` — leaving without crashing

`close(fd)`, erase the map entry, erase the pollfd. The order of the main
loop matters here: we iterate the clients **backwards** (`for i = size-1 ... 1`)
so that erasing entry `i` never shifts an entry we still have to visit.

### `main()` — the loop itself

```cpp
while (g_running)
{
    rebuild events (POLLIN, + POLLOUT if outBuf not empty)
    poll(...)                          // the ONLY place the program waits
    if listen socket readable  -> acceptNewClient()
    for each client (backwards):
        POLLIN                        -> readFromClient()
        else POLLERR/POLLHUP/POLLNVAL -> disconnect
        POLLOUT                       -> writeToClient()
}
```

One ordering detail that really matters: **POLLIN is checked before POLLHUP**.
A client that sends a last line and closes immediately (think
`printf "bye\n" | nc 127.0.0.1 6667`, or an IRC client sending `QUIT` and
quitting) makes poll() report POLLIN **and** POLLHUP in the same wakeup —
the bytes are still waiting in the kernel buffer. If you disconnect on
POLLHUP first, that final line is silently lost. So: read first, and let
`recv()` returning 0 be the real "the client is gone" signal. We tested it:
with the wrong order, scripted clients lost their last line almost every time.

Signals: `SIGINT` (ctrl+C) sets `g_running = 0` — `poll()` returns `-1`
with `errno == EINTR`, which we treat as "loop again / exit", not as a fatal
error. The flag is a `volatile sig_atomic_t` because that is the only type
C++98 allows a signal handler to write. `SIGPIPE` is ignored, because writing
to a socket whose peer vanished would otherwise **kill the process** —
remember the subject rule: *your server must never crash*.

---

## 5. The rules this design protects you from

| Subject rule | Where it is respected here |
|---|---|
| Only **one** poll() for read AND write AND listen | the single `poll()` in `main` watches `fds[0]` (listen) and every client |
| Grade 0 if you recv/send without poll | `recv` happens only after POLLIN, `send` only after POLLOUT; `broadcast` and the welcome only *queue* |
| All fds non-blocking, `fcntl(fd, F_SETFL, O_NONBLOCK)` only | `setNonBlocking()`, called on the listen socket and every accepted client |
| No forking, many clients, no hanging | one process, one loop; a stuck client just keeps its data in buffers |
| Handle partial data ("com" "man" "d") | `inBuf` + the `find('\n')` loop |
| Never crash / never quit unexpectedly | SIGPIPE ignored, EINTR handled, every recv/send error → clean disconnect |

---

## 6. From this mini server to ft_irc

What you add, in a sensible order:

1. **argv**: take `<port>` **and** `<password>`.
2. **Parser**: turn each extracted line into `prefix / COMMAND / params`
   (IRC grammar: `PRIVMSG #chan :hello there`, the `:` trailing part may
   contain spaces).
3. **Registration state machine**: a client must send `PASS` (correct
   password), `NICK`, `USER` before anything else; then send the `001`
   welcome numeric. Until registered, refuse other commands.
4. **Client class**: nickname, username, registered flag + these two buffers.
5. **Channel class**: name, topic, members, operators, modes (`i t k o l`),
   user limit. `JOIN` creates/joins, `PRIVMSG #chan` = `broadcast()` limited
   to members.
6. **Commands**: NICK, USER, JOIN, PRIVMSG, then the operator ones:
   KICK, INVITE, TOPIC, MODE. Each replies with the correct **numerics**
   (`001`, `433` nick in use, `403` no such channel...) — a real client
   (irssi, HexChat...) needs them to behave correctly.
7. Replace `client 5:` with real IRC message format:
   `:nick!user@host PRIVMSG #chan :text`.

The event loop — the part people fear — **does not change**.

---

## 7. Mini glossary

| Word | Meaning |
|---|---|
| **fd** (file descriptor) | a small int the OS gives you to name an open "file" — here, sockets |
| **socket** | an fd representing one end of a network connection |
| **listening socket** | special socket that receives *connection requests*; `accept()` turns each request into a new client socket |
| **non-blocking** | a call like `recv()` returns immediately instead of waiting — the *guarantee* the server can't freeze, with poll() as the efficient "when to try" signal |
| **poll()** | "here is my list of fds and what I care about; wake me when something is ready" |
| **POLLIN / POLLOUT** | "readable" (data or a new connection waiting) / "writable" (kernel can accept bytes) |
| **POLLHUP / POLLERR / POLLNVAL** | peer hung up / socket error / invalid fd — disconnect, but only after reading any data that came with POLLIN |
| **big-endian / htons** | network protocols store numbers most-significant-byte first; `htons`/`htonl` convert from your CPU's order |
| **TIME_WAIT / SO_REUSEADDR** | a closed port stays reserved briefly; the option lets you rebind immediately |
| **partial read/write** | TCP may deliver or accept *fewer* bytes than you expect — the reason both buffers exist |
