#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Preview current portal assets with illustrative data; no hardware is contacted.

Run --profile dht|zigbee|matter|thread --port 8767, then capture with a browser.
POST operations are deliberately disabled. The public development QR is a fixture.
"""
import argparse
import base64
import hashlib
import json
import struct
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse, parse_qs

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'src/tools'))
from generate_portal_assets import portal_parts
from control_assets import selected
from portal_identifiers import identifier_maps, html_names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=['dht', 'zigbee', 'matter', 'thread'], default='dht')
    parser.add_argument('--port', type=int, default=8767)
    args = parser.parse_args()
    mesh = args.profile == 'zigbee'
    matter = args.profile in ('matter', 'thread')
    features = {'ZIGBEE': mesh, 'MATTER': matter, 'MATTER_CONSOLE': matter}
    css, script, shell = portal_parts(ROOT / 'src', guarded=True, controls={'text', 'slider', 'select'})
    css, script, shell = [selected(x, {'text', 'slider', 'select'}, features) for x in (css, script, shell)]
    head = (ROOT / 'src/PortalPage.h').read_text().split('R"HEAD(', 1)[1].split(')HEAD"', 1)[0]
    catalog = [{'id': 'home', 'name': 'Home', 'order': 0}, {'id': 'settings', 'name': 'Sensor settings', 'order': 1}]
    classes, ids = identifier_maps((ROOT / 'src/PortalPage.h').read_text())
    html = html_names(head, classes, ids) + json.dumps(catalog) + shell
    language = json.loads((ROOT / 'src/languages/en.json').read_text())['strings']
    config = dict(deviceName='ArdPortal demo', deviceDescription='Room climate sensor', ssid='Home Wi-Fi', host='mqtt.example.local', port=1883, user='sensor', mqttTls=0, caCert='', mqttName='ArdPortal-demo', apName='ArdPortal-demo', apBand=24, apDualBand=True)
    status = dict(deviceName=config['deviceName'], wifi=True, mqtt=True, ip='192.168.1.50', ap=False, clockReady=True, appConfigPageCount=2, zigbee=mesh, zigbeeConnected=mesh, matterCommissioned=matter, matterConnected=matter, portalConfigRevision=1)
    if args.profile == 'thread':
        status['threadConnected'] = True
    fields = [dict(id='temperature', type='text', name='Temperature', default=22.5, **{'class': 'temperature', 'unit_of_measurement': '°C'}), dict(id='humidity', type='text', name='Humidity', default=48, **{'class': 'humidity', 'unit_of_measurement': '%'}), dict(id='calibration', type='slider', name='Temperature calibration', min=-5, max=5, step=.1, default=0), dict(id='interval', type='select', name='MQTT updates', options=[{'value': 'change', 'name': 'On reading change'}, {'value': 'minute', 'name': 'Every minute'}], default='change')]
    page = dict(id='settings', name='Sensor settings', fields=fields[2:])
    home = dict(id='home', name='Home', fields=fields[:2])
    values = {f['id']: f['default'] for f in fields}
    info = dict(deviceName=config['deviceName'], deviceDescription=config['deviceDescription'], firmwareVersion='1.0', chip='ESP32-C5', chipId='DEMO', flashBytes=4194304, physicalFlashBytes=4194304, heapBytes=327680, freeHeapBytes=98304, minimumFreeHeapBytes=81920, maximumHeapBlockBytes=65536, resetReason='Power on', sketchBytes=2097152, otaFreeBytes=0, uptimeSeconds=86465, utc='2026-10-26T12:00:00Z', wifi=True, mqtt=True, ip=status['ip'], apIp='192.168.4.1', filesystemMounted=True)

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def reply(self, data, kind='application/json'):
            content = data.encode() if isinstance(data, str) else json.dumps(data).encode()
            self.send_response(200)
            self.send_header('Content-Type', kind + '; charset=utf-8')
            self.send_header('Content-Length', str(len(content)))
            self.end_headers()
            self.wfile.write(content)

        def socket_preview(self):
            key = self.headers['Sec-WebSocket-Key']
            accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
            self.send_response(101)
            self.send_header('Upgrade', 'websocket')
            self.send_header('Connection', 'Upgrade')
            self.send_header('Sec-WebSocket-Accept', accept)
            self.end_headers()
            def send(data):
                raw = json.dumps(data).encode()
                self.wfile.write(bytes([0x81, len(raw)]) + raw if len(raw) < 126 else b'\x81\x7e' + struct.pack('!H', len(raw)) + raw)
                self.wfile.flush()
            try:
                send(dict(type='status', mqtt=True))
                if self.path == '/api/console':
                    for i, text in enumerate(['RX: home/ArdPortal-demo/get — state', 'TX: home/ArdPortal-demo/temperature — 22.5', 'TX: home/ArdPortal-demo/humidity — 48']):
                        send(dict(type='log', channel='mqtt', id=i+1, text='[12:00:00] ' + text))
                while True:
                    time.sleep(10)
                    send(dict(type='status', mqtt=True))
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            route = urlparse(self.path)
            path = route.path
            if self.headers.get('Upgrade', '').lower() == 'websocket':
                return self.socket_preview()
            if path == '/portal.css':
                return self.reply(css, 'text/css')
            if path == '/portal.js':
                return self.reply(script, 'text/javascript')
            data = {
                '/api/languages': {'languages': [{'code': 'en', 'name': 'English', 'locale': 'en-GB'}], 'default': 'en'},
                '/api/language': language, '/api/config': config, '/api/status': status,
                '/api/info': info, '/api/page-catalog': catalog, '/api/page-conditions': {},
                '/api/pages': home if parse_qs(route.query).get('page') == ['home'] else page, '/api/app': {'values': values, 'conditions': {k: {'visible': True} for k in values}},
                '/api/networks': [{'ssid': 'Home Wi-Fi', 'rssi': -42, 'secured': True, 'channel': 6}, {'ssid': 'Home Wi-Fi 5 GHz', 'rssi': -58, 'secured': True, 'channel': 36}],
                '/api/zigbee/status': dict(role='End Device', paired=True, busy=False, channel=15, panId=0x1A62, attempts=1, endpoints=4, state='connected', error='', converterReady=True, zhaQuirkReady=True),
                '/api/matter/status': dict(transport='thread' if args.profile == 'thread' else 'wifi', state='connected', threadRole='router', threadDeviceType='ftd', threadIcd=False, credentials='development', endpoints=3, unsupported=0, error='', manualCode='34970112332', qrUrl='https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K9042C00KA0648G00'),
                '/api/matter/qr': json.loads((ROOT / 'docs/tools/fixtures/matter-demo-qr.json').read_text()),
            }
            if path in data:
                return self.reply(data[path])
            if path.startswith('/api/'):
                return self.reply({})
            self.reply(html, 'text/html')

        def do_POST(self):
            self.send_error(405, 'Read-only documentation preview')

    print(f'Portal preview ({args.profile}): http://127.0.0.1:{args.port}', flush=True)
    ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()


if __name__ == '__main__':
    main()
