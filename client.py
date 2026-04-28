import argparse
import cmd
import sys
import socket
import time

class Client(cmd.Cmd):
    """Basic TCP client"""

    def __init__(self) -> None:
        super().__init__()

    def parse_args(self) -> None:
        """Parse command-line options"""

        parser = argparse.ArgumentParser(description="Echo client")
        parser.add_argument("rhost", nargs="?", default="127.0.0.1", help="Remote host")
        parser.add_argument("rport", type=int, help="Remote port")
        parser.add_argument("-l", "--lhost", default="0.0.0.0", help="Local host")
        parser.add_argument("-p", "--lport", type=int, default=0, help="Local port")

        args = parser.parse_args()
        self.rhost = args.rhost
        self.rport = args.rport
        self.lhost = args.lhost
        self.lport = args.lport

    def connect(self) -> bool:
        """Connect to server"""

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            self.sock.bind((self.lhost, self.lport))
            if (self.lhost == "0.0.0.0") or (self.lport == 0):
                self.lhost, self.lport = self.sock.getsockname()
            print(f"[*] Bound to client {self.lhost}:{self.lport}")
        except OSError as e:
            print(f"[!] Error binding to client: {e}")
            return True

        try:
            self.sock.connect((self.rhost, self.rport))
            print(f"[+] Connected to server {self.rhost}:{self.rport}")
        except ConnectionRefusedError as e:
            print(f"[!] Error connecting to server: {e}")
            return True

        return False

    def disconnect(self) -> None:
        """Disconnect from server"""

        if self.sock is not None:
            self.sock.close()
            self.sock = None
            print(f"[-] Disconnected from server {self.rhost}:{self.rport}")

    def recvall(self, size) -> bytes:
        """Receive all data from server"""

        packet = b''
        while len(packet) < size:
            data = self.sock.recv(size - len(packet))
            if not data:
                raise ConnectionError("Server disconnected")
            packet += data
        return packet

    def preloop(self) -> bool:
        """Executed once before the cmdloop starts."""

        # Wait to receive a potential shutdown
        time.sleep(0.01)

        self.sock.setblocking(False)
        try:
            data = self.sock.recv(1, socket.MSG_PEEK)
            if len(data) == 0:
                print("[!] Rejected by server")
                sys.exit(1)
        except BlockingIOError:
            # NOTE: Socket is open and empty
            self.sock.setblocking(True)

        return False

    def cmdloop(self, intro=None) -> None:
        """Handle Ctrl+C gracefully"""

        try:
            super().cmdloop(intro)
        except KeyboardInterrupt:
            print()
            self.do_quit("")
            self.disconnect()

def main() -> int:
    return 0

if __name__ == "__main__":
    sys.exit(main())
