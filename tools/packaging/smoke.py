#!/usr/bin/env python3
"""Check a relocated package and SDK executable lookup without touching real devices."""
import ctypes as C
import json
import os
from pathlib import Path
import platform
import shutil
import socket
import subprocess
import sys
import tempfile

package = Path(sys.argv[1]).resolve()
windows = platform.system() == 'Windows'
with tempfile.TemporaryDirectory(prefix='xdb installed ') as temporary:
    temporary = Path(temporary)
    relocated = temporary / 'relocated package'
    shutil.copytree(package, relocated)
    executable = relocated / 'bin' / ('xdb.exe' if windows else 'xdb')
    library = relocated / ('bin/libxdb_client.dll' if windows else 'lib/libxdb_client.dylib' if platform.system() == 'Darwin' else 'lib/libxdb_client.so')
    os.environ.pop('XDB_EXECUTABLE', None)
    os.environ.update(HOME=str(temporary), XDG_DATA_HOME=str(temporary), LOCALAPPDATA=str(temporary))
    if windows:
        dll_directory = os.add_dll_directory(str(relocated / 'bin'))
    # Test both SDK sibling-bin lookup and a consumer shipping its own client on PATH.
    for from_path in (False, True):
        candidate = library
        if from_path:
            consumer = temporary / 'consumer'
            consumer.mkdir()
            candidate = consumer / library.name
            shutil.copy2(library, candidate)
            os.environ['PATH'] = str(relocated / 'bin') + os.pathsep + os.environ['PATH']
        client = C.CDLL(str(candidate))
        client.xdb_client_connect.argtypes = [C.c_uint16, C.c_void_p, C.c_void_p]
        client.xdb_client_connect.restype = C.c_void_p
        client.xdb_client_destroy.argtypes = [C.c_void_p]
        client.xdb_client_request.argtypes = [C.c_void_p, C.c_char_p, C.c_char_p, C.POINTER(C.c_void_p)]
        client.xdb_response_json.argtypes = [C.c_void_p]
        client.xdb_response_json.restype = C.c_char_p
        client.xdb_response_destroy.argtypes = [C.c_void_p]
        client.xdb_last_error.restype = C.c_char_p
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        handle = client.xdb_client_connect(port, None, None)
        assert handle, client.xdb_last_error()
        try:
            response = C.c_void_p()
            assert client.xdb_client_request(handle, b'server.info', b'{}', C.byref(response)), client.xdb_last_error()
            try:
                result = json.loads(client.xdb_response_json(response))
                assert result['ok'] and result['result']['protocol'] == 1, result
            finally:
                client.xdb_response_destroy(response)
        finally:
            client.xdb_client_destroy(handle)
            subprocess.run([str(executable), '--port', str(port), 'kill-server'], check=True, timeout=15)
    assert not list(relocated.rglob('CMakeCache.txt'))
    assert not list(relocated.rglob('*.a'))
print('Relocated CLI, SDK sibling-bin lookup and consumer PATH lookup passed')
