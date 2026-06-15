
# TODO

## Server

- Add foreach function for SLL and hash table
- Auth, room joining, file transfer, chat messages go in to own source files
- Add function that handles both PM and file transfer requests
- Add function that handles both PM and file transfer responses
- Disallow PM or file transfer requests sent to self
- Remove sender's name from all request values when they leave room
- Send direct messages between users (via server)
- Add admin user role and abilities
- Add more descriptive return codes besides RETCODE_FAILURE
- Reduce function sizes
- Enforce cap on hash table items count
- Add max retries parameter to sendall/recvall
- Send banner on client connect

## Client

- PM and file transfer request handling
- Add max retries parameter to recvall
- Receive banner on client connect
