# Multithreaded Chat Server Project Requirements

## 1. Overview

Build a multithreaded chat system with:

- **Server:** C
- **Client:** Python
- **Transport:** TCP
- **Protocol:** Binary, packet-based

Features include authentication, session management, direct messaging, lobbies, PM workflows, file relay (no storage), and admin controls.

---

## 2. Core Functionality

### 2.1 Accounts & Authentication

- Create account, login, logout
- Reject duplicate usernames and invalid credentials
- Support user roles: **standard** and **admin**

---

### 2.2 Session Management

- Server generates a unique **session ID** on login (e.g., from `/dev/urandom`)
- Client must include session ID in all authenticated requests
- Server validates session before processing requests
- Invalid/missing sessions → request rejected
- Sessions are invalidated on logout
- Session IDs must be fixed-width or clearly defined

---

### 2.3 Messaging

- Send direct messages between users (via server)
- List active users
- Reject messages to offline/nonexistent users
- Preserve sender identity
- Support variable-length messages

---

### 2.4 Lobbies

- Create, join, leave lobbies
- Send messages to users in the same lobby only
- List available lobbies
- Server tracks membership and validates joins
- Define behavior for empty lobbies (e.g., delete or persist)

---

### 2.5 Private Messaging (PM Workflow)

- Send PM request → recipient accepts/declines
- Decline → notify both users
- Accept → create **private lobby**
- Private lobby is isolated and only includes participants
- Prevent conflicting or duplicate PM requests

---

### 2.6 File Transfer (Relay Only)

- Sender requests transfer → recipient accepts/declines
- If accepted, server relays file (no storage)
- Use **chunked transfer** with metadata:
  - filename + length
  - chunk length + payload
  - optional total size / transfer ID
- Include explicit end-of-file signal
- Define behavior for interrupted transfers

---

### 2.7 Admin Features

- Distinguish admin users from standard users
- Admin-only operations must be enforced
- Admins can:
  - Promote users to admin
  - List users and lobbies
  - Disconnect users
  - Delete lobbies

---

## 3. Concurrency

- Server must support multiple clients concurrently (multithreaded design)

---

## 4. Protocol Requirements

### 4.1 Packet Structure

All communication uses packets:

- **Header (fixed-size)**
- **Payload (variable-length, if applicable)**

Header must include:

- Opcode (message type)
- Session ID (if authenticated)
- Length field(s) for payload

---

### 4.2 Variable-Length Data

- All variable fields must include explicit length in header
- Examples: username, message, lobby name, filename, file chunk

---

### 4.3 Framing & Reliability

- Receiver reads header → then exact payload length
- Must handle partial `send()` / `recv()`
- Define field order and format clearly

---

### 4.4 Byte Order

- All multibyte values use **network byte order**

---

### 4.5 Error Handling

- Server returns structured error packets with codes
- Example errors:
  - Invalid credentials
  - Invalid session
  - User/lobby not found
  - Unauthorized action
  - Transfer declined
  - Malformed packet

---

## 5. Packet Types (Minimum)

- **Auth:** register, login, logout
- **Messaging:** direct message, user list
- **Lobbies:** create, join, leave, list, lobby chat
- **PM:** request, accept, decline, private lobby assignment
- **File Transfer:** request, accept/decline, chunk, complete
- **Admin:** list users/lobbies, disconnect user, delete lobby
- **Generic:** success, failure, error

---

## 6. Documentation

### 6.1 README

Include:

- Project overview and architecture
- Build/run instructions
- Protocol summary
- Feature list
- Limitations
- Testing instructions

### 6.2 Known Issues

- Document bugs, limitations, and potential improvements
