*This project has been created as part of the 42 curriculum by apestana, vbullock.*

# ft_irc

## Description

ft_irc is a C++98 IRC server implementing the mandatory 42 project features: password authentication, nicknames, channels, private messages, and channel operator commands. It serves multiple clients in one process through non-blocking IPv4 TCP sockets and a single `poll()` loop.

The reference client is **HexChat 2.16.0**. Netcat can also be used to send IRC commands directly. Minimal CAP negotiation, PING/PONG and WHO support allow HexChat to connect and obtain channel information.

This is a subset of IRC, not a complete implementation of every RFC 2812 feature.

## Instructions

### Build and run

Requirements: a C++98 compiler, Make, and a POSIX environment with sockets and `poll()`. Development and testing use Linux. No external library is required by the server.

```sh
make
./ircserv 2020 samurai
```

The arguments are a port between 1 and 65535 and the server password. The server listens on all IPv4 interfaces. Choose a port your user can bind and that is not already in use.

The Makefile enables `-Wall -Wextra -Werror -std=c++98`. `make clean` removes object and dependency files; `make fclean` also removes the executable; `make re` rebuilds everything. Both `make -j4` and `make -j4 re` are supported: `re` waits for `fclean` to finish before invoking `$(MAKE) all`, which preserves parallel compilation.

Use Ctrl+C to stop the server. SIGINT, SIGTERM and SIGQUIT request orderly shutdown; SIGPIPE is ignored.

### Connect with HexChat

Create a network entry pointing to `127.0.0.1/2020`, disable SSL/TLS, and select **Server password (/PASS password)** as the login method, with `samurai` as the password. Each client needs a different nickname, at most nine characters long. SASL is not supported.

Alternatively, from HexChat's command input:

```text
/server irc://127.0.0.1:2020 samurai
/join #test
```

Connect another instance with a different nickname and join the same channel. Send ordinary text in the channel, or use `/msg bob Hello Bob` for a private message. CAP advertises an empty capability list; registration continues when the client ends negotiation.

### Connect with netcat

With OpenBSD netcat, `-C` converts input line endings to IRC's CRLF:

```sh
nc -C 127.0.0.1 2020
```

Then enter:

```text
PASS samurai
NICK alice
USER alice 0 * :Alice Smith
JOIN #test
```

In another terminal, connect again and register `bob` with the same password. After Bob joins `#test`, Alice can send:

```text
PRIVMSG #test :Hello everyone
PRIVMSG bob :Hello Bob
PING :check
JOIN 0
QUIT :Goodbye
```

Channel messages go to the other members; the sender does not receive an echo. `JOIN 0` leaves all channels. `ERROR :Closing Link: ... (Quit: ...)` is the normal closing notification after QUIT. The server also accepts LF input for interactive clients without netcat's `-C` option.

### Commands and operator modes

| Purpose | Commands |
| --- | --- |
| Registration and identity | `PASS`, `NICK`, `USER`, `CAP` |
| Channels and messages | `JOIN`, `PART`, `PRIVMSG`, `QUIT` |
| Channel administration | `KICK`, `INVITE`, `TOPIC`, `MODE` |
| Connection and member information | `PING`, `PONG`, `WHO` |

The first member of a new channel becomes its operator. Only channel operators can KICK or change channel modes.

| Mode | Effect | Example |
| --- | --- | --- |
| `i` | Require an invitation to join | `MODE #test +i` |
| `t` | Restrict topic changes to operators | `MODE #test +t` |
| `k` | Require a channel key | `MODE #test +k secret` |
| `o` | Grant or remove channel operator status | `MODE #test +o bob` |
| `l` | Limit channel membership | `MODE #test +l 3` |

Use `-` to remove modes: for example, `MODE #test -k secret`, `MODE #test -o bob`, or `MODE #test -l`. `MODE #test` queries the current modes.

Under `+i`, INVITE requires operator status; under `-i`, ordinary members may invite. Under `+t`, only operators may change TOPIC; under `-t`, ordinary members may do so. Enable these modes before testing the corresponding permission errors. Mode `n` is outside the implemented scope, so channel PRIVMSG does not require the sender to be a member.

### Tests

Python 3 is required for the automated suites; OpenBSD netcat is needed for the netcat integration case. Tests start their own servers on temporary ports and use their own password.

```sh
make
python3 tests/test_evaluation.py ./ircserv
python3 tests/test_cap.py ./ircserv
```

These run 24 and 19 cases respectively, including 12 shared PING/PONG/WHO tests: 31 distinct tests overall. The shared tests can also be run separately with `python3 tests/test_ping_pong_who.py ./ircserv`.

The earlier [evaluation report](docs/EVALUATION_REPORT.md) records additional actual HexChat sessions, terminal Ctrl+Z/fg tests, flood tests, allocation failure injection, sanitizer checks, Valgrind results, and findings from that review. Those additional audit harnesses are not part of the two permanent suites above.

## Structure and limits

| Location | Responsibility |
| --- | --- |
| `src/main.cpp` | Validate startup arguments and start the server |
| `src/server/` | Event loop, socket I/O, logging, registration, message dispatch and shared client/channel operations |
| `src/commands/` | One implementation file per IRC command |
| `src/Client.cpp` | Client identity, state and input/output buffers |
| `src/Channel.cpp` | Membership, operators, invitations, topic and channel modes |
| `src/IrcMessage.cpp` | Parse IRC messages |
| `includes/IrcLimits.hpp`, `includes/IrcCaseMapping.hpp`, `src/IrcParameters.cpp` | Shared limits, IRC name comparison and parameter helpers |

IRC frames are limited to 512 bytes including CRLF, with at most 15 parameters. A trailing parameter introduced with `:` may contain more than 15 words. Nicknames are limited to nine characters and usernames to 32. Supported channel prefixes are `#` and `&`; names and nicknames use IRC case mapping, while passwords and channel keys are case-sensitive.

Registration, including CAP negotiation, has a 60-second deadline. Each client's application output queue is limited to 64 KiB: an excessively slow reader is disconnected to protect the other clients. A suspended client can recover queued data when resumed, provided it has not exceeded this limit. Diagnostic logs also have a bounded queue and may be discarded under load.

TLS, SASL, server-to-server links and channel modes beyond `i`, `t`, `k`, `o`, `l` are not implemented.

## Resources

- The ft_irc subject, version 11.0, supplied through the 42 curriculum.
- [RFC 2812: Internet Relay Chat — Client Protocol](https://www.rfc-editor.org/rfc/rfc2812.html).
- [RFC 2811: Internet Relay Chat — Channel Management](https://www.rfc-editor.org/rfc/rfc2811.html).
- [IRCv3 capability negotiation](https://ircv3.net/specs/extensions/capability-negotiation).
- [HexChat getting started](https://hexchat.readthedocs.io/en/latest/getting_started.html).
- Linux manual pages for [poll](https://man7.org/linux/man-pages/man2/poll.2.html) and [fcntl](https://man7.org/linux/man-pages/man2/fcntl.2.html).

### AI use

Codex/ChatGPT and Claude were used during development to help interpret the subject and IRC specifications, review and improve parsing and network handling, reorganize server modules, and implement or review command handlers and error handling. AI assistance also covered message size and allocation failure handling, TCP regression tests, temporary HexChat and stress-test harnesses, and documentation. The authors remain responsible for understanding the code and validating its behavior against the subject.
