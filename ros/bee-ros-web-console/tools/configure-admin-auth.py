#!/usr/bin/env python3
import argparse
import base64
import getpass
import hashlib
import os
import secrets
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description='Create the protected Bee ROS Web Console authentication environment file.')
    parser.add_argument('--username', default='FYX')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()

    password = getpass.getpass('Administrator password: ')
    confirmation = getpass.getpass('Confirm administrator password: ')
    if password != confirmation:
        raise SystemExit('Passwords do not match.')
    if len(password) < 8:
        raise SystemExit('Password must contain at least 8 characters.')
    if not args.username or len(args.username) > 64 or '\n' in args.username or '\r' in args.username:
        raise SystemExit('Invalid username.')

    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), salt, 180000)
    password_hash = base64.b64encode(salt + digest).decode('ascii')
    session_secret = secrets.token_urlsafe(48)
    content = (
        f'BEE_ADMIN_USERNAME={args.username}\n'
        f'BEE_ADMIN_PASSWORD_HASH={password_hash}\n'
        f'BEE_SESSION_SECRET={session_secret}\n'
        'BEE_COOKIE_SECURE=0\n'
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix='.bee-auth-', dir=str(args.output.parent), text=True)
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8', newline='\n') as stream:
            stream.write(content)
        os.chmod(temporary_name, 0o600)
        os.replace(temporary_name, args.output)
    finally:
        if os.path.exists(temporary_name):
            os.unlink(temporary_name)
    print(f'Authentication environment written to {args.output} with mode 0600.')


if __name__ == '__main__':
    main()
