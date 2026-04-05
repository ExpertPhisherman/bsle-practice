
# Simple Client–Server Project Specification

**Server:** C (POSIX Sockets)
**Client:** Python 3 (socket module)

---

## 1. Overview

This project implements a basic TCP client–server system where:

- A **C server** accepts and manages a client connection.
- A **Python client** connects to the server and sends structured requests.
- The system uses a clearly defined communication protocol.
- The server processes commands and returns structured responses.

---

## 2. Functional Requirements

### 2.1 Server (C)

#### 2.1.1 Networking

- Use TCP sockets (`AF_INET`, `SOCK_STREAM`)
- Bind to configurable IP and port
- Use `listen()` and `accept()`

#### 2.1.2 Packet Handling

- Receive packets reliably (handle partial `recv()`)
- Validate packet size before processing
- Send responses reliably (handle partial `send()`)
- Detect client disconnects
- Handle invalid or malformed packets safely

#### 2.1.3 Commands

Minimum required commands:

| Command | Description                 |
|---------|-----------------------------|
| PING    | Respond with PONG           |
| ECHO    | Return the provided message |
| QUIT    | Close client connection     |

---

### 2.2 Client (Python)

#### 2.2.1 Networking

- Use Python `socket` module
- Connect to configurable server IP and port
- Implement reliable receive logic (`recv_all`)
- Gracefully handle server disconnect

#### 2.2.2 Interface

- Provide CLI interface (hint:use CMD module in python)
- Accept user commands
- Serialize commands into protocol format
- Display formatted server responses

---

## 3. Protocol Specification

All multi-byte integers must use **network byte order (big-endian)**.

### 3.1 Request Packet Format

| Field   | Size     | Description        |
|---------|----------|--------------------|
| Opcode  | 1 byte   | Command identifier |
| Length  | 4 bytes  | Payload size       |
| Payload | Variable | Command data       |

### 3.2 Response Packet Format

| Field    | Size     | Description                     |
|----------|----------|-------------                    |
| Status   | 1 byte   | Success (0) or Error (non-zero) |
| Length   | 4 bytes  | Payload size                    |
| Payload  | Variable | Response data                   |

---

## 4. Opcode Definitions

| Opcode | Value | Meaning          |
|--------|-------|------------------|
| PING   | 0x01  | Health check     |
| ECHO   | 0x02  | Echo message     |
| QUIT   | 0x03  | Close connection |

---

## 5. Error Handling

### Server

- Validate payload length before allocation
- Enforce maximum payload size (e.g., 4096 bytes)
- Handle:
  - Interrupted syscalls
  - Partial sends
  - Invalid opcodes
  - Abrupt client disconnects
- Log errors to stderr or log file

### Client

- Detect connection failure
- Detect server disconnect
- Validate response length
- Exit gracefully on fatal errors

---

## 6. Security Requirements

- No unsafe C functions
- Check return values of all socket operations
- Prevent integer overflow when calculating sizes
- Reject oversized payloads
- Validate all client-provided data on server

## 7. Stretch goals

- Make the server able to handle multiple client connections via multithreading or polling
- Implement an authentication feature that allows for creation of a username/password and login

## 8. Learning topics

- Network byte order/Serialization (endianness)
- Handling of partial sends/receives
- SIGINT handling for graceful shutdown
- Partial sends & partial receives
- File descriptors
