import argparse
import struct
import sys
from typing import Any, Callable
from pathlib import Path
sys.path.append(str(Path(__file__).absolute().parent.parent))
from client import Client

OPCODE_DEFAULT  = 0x00
OPCODE_PING     = 0x01
OPCODE_ECHO     = 0x02
OPCODE_QUIT     = 0x03
OPCODE_LOGIN    = 0x04
OPCODE_LOGOUT   = 0x05
OPCODE_MSG_SEND = 0x06
OPCODE_MSG_RECV = 0x07

RETCODE_SUCCESS       = 0x01
RETCODE_SESSION_ERROR = 0x02
RETCODE_FAILURE       = 0xff

def with_parser(
    description: str,
    args: dict[str, dict[str, Any]] | None = None,
    epilog: str | None = None
) -> Callable[[Callable[..., bool]], Callable[..., bool]]:
    """Attach argparse parser to do_* methods in client"""

    parser = argparse.ArgumentParser(
        description=description,
        epilog=epilog,
        add_help=False,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    for name, kwargs in (args or {}).items():
        parser.add_argument(name, **kwargs)

    def decorator(func: Callable[..., bool]) -> Callable[..., bool]:
        parser.prog = func.__name__.removeprefix("do_")
        func.__doc__ = parser.format_help()
        func.parser = parser
        return func

    return decorator

class ChatClient(Client):
    """Chat client"""

    def __init__(self) -> None:
        super().__init__()
        self.prompt = "chat> "
        self.session_id = 0
        self.request = b""
        self.username = None
        self.password = None

    def send_request(self) -> bool:
        """Send request to server"""

        if self.sock is None:
            return True
        try:
            print(f"{self.session_id=}")
            self.sock.sendall(self.request)
            print(f"Request bytes: {self.request}")
            return False
        except (ConnectionError, KeyboardInterrupt) as e:
            if isinstance(e, KeyboardInterrupt):
                print()
            print(f"[!] Error sending data: {e}")
            return True

    def recv_response(self, size: int) -> bytes:
        """Receive response from server"""

        if self.sock is None:
            return None
        try:
            response = self.recvall(size)
            print(f"Response bytes: {response}")
            return response
        except (ConnectionError, KeyboardInterrupt) as e:
            if isinstance(e, KeyboardInterrupt):
                print()
            print(f"[!] Error receiving data: {e}")
            return None

    @with_parser(
        description="Log in with credentials",
        args={
            "username": {"help": "username"},
            "password": {"help": "password"}
        },
        epilog=(
            "Username: 3-16 alphanumeric characters or underscore\n"
            "Password: 8-128 printable characters excluding space"
        )
    )
    def do_login(self, line: str) -> bool:
        try:
            args = self.do_login.parser.parse_args(line.split())
        except SystemExit:
            return False

        username = args.username
        password = args.password

        if not (
            (3 <= len(username) <= 16) and
            (8 <= len(password) <= 128) and
            all((c.isalnum() or (c == "_")) for c in username) and
            all((c.isprintable() and (c != " ")) for c in password)
        ):
            self.do_help("login")
            return False

        opcode = OPCODE_LOGIN
        user_flag = 0

        self.request = struct.pack(
            "!BBxxHHI",
            opcode,
            user_flag,
            len(username),
            len(password),
            self.session_id
        )

        self.request += f"{username}{password}".encode("utf-8")

        print(f"Attempting login to user: {username}")
        self.username = username
        self.password = password

        if self.send_request():
            return True

        response = self.recv_response(6)
        if response is None:
            return True

        response_opcode, retcode, session_id = struct.unpack("!BBI", response)

        if retcode == RETCODE_SUCCESS:
            self.session_id = session_id
            print(f"Login, set session ID to: {self.session_id}")
        elif retcode == RETCODE_FAILURE:
            print("Failed login")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    @with_parser(description="Respond with PONG")
    def do_ping(self, line: str) -> bool:
        opcode = OPCODE_PING

        self.request = struct.pack(
            "!BxI",
            opcode,
            self.session_id
        )

        if self.send_request():
            return True

        response = self.recv_response(2)
        if response is None:
            return True

        response_opcode, retcode = struct.unpack("!BB", response)

        if retcode == RETCODE_SUCCESS:
            print("PONG")
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            print("Failed ping")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    @with_parser(
        description="Return the provided message",
        args={"message": {"help": "message to echo"}}
    )
    def do_echo(self, line: str) -> bool:
        opcode = OPCODE_ECHO

        self.request = struct.pack(
            "!BxHI",
            opcode,
            len(line),
            self.session_id
        )

        self.request += line.encode("utf-8")

        if self.send_request():
            return True

        response = self.recv_response(2 + len(line))
        if response is None:
            return True

        response_opcode, retcode = struct.unpack("!BB", response[:2])

        if retcode == RETCODE_SUCCESS:
            print(response[2:].decode("utf-8"))
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            print("Failed echo")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    @with_parser(description="Close client connection")
    def do_quit(self, line: str) -> bool:
        opcode = OPCODE_QUIT

        self.request = struct.pack("!Bx", opcode)

        if self.send_request():
            return True

        response = self.recv_response(2)
        if response is None:
            return True

        response_opcode, retcode = struct.unpack("!BB", response)

        if retcode == RETCODE_SUCCESS:
            print("Goodbye!")
        else:
            print(f"Unknown return code: {retcode}")

        return True

    @with_parser(description="Log out")
    def do_logout(self, line: str) -> bool:
        opcode = OPCODE_LOGOUT

        self.request = struct.pack(
            "!BxI",
            opcode,
            self.session_id
        )

        if self.send_request():
            return True

        response = self.recv_response(2)
        if response is None:
            return True

        response_opcode, retcode = struct.unpack("!BB", response)

        if retcode == RETCODE_SUCCESS:
            self.username = None
            self.password = None
            self.session_id = 0
            print(f"Logout, set session ID to {self.session_id}")
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            print("Failed logout")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    @with_parser(description="Handle EOF (Ctrl+D)")
    def do_EOF(self, line: str) -> bool:
        print()
        return self.do_quit(line)

def main() -> int:
    chat_client = ChatClient()

    chat_client.parse_args()

    failed = chat_client.connect()
    if failed:
        return 1

    chat_client.do_login("admin password")
    chat_client.cmdloop()
    chat_client.disconnect()
    return 0

if __name__ == "__main__":
    sys.exit(main())
