# Simple Server — start here

One file, ~100 lines. A server that **repeats back** every line you send it.
That's all. But it already contains the 3 ideas ft_irc is built on.

## Run it

```bash
make
./simple_serv 6667
```

In another terminal:

```bash
nc 127.0.0.1 6667
hello           <- you type this
you said: hello <- the server answers
```

Open a third terminal with another `nc` — both clients work at the same time.
That is the point of `poll()`.

## The 3 ideas

### Idea 1 — one loop, one poll(), no waiting on clients

```
while (true)
{
    poll(...)        <- sleep until ANY socket has news
    for each socket that has news:
        the door?    -> accept() the new client
        a client?    -> recv() their bytes
}
```

The server never says "I wait for client A". It says "wake me when *anyone*
does *anything*". So 1 client or 100 clients — same loop, nobody can block it.

### Idea 2 — the door vs. the clients

- `socket()` + `bind()` + `listen()` create the **door**: a socket where
  clients knock. It is always `fds[0]`.
- When the door has an "event", it's a knock → `accept()` gives you a **new
  socket** just for that client. We add it to the `fds` list so poll watches
  it too.

### Idea 3 — TCP sends bytes, not lines

When you `recv()`, you may get **half a line, or three lines** — TCP does not
respect your messages. So each client has a `std::string` buffer:

```cpp
inBuf[fd].append(tmp, n);            // store whatever arrived
while (buf.find('\n') != npos)       // only react to COMPLETE lines
    ...take the line, answer it...
```

Test it yourself: with `nc -C 127.0.0.1 6667`, type `com` then press
**Ctrl+D** (sends without newline), then `man` + Ctrl+D, then `d` + Enter.
The server answers `you said: command` — one line, rebuilt from 3 pieces.
This exact test is in the ft_irc subject.

## What is (deliberately) missing

This file cheats in one place to stay simple: it calls `send()` directly
after reading. Real ft_irc must also wait for poll's permission **to write**
(POLLOUT) and keep an output buffer per client — otherwise: grade 0. It also
doesn't broadcast to other clients, and doesn't handle Ctrl+C nicely.

All of that is in the next step: [`../mini_server/`](../mini_server/) — same
loop, plus the write buffer, broadcast (= a chat), and clean error handling.

**Learning path:** understand this file 100% → read `mini_server/` → start ft_irc.
