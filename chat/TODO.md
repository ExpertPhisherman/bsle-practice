
# TODO

## Server

- Add admin user role and abilities
- Add file_send function and use it in opcode_file_send
- Make file transfers chunked with chunk number
- Remove sender's name from all request values when they leave room
- Move variable declarations to the top of functions
- Auth, room joining, file transfer, chat messages go in to own source files
- Add foreach function for SLL and hash table
- Disallow PM or file transfer requests sent to self
- Add more descriptive return codes besides RETCODE_FAILURE
- Reduce function sizes
- Enforce cap on hash table items count
- Add max retries parameter to sendall/recvall
- Send banner on client connect

## Client

- Add max retries parameter to recvall
- Receive banner on client connect
