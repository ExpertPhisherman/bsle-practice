import sys
import struct
from pathlib import Path
sys.path.append(str(Path(__file__).absolute().parent.parent))
from client import Client

class ChatClient(Client):
    """Chat client"""

    def __init__(self) -> None:
        super().__init__()
        self.prompt = "chat> "

    def send_request(self) -> bool:
        """Send request to server"""

        if self.sock is None:
            return True
        try:
            request = struct.pack("!BI", self.opcode, self.length)
            request += self.payload
            self.sock.sendall(request)
            return False
        except ConnectionError as e:
            print(f"[!] Error sending data: {e}")
            return True

    def recv_response(self) -> bool:
        """Receive response from server"""

        if self.sock is None:
            return True
        try:
            # Receive opcode and payload size in one recvall
            response = self.recvall(5)
            status = response[0]
            if (status != 0x00) and (status != 0x01):
                raise ConnectionError("Unknown status received from server")
            size = struct.unpack('!I', response[1:5])[0]

            # Receive payload
            response += self.recvall(size)
            payload = response[5:size+5]
            if len(payload) != size:
                raise ConnectionError("Unexpected response length")

            print(payload.decode("utf-8"))
            return False

        except ConnectionError as e:
            print(f"[!] Error receiving data: {e}")
            return True

    def do_login(self, line) -> bool:
        """Login with credentials"""

        self.opcode = 0x04
        self.length = len(line)
        self.payload = line.encode("utf-8")
        self.send_request()
        return self.recv_response()

    def do_ping(self, line) -> bool:
        """Health check"""

        self.opcode = 0x01
        self.length = 1
        self.payload = b'\x00'
        self.send_request()
        return self.recv_response()

    def do_echo(self, line) -> bool:
        """Echo message"""

        self.opcode = 0x02
        self.length = len(line)
        self.payload = line.encode("utf-8")
        self.send_request()
        return self.recv_response()

    def do_quit(self, line) -> bool:
        """Close connection"""

        self.opcode = 0x03
        self.length = 1
        self.payload = b'\x00'
        self.send_request()
        self.recv_response()
        return True

    def do_EOF(self, line) -> bool:
        """Handle EOF (Ctrl+D)"""

        print()
        return self.do_quit(line)

def main() -> int:
    chat_client = ChatClient()

    chat_client.parse_args()

    failed = chat_client.connect()

    if failed:
        return 1

    chat_client.cmdloop()
    chat_client.disconnect()
    return 0

if __name__ == "__main__":
    sys.exit(main())
