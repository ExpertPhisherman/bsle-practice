import argparse
import struct
import sys
import threading
import termios
import readline
from typing import Any, Callable
from pathlib import Path
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

RETCODE_SUCCESS       = 0x01
RETCODE_SESSION_ERROR = 0x02
RETCODE_FAILURE       = 0xff

def with_lock(lock: threading.Lock) -> Callable[[Callable], Callable]:
    """Acquire lock around function call"""
    def decorator(func: Callable) -> Callable:
        def wrapper(*args, **kwargs):
            with lock:
                return func(*args, **kwargs)
        return wrapper
    return decorator

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

    _listener_lock = threading.Lock()

    def __init__(self) -> None:
        super().__init__()
        self.prompt       = "chat> "
        self.session_id   = 0
        self.request      = b""
        self.username     = None
        self.password     = None
        self.opcode       = OPCODE_DEFAULT
        self.opcode_funcs = [None] * (UINT8_MAX + 1)

        self.listening_thread = threading.Thread(
            target=self.listener,
            daemon=False
        )

        self._prompt_active = False
        self._stdout_lock   = threading.Lock()
        self._quit_event    = threading.Event()

        self.opcode_funcs[OPCODE_DEFAULT]  = self.opcode_default
        self.opcode_funcs[OPCODE_PING]     = self.opcode_ping
        self.opcode_funcs[OPCODE_ECHO]     = self.opcode_echo
        self.opcode_funcs[OPCODE_QUIT]     = self.opcode_quit
        self.opcode_funcs[OPCODE_LOGIN]    = self.opcode_login
        self.opcode_funcs[OPCODE_LOGOUT]   = self.opcode_logout
        self.opcode_funcs[OPCODE_MSG_SEND] = None
        self.opcode_funcs[OPCODE_MSG_RECV] = None

    def preloop(self) -> None:
        super().preloop()
        self._prompt_active = True

    def cmdloop(self, intro=None) -> None:
        try:
            super().cmdloop(intro)
        except (KeyboardInterrupt, EOFError):
            print()
            self.do_quit("")

    def postcmd(self, stop: bool, line: str) -> bool:
        return stop or self._quit_event.is_set()

    def async_print(self, msg: str) -> None:
        """Print msg without clobbering the readline prompt."""
        with self._stdout_lock:
            if self._prompt_active and threading.current_thread() is not threading.main_thread():
                buf = readline.get_line_buffer()
                sys.stdout.write('\r\033[K')
                sys.stdout.write(msg + '\n')
                sys.stdout.write(self.prompt + buf)
                sys.stdout.flush()
            else:
                print(msg)

    def listener(self) -> None:
        while True:
            response = self.recv_response(1)
            if response is None:
                self._prompt_active = False
                self._quit_event.set()
                break

            opcode = response[0]

            opcode_func = self.opcode_funcs[opcode]
            if opcode_func is None:
                opcode_func = self.opcode_funcs[OPCODE_DEFAULT]
                if opcode_func is None:
                    print("Default opcode function doesn't exist")
                    continue

            # Run specific opcode function
            if opcode_func():
                break

    @with_lock(_listener_lock)
    def opcode_default(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            self.async_print(f"Unknown operation code: {self.opcode}")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        return False

    @with_lock(_listener_lock)
    def opcode_ping(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            self.async_print("PONG")
        elif retcode == RETCODE_SESSION_ERROR:
            self.async_print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            self.async_print("Failed ping")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        return False

    @with_lock(_listener_lock)
    def opcode_echo(self) -> bool:
        response = self.recv_response(3)
        if response is None:
            return

        retcode, size = struct.unpack("!BH", response)

        payload = self.recv_response(size)
        if payload is None:
            return

        if retcode == RETCODE_SUCCESS:
            self.async_print(payload.decode("utf-8"))
        elif retcode == RETCODE_SESSION_ERROR:
            self.async_print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            self.async_print("Failed echo")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        return False

    @with_lock(_listener_lock)
    def opcode_quit(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            self.async_print("Goodbye!")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        self._quit_event.set()
        return True

    @with_lock(_listener_lock)
    def opcode_login(self) -> bool:
        response = self.recv_response(5)
        if response is None:
            return

        retcode, session_id = struct.unpack("!BI", response)

        if retcode == RETCODE_SUCCESS:
            self.session_id = session_id
            self.async_print(f"Login, set session ID to: {self.session_id}")
        elif retcode == RETCODE_FAILURE:
            self.async_print("Failed login")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        return False

    @with_lock(_listener_lock)
    def opcode_logout(self) -> bool:
        response = self.recv_response(1)
        if response is None:
            return

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            self.username = None
            self.password = None
            self.session_id = 0
            self.async_print(f"Logout, set session ID to {self.session_id}")
        elif retcode == RETCODE_SESSION_ERROR:
            self.async_print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            self.async_print("Failed logout")
        else:
            self.async_print(f"Unknown return code: {retcode}")

        return False

    def send_request(self) -> bool:
        """Send request to server"""

        if self.sock is None:
            return True
        try:
            self.async_print(f"{self.session_id=}")
            self.sock.sendall(self.request)
            self.async_print(f"Request bytes: {self.request}")
            return False
        except (ConnectionError, KeyboardInterrupt) as e:
            if isinstance(e, KeyboardInterrupt):
                self.async_print("")
            self.async_print(f"[!] Error sending data: {e}")
            return True

    def recv_response(self, size: int) -> bytes:
        """Receive response from server"""

        if self.sock is None:
            return None
        try:
            response = self.recvall(size)
            self.async_print(f"Response bytes: {response}")
            return response
        except (ConnectionError, KeyboardInterrupt) as e:
            if isinstance(e, KeyboardInterrupt):
                self.async_print("")
            else:
                self._prompt_active = False
            self.async_print(f"[!] Error receiving data: {e}")
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

        user_flag = 0

        self.request = struct.pack(
            "!BBxxHHI",
            OPCODE_LOGIN,
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

        return False

    @with_parser(description="Respond with PONG")
    def do_ping(self, line: str) -> bool:
        self.request = struct.pack(
            "!BxI",
            OPCODE_PING,
            self.session_id
        )

        if self.send_request():
            return True

        return False

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

        if self.send_request():
            return True

        return False

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
        self.request = struct.pack(
            "!BxI",
            OPCODE_LOGOUT,
            self.session_id
        )

        if self.send_request():
            return True

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

    try:
        fd = sys.stdin.fileno()
        old_term = termios.tcgetattr(fd)
    except termios.error:
        fd, old_term = None, None

    chat_client.listening_thread.start()

    chat_client.do_login("admin password")

    cmdloop_thread = threading.Thread(target=chat_client.cmdloop, daemon=True)
    cmdloop_thread.start()

    try:
        chat_client.listening_thread.join()
    except KeyboardInterrupt:
        print()
        chat_client.do_quit("")
        chat_client.listening_thread.join()

    chat_client.disconnect()

    if old_term is not None:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_term)

    return 0

if __name__ == "__main__":
    sys.exit(main())
