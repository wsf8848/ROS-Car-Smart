#!/usr/bin/env python3
import argparse
import getpass
import http.client
import json
import os
from http.cookies import SimpleCookie
from pathlib import Path

from websockets.exceptions import ConnectionClosed
from websockets.sync.client import connect


def request(host, port, method, path, payload=None, cookie=None):
    headers = {'Origin': f'http://{host}:{port}'}
    body = None
    if payload is not None:
        body = json.dumps(payload, separators=(',', ':'))
        headers['Content-Type'] = 'application/json'
    if cookie:
        headers['Cookie'] = cookie
    connection = http.client.HTTPConnection(host, port, timeout=5)
    connection.request(method, path, body=body, headers=headers)
    response = connection.getresponse()
    raw = response.read()
    result = {
        'status': response.status,
        'location': response.getheader('Location'),
        'set_cookie': response.getheader('Set-Cookie'),
        'body': json.loads(raw.decode('utf-8')) if raw and response.getheader('Content-Type', '').startswith('application/json') else None,
    }
    connection.close()
    return result


def session_cookie(set_cookie):
    parsed = SimpleCookie()
    parsed.load(set_cookie)
    morsel = parsed.get('bee_session')
    if not morsel:
        raise AssertionError('session cookie missing')
    return f'bee_session={morsel.value}'


def expect(condition, label):
    if not condition:
        raise AssertionError(label)
    print(f'PASS {label}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--http-port', type=int, default=8080)
    parser.add_argument('--ws-port', type=int, default=8765)
    parser.add_argument('--username', default='FYX')
    parser.add_argument('--cookie-output', type=Path)
    parser.add_argument('--verify-cookie', type=Path)
    args = parser.parse_args()
    if args.verify_cookie:
        cookie = args.verify_cookie.read_text(encoding='ascii').strip()
        status = request(args.host, args.http_port, 'GET', '/api/auth/status', cookie=cookie)
        expect(status['status'] == 200 and status['body']['authenticated'], 'remembered session survives gateway restart')
        print('AUTH_PERSISTENCE_PASS')
        return
    password = getpass.getpass('Administrator password for test: ')

    result = request(args.host, args.http_port, 'GET', '/')
    expect(result['status'] == 302 and result['location'] == '/login.html', 'unauthenticated root redirects')
    expect(request(args.host, args.http_port, 'GET', '/login.html')['status'] == 200, 'login page is public')
    expect(request(args.host, args.http_port, 'GET', '/login.css')['status'] == 200, 'login stylesheet is public')
    result = request(args.host, args.http_port, 'GET', '/api/health')
    expect(result['status'] == 401 and result['body']['error'] == 'authentication_required', 'business API rejects unauthenticated access')
    result = request(args.host, args.http_port, 'GET', '/api/auth/status')
    expect(result['status'] == 200 and not result['body']['authenticated'], 'anonymous auth status')

    bad = request(args.host, args.http_port, 'POST', '/api/auth/login', {
        'username': args.username, 'password': 'definitely-wrong-password', 'remember': False,
    })
    expect(bad['status'] == 401 and bad['body']['error'] == 'invalid_credentials', 'wrong password is rejected')

    login = request(args.host, args.http_port, 'POST', '/api/auth/login', {
        'username': args.username, 'password': password, 'remember': False,
    })
    expect(login['status'] == 200, 'session login succeeds')
    expect('HttpOnly' in login['set_cookie'] and 'SameSite=Strict' in login['set_cookie'], 'session cookie security attributes')
    expect('Max-Age=' not in login['set_cookie'], 'session login has no persistent max age')
    cookie = session_cookie(login['set_cookie'])
    status = request(args.host, args.http_port, 'GET', '/api/auth/status', cookie=cookie)
    expect(status['status'] == 200 and status['body']['authenticated'], 'authenticated status succeeds')
    expect(request(args.host, args.http_port, 'GET', '/api/health', cookie=cookie)['status'] == 200, 'authenticated business API succeeds')

    unauth_ws_denied = False
    try:
        with connect(f'ws://{args.host}:{args.ws_port}', open_timeout=5) as websocket:
            websocket.recv(timeout=2)
    except ConnectionClosed as error:
        unauth_ws_denied = error.code == 1008
    expect(unauth_ws_denied, 'WebSocket rejects unauthenticated client')
    with connect(
        f'ws://{args.host}:{args.ws_port}',
        additional_headers={'Cookie': cookie},
        open_timeout=5,
    ):
        pass
    expect(True, 'WebSocket accepts authenticated client')

    logout = request(args.host, args.http_port, 'POST', '/api/auth/logout', {}, cookie=cookie)
    expect(logout['status'] == 200 and 'Max-Age=0' in logout['set_cookie'], 'logout expires cookie')
    expect(request(args.host, args.http_port, 'GET', '/api/health')['status'] == 401, 'API remains protected after logout')

    remembered = request(args.host, args.http_port, 'POST', '/api/auth/login', {
        'username': args.username, 'password': password, 'remember': True,
    })
    expect(remembered['status'] == 200 and 'Max-Age=2592000' in remembered['set_cookie'], 'remember login persists for 30 days')
    if args.cookie_output:
        cookie_value = session_cookie(remembered['set_cookie'])
        descriptor = os.open(args.cookie_output, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
        with os.fdopen(descriptor, 'w', encoding='ascii') as stream:
            stream.write(cookie_value)
    print('AUTH_FLOW_PASS')


if __name__ == '__main__':
    main()
