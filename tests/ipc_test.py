# SPDX-License-Identifier: LGPL-3.0-only
import ctypes as C
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

exe, client_lib, server_lib = map(Path, sys.argv[1:])
client = C.CDLL(str(client_lib.resolve()))
server = C.CDLL(str(server_lib.resolve()))
server.xdb_server_start.argtypes = [C.c_uint16,C.c_char_p,C.c_char_p]
server.xdb_server_start.restype = C.c_void_p
server.xdb_server_port.argtypes = [C.c_void_p]
server.xdb_server_port.restype = C.c_uint16
server.xdb_server_stop.argtypes = [C.c_void_p]
server.xdb_server_last_error.restype = C.c_char_p
client.xdb_client_connect.argtypes = [C.c_uint16,C.c_char_p,C.c_void_p,C.c_void_p]
client.xdb_client_connect.restype = C.c_void_p
client.xdb_client_request.argtypes = [C.c_void_p,C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)]
client.xdb_client_destroy.argtypes = [C.c_void_p]
client.xdb_response_json.argtypes = [C.c_void_p]
client.xdb_response_json.restype = C.c_char_p
client.xdb_response_destroy.argtypes = [C.c_void_p]
client.xdb_last_error.restype = C.c_char_p
TOKEN=b'0123456789abcdef-test'

def packet(value, binary=b''):
    text=json.dumps(value).encode()
    return b'XDB1'+struct.pack('>III',1,len(text),len(binary))+text+binary

def read_exact(s,n):
    out=b''
    while len(out)<n:
        part=s.recv(n-len(out))
        if not part:
            raise EOFError()
        out+=part
    return out

def receive(s):
    header=read_exact(s,16)
    assert header[:4]==b'XDB1'
    version,n,b=struct.unpack('>III',header[4:])
    assert version==1
    value=json.loads(read_exact(s,n))
    data=read_exact(s,b)
    return value,data

def hello(s):
    data=packet({'id':1,'method':'hello','params':{'protocol':1,'token':TOKEN.decode()}})
    # TCP packet boundaries are not message boundaries.
    for start in range(0,len(data),3):
        s.sendall(data[start:start+3])
    value,_=receive(s)
    assert value['ok'], value

def request(c,method,params=None):
    out=C.c_void_p()
    assert client.xdb_client_request(c,method.encode(),json.dumps(params or {}).encode(),C.byref(out)), client.xdb_last_error()
    try:
        return json.loads(client.xdb_response_json(out))
    finally:
        client.xdb_response_destroy(out)

with tempfile.TemporaryDirectory(prefix='xdb-ipc-') as tmp:
    data=str(Path(tmp)/'private-data').encode()
    assert not server.xdb_server_start(0,TOKEN,b'relative'), 'relative data path accepted'
    handle=server.xdb_server_start(0,TOKEN,data)
    assert handle, server.xdb_server_last_error()
    port=server.xdb_server_port(handle)
    assert port
    try:
        assert not client.xdb_client_connect(port,b'wrong-token',None,None)
        c=client.xdb_client_connect(port,TOKEN,None,None)
        assert c, client.xdb_last_error()
        try:
            assert request(c,'server.info')['result']['protocol']==1
            assert request(c,'devices',{'transport':'wifi','timeout_ms':0})['ok']
            assert not request(c,'pairing.import',{'record':'invalid'})['ok']
            assert not Path(data.decode()).exists(), 'invalid import wrote persistent state'
            assert not request(c,'connection.open',{'udid':''})['ok']
            assert request(c,'connection.close')['ok']
            assert not request(c,'screenshot')['ok']
            assert request(c,'server.info')['ok'], 'failed operation poisoned connection'
        finally:
            client.xdb_client_destroy(c)
        with socket.create_connection(('127.0.0.1',port),timeout=2) as s:
            hello(s)
            s.sendall(packet({'id':2,'method':'server.info','params':{}}))
            value,binary=receive(s)
            assert value['id']==2 and value['ok'] and binary==b''
        for bad_header in [b'NOPE'+struct.pack('>III',1,2,0), b'XDB1'+struct.pack('>III',2,2,0), b'XDB1'+struct.pack('>III',1,1024*1024+1,0)]:
            with socket.create_connection(('127.0.0.1',port),timeout=2) as s:
                s.sendall(bad_header)
                assert s.recv(1)==b'', 'invalid wire header did not close connection'
        result=subprocess.run([str(exe.resolve()),'--port',str(port),'--token',TOKEN.decode(),'rpc','server.info'],text=True,capture_output=True,timeout=5)
        assert result.returncode==0, result.stderr
        assert json.loads(result.stdout)['ok']
        result=subprocess.run([str(exe.resolve()),'--port',str(port),'--token',TOKEN.decode(),'devices','--wifi','--timeout','0'],text=True,capture_output=True,timeout=5)
        assert result.returncode==0 and result.stdout.startswith('List of iOS devices attached'), result.stderr
        # Stop must release idle clients blocked in recv, without requiring their cooperation.
        idle=socket.create_connection(('127.0.0.1',port),timeout=2)
        hello(idle)
    finally:
        before=time.monotonic()
        server.xdb_server_stop(handle)
        assert time.monotonic()-before<3, 'idle-client shutdown blocked'
    assert idle.recv(1)==b''
    idle.close()
print('PASS: embedded lifecycle, C client, CLI, framing, authentication, invalid requests, no-side-effect failures and shutdown')
