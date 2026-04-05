import cmd
import sys
import socket
import struct

class EchoClient(cmd.Cmd):
    """Echo client"""

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
        self.request = None
        self.response = None

    def connect(self, host, port):
        """Connect to echo server"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((host, port))
        except ConnectionRefusedError as e:
            print(f"Error connecting to server: {e}")
            self.init_vars()
            exit(1)
        print(f"Connected to {host}:{port}")

    def send(self):
        """Send request to server"""
        if self.sock is None:
            return
        try:
            self.request = struct.pack("!BI", self.opcode, self.length)
            self.request += self.payload
            #print(f"Request bytes: {self.request}")
            self.sock.sendall(self.request)
        except ConnectionError as e:
            print(f"Error sending data: {e}")
            self.init_vars()
            exit(1)

    def recvall(self, size):
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
            return
        try:
            # TODO: Receive opcode and payload size in one recvall
            self.response = self.recvall(1)
            status = self.response[0]
            if status == 0x00 or status == 0x01:
                self.response += self.recvall(4)
                size = struct.unpack('!I', self.response[1:5])[0]
                self.response += self.recvall(size)
                payload = self.response[5:size+5]
                if len(payload) != size:
                    raise ConnectionError("Incomplete payload received")
                print(payload.decode('utf-8'))
            else:
                raise ConnectionError("Unknown status received from server")

        except ConnectionError as e:
            print(f"Error receiving data: {e}")
            self.init_vars()
            exit(1)

    def do_ping(self, line):
        """Health check"""
        self.opcode = 0x01
        self.length = 1
        self.payload = b'\x00'
        self.send()
        self.recv()

    def do_echo(self, line):
        """Echo message"""
        self.opcode = 0x02
        self.length = len(line)
        self.payload = line.encode('utf-8')
        self.send()
        self.recv()

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
    client.connect(sys.argv[1], int(sys.argv[2]))
    client.cmdloop()

    return 0

if __name__ == "__main__":
    exit(main())