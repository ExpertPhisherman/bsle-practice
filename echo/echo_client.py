import argparse
import sys
import struct
from pathlib import Path
sys.path.append(str(Path(__file__).absolute().parent.parent))
from client import Client

class EchoClient(Client):
    """Echo client"""

    def __init__(self) -> None:
        super().__init__()

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

            print(payload.decode('utf-8'))
            return False

        except ConnectionError as e:
            print(f"[!] Error receiving data: {e}")
            return True

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
        self.payload = line.encode('utf-8')
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
    parser = argparse.ArgumentParser(description="Echo client")

    parser.add_argument("rhost", help="Remote host")
    parser.add_argument("rport", type=int, help="Remote port")
    parser.add_argument("-l", "--lhost", help="Local host")
    parser.add_argument("-p", "--lport", type=int, help="Local port")

    args = parser.parse_args()

    client = EchoClient()

    failed = client.connect(args.rhost, args.rport, args.lhost, args.lport)

    if failed:
        return 1

    client.cmdloop()
    client.sock.close()
    return 0

if __name__ == "__main__":
    sys.exit(main())
