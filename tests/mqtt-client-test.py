"""Run the standalone client, without any portal or Wi-Fi headers."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 exe=Path(d)/'mqtt'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/mqtt-host'),'-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),str(root/'tests/mqtt-host/client.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: standalone MQTT connect, publish, subscription ACKs, fragmented binary retained messages, keepalive, timeout and malformed packets')
