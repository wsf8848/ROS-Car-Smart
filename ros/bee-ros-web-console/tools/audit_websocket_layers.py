#!/usr/bin/env python3
import asyncio
import json

import websockets


async def audit():
    layers = {}
    scan = False
    map_seen = False
    async with websockets.connect('ws://127.0.0.1:8765') as websocket:
        deadline = asyncio.get_running_loop().time() + 5.0
        while asyncio.get_running_loop().time() < deadline:
            message = json.loads(await asyncio.wait_for(websocket.recv(), 1.0))
            if message.get('type') == 'sim_layer':
                layers[message['layer']] = message.get('is_test') is True
            elif message.get('type') == 'scan':
                scan = True
            elif message.get('type') == 'map':
                map_seen = True
            if len(layers) == 8 and scan:
                break
    result = {'layers': layers, 'scan': scan, 'map': map_seen}
    print(json.dumps(result, sort_keys=True))
    if len(layers) != 8 or not all(layers.values()) or not scan:
        raise SystemExit(1)


if __name__ == '__main__':
    asyncio.run(audit())
