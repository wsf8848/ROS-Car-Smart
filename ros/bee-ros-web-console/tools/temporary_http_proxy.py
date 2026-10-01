#!/usr/bin/env python3
"""Development-only loopback HTTP CONNECT proxy for offline board installs."""
import select
import socket
import socketserver
from urllib.parse import urlsplit


class ProxyHandler(socketserver.StreamRequestHandler):
    timeout = 20

    def handle(self):
        request_line = self.rfile.readline(65536)
        if not request_line:
            return
        try:
            method, target, version = request_line.decode('iso-8859-1').strip().split(' ', 2)
        except ValueError:
            return
        headers = []
        while True:
            line = self.rfile.readline(65536)
            if not line or line in (b'\r\n', b'\n'):
                break
            headers.append(line)

        if method.upper() == 'CONNECT':
            host, _, port_text = target.rpartition(':')
            port = int(port_text or '443')
            upstream = socket.create_connection((host, port), timeout=self.timeout)
            self.wfile.write(f'{version} 200 Connection Established\r\n\r\n'.encode('ascii'))
            self.wfile.flush()
            self.relay(upstream)
            return

        parsed = urlsplit(target)
        host = parsed.hostname
        if not host:
            return
        port = parsed.port or (443 if parsed.scheme == 'https' else 80)
        path = parsed.path or '/'
        if parsed.query:
            path += '?' + parsed.query
        upstream = socket.create_connection((host, port), timeout=self.timeout)
        upstream.sendall(f'{method} {path} {version}\r\n'.encode('iso-8859-1'))
        for line in headers:
            if line.lower().startswith((b'proxy-connection:', b'connection:')):
                continue
            upstream.sendall(line)
        upstream.sendall(b'Connection: close\r\n\r\n')
        self.relay(upstream)

    def relay(self, upstream):
        sockets = [self.connection, upstream]
        try:
            while True:
                readable, _, exceptional = select.select(sockets, [], sockets, 30)
                if exceptional or not readable:
                    break
                for source in readable:
                    data = source.recv(65536)
                    if not data:
                        return
                    destination = upstream if source is self.connection else self.connection
                    destination.sendall(data)
        finally:
            upstream.close()


class ThreadedProxy(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == '__main__':
    with ThreadedProxy(('127.0.0.1', 31289), ProxyHandler) as server:
        server.serve_forever()
