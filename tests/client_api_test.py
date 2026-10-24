# SPDX-License-Identifier: LGPL-3.0-only
"""Exercise the standalone client SDK against compatible and incompatible peers."""
import ctypes as C
import json
from pathlib import Path
import socket
import struct
import sys
import threading

client = C.CDLL(str(Path(sys.argv[1]).resolve()))
client.xdb_client_version.restype = C.c_char_p
client.xdb_client_connect.argtypes = [C.c_uint16, C.c_void_p, C.c_void_p]
client.xdb_client_connect.restype = C.c_void_p
client.xdb_client_server_version.argtypes = [C.c_void_p]
client.xdb_client_server_version.restype = C.c_char_p
client.xdb_client_destroy.argtypes = [C.c_void_p]
client.xdb_last_error.restype = C.c_char_p
assert client.xdb_client_version().decode() == sys.argv[2]
assert client.xdb_client_server_version(None) is None

def exact(sock, size):
    data = b''
    while len(data) < size:
        part = sock.recv(size - len(data))
        if not part:
            raise EOFError('client disconnected during hello')
        data += part
    return data

for advertised in ({'protocol': 1, 'version': '99.2.3'}, {'protocol': 1}, {'protocol': 2}):
    with socket.socket() as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen()
        listener.settimeout(5)
        errors = []
        def serve():
            try:
                with listener.accept()[0] as peer:
                    peer.settimeout(5)
                    header = exact(peer, 16)
                    protocol, length, binary = struct.unpack('>III', header[4:])
                    assert header[:4] == b'XDB1' and protocol == 1 and not binary
                    request = json.loads(exact(peer, length))
                    assert request['method'] == 'hello' and request['params']['protocol'] == 1
                    body = json.dumps({'id': request['id'], 'ok': True, 'result': advertised}).encode()
                    peer.sendall(b'XDB1' + struct.pack('>III', 1, len(body), 0) + body)
                    assert peer.recv(1) == b'', 'client did not close its socket'
            except BaseException as error:
                errors.append(error)
        worker = threading.Thread(target=serve)
        worker.start()
        handle = None
        try:
            handle = client.xdb_client_connect(listener.getsockname()[1], None, None)
            if advertised['protocol'] == 1:
                assert handle, client.xdb_last_error()
                assert client.xdb_client_server_version(handle).decode() == advertised.get('version', '')
            else:
                assert not handle and b'incompatible protocol' in client.xdb_last_error()
        finally:
            if handle:
                client.xdb_client_destroy(handle)
            worker.join(6)
        assert not worker.is_alive() and not errors, errors
print('Standalone SDK version, optional hello fields and protocol rejection passed')
