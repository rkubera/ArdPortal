# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compile protocol-neutral presets into the existing firmware resources.

Named controls identify semantic actions/properties in the authoring catalogue.
Names are consumed here, never stored in MCU RAM or flash. Protocol vocabulary
belongs in this adapter; payloads, command suffixes, options and bounds belong
in control_models.json. User-supplied HA overrides remain an explicit escape hatch.
"""
from copy import deepcopy
import json


def runtime_model(model):
    """Keep the runtime JSON layout, without compiler-only semantic names."""
    result = deepcopy(model)
    result.pop('states', None)  # Compiler-only semantic vocabulary, not initial state.
    result['controls'] = list(result['controls'].values())
    return result


def ha_defaults(name, model):
    """Translate one built-in preset to MQTT Discovery before compilation."""
    controls = model['controls']
    out = {}

    def command(role, topic='command_topic'):
        suffix = controls[role]['command']
        out[topic] = '$command' + (':' + suffix if suffix else '')

    def state(topic='state_topic', template=None, key=None, optional=False):
        out[topic] = '$state'
        if template:
            fallback = " | default('', true)" if optional else ''
            out[template] = '{{ value_json.' + key + fallback + ' }}'

    def property(role, command_key, state_key, template):
        command(role, command_key)
        state(state_key, template, controls[role]['key'])

    def payload(role, key):
        out[key] = controls[role]['payload']

    def optional_payload(role, key, protocol_default):
        # Omit protocol defaults to keep the generated resources compact, but
        # derive an override when the shared action uses a different payload.
        if role in controls and controls[role]['payload'] != protocol_default:
            payload(role, key)

    def literal_template(value):
        # Plain templates avoid extra bytes for ordinary action strings. Quote
        # template delimiters so a literal payload cannot become Jinja code.
        return value if value and not any(token in value for token in ('{{', '{%', '{#')) else '{{ ' + json.dumps(value) + ' }}'

    def options(role, key):
        out[key] = list(controls[role]['options'])

    def switch_payloads(role, prefix='payload_', defaults=('ON', 'OFF')):
        control = controls[role]
        on, off = control.get('on'), control.get('off')
        if not isinstance(on, str) or not isinstance(off, str) or on == off:
            raise ValueError(name + '/' + role + ': on/off must be distinct strings')
        for state, value, default in zip(('on', 'off'), (on, off), defaults):
            if value != default:
                out[prefix + state] = value

    if name == 'binary_sensor':
        state()
        out.update(payload_on='ON', payload_off='OFF')
    elif name in ('date', 'time', 'datetime'):
        state()
        command('value')
    elif name in ('button', 'scene', 'update'):
        role, key = {'button': ('press', 'payload_press'), 'scene': ('activate', 'payload_on'),
                     'update': ('install', 'payload_install')}[name]
        if not model.get('action_only'):
            state()
        command(role)
        payload(role, key)
    elif name == 'notify':
        command('send')
    elif name == 'infrared':
        out['schema'] = 'emitter'
        command('transmit')
    elif name == 'event':
        state()
    elif name == 'device_trigger':
        out.update(automation_type='trigger', topic='$state', type='button_short_press',
                   subtype='button_1', payload=model['default'])
    elif name == 'tag':
        state('topic')
    elif name == 'device_tracker':
        state()
        present, absent = model['states']['present'], model['states']['absent']
        if not isinstance(present, str) or not isinstance(absent, str) or present == absent:
            raise ValueError('device_tracker: present/absent must be distinct strings')
        out.update(payload_home=present, payload_not_home=absent)
    elif name == 'light':
        out['schema'] = 'json'
        command('state')
        state()
        if 'brightness' in controls:
            out['brightness'] = True
        if 'color' in controls:
            out['supported_color_modes'] = [model['default']['color_mode']]
    elif name == 'fan':
        property('state', 'command_topic', 'state_topic', 'state_value_template')
        switch_payloads('state')
        for role in ('percentage', 'oscillation', 'direction'):
            if role in controls:
                property(role, role + '_command_topic', role + '_state_topic', role + '_value_template')
        if 'oscillation' in controls:
            switch_payloads('oscillation', 'payload_oscillation_', ('oscillate_on', 'oscillate_off'))
    elif name == 'cover':
        command('open')
        state(template='value_template', key='state')
        if 'position' in controls:
            state('position_topic', 'position_template', controls['position']['key'])
            command('position', 'set_position_topic')
        if 'tilt' in controls:
            property('tilt', 'tilt_command_topic', 'tilt_status_topic', 'tilt_status_template')
        for role in ('open', 'close', 'stop'):
            optional_payload(role, 'payload_' + role, role.upper())
    elif name == 'valve':
        command('open')
        state()
        out['reports_position'] = 'position' in controls
    elif name == 'lock':
        command('lock')
        state()
        for role in ('lock', 'unlock', 'open'):
            if role in controls:
                payload(role, 'payload_' + role)
    elif name == 'alarm_control_panel':
        command('arm_home')
        state()
        out.update(code_arm_required=False, code_disarm_required=False, code_trigger_required=False)
        for role in ('arm_home', 'arm_away', 'arm_night', 'disarm', 'trigger'):
            optional_payload(role, 'payload_' + role, role.upper())
    elif name == 'humidifier':
        property('state', 'command_topic', 'state_topic', 'state_value_template')
        switch_payloads('state')
        property('humidity', 'target_humidity_command_topic', 'target_humidity_state_topic', 'target_humidity_state_template')
        if 'current_humidity' in model['default']:
            state('current_humidity_topic', 'current_humidity_template', 'current_humidity', optional=True)
        if 'mode' in controls:
            property('mode', 'mode_command_topic', 'mode_state_topic', 'mode_state_template')
            options('mode', 'modes')
    elif name == 'siren':
        command('state')
        state()
        if 'tone' in controls:
            options('tone', 'available_tones')
        out.update(support_duration='duration' in controls, support_volume_set='volume_level' in controls,
                   support_turn_on='state' in controls, support_turn_off='state' in controls,
                   support_tones='tone' in controls)
    elif name == 'vacuum':
        command('start')
        state()
        # Keep HA's feature order stable, independent of authoring/UI order.
        out['supported_features'] = [role for role in ('start', 'stop', 'pause', 'return_home') if role in controls]
        if 'state' in model['default']:
            out['supported_features'].append('status')
        out['supported_features'] += [role for role in ('locate', 'clean_spot', 'fan_speed', 'send_command') if role in controls]
        if 'fan_speed' in controls:
            command('fan_speed', 'set_fan_speed_topic')
            options('fan_speed', 'fan_speed_list')
        if 'send_command' in controls:
            command('send_command', 'send_command_topic')
        for role in ('start', 'stop', 'pause', 'return_home', 'locate', 'clean_spot'):
            wire = 'return_to_base' if role == 'return_home' else role
            optional_payload(role, 'payload_' + wire, wire)
    elif name == 'lawn_mower':
        state('activity_state_topic')
        for role, topic in [('start', 'start_mowing_command_topic'), ('pause', 'pause_command_topic'), ('dock', 'dock_command_topic')]:
            if role in controls:
                command(role, topic)
                # HA's untemplated payloads are start_mowing/pause/dock, whereas
                # the shared action contract uses START/PAUSE/DOCK.
                out[topic.replace('_topic', '_template')] = literal_template(controls[role]['payload'])
    elif name == 'water_heater':
        property('temperature', 'temperature_command_topic', 'temperature_state_topic', 'temperature_state_template')
        if 'current_temperature' in model['default']:
            state('current_temperature_topic', 'current_temperature_template', 'current_temperature', optional=True)
        if 'mode' in controls:
            property('mode', 'mode_command_topic', 'mode_state_topic', 'mode_state_template')
            options('mode', 'modes')
        out.update(min_temp=controls['temperature']['min'], max_temp=controls['temperature']['max'], temperature_unit='C')
    else:
        raise ValueError('Missing HA preset adapter: ' + name)
    return out
