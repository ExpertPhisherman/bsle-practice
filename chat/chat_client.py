import argparse
import struct
import os
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

OPCODE_DEFAULT    = 0x00
OPCODE_PING       = 0x01
OPCODE_ECHO       = 0x02
OPCODE_QUIT       = 0x03
OPCODE_LOGIN      = 0x04
OPCODE_LOGOUT     = 0x05
OPCODE_MSG_SEND   = 0x06
OPCODE_MSG_RECV   = 0x07
OPCODE_JOIN       = 0x08
OPCODE_LIST       = 0x09
OPCODE_REQUEST    = 0x0a
OPCODE_RESPOND    = 0x0b
OPCODE_FILE_SEND  = 0x0c
OPCODE_PROMOTE    = 0x0d
OPCODE_DISCONNECT = 0x0e
OPCODE_DELETE     = 0x0f

RETCODE_SUCCESS       = 0x01
RETCODE_SESSION_ERROR = 0x02
RETCODE_OVERFLOW      = 0x03
RETCODE_PENDING       = 0x04
RETCODE_NOT_PENDING   = 0x05
RETCODE_UNALLOWED     = 0x06
RETCODE_UNAUTHORIZED  = 0x07
RETCODE_FAILURE       = 0xff

FIELD_SIZE_OPCODE     = 1
FIELD_SIZE_RETCODE    = 1
FIELD_SIZE_FLAG       = 1
FIELD_SIZE_SIZE       = 2
FIELD_SIZE_SESSION_ID = 4

LIST_FLAG_ROOM = 0x00
LIST_FLAG_USER = 0x01

REQ_FLAG_TYPE_PM   = 0x00
REQ_FLAG_TYPE_FILE = 0x01

RESP_FLAG_TYPE_PM   = 0x00
RESP_FLAG_TYPE_FILE = 0x01

RESP_FLAG_CHOICE_ACCEPT  = 0x00
RESP_FLAG_CHOICE_DECLINE = 0x01

MSG_FLAG_MSG   = 0x00
MSG_FLAG_NOTIF = 0x01
MSG_FLAG_LIST  = 0x02
MSG_FLAG_JOIN  = 0x03
MSG_FLAG_FILE  = 0x04

FILE_CHUNK_FLAG_FIRST = 0x01
FILE_CHUNK_FLAG_LAST  = 0x02
FILE_CHUNK_SIZE       = 512

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
        self.session_id        = 0
        self.request           = b""
        self.username          = None
        self.password          = None
        self.room_name         = None
        self._file_recv_name   = None
        self._file_recv_buffer = bytearray()
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

        self.opcode_funcs[OPCODE_DEFAULT]    = self.opcode_default
        self.opcode_funcs[OPCODE_PING]       = self.opcode_ping
        self.opcode_funcs[OPCODE_ECHO]       = self.opcode_echo
        self.opcode_funcs[OPCODE_QUIT]       = self.opcode_quit
        self.opcode_funcs[OPCODE_LOGIN]      = self.opcode_login
        self.opcode_funcs[OPCODE_LOGOUT]     = self.opcode_logout
        self.opcode_funcs[OPCODE_MSG_SEND]   = self.opcode_msg_send
        self.opcode_funcs[OPCODE_MSG_RECV]   = self.opcode_msg_recv
        self.opcode_funcs[OPCODE_JOIN]       = self.opcode_join
        self.opcode_funcs[OPCODE_LIST]       = self.opcode_list
        self.opcode_funcs[OPCODE_REQUEST]    = self.opcode_request
        self.opcode_funcs[OPCODE_RESPOND]    = self.opcode_respond
        self.opcode_funcs[OPCODE_FILE_SEND]  = self.opcode_file_send
        self.opcode_funcs[OPCODE_PROMOTE]    = self.opcode_promote
        self.opcode_funcs[OPCODE_DISCONNECT] = self.opcode_disconnect
        self.opcode_funcs[OPCODE_DELETE]     = self.opcode_delete

    def _get_prompt(self) -> str:
        prompt = ""
        prompt += "(" if self.username else ""
        prompt += f"{self.username}" if self.username else "chat"
        prompt += f"/{self.room_name}" if self.room_name else ""
        prompt += ")" if self.username else ""
        prompt += "> "

        return prompt

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
                    func(arg)
                    self._quit_event.wait()
                    break
                else:
                    with self._listener_lock:
                        if func(arg):
                            break

    def listener(self) -> None:
        while True:
            response = self.recv_response(FIELD_SIZE_OPCODE)
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

    def _dispatch_retcode(
        self,
        retcode: int,
        messages: dict[int, str | None]
    ) -> bool:
        msg = messages.get(retcode, f"Unknown return code: {retcode:02x}")
        if msg is not None:
            print(msg)
        return False

    def opcode_default(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS: "Unknown operation code",
        })

    def opcode_ping(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       "PONG",
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_FAILURE:       "Failed ping",
        })

    def opcode_echo(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE + FIELD_SIZE_SIZE)
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
            print(f"Unknown return code: {retcode:02x}")

        return False

    def opcode_join(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print(f"{self.username} joined room: \"{self.room_name}\"")
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
            print(f"Unknown return code: {retcode:02x}")

        return False

    def opcode_quit(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            print("Goodbye!")
        else:
            print(f"Unknown return code: {retcode:02x}")

        self._quit_event.set()
        return True

    def opcode_login(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE + FIELD_SIZE_SESSION_ID)
        if response is None:
            return False

        retcode, session_id = struct.unpack("!BI", response)

        if retcode == RETCODE_SUCCESS:
            self.session_id = session_id
            print(f"Login, set session ID to: {self.session_id}")
        elif retcode == RETCODE_FAILURE:
            self.session_id = 0
            self.username   = None
            self.password   = None
            self.room_name  = None
            print("Failed login")
        elif retcode == RETCODE_OVERFLOW:
            self.session_id = 0
            self.username   = None
            self.password   = None
            self.room_name  = None
            print("Exceeds maximum packet size")
        else:
            self.session_id = 0
            self.username   = None
            self.password   = None
            self.room_name  = None
            print(f"Unknown return code: {retcode:02x}")

        return False

    def opcode_logout(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False

        retcode = response[0]

        if retcode == RETCODE_SUCCESS:
            self.session_id = 0
            self.username   = None
            self.password   = None
            self.room_name  = None
            print(f"Logout, set session ID to {self.session_id}")
        elif retcode == RETCODE_SESSION_ERROR:
            print("Invalid session")
        elif retcode == RETCODE_FAILURE:
            print("Failed logout")
        else:
            print(f"Unknown return code: {retcode:02x}")

        return False

    def opcode_msg_send(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       None, # Sender also receives message
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_OVERFLOW:      "Exceeds maximum packet size",
            RETCODE_FAILURE:       "Failed message send",
        })

    def opcode_msg_recv(self) -> bool:
        response = self.recv_response(
            FIELD_SIZE_RETCODE +
            FIELD_SIZE_FLAG +
            FIELD_SIZE_SIZE
        )
        if response is None:
            return False

        retcode, flag, size = struct.unpack("!BBH", response)

        payload = self.recv_response(size)
        if payload is None:
            return False

        if retcode == RETCODE_SUCCESS:
            if flag == MSG_FLAG_JOIN:
                room_name = payload.decode("utf-8")
                if room_name == "":
                    room_name = None
                self.room_name = room_name
                if self.room_name is None:
                    print(f"{self.username} left room")
                else:
                    print(f"{self.username} joined room: \"{self.room_name}\"")
            elif flag == MSG_FLAG_NOTIF:
                print(f"Notification: {payload.decode('utf-8')}")
            elif flag == MSG_FLAG_FILE:
                chunk_flags = payload[0]

                if chunk_flags & FILE_CHUNK_FLAG_FIRST:
                    filename_size = struct.unpack("!H", payload[1:3])[0]
                    self._file_recv_name = os.path.basename(payload[3:3 + filename_size].decode("utf-8"))
                    self._file_recv_buffer = bytearray(payload[3 + filename_size:])
                else:
                    self._file_recv_buffer.extend(payload[1:])

                if chunk_flags & FILE_CHUNK_FLAG_LAST:
                    if self._file_recv_name is not None:
                        with open(self._file_recv_name, "wb") as f:
                            f.write(self._file_recv_buffer)
                        print(f"Received file: \"{self._file_recv_name}\"")
                    self._file_recv_name   = None
                    self._file_recv_buffer = bytearray()
            else:
                print(payload.decode("utf-8"))
        else:
            print(f"Unknown return code: {retcode:02x}")

        return False

    def opcode_list(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       "List success",
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_FAILURE:       "Failed list",
        })

    def opcode_request(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       "Request success",
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_OVERFLOW:      "Exceeds maximum packet size",
            RETCODE_PENDING:       "Pending request to user already exists",
            RETCODE_FAILURE:       "Failed request",
        })

    def opcode_respond(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       "Respond success",
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_OVERFLOW:      "Exceeds maximum packet size",
            RETCODE_NOT_PENDING:   "Pending request from user does not exist",
            RETCODE_FAILURE:       "Failed respond",
        })

    def opcode_file_send(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:       "File sent successfully",
            RETCODE_SESSION_ERROR: "Invalid session",
            RETCODE_OVERFLOW:      "Exceeds maximum packet size",
            RETCODE_UNALLOWED:     "Recipient hasn't allowed file transfer",
            RETCODE_FAILURE:       "Failed file send",
        })

    def opcode_promote(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:        "Promotion successful",
            RETCODE_SESSION_ERROR:  "Invalid session",
            RETCODE_OVERFLOW:       "Exceeds maximum packet size",
            RETCODE_UNAUTHORIZED:   "You are not an admin!",
            RETCODE_FAILURE:        "Failed promotion",
        })

    def opcode_disconnect(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:        "Admin disconnect successful",
            RETCODE_SESSION_ERROR:  "Invalid session",
            RETCODE_OVERFLOW:       "Exceeds maximum packet size",
            RETCODE_UNAUTHORIZED:   "You are not an admin!",
            RETCODE_FAILURE:        "Failed admin disconnect",
        })

    def opcode_delete(self) -> bool:
        response = self.recv_response(FIELD_SIZE_RETCODE)
        if response is None:
            return False
        return self._dispatch_retcode(response[0], {
            RETCODE_SUCCESS:        "Room deletion successful",
            RETCODE_SESSION_ERROR:  "Invalid session",
            RETCODE_OVERFLOW:       "Exceeds maximum packet size",
            RETCODE_UNAUTHORIZED:   "You are not an admin!",
            RETCODE_FAILURE:        "Failed room deletion",
        })

    def send_request(self) -> bool:
        """Send request to server"""
        if self.sock is None:
            return True
        try:
            #print(f"{self.session_id=}")
            self.sock.sendall(self.request)
            #print(f"Request bytes: {self.request}")
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
            #print(f"Response bytes: {response}")
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

        username_enc = username.encode("utf-8")
        password_enc = password.encode("utf-8")

        if not (
            (3 <= len(username_enc) <= 16) and
            (8 <= len(password_enc) <= 128) and
            all((c.isalnum() or (c == "_")) for c in username) and
            all((c.isprintable() and (c != " ")) for c in password)
        ):
            print(self.do_login.__doc__)
            return False

        self.request = struct.pack(
            "!BxHH",
            OPCODE_LOGIN,
            len(username_enc),
            len(password_enc),
        )
        self.request += username_enc + password_enc

        self.room_name = None
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
        line_enc = line.encode("utf-8")

        self.request = struct.pack(
            "!BxHI",
            OPCODE_ECHO,
            len(line_enc),
            self.session_id
        )
        self.request += line_enc
        return self.send_request()

    @with_parser(
        description="Send message to all users in room",
        args={"message": {"help": "message to send"}},
        epilog="Can invoke by typing text without a prepended slash character"
    )
    def do_msg_send(self, line: str) -> bool:
        line_enc = line.encode("utf-8")

        message_enc = line_enc

        # Prepend sender username to message
        # NOTE: This is neither enforced nor done by the server
        if self.username is not None:
            message_enc = self.username.encode("utf-8") + b": " + message_enc

        self.request = struct.pack(
            "!BxHI",
            OPCODE_MSG_SEND,
            len(message_enc),
            self.session_id
        )
        self.request += message_enc
        return self.send_request()

    @with_parser(
        description="Send file to server for relay",
        args={
            "username": {"help": "username to relay file to"},
            "src": {"help": "file to read from client"},
            "dst": {
                "help": "file to write to server (default: same as src)",
                "nargs": "?"
            }
        }
    )
    def do_put(self, line: str) -> bool:
        try:
            args = self.do_put.parser.parse_args(line.split())
        except SystemExit:
            return False

        username_enc = args.username.encode("utf-8")
        filename_src = args.src
        filename_dst = args.dst

        if filename_dst is None:
            filename_dst = filename_src

        filename_dst_enc = filename_dst.encode("utf-8")

        try:
            with open(filename_src, "rb") as f:
                file_content = f.read()
        except OSError as e:
            print(f"[!] Could not read \"{filename_src}\": {e}")
            return False

        chunks = [
            file_content[i:i + FILE_CHUNK_SIZE]
            for i in range(0, max(len(file_content), 1), FILE_CHUNK_SIZE)
        ]

        packets = []
        for i, chunk in enumerate(chunks):
            is_first = (i == 0)
            is_last  = (i == len(chunks) - 1)
            flags    = (FILE_CHUNK_FLAG_FIRST if is_first else 0) | \
                       (FILE_CHUNK_FLAG_LAST  if is_last  else 0)

            if is_first:
                pkt = struct.pack(
                    "!BBHHHI",
                    OPCODE_FILE_SEND,
                    flags,
                    len(username_enc),
                    len(filename_dst_enc),
                    len(chunk),
                    self.session_id
                )
                pkt += username_enc + filename_dst_enc + chunk
            else:
                pkt = struct.pack("!BH", flags, len(chunk))
                pkt += chunk

            packets.append(pkt)

        self.request = b"".join(packets)
        return self.send_request()

    @with_parser(
        description="Join room or create it if it doesn't exist",
        args={"room_name": {"help": "Room name to join"}}
    )
    def do_join(self, line: str) -> bool:
        line_enc = line.encode("utf-8")

        self.request = struct.pack(
            "!BxHI",
            OPCODE_JOIN,
            len(line_enc),
            self.session_id
        )
        self.request += line_enc

        self.room_name = line

        return self.send_request()

    @with_parser(
        description="List all available rooms or users in current room",
        args={"flag": {"help": "rooms or users"}}
    )
    def do_list(self, line: str) -> bool:
        flags = {"rooms": LIST_FLAG_ROOM, "users": LIST_FLAG_USER}

        if line not in flags:
            print(f"Invalid flag: {line}")
            return False

        self.request = struct.pack(
            "!BBI",
            OPCODE_LIST,
            flags[line],
            self.session_id
        )

        return self.send_request()

    @with_parser(
        description="Request PM or file transfer to user",
        args={
            "flag_type": {"help": "pm or file"},
            "username" : {"help": "username to send request to"}
        }
    )
    def do_request(self, line: str) -> bool:
        try:
            args = self.do_request.parser.parse_args(line.split())
        except SystemExit:
            return False

        flag_type = args.flag_type
        username  = args.username

        flag_types = {"pm": REQ_FLAG_TYPE_PM, "file": REQ_FLAG_TYPE_FILE}
        username_enc = username.encode("utf-8")

        if flag_type not in flag_types:
            print(f"Invalid flag type: {flag_type}")
            return False

        self.request = struct.pack(
            "!BBHI",
            OPCODE_REQUEST,
            flag_types[flag_type],
            len(username_enc),
            self.session_id
        )
        self.request += username_enc

        return self.send_request()

    @with_parser(
        description="Accept PM or file transfer request from user",
        args={
            "flag_type": {"help": "pm or file"},
            "username" : {"help": "username to send response to"}
        }
    )
    def do_accept(self, line: str) -> bool:
        try:
            args = self.do_accept.parser.parse_args(line.split())
        except SystemExit:
            return False

        flag_type = args.flag_type
        username  = args.username

        flag_types = {"pm": RESP_FLAG_TYPE_PM, "file": RESP_FLAG_TYPE_FILE}
        username_enc = username.encode("utf-8")

        if flag_type not in flag_types:
            print(f"Invalid flag type: {flag_type}")
            return False

        self.request = struct.pack(
            "!BBBHI",
            OPCODE_RESPOND,
            flag_types[flag_type],
            RESP_FLAG_CHOICE_ACCEPT,
            len(username_enc),
            self.session_id
        )
        self.request += username_enc

        return self.send_request()

    @with_parser(
        description="Decline PM or file transfer request from user",
        args={
            "flag_type": {"help": "pm or file"},
            "username" : {"help": "username to send response to"}
        }
    )
    def do_decline(self, line: str) -> bool:
        try:
            args = self.do_decline.parser.parse_args(line.split())
        except SystemExit:
            return False

        flag_type = args.flag_type
        username  = args.username

        flag_types = {"pm": RESP_FLAG_TYPE_PM, "file": RESP_FLAG_TYPE_FILE}
        username_enc = username.encode("utf-8")

        if flag_type not in flag_types:
            print(f"Invalid flag type: {flag_type}")
            return False

        self.request = struct.pack(
            "!BBBHI",
            OPCODE_RESPOND,
            flag_types[flag_type],
            RESP_FLAG_CHOICE_DECLINE,
            len(username_enc),
            self.session_id
        )
        self.request += username_enc

        return self.send_request()

    @with_parser(
        description="Promote user to admin",
        args={"username": {"help": "Username to promote to admin"}}
    )
    def do_promote(self, line: str) -> bool:
        line_enc = line.encode("utf-8")

        self.request = struct.pack(
            "!BxHI",
            OPCODE_PROMOTE,
            len(line_enc),
            self.session_id
        )
        self.request += line_enc

        return self.send_request()

    @with_parser(
        description="Disconnect user from server",
        args={"username": {"help": "Username to disconnect from server"}}
    )
    def do_disconnect(self, line: str) -> bool:
        line_enc = line.encode("utf-8")

        self.request = struct.pack(
            "!BxHI",
            OPCODE_DISCONNECT,
            len(line_enc),
            self.session_id
        )
        self.request += line_enc

        return self.send_request()

    @with_parser(
        description="Delete room",
        args={"room_name": {"help": "Room name to delete"}}
    )
    def do_delete(self, line: str) -> bool:
        line_enc = line.encode("utf-8")

        self.request = struct.pack(
            "!BxHI",
            OPCODE_DELETE,
            len(line_enc),
            self.session_id
        )
        self.request += line_enc

        return self.send_request()

    @with_parser(description="Close client connection")
    def do_quit(self, line: str) -> bool:
        if self._quit_event.is_set():
            return True

        self.request = struct.pack("!Bx", OPCODE_QUIT)

        if self.send_request():
            return True

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

    # Testing Examples:

    chat_client.do_login("admin password")
    time.sleep(0.1)
    chat_client.do_join("general")
    time.sleep(0.1)

    if chat_client.username is None:
        chat_client.do_login("asdf asdfasdf")
        time.sleep(0.1)
        chat_client.do_join("general")
        time.sleep(0.1)

    chat_client.cmdloop()
    chat_client.listening_thread.join()
    chat_client.disconnect()

    return 0

if __name__ == "__main__":
    sys.exit(main())
