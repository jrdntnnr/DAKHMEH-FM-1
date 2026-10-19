#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run an argv command through an existing Lima SSH ControlMaster.

Fallback for macOS sessions where getpwuid() is unavailable. Uses the
OpenSSH passenger protocol, not a second SSH implementation. No keys are
read and the existing master owns authentication and the connection.
Protocol: https://github.com/openssh/openssh-portable/blob/master/PROTOCOL.mux
"""
import argparse
import array
import os
import shlex
import socket
import struct
import sys


def u32(n):
    return struct.pack('!I', n)


def string(s):
    b = s.encode('utf-8')
    return u32(len(b)) + b


def receive(sock, size):
    data = bytearray()
    while len(data) < size:
        block = sock.recv(size - len(data))
        if not block:
            raise EOFError('SSH control connection closed without a complete reply')
        data.extend(block)
    return bytes(data)


def packet(sock):
    size, = struct.unpack('!I', receive(sock, 4))
    if size < 4 or size > 1024 * 1024:
        raise ValueError('Invalid SSH control packet size')
    return receive(sock, size)


def send(sock, data):
    sock.sendall(u32(len(data)) + data)


def run(path, argv):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.settimeout(15)
        sock.connect(path)
        send(sock, u32(1) + u32(4))
        hello = packet(sock)
        if len(hello) < 8 or struct.unpack('!II', hello[:8]) != (1, 4):
            raise ValueError('Unsupported SSH multiplexing protocol')
        # OpenSSH encodes these four flags as uint32, despite the protocol
        # document calling them bool. No PTY, X11, agent forwarding or subsystem.
        send(sock, u32(0x10000002) + u32(1) + string('') + u32(0) * 4 +
             u32(0xffffffff) + string('') + string(shlex.join(argv)))
        for fd in (0, 1, 2):
            sock.sendmsg([b'\0'], [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array('i', [fd]))])
        response = packet(sock)
        if len(response) < 8:
            raise ValueError('Truncated SSH session response')
        kind, request = struct.unpack('!II', response[:8])
        if request != 1:
            raise ValueError('Unexpected SSH request ID')
        if kind in (0x80000002, 0x80000003):
            raise RuntimeError(response[12:].decode('utf-8', errors='replace'))
        if kind != 0x80000006 or len(response) != 12:
            raise ValueError('SSH master did not open a session')
        session, = struct.unpack('!I', response[8:])
        sock.settimeout(None)
        exit_code = None
        while True:
            try:
                response = packet(sock)
            except EOFError:
                if exit_code is not None:
                    return exit_code
                raise
            kind, = struct.unpack('!I', response[:4])
            if kind == 0x80000004:
                if len(response) != 12:
                    raise ValueError('Truncated SSH exit response')
                sid, exit_code = struct.unpack('!II', response[4:])
                if sid != session:
                    raise ValueError('Unexpected SSH session ID')
            elif kind != 0x80000001:
                raise ValueError(f'Unexpected SSH control response: {kind:#x}')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--socket', required=True, help='Existing Lima instance ssh.sock')
    ap.add_argument('command', nargs=argparse.REMAINDER)
    args = ap.parse_args()
    argv = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not argv:
        ap.error('a command is required')
    try:
        return run(os.path.expanduser(args.socket), argv)
    except (OSError, EOFError, ValueError, RuntimeError) as e:
        print(f'lima_shell: {e}. The Lima VM and its SSH master must already be running.', file=sys.stderr)
        return 255


if __name__ == '__main__':
    sys.exit(main())
