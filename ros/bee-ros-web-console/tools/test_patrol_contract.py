#!/usr/bin/env python3
"""在已安装 ROS2/rclpy 的板端环境验证巡逻契约和 fail-closed 安全门。"""

import importlib.util
import json
import math
import os
import tempfile
from pathlib import Path


def expect_value_error(callback, expected):
    try:
        callback()
    except ValueError as error:
        assert str(error) == expected, (str(error), expected)
    else:
        raise AssertionError(f'expected ValueError: {expected}')


gateway_path = Path(os.environ.get('BEE_GATEWAY_PATH', '/opt/bee-ros-web-console/gateway/bee_gateway.py'))
if not gateway_path.is_file():
    raise FileNotFoundError(f'bee_gateway.py not found: {gateway_path}')
spec = importlib.util.spec_from_file_location('bee_gateway_patrol_test', gateway_path)
gateway = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gateway)

with tempfile.TemporaryDirectory(prefix='bee-patrol-test-') as temporary_root:
    root = Path(temporary_root)
    maps = root / 'maps'
    patrols = root / 'patrols'
    maps.mkdir()
    for map_id in ('map_alpha', 'map_beta'):
        (maps / f'{map_id}.yaml').write_text('image: placeholder.pgm\n', encoding='utf-8')
        (maps / f'{map_id}.pgm').write_bytes(b'P5\n1 1\n255\n\xff')

    manager = gateway.PatrolDraftManager(patrols, maps)
    request = {
        'id': None,
        'name': '仓库巡逻',
        'map_id': 'map_alpha',
        'repeat_count': 3,
        'waypoints': [
            {'x': 1.25, 'y': -2.5, 'yaw': 0.0},
            {'x': 2.0, 'y': -1.0, 'yaw': math.pi},
        ],
    }
    saved = manager.save(request)
    assert saved['repeat_count'] == 3 and len(saved['waypoints']) == 2
    assert manager.list('map_alpha')[0]['id'] == saved['id']

    updated_request = dict(request, id=saved['id'], repeat_count=99)
    updated = manager.save(updated_request)
    assert updated['repeat_count'] == 99 and updated['created_at'] == saved['created_at']

    expect_value_error(lambda: manager.save(dict(updated_request, map_id='map_beta')), 'patrol_map_mismatch')
    expect_value_error(lambda: manager.save(dict(request, map_id='map_missing')), 'patrol_map_not_found')
    expect_value_error(lambda: manager.save(dict(request, repeat_count=True)), 'invalid_repeat_count')
    expect_value_error(lambda: manager.save(dict(request, repeat_count=100)), 'invalid_repeat_count')
    expect_value_error(lambda: manager.save(dict(request, waypoints=[{'x': True, 'y': 0, 'yaw': 0}])), 'invalid_waypoints')
    expect_value_error(lambda: manager.save(dict(request, waypoints=[{'x': 100001, 'y': 0, 'yaw': 0}])), 'invalid_waypoints')
    expect_value_error(lambda: manager.save(dict(request, waypoints=[{'x': 0, 'y': 0, 'yaw': math.pi + 0.01}])), 'invalid_waypoints')
    expect_value_error(lambda: manager.save(dict(request, map_id='../escape')), 'invalid_map_id')

    deleted = manager.delete(saved['id'])
    assert deleted == saved['id'] and not manager.list('map_alpha')

safety = gateway.TaskSafetyManager()
valid_patrol = {
    'action': 'patrol_start',
    'map_id': 'map_alpha',
    'repeat_count': 2,
    'waypoints': [{'x': 1.0, 'y': 2.0, 'yaw': 0.0}],
}
assert safety.validate(valid_patrol) == 'patrol_start'
expect_value_error(lambda: safety.validate(dict(valid_patrol, repeat_count=True)), 'invalid_repeat_count')
expect_value_error(
    lambda: safety.validate(dict(valid_patrol, waypoints=[{'x': False, 'y': 0, 'yaw': 0}])),
    'invalid_waypoints',
)
try:
    safety.execute(valid_patrol)
except RuntimeError as error:
    assert 'physical_chassis_not_verified' in str(error)
else:
    raise AssertionError('motion safety gate unexpectedly allowed patrol_start')

print(json.dumps({'ok': True, 'checks': 'patrol_contract_and_fail_closed'}, ensure_ascii=False))
