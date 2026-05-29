
# TODO

## Server

- Hash table inside every room that maps [receiver of request] -> [sender of request], so every user checks it before sending a new request to someone who already sent one to them
- Send direct messages between users (via server)
- Add file transfer within direct messages
- Add admin user role and abilities
- Add more descriptive return codes
- Reduce function sizes
- Enforce cap on hash table items count
- Add max retries parameter to sendall/recvall
- Send banner on client connect

## Client

- Add max retries parameter to recvall
- Receive banner on client connect
