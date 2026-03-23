import socket
import struct
import sys
import time

HOST = "127.0.0.1"
PORT = 8080

def encode_msg(msg: bytes) -> bytes:
    return struct.pack("!I", len(msg)) + msg

def recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("socket closed while receiving")
        buf += chunk
    return buf

def recv_packet(sock: socket.socket) -> bytes:
    header = recv_exact(sock, 4)
    body_len = struct.unpack("!I", header)[0]
    if body_len > 1024 * 1024:
        raise ValueError(f"body too large: {body_len}")
    return recv_exact(sock, body_len)

def test_normal():
    print("== test_normal ==")
    sock = socket.create_connection((HOST, PORT))
    sock.sendall(encode_msg(b"hello"))
    resp = recv_packet(sock)
    print("resp:", resp.decode(errors="replace"))
    sock.close()

def test_shutdown_write():
    print("== test_shutdown_write ==")
    sock = socket.create_connection((HOST, PORT))
    sock.sendall(encode_msg(b"hello_after_shutdown"))
    sock.shutdown(socket.SHUT_WR)   # 半关闭：不再发数据
    resp = recv_packet(sock)
    print("resp:", resp.decode(errors="replace"))

    # 再读一次，期待对端最终关闭
    more = sock.recv(1024)
    print("final recv bytes:", len(more))
    sock.close()

def test_invalid_packet():
    print("== test_invalid_packet ==")
    sock = socket.create_connection((HOST, PORT))

    # 发一个超大长度头，但不发body
    sock.sendall(struct.pack("!I", 0x7fffffff))

    time.sleep(0.5)
    try:
        data = sock.recv(1024)
        print("recv after invalid packet:", data)
    except Exception as e:
        print("recv exception:", e)

    sock.close()

def test_multi_packets():
    print("== test_multi_packets ==")
    sock = socket.create_connection((HOST, PORT))
    payload = (
        encode_msg(b"one") +
        encode_msg(b"two") +
        encode_msg(b"three")
    )
    sock.sendall(payload)

    for i in range(3):
        resp = recv_packet(sock)
        print(f"resp {i}:", resp.decode(errors="replace"))

    sock.close()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python3 test_client.py [normal|shutdown|invalid|multi]")
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "normal":
        test_normal()
    elif cmd == "shutdown":
        test_shutdown_write()
    elif cmd == "invalid":
        test_invalid_packet()
    elif cmd == "multi":
        test_multi_packets()
    else:
        print("unknown test case")