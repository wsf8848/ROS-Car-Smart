#!/usr/bin/env python3
import asyncio
import json

import websockets


async def audit():
    messages = []
    async with websockets.connect('ws://127.0.0.1:8765') as websocket:
        deadline = asyncio.get_running_loop().time() + 2.0
        while asyncio.get_running_loop().time() < deadline:
            try:
                message = json.loads(await asyncio.wait_for(websocket.recv(), 0.5))
            except asyncio.TimeoutError:
                continue
            messages.append((message.get('type'), message.get('layer')))
    print(json.dumps(messages[:20]))
    if not any(message_type == 'scan' for message_type, _ in messages):
        raise SystemExit('real_scan_missing')
    if any(message_type == 'sim_layer' for message_type, _ in messages):
        raise SystemExit('stale_sim_layer_present')


if __name__ == '__main__':
    asyncio.run(audit())
