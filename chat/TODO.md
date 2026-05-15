
# TODO

## Server

- Add mutex lock to client struct for all sends to server
- Add max retries parameter to sendall/recvall
- Add message_t type that includes message content and timestamp
- Add OPCODE_MSG_RECV and OPCODE_MSG_SEND opcode functions
- Modify request and response structs to follow requirements
- Send banner on client connect

## Client

- Use threading to be in recv loop (while_running) receiving 1 byte opcode
- Separate send and recv calls
- Add max retries parameter to recvall
- Periodically receive newer messages
- Receive banner on client connect
