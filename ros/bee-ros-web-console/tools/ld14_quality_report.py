#!/usr/bin/env python3
import argparse
import json
import math
import statistics
import time
from datetime import datetime, timezone
from pathlib import Path

import rclpy
from rclpy.node import Node
from rclpy.signals import SignalHandlerOptions
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import LaserScan


def percentile(values, ratio):
    if not values:
        return None
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round((len(ordered) - 1) * ratio)))
    return ordered[index]


class ScanCollector(Node):
    def __init__(self):
        super().__init__('bee_ld14_quality_report')
        self.frames = []
        self.arrivals = []
        self.create_subscription(LaserScan, '/scan', self.on_scan, qos_profile_sensor_data)

    def on_scan(self, msg):
        now = time.time()
        finite = []
        intensities = []
        near = 0
        below_declared = 0
        for index, raw in enumerate(msg.ranges):
            value = float(raw)
            if not math.isfinite(value):
                continue
            if value < float(msg.range_min) or value > float(msg.range_max):
                below_declared += 1
                continue
            finite.append(value)
            if 0.15 <= value < 0.30:
                near += 1
            if index < len(msg.intensities):
                intensities.append(float(msg.intensities[index]))
        stamp = float(msg.header.stamp.sec) + float(msg.header.stamp.nanosec) / 1e9
        coverage = max(0.0, float(msg.angle_max) - float(msg.angle_min))
        self.arrivals.append(now)
        self.frames.append({
            'arrival': now,
            'stamp': stamp,
            'age_ms': max(0.0, (now - stamp) * 1000.0) if stamp > 1_000_000_000 else None,
            'total': len(msg.ranges),
            'valid': len(finite),
            'near_15_30cm': near,
            'invalid_in_declared_range': below_declared,
            'minimum_m': min(finite) if finite else None,
            'maximum_m': max(finite) if finite else None,
            'median_m': statistics.median(finite) if finite else None,
            'mean_intensity': statistics.fmean(intensities) if intensities else None,
            'coverage_deg': math.degrees(coverage),
            'angle_increment_deg': math.degrees(float(msg.angle_increment)),
            'range_min_m': float(msg.range_min),
            'range_max_m': float(msg.range_max),
            'frame_id': msg.header.frame_id,
        })


def summarize(frames, arrivals, requested_seconds):
    intervals = [(b - a) for a, b in zip(arrivals, arrivals[1:]) if b > a]
    duration = (arrivals[-1] - arrivals[0]) if len(arrivals) > 1 else 0.0
    valid_counts = [f['valid'] for f in frames]
    total_counts = [f['total'] for f in frames]
    minimums = [f['minimum_m'] for f in frames if f['minimum_m'] is not None]
    maximums = [f['maximum_m'] for f in frames if f['maximum_m'] is not None]
    ages = [f['age_ms'] for f in frames if f['age_ms'] is not None and f['age_ms'] < 60000]
    intensities = [f['mean_intensity'] for f in frames if f['mean_intensity'] is not None]
    frequency = (len(frames) - 1) / duration if duration > 0 and len(frames) > 1 else 0.0
    long_gaps = [value for value in intervals if frequency > 0 and value > max(0.35, 2.5 / frequency)]
    first = frames[0] if frames else {}
    return {
        'generated_at_utc': datetime.now(timezone.utc).isoformat(),
        'topic': '/scan',
        'requested_seconds': requested_seconds,
        'observed_seconds': round(duration, 3),
        'frames': len(frames),
        'frequency_hz': round(frequency, 3),
        'interval_ms': {
            'median': round(statistics.median(intervals) * 1000, 3) if intervals else None,
            'p95': round(percentile(intervals, 0.95) * 1000, 3) if intervals else None,
            'maximum': round(max(intervals) * 1000, 3) if intervals else None,
            'long_gap_count': len(long_gaps),
        },
        'scan_contract': {
            'frame_id': first.get('frame_id'),
            'range_min_m': first.get('range_min_m'),
            'range_max_m': first.get('range_max_m'),
            'coverage_deg': round(first.get('coverage_deg', 0.0), 3),
            'angle_increment_deg': round(first.get('angle_increment_deg', 0.0), 6),
            'points_per_frame_median': round(statistics.median(total_counts), 1) if total_counts else 0,
        },
        'valid_points': {
            'minimum': min(valid_counts) if valid_counts else 0,
            'median': round(statistics.median(valid_counts), 1) if valid_counts else 0,
            'p95': percentile(valid_counts, 0.95),
            'maximum': max(valid_counts) if valid_counts else 0,
        },
        'distance_m': {
            'observed_minimum': round(min(minimums), 3) if minimums else None,
            'median_of_frame_minimums': round(statistics.median(minimums), 3) if minimums else None,
            'observed_maximum': round(max(maximums), 3) if maximums else None,
        },
        'near_points_15_30cm': sum(f['near_15_30cm'] for f in frames),
        'declared_range_violations': sum(f['invalid_in_declared_range'] for f in frames),
        'message_age_ms': {
            'median': round(statistics.median(ages), 3) if ages else None,
            'p95': round(percentile(ages, 0.95), 3) if ages else None,
            'maximum': round(max(ages), 3) if ages else None,
        },
        'intensity_mean': round(statistics.fmean(intensities), 3) if intensities else None,
        'checks': {
            'received_frames': len(frames) >= max(5, requested_seconds * 3),
            'frequency_healthy_4_to_12_hz': 4.0 <= frequency <= 12.0,
            'coverage_at_least_350_deg': first.get('coverage_deg', 0.0) >= 350.0,
            'range_min_is_0_15m': abs(first.get('range_min_m', 0.0) - 0.15) <= 0.011,
            'has_valid_points': bool(valid_counts and statistics.median(valid_counts) >= 20),
            'no_long_gaps': len(long_gaps) == 0,
        },
    }


def markdown(report):
    checks = '\n'.join(f"- {'PASS' if value else 'FAIL'}: `{name}`" for name, value in report['checks'].items())
    return f"""# LD14 实体质量自动报告

- 生成时间UTC：{report['generated_at_utc']}
- 连续采样：{report['observed_seconds']} 秒 / {report['frames']} 帧
- 实测频率：{report['frequency_hz']} Hz
- 帧间隔中位数/P95/最大值：{report['interval_ms']['median']} / {report['interval_ms']['p95']} / {report['interval_ms']['maximum']} ms
- 长间隔数量：{report['interval_ms']['long_gap_count']}
- frame：`{report['scan_contract']['frame_id']}`
- 角度覆盖：{report['scan_contract']['coverage_deg']}°
- 声明量程：{report['scan_contract']['range_min_m']} ~ {report['scan_contract']['range_max_m']} m
- 每帧有效点：最少 {report['valid_points']['minimum']} / 中位 {report['valid_points']['median']} / 最大 {report['valid_points']['maximum']}
- 本次最小/最大实体回波：{report['distance_m']['observed_minimum']} / {report['distance_m']['observed_maximum']} m
- 15~30cm近距点累计：{report['near_points_15_30cm']}
- 消息年龄中位/P95/最大值：{report['message_age_ms']['median']} / {report['message_age_ms']['p95']} / {report['message_age_ms']['maximum']} ms
- 平均强度：{report['intensity_mean']}

## 自动检查

{checks}

## 验证边界

本报告只证明当前静态环境下ROS `/scan`数据质量。白墙、黑色材料、玻璃、斜面、强光、不同距离标尺和移动场景仍需现场摆放目标逐项验证。
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = ScanCollector()
    deadline = time.monotonic() + max(5, args.seconds)
    try:
        while time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.25)
    finally:
        report = summarize(node.frames, node.arrivals, args.seconds)
        node.destroy_node()
        rclpy.try_shutdown()
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    output.with_suffix('.md').write_text(markdown(report), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if not all(report['checks'].values()):
        raise SystemExit(2)


if __name__ == '__main__':
    main()
