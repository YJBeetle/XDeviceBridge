# SPDX-License-Identifier: LGPL-3.0-only
"""Real background processes and SDK calls, isolated from user devices and credentials."""
import ctypes as C
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import threading

exe, library = (Path(value).resolve() for value in sys.argv[1:])
client = C.CDLL(str(library))
client.xdb_client_connect.argtypes = [C.c_uint16, C.c_void_p, C.c_void_p]
client.xdb_client_connect.restype = C.c_void_p
client.xdb_client_destroy.argtypes = [C.c_void_p]
client.xdb_client_request.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p, C.POINTER(C.c_void_p)]
client.xdb_response_json.argtypes = [C.c_void_p]
client.xdb_response_json.restype = C.c_char_p
client.xdb_response_destroy.argtypes = [C.c_void_p]
client.xdb_client_kill_server.argtypes = [C.c_uint16]
client.xdb_last_error.restype = C.c_char_p


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def listening(port):
    with socket.socket() as sock:
        sock.settimeout(1)
        return sock.connect_ex(('127.0.0.1', port)) == 0


def run(port, *args, success=True):
    result = subprocess.run([str(exe), '--port', str(port), *args],
                            capture_output=True, text=True, encoding='utf-8', timeout=12)
    assert (result.returncode == 0) == success, (args, result.returncode, result.stdout, result.stderr)
    return result


def request(handle, method):
    response = C.c_void_p()
    assert client.xdb_client_request(handle, method, b'{}', C.byref(response)), client.xdb_last_error()
    try:
        result = json.loads(client.xdb_response_json(response))
        assert result['ok'], result
        return result['result']
    finally:
        client.xdb_response_destroy(response)


def info(port):
    return json.loads(run(port, 'rpc', 'server.info').stdout)['result']


with tempfile.TemporaryDirectory(prefix='xdb-lifecycle-') as tmp:
    os.environ.update(HOME=tmp, LOCALAPPDATA=tmp, XDG_DATA_HOME=tmp, XDB_EXECUTABLE=str(exe))
    port = free_port()
    try:
        run(port, 'kill-server')
        assert not listening(port), 'kill-server launched a missing server'
        # Captured output must close when the client exits; the server must not inherit those pipes.
        run(port, 'devices', '--wifi', '--timeout', '0')
        pid = info(port)['pid']
        run(port, 'start-server', '--data-dir', str(Path(tmp) / 'private data'))
        assert info(port)['pid'] == pid, 'start-server replaced a running server'
        held = client.xdb_client_connect(port, None, None)
        assert held
        try:
            run(port, 'kill-server')
            assert not listening(port)
            response = C.c_void_p()
            assert not client.xdb_client_request(held, b'server.info', b'{}', C.byref(response))
        finally:
            client.xdb_client_destroy(held)
        run(port, 'kill-server')
        assert not listening(port), 'second kill-server launched a server'
        # Several first clients must share the one listener; losing startup races are harmless.
        processes = [subprocess.Popen([str(exe), '--port', str(port), 'start-server',
                                       '--data-dir', str(Path(tmp) / 'private data')],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE) for _ in range(4)]
        for process in processes:
            stdout, stderr = process.communicate(timeout=12)
            assert process.returncode == 0, (stdout, stderr)
        assert info(port)['pid'] != pid
        assert client.xdb_client_kill_server(port), client.xdb_last_error()
        assert not listening(port)
        # SDK callers get the same automatic startup, without invoking a CLI themselves.
        os.environ['XDB_EXECUTABLE'] = str(Path(tmp) / 'missing-xdb')
        assert not client.xdb_client_connect(port, None, None)
        assert b'Cannot find xdb executable' in client.xdb_last_error()
        assert not listening(port)
        os.environ['XDB_EXECUTABLE'] = str(exe)
        handle = client.xdb_client_connect(port, None, None)
        assert handle, client.xdb_last_error()
        try:
            pid = request(handle, b'server.info')['pid']
        finally:
            client.xdb_client_destroy(handle)
        assert info(port)['pid'] == pid, 'server died with the SDK client'
        assert client.xdb_client_kill_server(port), client.xdb_last_error()
        # The installed/build-tree layout also works without an executable override.
        del os.environ['XDB_EXECUTABLE']
        run(port, 'start-server')
        run(port, 'kill-server')
        # A non-xdb listener must neither be replaced nor trigger an automatic startup.
        with socket.socket() as other:
            other.bind(('127.0.0.1', 0))
            other.listen()
            def reject():
                connection, _ = other.accept()
                with connection:
                    connection.recv(4096)
                    connection.sendall(b'NOPE' + struct.pack('>III', 1, 2, 0))
            thread = threading.Thread(target=reject)
            thread.start()
            result = run(other.getsockname()[1], 'start-server', success=False)
            thread.join(timeout=3)
            assert not thread.is_alive()
            assert 'unsupported xdb wire version' in result.stderr, result.stderr
    finally:
        assert client.xdb_client_kill_server(port), client.xdb_last_error()
        assert not listening(port)
print('PASS: CLI/SDK autostart, detached lifetime, reuse, concurrent startup, kill, restart and occupied port')
