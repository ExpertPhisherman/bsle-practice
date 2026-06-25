
# Documentation

## README

### Project overview and architecture

- None

### Build/run instructions

- Install [prompt_toolkit](https://github.com/prompt-toolkit/python-prompt-toolkit) Python library with the command:
`python3 -m pip install prompt_toolkit`
- Run `make` from the chat project directory (the directory this README is in)

### Protocol summary

- None

### Feature list

- None

### Testing instructions

- See example commands in chat_client.py

## Known issues

### Bugs

- None

### Limitations

- Displaying of requests and reponses only shows synchronous packets (async packets, such as from msg_send, don't get displayed)

### Potential improvements

- Put all linked source and header files under the same project directory
- Enforce project structure to be a [DAG](https://en.wikipedia.org/wiki/Directed_acyclic_graph) (directed acyclic graph), i.e. forbid two header files to include each other

#### Server

- Remove sender's name from all request values when they leave room
- Add foreach function for SLL and hash table
- Disallow PM or file transfer requests sent to self
- Enforce cap on hash table items count
- Add max retries parameter to sendall/recvall
- Send banner on client connect

#### Client

- Add max retries parameter to recvall
- Receive banner on client connect
