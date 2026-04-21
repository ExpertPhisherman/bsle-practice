import cmd
import sys
import socket
import struct
import time

class EchoClient(cmd.Cmd):
    """Echo client"""

    def preloop(self):
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
            # Socket is open and empty
            self.sock.setblocking(True)

        return False

    def __init__(self):
        super().__init__()
        self.prompt = "echo> "
        self.sock = None
        self.init_vars()

    def init_vars(self):
        if self.sock is not None:
            self.sock.close()
            self.sock = None
        self.opcode = None
        self.length = None
        self.payload = None

    def connect(self, rhost, rport, lhost=None, lport=None):
        """Connect to echo server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        if lport is not None:
            if lhost is None:
                lhost = "0.0.0.0"
            try:
                # Use specified source port
                self.sock.bind((lhost, lport))
            except OSError as e:
                print(f"Error binding to client: {e}")
                self.init_vars()
                return True

        try:
            self.sock.connect((rhost, rport))
            print(f"Connected to {rhost}:{rport}")
        except ConnectionRefusedError as e:
            print(f"Error connecting to server: {e}")
            self.init_vars()
            return True

        return False

    def send(self):
        """Send request to server"""
        if self.sock is None:
            return True
        try:
            request = struct.pack("!BI", self.opcode, self.length)
            request += self.payload
            self.sock.sendall(request)
            return False
        except ConnectionError as e:
            print(f"Error sending data: {e}")
            self.init_vars()
            return True

    def recvall(self, size):
        """Receive all data from server"""
        packet = b''
        while len(packet) < size:
            data = self.sock.recv(size - len(packet))
            if not data:
                raise ConnectionError("Socket closed")
            packet += data
        return packet

    def recv(self):
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
            response += self.recvall(size)
            payload = response[5:size+5]
            if len(payload) != size:
                raise ConnectionError("Unexpected response length")
            print(payload.decode('utf-8'))
            return False

        except ConnectionError as e:
            print(f"Error receiving data: {e}")
            self.init_vars()
            return True

    def do_ping(self, line):
        """Health check"""
        self.opcode = 0x01
        self.length = 1
        self.payload = b'\x00'
        self.send()
        return self.recv()

    def do_echo(self, line):
        """Echo message"""
        self.opcode = 0x02
        self.length = len(line)
        self.payload = line.encode('utf-8')
        self.send()
        return self.recv()

    def do_quit(self, line):
        """Close connection"""
        self.opcode = 0x03
        self.length = 1
        self.payload = b'\x00'
        self.send()
        self.recv()
        self.init_vars()
        return True

    def do_EOF(self, line):
        """Handle EOF (Ctrl+D)"""
        print()
        return self.do_quit(line)

def main():
    client = EchoClient()

    # TODO: Read command line options and arguments
    if len(sys.argv) > 3:
        failed = client.connect(sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4]))
    else:
        failed = client.connect(sys.argv[1], int(sys.argv[2]))

    if failed:
        return 1

    client.cmdloop()
    return 0

if __name__ == "__main__":
    sys.exit(main())