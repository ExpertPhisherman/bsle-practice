import cmd
import sys
import socket
import time

class Client(cmd.Cmd):
    """Basic TCP client"""

    def __init__(self) -> None:
        super().__init__()
        self.prompt = "echo> "
        self.sock = None

    def preloop(self) -> bool:
        """Executed once before the cmdloop starts."""
        if self.sock is None:
            return True

        # Wait to receive a potential shutdown
        time.sleep(0.01)

        self.sock.setblocking(False)
        try:
            data = self.sock.recv(1, socket.MSG_PEEK)
            if len(data) == 0:
                print("[!] max_clients reached")
                sys.exit(1)
        except BlockingIOError:
            # NOTE: Socket is open and empty
            self.sock.setblocking(True)

        return False

    def connect(self, rhost, rport, lhost=None, lport=None) -> bool:
        """Connect to echo server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        if lport is not None:
            if lhost is None:
                lhost = "0.0.0.0"
            try:
                # Use specified source port
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                self.sock.bind((lhost, lport))
            except OSError as e:
                print(f"[!] Error binding to client: {e}")
                return True

        try:
            self.sock.connect((rhost, rport))
            print(f"Connected to {rhost}:{rport}")
        except ConnectionRefusedError as e:
            print(f"[!] Error connecting to server: {e}")
            return True

        return False

    def recvall(self, size) -> bytes:
        """Receive all data from server"""
        packet = b''
        while len(packet) < size:
            data = self.sock.recv(size - len(packet))
            if not data:
                raise ConnectionError("Server disconnected")
            packet += data
        return packet

def main() -> int:
    return 0

if __name__ == "__main__":
    sys.exit(main())
