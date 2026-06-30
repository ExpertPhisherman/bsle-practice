# Documentation

## README

### Project overview and architecture

A multithreaded TCP chat server written in C (C11) with an interactive Python client.

**Server architecture:**

- Thread pool (`tpool`) assigns one worker thread per client request
- Mutex lock in `appdata_t` protects shared application data (session store, room store, admin table)
- Mutex lock in `appdata_t` serializes file transfers
- One per-client `pthread_mutex_t` in `client_t` protects socket writes
- Application data (`appdata_t`): credential hash table, room list (SLL), admin hash table, session lookup hash table, opcode dispatch table
- Per-client state (`state_t`): session (username, session ID, current room, user_allow), reusable request/response packet buffers

**Source layout:**

| File | Contents |
| --- | --- |
| `chat.c` | Server lifecycle, client init/free, request dispatch loop, global constants |
| `chat_internal.c` | Room create/destroy, user login/logout/join/leave, session lookup |
| `chat_auth.c` | `opcode_login`, `opcode_logout` |
| `chat_basic.c` | `opcode_ping`, `opcode_echo`, `opcode_quit`, `opcode_default`, `validate_session`, `opcode_args_valid` |
| `chat_msg.c` | `opcode_msg_send`, `msg_send`, `msg_send_username`, `msg_send_room` |
| `chat_room.c` | `opcode_join`, `opcode_list` |
| `chat_transfer.c` | `opcode_request`, `opcode_respond`, `opcode_file_send`, `file_relay`, `drain_file_chunks` |
| `chat_admin.c` | `opcode_promote`, `opcode_disconnect`, `opcode_delete` |
| `chat_client.py` | Python interactive client (prompt_toolkit, background listener thread, slash-command dispatch) |

---

### Build/run instructions

**Dependencies:**

- Install [prompt_toolkit](https://github.com/prompt-toolkit/python-prompt-toolkit):
  `python3 -m pip install prompt_toolkit`
- Run `make` from the `chat/` directory to build the server binary at `bin/main`

**Running the server:**

```text
bin/main [-v] [-p port]
```

- `-v` - verbose mode: prints hex dumps of every synchronous request/response packet
- `-p port` - listen port (default: 3333)

**Running the client:**

```text
python3 chat_client.py [rhost] rport [-l lhost] [-p lport]
```

- Remote host (`rhost`) to connect to, defaults to 127.0.0.1
- Remote port (`rport`) to connect to, required argument
- Local host (`lhost`) to bind to and listen on, defaults to 0.0.0.0 (any interface)
- Local port (`lport`) to bind to and listen on, defaults to 0 (random high port)
- On startup, the client auto-logs in as `admin`/`password` and joins `general`. If that fails (e.g., admin is already connected) it falls back to `asdf`/`asdfasdf`

---

### Protocol summary

All communication is over TCP. Every packet starts with a 1-byte opcode immediately followed by a packed fixed-size header struct and then variable-length payload. Every synchronous response is at least 2 bytes: opcode + retcode. All multi-byte integer fields are **network byte order** (big-endian). Here are some examples of the more complex packets:

**Async server response (MSG_RECV):**

```text
[OPCODE_MSG_RECV 1B][retcode 1B][flag 1B][msg_size 2B][payload msg_size B]
```

**Message flags:**

- `MSG` (0x00) broadcast
- `NOTIF` (0x01) notification
- `LIST` (0x02) room/user list
- `JOIN` (0x03) room join event
- `FILE` (0x04) file chunk

**File transfer protocol (chunked):**

Client sends first chunk (follows OPCODE_FILE_SEND byte):

```text
[flags 1B][username_size 2B][filename_size 2B][chunk_data_size 2B][session_id 4B]
[username][filename][chunk_data]
```

Client sends subsequent chunks (no opcode byte):

```text
[flags 1B][chunk_data_size 2B][chunk_data]
```

Server sends first chunk (MSG_RECV payload, MSG_FLAG_FILE):

```text
[flags 1B][filename_size 2B][filename][chunk_data]
```

Server sends subsequent chunks:

```text
[flags 1B][chunk_data]
```

Chunk flags: `FILE_CHUNK_FLAG_FIRST` (0x01), `FILE_CHUNK_FLAG_LAST` (0x02). A single-chunk transfer sets both. Chunk size is governed by `g_file_chunk_size` (default 512 bytes).

**Return codes:**

- `SUCCESS` (0x01)
- `SESSION_ERROR` (0x02)
- `OVERFLOW` (0x03)
- `PENDING` (0x04)
- `NOT_PENDING` (0x05)
- `UNALLOWED` (0x06)
- `UNAUTHORIZED` (0x07)
- `FAILURE` (0xFF)

---

### Feature list

**Authentication:**

- `/login <username> <password>` - log in, creates account on first use. Username: 3-16 alphanumeric/underscore. Password: 8-128 printable non-space characters. Duplicate concurrent sessions are rejected.
- `/logout` - log out without disconnecting

**Rooms:**

- `/join <room_name>` - join an existing room or create it (alphanumeric names). A default `general` room exists at startup.
- `/list rooms` - list all rooms
- `/list users` - list users in the current room

**Messaging:**

- Typing any non-slash line sends a broadcast message to everyone in the current room
- `/echo <message>` - echo a message back from the server

**Private messaging (PM) workflow:**

1. Requester: `/request pm <username>`
2. Target: `/accept pm <username>` or `/decline pm <username>`
3. On accept: a private room named `user1+user2` is created and both users are moved into it

**File transfer workflow:**

1. Sender: `/request file <username>`
2. Target: `/accept file <username>` or `/decline file <username>`
3. On accept: sender runs `/put <username> <src_file> [dst_filename]`
4. File is relayed in chunks, recipient writes it to disk on last chunk

**Admin commands** (require the sending user to be in the admin table):

- `/promote <username>` - add user to the admin table and notify them
- `/disconnect <username>` - forcibly disconnect a user
- `/delete <room_name>` - destroy a room and clear all members' room pointers

**Diagnostics:**

- `/ping` - send PING, server responds PONG

---

### Testing instructions

1. Start the server: `bin/main`
2. Open two terminals, run `python3 chat_client.py <rport>` in each
3. Both clients auto-join `general` on startup, send messages by typing without a slash

**PM workflow test:**

```text
# Client A (admin)
/request pm asdf
# Client B (asdf)
/accept pm admin
# Both clients are moved to the private room admin+asdf
```

**File transfer test:**

```text
# Client A
/request file asdf
# Client B
/accept file admin
# Client A
/put asdf src.txt dst.txt
# Client B receives dst.txt in the current working directory
```

**Admin command test:**

```text
# Client A (admin)
/promote asdf
/delete general
/disconnect asdf
```

---

## Known issues

### Bugs

- There is a potential race window when `client_destroy` is called inside `opcode_disconnect` on a client whose request is in the middle of being processed by the server. For example, admin disconnects asdf while they are sending a file.

### Limitations

- Displaying of requests and responses only shows synchronous packets (async packets, such as from `msg_send`, don't get displayed)

### Potential improvements

- Put all linked source and header files under the same project directory
- Enforce project structure to be a [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph) (directed acyclic graph), i.e. forbid two header files to include each other

#### Server

- Add foreach function for SLL and hash table
- Disallow PM or file transfer requests sent to self
- Enforce cap on hash table items count
- Add max retries parameter to sendall/recvall
- Send banner on client connect

#### Client

- Add max retries parameter to recvall
- Receive banner on client connect
