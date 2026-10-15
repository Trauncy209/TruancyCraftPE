#!/usr/bin/env python3
"""Run real native UDP + PNG/status checks against a completed CMake Makefiles build."""
from pathlib import Path
import argparse, os, shlex, socket, struct, subprocess, tempfile, zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = args.build.resolve()
flags_file = build / 'CMakeFiles/MinecraftPE.dir/flags.make'
if not flags_file.is_file():
    parser.error('Use a completed Unix Makefiles CMake build for this runner.')
flags = dict(line.split(' = ', 1) for line in flags_file.read_text().splitlines() if ' = ' in line)
with tempfile.TemporaryDirectory(prefix='tcpe-native-network-') as directory:
    temp = Path(directory)
    obj, exe = temp / 'network.o', temp / 'network-test'
    subprocess.run(['clang++', *shlex.split(flags['CXX_DEFINES']),
                    *shlex.split(flags['CXX_INCLUDES']), '-std=c++14', '-O0', '-UNDEBUG',
                    '-Wno-c++11-narrowing', '-c', str(root / 'tests/native_tcpe_network.cpp'),
                    '-o', str(obj)], check=True)
    command = shlex.split((build / 'CMakeFiles/MinecraftPE.dir/link.txt').read_text())
    command.remove('CMakeFiles/MinecraftPE.dir/src/main.cpp.o')
    command.insert(1, str(obj))
    command[command.index('-o') + 1] = str(exe)
    for i in range(len(command) - 2, -1, -1):
        if command[i] == '-Xlinker' and command[i + 1].startswith('--dependency-file='):
            del command[i:i + 2]
    subprocess.run(command, cwd=build, check=True)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 64, 8, 6, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress((b'\0' + bytes([17, 34, 51, 255]) * 64) * 64)) + chunk(b'IEND', b''))
    (temp / 'server-icon.png').write_bytes(png)
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    subprocess.run([str(exe), str(port)], cwd=temp, env=os.environ.copy(), check=True, timeout=60)
    limits = temp / 'status-limits'
    subprocess.run(['clang++', '-std=c++14', '-pthread', '-I', str(root / 'src'),
                    str(root / 'tests/native_server_status_limits.cpp'), '-o', str(limits)], check=True)
    subprocess.run([str(limits)], cwd=temp, check=True, timeout=10)
