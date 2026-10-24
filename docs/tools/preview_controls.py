#!/usr/bin/env python3
"""Render the library frontend with mock data for Wiki control screenshots.

Run locally, then open http://127.0.0.1:8766/p/light. No hardware is contacted.
"""
import json,re,sys
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlparse,parse_qs
ROOT=Path(__file__).resolve().parents[2]
TYPES=json.loads((ROOT/'src/tools/control_types.json').read_text())
sys.path.insert(0,str(ROOT/'src/tools'))
from control_protocols import runtime_model
specs={name:runtime_model(model) for name,model in json.loads((ROOT/'src/tools/control_models.json').read_text()).items()}
def label(typ):return typ.replace('_',' ').capitalize()
fields={typ:{'id':typ,'type':typ,'name':label(typ),'extended':typ in specs} for typ in TYPES}
fields['slider'].update(min=0,max=100,step=1,default=65)
fields['text'].update(name='Room temperature',default=22.5,unit_of_measurement='°C')
fields['select'].update(options=[{'value':'eco','name':'Economy'},{'value':'comfort','name':'Comfort'}],default='eco')
fields['edit'].update(name='Room name',default='Living room')
fields['switch'].update(name='Enabled',default=True)
fields['climate'].update(min=10,max=30,step=.5,modes=['off','heat','cool','auto'],fan_modes=['auto','low','high'],default={'temperature':22,'current_temperature':20.5,'mode':'heat','fan_mode':'auto','action':'heating'})
values={typ:(specs[typ]['default'] if typ in specs else fields[typ]['default']) for typ in TYPES}
values.update(binary_sensor=True,date='2026-10-10',time='14:30:00',datetime='2026-10-10T14:30:00Z',device_tracker='home',tag='tag-123',light={'state':'ON','brightness':180,'color':{'r':255,'g':80,'b':0},'color_mode':'rgb'},fan={'state':'ON','percentage':65,'oscillation':'oscillate_on','direction':'forward'},cover={'state':'open','position':60,'tilt':30},valve={'state':'open','position':70},humidifier={'state':'ON','humidity':55,'current_humidity':48,'mode':'normal'},water_heater={'temperature':55,'current_temperature':47,'mode':'electric'},siren={'state':'ON','tone':'bell','duration':30,'volume_level':.4},vacuum={'state':'cleaning','fan_speed':'normal'},lawn_mower='mowing',alarm_control_panel='armed_home',update={'installed_version':'0.7.7','latest_version':'0.7.8','in_progress':False})
catalog=[{'id':typ,'name':label(typ)+' control','order':i} for i,typ in enumerate(TYPES)]
page_source=(ROOT/'src/PortalPage.h').read_text()
head=re.search(r'R"HEAD\((.*?)\)HEAD"',page_source,re.S)[1]
body=re.search(r'R"HTML\((.*?)\)HTML"',page_source,re.S)[1]
HTML=(head+json.dumps(catalog)+body).encode()
language=json.loads((ROOT/'src/languages/en.json').read_text())
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        route=urlparse(self.path);query=parse_qs(route.query);path=route.path
        if not path.startswith('/api/'):
            self.send_response(200);self.send_header('Content-Type','text/html; charset=utf-8');self.end_headers();self.wfile.write(HTML);return
        typ=query.get('page',['light'])[0]
        data={
          '/api/languages':{'languages':[{'code':'en','name':'English','locale':'en-GB'}],'default':'en'},
          '/api/language':language['strings'],
          '/api/config':{'deviceName':'ArdPortal controls','mqttName':'demo','apName':'ArdPortal-demo','port':1883},
          '/api/status':{'wifi':True,'mqtt':False,'ip':'127.0.0.1','ap':False,'clockReady':True,'appConfigPageCount':len(TYPES)},
          '/api/info':{},'/api/page-catalog':catalog,'/api/page-conditions':{},
          '/api/pages':{'id':typ,'name':label(typ)+' control','fields':[fields[typ]]},
          '/api/app':{'values':{typ:values[typ]},'conditions':{typ:{'visible':True}}},
          '/api/entity-type':specs.get(query.get('type',[''])[0],{})
        }.get(path,{})
        content=json.dumps(data).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
if __name__=='__main__':
    print('Wiki control preview: http://127.0.0.1:8766/p/light',flush=True)
    ThreadingHTTPServer(('127.0.0.1',8766),Handler).serve_forever()
