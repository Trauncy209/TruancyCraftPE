#!/usr/bin/env python3
"""Exercise a real native TCPE dedicated world with two protocol clients and a restart."""
from pathlib import Path
import argparse, tempfile, subprocess, socket, time, signal, json, struct, shutil, os, shlex, zlib
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,required=True,help='Completed Unix Makefiles dedicated CMake build')
args=parser.parse_args();build=args.build.resolve();root=Path(__file__).resolve().parents[1]
exe=build/'tcpe-server';flags=build/'CMakeFiles/tcpe-server.dir/flags.make'
if not exe.is_file() or not flags.is_file():parser.error('Build tcpe-server with the Unix Makefiles generator first.')
evidence=build/'dedicated-test-logs';evidence.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tcpe-dedicated-client-') as harness:
    v=dict(line.split(' = ',1) for line in flags.read_text().splitlines() if ' = ' in line)
    obj=Path(harness,'client.o');client=Path(harness,'client')
    subprocess.run(['clang++',*shlex.split(v['CXX_DEFINES']),*shlex.split(v['CXX_INCLUDES']),'-std=c++14','-O0','-UNDEBUG','-Wno-c++11-narrowing','-c',str(root/'tests/native_dedicated_client.cpp'),'-o',str(obj)],check=True)
    command=shlex.split((build/'CMakeFiles/tcpe-server.dir/link.txt').read_text());command.remove('CMakeFiles/tcpe-server.dir/src/main_dedicated.cpp.o');command.insert(1,str(obj));command[command.index('-o')+1]=str(client)
    for i in range(len(command)-2,-1,-1):
     if command[i]=='-Xlinker' and command[i+1].startswith('--dependency-file='):del command[i:i+2]
    subprocess.run(command,cwd=build,check=True)
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',64,64,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+bytes([17,34,51,255])*64)*64))+chunk(b'IEND',b'')
    Path(harness,'server-icon.png').write_bytes(png)
    def varint(n):
     out=b''
     while True:
      b=n&127;n>>=7;out+=bytes([b|(128 if n else 0)])
      if not n:return out
    def frame(data):return varint(len(data))+data
    def exact(s,n):
     out=b''
     while len(out)<n:
      part=s.recv(n-len(out));assert part;out+=part
     return out
    def readvar(s):
     n=0
     for i in range(5):
      b=exact(s,1)[0];n|=(b&127)<<(7*i)
      if not b&128:return n
     raise AssertionError('bad VarInt')
    def status(port):
     with socket.create_connection(('127.0.0.1',port),timeout=3) as s:
      host=b'127.0.0.1';s.sendall(frame(b'\0'+varint(774)+varint(len(host))+host+struct.pack('>H',port)+b'\1')+frame(b'\0'));length=readvar(s);assert exact(s,1)==b'\0';size=readvar(s);return json.loads(exact(s,size))
    def wait_text(path,text,p,timeout=30):
     end=time.monotonic()+timeout
     while text not in path.read_text() and p.poll() is None and time.monotonic()<end:time.sleep(.05)
     assert text in path.read_text(),path.read_text()[-2500:]
    with tempfile.TemporaryDirectory(prefix='tcpe-dedicated-life-') as tmp:
     shutil.copy2(Path(harness,'server-icon.png'),Path(tmp,'server-icon.png'))
     for phase in ['modify','verify']:
      with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
      serverlog=evidence/('server-'+phase+'.log');clientlog=evidence/('clients-'+phase+'.log')
      with serverlog.open('w') as out,clientlog.open('w') as cout:
       args=[str(exe),'--port',str(port),'--directory',tmp,'--world','fixture','--seed','123456' if phase=='modify' else '999','--max-players','2','--name','Native fixture','--autosave','10']
       if phase=='verify':args.append('--creative')
       p=subprocess.Popen(args,cwd=tmp,stdin=subprocess.PIPE,stdout=out,stderr=subprocess.STDOUT,text=True);c=None
       try:
        wait_text(serverlog,'READY:',p);initial=status(port);assert initial['players']=={'online':0,'max':2};assert initial['description']['text']=='Native fixture';assert initial['favicon'].startswith('data:image/png;base64,')
        rejected=subprocess.run([str(exe),'--port',str(port+1 if port<65535 else port-1),'--directory',tmp,'--world','fixture'],cwd=tmp,capture_output=True,text=True,timeout=5);assert rejected.returncode!=0 and 'already in use' in rejected.stderr
        c=subprocess.Popen([str(client),str(port),phase],cwd=tmp,stdout=cout,stderr=subprocess.STDOUT);wait_text(clientlog,'CLIENTS_READY',c,15);active=status(port);assert active['players']=={'online':2,'max':2};assert c.wait(timeout=10)==0,clientlog.read_text()
        if phase=='modify':wait_text(serverlog,'World saved',p,15)
        p.stdin.write('save\nlist\n');p.stdin.flush();wait_text(serverlog,'World saved',p);p.send_signal(signal.SIGTERM);assert p.wait(timeout=15)==0,serverlog.read_text()[-2000:];assert 'Server stopped cleanly' in serverlog.read_text()
        assert any(x.name=='level.dat' and x.stat().st_size for x in Path(tmp,'fixture').rglob('*'))
        print(phase+': real dedicated server: discovery, 2 logins/spawns, visibility, movement, chat, chunk/block exchange, 0/2→2/2 PNG listing, capacity/duplicate-world rejection and SIGTERM save PASS',flush=True)
       finally:
        if c and c.poll() is None:c.terminate();c.wait(timeout=10)
        if p.poll() is None:p.send_signal(signal.SIGTERM);p.wait(timeout=15)
     print('Persisted world seed and changed block recovered after full process restart PASS',flush=True)
     for args in [['--port','0'],['--port','65536'],['--world','../bad'],['--max-players','0'],['--unknown','value']]:assert subprocess.run([str(exe),*args],cwd=tmp,capture_output=True).returncode!=0
     print('Invalid settings rejected before hosting PASS',flush=True)
