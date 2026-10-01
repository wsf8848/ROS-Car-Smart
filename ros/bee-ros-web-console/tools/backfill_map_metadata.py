#!/usr/bin/env python3
"""为旧版.yaml/.pgm地图补齐Bee网页元数据侧车，不覆盖已有.json。"""

import argparse
import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path


def valid_map_id(value):
    return value and Path(value).name == value and all(character.isalnum() or character in '_-' for character in value)


def main():
    parser = argparse.ArgumentParser(description='补齐旧地图名称、场地和创建时间元数据')
    parser.add_argument('--maps-root', default='/opt/bee-ros-web-console/data/maps')
    parser.add_argument('--site', default='历史测试地图（待实车建图后替换）')
    args = parser.parse_args()
    root = Path(args.maps_root).resolve()
    if not root.is_dir():
        raise FileNotFoundError('maps_root_not_found')

    created = []
    skipped = []
    for yaml_path in sorted(root.glob('*.yaml')):
        map_id = yaml_path.stem
        if not valid_map_id(map_id):
            raise ValueError(f'invalid_map_id:{map_id}')
        pgm_path = root / f'{map_id}.pgm'
        json_path = root / f'{map_id}.json'
        if not pgm_path.is_file():
            raise FileNotFoundError(f'incomplete_map:{map_id}')
        if json_path.exists():
            skipped.append(map_id)
            continue
        created_at = datetime.fromtimestamp(yaml_path.stat().st_mtime, timezone.utc).isoformat().replace('+00:00', 'Z')
        metadata = {
            'id': map_id,
            'name': f'历史地图 {map_id}',
            'site': args.site,
            'created_at': created_at,
            'format': 'nav2_map_server_yaml_pgm',
        }
        descriptor, temporary_name = tempfile.mkstemp(prefix=f'.{map_id}.', suffix='.json.tmp', dir=root)
        try:
            with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
                json.dump(metadata, stream, ensure_ascii=False, indent=2)
                stream.write('\n')
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary_name, json_path)
        except Exception:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass
            raise
        created.append(map_id)

    print(json.dumps({'ok': True, 'created': created, 'skipped_existing': skipped}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
