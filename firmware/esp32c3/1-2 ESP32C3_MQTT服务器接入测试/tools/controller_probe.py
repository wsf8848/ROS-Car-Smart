import argparse
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


def main() -> int:
    parser = argparse.ArgumentParser(description="ESP32-C3 MQTT end-to-end probe")
    parser.add_argument("--env", type=Path, default=DEFAULT_ENV)
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()
    cfg = read_env(args.env)

    received_status: list[bytes] = []
    received_telemetry: list[bytes] = []
    connected = threading.Event()

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"controller-probe-{int(time.time())}")
    client.username_pw_set(cfg["CONTROLLER_USERNAME"], cfg["CONTROLLER_PASSWORD"])
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code != 0:
            print(f"Controller probe connection failed: {reason_code}")
            return
        client.subscribe([(cfg["STATUS_TOPIC"], 0), (cfg["TELEMETRY_TOPIC"], 0)])
        connected.set()

    def on_message(client, userdata, message):
        if message.topic == cfg["STATUS_TOPIC"]:
            received_status.append(bytes(message.payload))
            print(f"STATUS received: {message.payload.decode(errors='replace')}")
        elif message.topic == cfg["TELEMETRY_TOPIC"]:
            received_telemetry.append(bytes(message.payload))
            print(f"TELEMETRY received: length={len(message.payload)}, hex={message.payload.hex(' ')}")

    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(cfg["MQTT_HOST"], int(cfg["MQTT_PORT"]), keepalive=30)
    client.loop_start()
    try:
        if not connected.wait(10):
            print("Controller probe did not connect within 10 seconds")
            return 2
        test_frame = bytes(range(1, 21))
        info = client.publish(cfg["CONTROL_TOPIC"], test_frame, qos=0, retain=False)
        info.wait_for_publish(5)
        print(f"CONTROL published: length=20, hex={test_frame.hex(' ')}")

        deadline = time.time() + args.timeout
        while time.time() < deadline:
            if received_status and any(len(item) == 20 for item in received_telemetry):
                print("END_TO_END_RESULT=PASS")
                return 0
            time.sleep(0.1)
        print("END_TO_END_RESULT=TIMEOUT")
        return 3
    finally:
        client.disconnect()
        client.loop_stop()


if __name__ == "__main__":
    raise SystemExit(main())
