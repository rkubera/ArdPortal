# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compile conditionally selected DEFLATE blocks for portal controls."""
import re
import json
import zlib
from pathlib import Path
CONTROLS=json.loads(Path(__file__).with_name('control_types.json').read_text())
PREFIX='ARDPORTAL_ENABLE_CONTROL_'
GROUPS={
 'EXTENDED':CONTROLS[6:], 'DIAL':['climate','humidifier','water_heater'],
 'SWITCH':['switch','light','fan','humidifier','siren'],
 'SLIDER':['slider','light','fan','cover','valve','humidifier','siren','water_heater'],
 'SELECT':['select','climate','fan','humidifier','siren','water_heater','vacuum'],
 'ACTIONS':['button','scene','notify','infrared','cover','valve','lock','alarm_control_panel','vacuum','lawn_mower','update'],
 'GROUPED_ACTIONS':['cover','valve','lock','alarm_control_panel','vacuum','lawn_mower']}
def condition_enabled(condition, controls=None, features=None):
 values={'ARDPORTAL_ENABLE_DEPENDENCIES':1,'ARDPORTAL_ENABLE_DYNAMIC_PAGES':1}
 values.update({PREFIX+c.upper():int(controls is None or c in controls) for c in CONTROLS})
 values.update({'ARDPORTAL_CONTROL_SUPPORT_'+g:int(any(values[PREFIX+c.upper()] for c in cs)) for g,cs in GROUPS.items()})
 if features:values.update({'ARDPORTAL_ENABLE_'+k:int(v) for k,v in features.items()})
 expression=re.sub(r'\bARDPORTAL_\w+\b',lambda m:str(values[m[0]]),condition)
 return bool(eval(expression.replace('&&',' and ').replace('||',' or ').replace('!',' not '),{'__builtins__':{}},{}))
def segments(source):
 stack=[];cursor=0;parts=[]
 for marker in re.finditer(r'/\*@(?:(if) ([^*]+)|(endif))\*/',source):
  text=source[cursor:marker.start()]
  if text:parts.append((' && '.join('('+c+')' for c in stack) or '1',text))
  if marker[1]:stack.append(marker[2].strip())
  else:stack.pop()
  cursor=marker.end()
 if stack:raise ValueError('Unclosed control guard')
 if source[cursor:]:parts.append(('1',source[cursor:]))
 # Merge adjacent blocks with the same condition for better compression.
 merged=[]
 for condition,text in parts:
  if merged and merged[-1][0]==condition:merged[-1]=(condition,merged[-1][1]+text)
  else:merged.append((condition,text))
 return merged

def selected(source,controls=None,features=None):
 return ''.join(text for condition,text in segments(source) if condition_enabled(condition,controls,features))

def emit_chunks(name,parts,gzip_array):
 # A single part uses ordinary gzip. Conditional parts form one DEFLATE stream,
 # not multiple gzip members: browsers need one checksum/footer for the response.
 if len(parts)==1 and parts[0][0]=='1':
  chunk=name+'_0'
  return gzip_array(chunk,parts[0][1])+['static const ArdAssetChunk '+name+'[] PROGMEM = {','  {'+chunk+',sizeof('+chunk+')-1},','};']
 lines=[];entries=[]
 for i,(condition,text) in enumerate(parts):
  chunk=name+'_'+str(i);raw=text.encode('utf-8')
  compressor=zlib.compressobj(9,zlib.DEFLATED,-15)
  data=compressor.compress(raw)+compressor.flush(zlib.Z_SYNC_FLUSH)
  if condition!='1':lines.append('#if '+condition)
  lines.append('static const char '+chunk+'[] PROGMEM =')
  for offset in range(0,len(data),32):lines.append('"'+''.join('\\x%02x'%v for v in data[offset:offset+32])+'"')
  lines[-1]+=';'
  if condition!='1':lines.append('#endif')
  entries.append((condition,chunk,zlib.crc32(raw),len(raw)))
 lines.append('static const ArdAssetChunk '+name+'[] PROGMEM = {')
 lines.append('  {ARD_ASSET_GZIP_HEAD,sizeof(ARD_ASSET_GZIP_HEAD)-1,0,UINT32_MAX},')
 for condition,chunk,crc,length in entries:
  if condition!='1':lines.append('#if '+condition)
  lines.append('  {'+chunk+',sizeof('+chunk+')-1,'+str(crc)+'U,'+str(length)+'U},')
  if condition!='1':lines.append('#endif')
 lines+=['  {ARD_ASSET_DEFLATE_END,sizeof(ARD_ASSET_DEFLATE_END)-1},','};']
 return lines


def emit_profile_chunks(name, source, gzip_array):
    lines=['#if !ARDPORTAL_ENABLE_DYNAMIC_PAGES']
    lines += emit_chunks(name,[('1',selected(source,[],{'DYNAMIC_PAGES':0,'DEPENDENCIES':0}))],gzip_array)
    lines += ['#elif ARDPORTAL_CONTROL_PROFILE_FULL']
    lines += ['#if ARDPORTAL_ENABLE_DEPENDENCIES']
    lines += emit_chunks(name,[('1',selected(source))],gzip_array)
    lines += ['#else']
    lines += emit_chunks(name,[('1',selected(source,features={'DEPENDENCIES':0}))],gzip_array)
    lines += ['#endif']
    lines += ['#elif ARDPORTAL_CONTROL_PROFILE_BASIC']
    lines += ['#if ARDPORTAL_ENABLE_DEPENDENCIES']
    lines += emit_chunks(name,[('1',selected(source,['slider','text','select','edit']))],gzip_array)
    lines += ['#else']
    lines += emit_chunks(name,[('1',selected(source,['slider','text','select','edit'],{'DEPENDENCIES':0}))],gzip_array)
    lines += ['#endif']
    lines += ['#else']
    lines += emit_chunks(name,segments(source),gzip_array)
    return lines+['#endif']
