import argparse
import json
import ssl
import threading
import time
from pathlib import Path

import paho.mqtt.client as mqtt


DEFAULT_ENV = Path(
    r"C:\Users\Bee\Desktop\远程车控制系统\1-0 服务器接入说明\mqtt-connection.env"
)


def read_env(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip().strip('"').strip("'")
    return result


def crc8_atm(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def build_control(sequence: int, command: int) -> bytes:
    frame = bytearray(20)
    frame[0:5] = bytes((0x55, 0x43, 0x01, sequence & 0xFF, command & 0xFF))
    frame[18] = crc8_atm(frame[:18])
    frame[19] = 0x5B
    return bytes(frame)


def main() -> int:
    parser = argparse.ArgumentParser(description="Send a safe 0x49 STOP frame to ESP32-C3")
    parser.add_argument("--env", type=Path, default=DEFAULT_ENV)
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args()
    cfg = read_env(args.env)
    connected = threading.Event()
    acknowledged = threading.Event()
    sequence = int(time.time()) & 0xFF

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"safe-probe-{int(time.time())}")
    client.username_pw_set(cfg["CONTROLLER_USERNAME"], cfg["CONTROLLER_PASSWORD"])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            client.subscribe(cfg["STATUS_TOPIC"], qos=0)
            connected.set()

    def on_message(client, userdata, message):
        try:
            status = json.loads(message.payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        print("STATUS", json.dumps(status, ensure_ascii=False, separators=(",", ":")))
        if status.get("last_seq") == sequence and status.get("last_cmd") == 0x49:
            acknowledged.set()

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(cfg["MQTT_HOST"], int(cfg["MQTT_PORT"]), keepalive=30)
    client.loop_start()
    try:
        if not connected.wait(10):
            print("RESULT=MQTT_CONNECT_TIMEOUT")
            return 2
        frame = build_control(sequence, 0x49)
        info = client.publish(cfg["CONTROL_TOPIC"], frame, qos=0, retain=False)
        info.wait_for_publish(5)
        print(f"SAFE_STOP_SENT seq={sequence} length=20 hex={frame.hex(' ')}")
        if acknowledged.wait(args.timeout):
            print("RESULT=SAFE_STOP_ACK_PASS")
            return 0
        print("RESULT=ACK_TIMEOUT")
        return 3
    finally:
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    raise SystemExit(main())

