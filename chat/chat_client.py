import argparse
import struct
import sys
import time
import threading
from typing import Any, Callable
from pathlib import Path
from prompt_toolkit import PromptSession
from prompt_toolkit.patch_stdout import patch_stdout
from prompt_toolkit.completion import Completer, Completion
from prompt_toolkit.styles import Style
sys.path.append(str(Path(__file__).absolute().parent.parent))
from client import Client

UINT8_MAX = 255

OPCODE_DEFAULT  = 0x00
OPCODE_PING     = 0x01
OPCODE_ECHO     = 0x02
OPCODE_QUIT     = 0x03
OPCODE_LOGIN    = 0x04
OPCODE_LOGOUT   = 0x05
OPCODE_MSG_SEND = 0x06
OPCODE_MSG_RECV = 0x07
OPCODE_JOIN     = 0x08

RETCODE_SUCCESS       = 0x01
RETCODE_SESSION_ERROR = 0x02
RETCODE_OVERFLOW      = 0x03
RETCODE_FAILURE       = 0xff

FIELD_SIZE_OPCODE     = 1
FIELD_SIZE_RETCODE    = 1
FIELD_SIZE_SIZE       = 2
FIELD_SIZE_SESSION_ID = 4

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

class SlashCompleter(Completer):
    def __init__(self, commands: list[str]) -> None:
        self.commands = commands

    def get_completions(self, document, complete_event):
        text = document.text_before_cursor
        if not text.startswith("/"):
            return
        word = text[1:]
        if " " in word:
            return
        for cmd in self.commands:
            if cmd.startswith(word):
                yield Completion(cmd, start_position=-len(word))

class ChatClient(Client):
    """Chat client"""

    def __init__(self) -> None:
        super().__init__()
        self.session_id   = 0
        self.request      = b""
        self.username     = None
        self.password     = None
        self.room_name    = None
        self.opcode       = OPCODE_DEFAULT
        self.opcode_funcs = [None] * (UINT8_MAX + 1)
        self._quit_event  = threading.Event()
        cmds = [name[3:] for name in dir(self) if name.startswith("do_")]
        style = Style.from_dict({
            "aborting": "default noreverse noitalic nounderline noblink",
            "exiting":  "default noreverse noitalic nounderline noblink",
        })
        self._session = PromptSession(
            message=self._get_prompt,
            completer=SlashCompleter(cmds),
            style=style
        )

        self.listening_thread = threading.Thread(
            target=self.listener,
            daemon=False
        )

        self._listener_lock = threading.Lock()

        self.opcode_funcs[OPCODE_DEFAULT]  = self.opcode_default
        self.opcode_funcs[OPCODE_PING]     = self.opcode_ping
        self.opcode_funcs[OPCODE_ECHO]     = self.opcode_echo
        self.opcode_funcs[OPCODE_QUIT]     = self.opcode_quit
        self.opcode_funcs[OPCODE_LOGIN]    = self.opcode_login
        self.opcode_funcs[OPCODE_LOGOUT]   = self.opcode_logout
        self.opcode_funcs[OPCODE_MSG_SEND] = self.opcode_msg_send
        self.opcode_funcs[OPCODE_MSG_RECV] = None
        self.opcode_funcs[OPCODE_JOIN]     = self.opcode_join

    def _get_prompt(self) -> str:
        return f"({self.username}) " if self.username else "chat> "

    def cmdloop(self, intro=None) -> None:
        """Read and dispatch commands until quit."""
        with patch_stdout():
            while not self._quit_event.is_set():
                try:
                    line = self._session.prompt()
                except (KeyboardInterrupt, EOFError):
                    self.do_quit("")
                    break

                line = line.strip()
                if not line:
                    continue

                if not line.startswith("/"):
                    with self._listener_lock:
                        if self.do_msg_send(line):
                            break
                    continue

                cmd_line = line[1:]
                cmd, *rest = cmd_line.split(None, 1)
                arg = rest[0] if rest else ""

                func = getattr(self, f"do_{cmd}", None)
                if func is None:
                    print(f"Unknown command: /{cmd}")
                    continue

                if func is self.do_quit:
                    if func(arg):
                        break
                else:
                    with self._listener_lock:
                        if func(arg):
                            break

    def listener(self) -> None:
        while True:
            response = self.recv_response(1)
            if response is None:
                self._quit_event.set()
                loop = self._session.app.loop
                if loop is not None and loop.is_running():
                    loop.call_soon_threadsafe(
                        lambda: self._session.app.exit(exception=EOFError)
                    )
                break

            opcode = response[0]

            opcode_func = self.opcode_funcs[opcode]
            if opcode_func is None:
                opcode_func = self.opcode_funcs[OPCODE_DEFAULT]
                if opcode_func is None:
                    print("Default opcode function doesn't exist")
                    continue

            with self._listener_lock:
                if opcode_func():
                    break

    def opcode_default(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print(f"Unknown operation code: {self.opcode}")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    def opcode_ping(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print("PONG")
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            print("Failed ping")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    def opcode_echo(self) -> bool:
        response = self.recv_response(3)
        if response is None:
            return False

        retcode, size = struct.unpack("!BH", response)

        payload = self.recv_response(size)
        if payload is None:
            return False

        if retcode == RETCODE_SUCCESS:
            print(payload.decode("utf-8"))
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_OVERFLOW:
            print("Exceeds maximum packet size")
        elif retcode == RETCODE_FAILURE:
            print("Failed echo")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    def opcode_join(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print(f"{self.username} joined room: {self.room_name}")
        elif retcode == RETCODE_SESSION_ERROR:
            self.room_name = None
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            self.room_name = None
            print("Failed join")
        elif retcode == RETCODE_OVERFLOW:
            self.room_name = None
            print("Exceeds maximum packet size")
        else:
            self.room_name = None
            print(f"Unknown return code: {retcode}")

        return False

    def opcode_quit(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print("Goodbye!")
        else:
            print(f"Unknown return code: {retcode}")

        self._quit_event.set()
        return True

    def opcode_login(self) -> bool:
        response = self.recv_response(5)
        if response is None:
            return False

        retcode, session_id = struct.unpack("!BI", response)

        if retcode == RETCODE_SUCCESS:
            self.session_id = session_id
            print(f"Login, set session ID to: {self.session_id}")
        elif retcode == RETCODE_FAILURE:
            self.session_id = 0
            self.username = None
            self.password = None
            print("Failed login")
        elif retcode == RETCODE_OVERFLOW:
            self.session_id = 0
            self.username = None
            self.password = None
            print("Exceeds maximum packet size")
        else:
            self.session_id = 0
            self.username = None
            self.password = None
            print(f"Unknown return code: {retcode}")

        return False

    def opcode_logout(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

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

    def opcode_msg_send(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print("Message sent")
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_OVERFLOW:
            print("Exceeds maximum packet size")
        elif retcode == RETCODE_FAILURE:
            print("Failed message send")
        else:
            print(f"Unknown return code: {retcode}")

        return False

    def send_request(self) -> bool:
        """Send request to server"""
        if self.sock is None:
            return True
        try:
            print(f"{self.session_id=}")
            self.sock.sendall(self.request)
            print(f"Request bytes: {self.request}")
            return False
        except ConnectionError as e:
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
        except ConnectionError as e:
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
            print(self.do_login.__doc__)
            return False

        self.request = struct.pack(
            "!BBxxHHI",
            OPCODE_LOGIN,
            0,
            len(username),
            len(password),
            self.session_id
        )
        self.request += f"{username}{password}".encode("utf-8")

        self.username = username
        self.password = password

        return self.send_request()

    @with_parser(description="Respond with PONG")
    def do_ping(self, line: str) -> bool:
        self.request = struct.pack("!BxI", OPCODE_PING, self.session_id)
        return self.send_request()

    @with_parser(
        description="Return the provided message",
        args={"message": {"help": "message to echo"}}
    )
    def do_echo(self, line: str) -> bool:
        self.request = struct.pack(
            "!BxHI",
            OPCODE_ECHO,
            len(line),
            self.session_id
        )
        self.request += line.encode("utf-8")
        return self.send_request()

    @with_parser(
        description="Send message to all users in room",
        args={"message": {"help": "message to send"}},
        epilog="Can invoke by typing text without a prepended slash character"
    )
    def do_msg_send(self, line: str) -> bool:
        self.request = struct.pack(
            "!BxHI",
            OPCODE_MSG_SEND,
            len(line),
            self.session_id
        )
        self.request += line.encode("utf-8")
        return self.send_request()

    @with_parser(
        description="Join room or create it if it doesn't exist",
        args={"room_name": {"help": "Room name to join"}}
    )
    def do_join(self, line: str) -> bool:
        self.request = struct.pack(
            "!BxHI",
            OPCODE_JOIN,
            len(line),
            self.session_id
        )
        self.request += line.encode("utf-8")

        self.room_name = line

        return self.send_request()

    @with_parser(description="Close client connection")
    def do_quit(self, line: str) -> bool:
        if self._quit_event.is_set():
            return True

        self.request = struct.pack("!Bx", OPCODE_QUIT)

        if self.send_request():
            return True

        self._quit_event.wait()
        return True

    @with_parser(description="Log out")
    def do_logout(self, line: str) -> bool:
        self.request = struct.pack("!BxI", OPCODE_LOGOUT, self.session_id)
        return self.send_request()

    @with_parser(description="Handle EOF (Ctrl+D)")
    def do_EOF(self, line: str) -> bool:
        return self.do_quit(line)

def main() -> int:
    chat_client = ChatClient()

    chat_client.parse_args()

    failed = chat_client.connect()
    if failed:
        return 1

    chat_client.listening_thread.start()

    #chat_client.do_login(f"test {'password'*5}")
    chat_client.do_login("admin password")
    time.sleep(0.1)
    chat_client.do_echo("a"*22)
    time.sleep(0.1)
    chat_client.do_join("general")
    time.sleep(0.1)

    chat_client.cmdloop()
    chat_client.listening_thread.join()
    chat_client.disconnect()

    return 0

if __name__ == "__main__":
    sys.exit(main())
