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
# @brief Evaluate an asset feature expression against the selected control set.
# @param condition Visibility dependency condition to validate or evaluate.
# @param controls Optional set of controls included in the asset.
# @param features Feature selections used to evaluate the asset guard.
# @return True if the feature expression is enabled for the chosen controls.
def condition_enabled(condition, controls=None, features=None):
 values={'ARDPORTAL_ENABLE_BACKUP_RESTORE':1,'ARDPORTAL_ENABLE_GROUPS':0,'ARDPORTAL_ENABLE_ZIGBEE':0,'ARDPORTAL_ENABLE_MATTER':0,'ARDPORTAL_ENABLE_MATTER_CONSOLE':0,'ARDPORTAL_ENABLE_VISIBILITY':1,'ARDPORTAL_ENABLE_DEPENDENCIES':1,'ARDPORTAL_ENABLE_DYNAMIC_PAGES':1}
 values.update({PREFIX+c.upper():int(controls is None or c in controls) for c in CONTROLS})
 values.update({'ARDPORTAL_CONTROL_SUPPORT_'+g:int(any(values[PREFIX+c.upper()] for c in cs)) for g,cs in GROUPS.items()})
 if features:values.update({'ARDPORTAL_ENABLE_'+k:int(v) for k,v in features.items()})
 expression=re.sub(r'\bARDPORTAL_\w+\b',lambda m:str(values[m[0]]),condition)
 return bool(eval(expression.replace('&&',' and ').replace('||',' or ').replace('!',' not '),{'__builtins__':{}},{}))
# @brief Split source text into feature-guarded asset segments.
# @param source Input source or origin of a configuration change, as indicated by its type.
# @return Sequence of feature conditions and their corresponding source text.
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

# @brief Join only asset segments enabled for the selected controls.
# @param source Input source or origin of a configuration change, as indicated by its type.
# @param controls Optional set of controls included in the asset.
# @param features Feature selections used to evaluate the asset guard.
# @return Source text containing only enabled asset segments.
def inline_console_rows(result):
    result = re.sub(r'^function appendConsoleLine[^\n]*\n', '', result, flags=re.M)
    def inline_rows(match):
        return ("const rows=consoleRows[message.channel];if(rows.some(row=>row.id===message.id))return;"
                "rows.push(message);if(rows.length>200)rows.shift();const box=" + match[1] +
                ",atEnd=box.scrollHeight-box.scrollTop-box.clientHeight<30;"
                "box.textContent=rows.map(row=>formatConsoleLine(row.text)).join('\\n');"
                "if(atEnd)box.scrollTop=box.scrollHeight}")
    result, count = re.subn(r'appendConsoleLine\(message\.channel,message,(document\.querySelector\([^\n]+?)\)\}', inline_rows, result)
    if count != 1:
        raise ValueError('Unsupported single-console row renderer')
    return result


def selected(source,controls=None,features=None):
    result = ''.join(text for condition,text in segments(source) if condition_enabled(condition,controls,features))
    # Share only with multiple consumers; retain the smaller inlined common case.
    return inline_console_rows(result) if result.count('appendConsoleLine(') == 2 else result

# @brief Emit C++ declarations and feature guards for a set of asset chunks.
# @param name Fallback name or generated symbol name.
# @param parts Feature-guarded asset segments.
# @param gzip_array Encoder converting asset text to gzip C++ declarations.
# @return Generated C++ lines for the supplied chunk set.
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


# @brief Emit shared and profile-specific asset chunks without duplicate declarations.
# @param name Fallback name or generated symbol name.
# @param source Input source or origin of a configuration change, as indicated by its type.
# @param gzip_array Encoder converting asset text to gzip C++ declarations.
# @return Generated C++ lines for shared and conditional profile chunks.
# @brief Preserve optional Zigbee code when selecting an existing control profile.
# @param name Asset symbol name.
# @param source Guarded source text.
# @param controls Selected controls.
# @param features Other feature selections.
# @param gzip_array Asset encoder.
# @return Conditional C++ asset declarations.
def emit_selected(name,source,controls,features,gzip_array):
    # Whole-stream compression keeps the disabled profile as small as before;
    # groups add no split DEFLATE blocks or metadata when compiled out.
    if 'ARDPORTAL_ENABLE_GROUPS' in source and 'GROUPS' not in features and features.get('DYNAMIC_PAGES',1):
        return (['#if ARDPORTAL_ENABLE_GROUPS'] +
                emit_selected(name,source,controls,{**features,'GROUPS':1},gzip_array) +
                ['#else'] +
                emit_selected(name,source,controls,{**features,'GROUPS':0},gzip_array) +
                ['#endif'])
    if 'ARDPORTAL_ENABLE_BACKUP_RESTORE' in source and 'BACKUP_RESTORE' not in features:
        return (['#if ARDPORTAL_ENABLE_BACKUP_RESTORE'] +
                emit_selected(name,source,controls,{**features,'BACKUP_RESTORE':1},gzip_array) +
                ['#else'] +
                emit_selected(name,source,controls,{**features,'BACKUP_RESTORE':0},gzip_array) +
                ['#endif'])
    native=[key for key in ['ZIGBEE','MATTER'] if 'ARDPORTAL_ENABLE_'+key in source]
    if not native:
        return emit_chunks(name,[('1',selected(source,controls,features))],gzip_array)
    lines=[]
    for index,key in enumerate(native+[None]):
        lines.append(('#if ' if index==0 else '#elif ')+'ARDPORTAL_ENABLE_'+key if key else '#else')
        flags={item:int(item==key) for item in native}
        if key == 'MATTER' and 'ARDPORTAL_ENABLE_MATTER_CONSOLE' in source:
            lines.append('#if ARDPORTAL_ENABLE_MATTER_CONSOLE')
            lines+=emit_chunks(name,[('1',selected(source,controls,{**features,**flags,'MATTER_CONSOLE':1}))],gzip_array)
            lines.append('#else')
            lines+=emit_chunks(name,[('1',selected(source,controls,{**features,**flags,'MATTER_CONSOLE':0}))],gzip_array)
            lines.append('#endif')
        else:
            lines+=emit_chunks(name,[('1',selected(source,controls,{**features,**flags}))],gzip_array)
    return lines+['#endif']

def emit_profile_chunks(name, source, gzip_array):
    lines=['#if !ARDPORTAL_ENABLE_DYNAMIC_PAGES']
    lines += emit_selected(name,source,[],{'DYNAMIC_PAGES':0,'DEPENDENCIES':0,'VISIBILITY':0},gzip_array)
    lines += ['#elif ARDPORTAL_CONTROL_PROFILE_FULL']
    lines += ['#if ARDPORTAL_ENABLE_DEPENDENCIES']
    lines += emit_selected(name,source,None,{},gzip_array)
    lines += ['#elif ARDPORTAL_ENABLE_VISIBILITY']
    lines += emit_selected(name,source,None,{'DEPENDENCIES':0},gzip_array)
    lines += ['#else']
    lines += emit_selected(name,source,None,{'DEPENDENCIES':0,'VISIBILITY':0},gzip_array)
    lines += ['#endif']
    lines += ['#elif ARDPORTAL_CONTROL_PROFILE_BASIC']
    lines += ['#if ARDPORTAL_ENABLE_DEPENDENCIES']
    lines += emit_selected(name,source,['slider','text','select','edit'],{},gzip_array)
    lines += ['#elif ARDPORTAL_ENABLE_VISIBILITY']
    lines += emit_selected(name,source,['slider','text','select','edit'],{'DEPENDENCIES':0},gzip_array)
    lines += ['#else']
    lines += emit_selected(name,source,['slider','text','select','edit'],{'DEPENDENCIES':0,'VISIBILITY':0},gzip_array)
    lines += ['#endif']
    lines += ['#else']
    if 'function appendConsoleLine(' in source:
        lines += ['#if ARDPORTAL_ENABLE_ZIGBEE || ARDPORTAL_ENABLE_MATTER_CONSOLE']
        lines += emit_chunks(name,segments(source),gzip_array)
        lines += ['#else']
        lines += emit_chunks(name,segments(inline_console_rows(source)),gzip_array)
        lines += ['#endif']
    else:
        lines += emit_chunks(name,segments(source),gzip_array)
    return lines+['#endif']
