#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Check protocol compilation independently of the firmware implementation."""
import json
import sys
import unittest
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src/tools'))
from control_protocols import ha_defaults, runtime_model

MODELS = json.loads((ROOT / 'src/tools/control_models.json').read_text())
# Frozen public wire contract, not input to the production generator.
EXPECTED = json.loads((ROOT / 'tests/fixtures/ha-control-presets.json').read_text())


class ProtocolCompilation(unittest.TestCase):
    def test_switch_payloads_follow_shared_values(self):
        for name, role, prefix in [('fan', 'state', 'payload_'), ('humidifier', 'state', 'payload_'),
                                    ('fan', 'oscillation', 'payload_oscillation_')]:
            with self.subTest(control=name, role=role):
                model = deepcopy(MODELS[name])
                model['controls'][role].update(on='RUN', off='STOP')
                ha = ha_defaults(name, model)
                self.assertEqual((ha[prefix + 'on'], ha[prefix + 'off']), ('RUN', 'STOP'))
                model['controls'][role].pop('off')
                with self.assertRaises(ValueError):
                    ha_defaults(name, model)
                model['controls'][role]['off'] = 'RUN'
                with self.assertRaises(ValueError):
                    ha_defaults(name, model)
                model['controls'][role].pop('on')
                model['controls'][role].pop('off')
                with self.assertRaises(ValueError):
                    ha_defaults(name, model)

    def test_presence_vocabulary_is_independent_of_initial_state(self):
        model = deepcopy(MODELS['device_tracker'])
        for initial in ('home', 'not_home', 'office'):
            model['default'] = initial
            self.assertEqual(ha_defaults('device_tracker', model), EXPECTED['device_tracker'])
            self.assertEqual(runtime_model(model)['default'], initial)
        model['states'].update(present='PRESENT', absent='AWAY')
        ha = ha_defaults('device_tracker', model)
        self.assertEqual((ha['payload_home'], ha['payload_not_home']), ('PRESENT', 'AWAY'))
        self.assertNotIn('states', runtime_model(model))
        model['states']['absent'] = 'PRESENT'
        with self.assertRaises(ValueError):
            ha_defaults('device_tracker', model)

    def test_existing_wire_contract(self):
        self.assertEqual(MODELS.keys(), EXPECTED.keys())
        for name, model in MODELS.items():
            with self.subTest(control=name):
                self.assertEqual(ha_defaults(name, model), EXPECTED[name])
                # A protocol adapter must not modify the shared model.
                original = deepcopy(model)
                ha_defaults(name, model)
                self.assertEqual(model, original)

    def test_shared_catalogue_has_no_protocol_fields(self):
        def check(value):
            if isinstance(value, dict):
                for key, child in value.items():
                    self.assertNotIn(key, ('ha', 'mqtt', 'zigbee', 'matter', 'model'))
                    self.assertFalse(key.endswith(('_topic', '_template')))
                    check(child)
            elif isinstance(value, list):
                for child in value:
                    check(child)
        check(MODELS)

    def test_payload_and_route_follow_the_action(self):
        for name, role, payload_key in [('button', 'press', 'payload_press'), ('scene', 'activate', 'payload_on'),
                                        ('lock', 'open', 'payload_open'), ('update', 'install', 'payload_install')]:
            model = deepcopy(MODELS[name])
            model['controls'][role]['payload'] = 'CUSTOM'
            self.assertEqual(ha_defaults(name, model)[payload_key], 'CUSTOM')
        model = deepcopy(MODELS['button'])
        model['controls']['press']['command'] = 'calibrate'
        self.assertEqual(ha_defaults('button', model)['command_topic'], '$command:calibrate')
        model = deepcopy(MODELS['device_trigger'])
        model['default'] = 'CUSTOM_EVENT'
        self.assertEqual(ha_defaults('device_trigger', model)['payload'], 'CUSTOM_EVENT')
        for name, role, key in [('cover', 'open', 'payload_open'), ('alarm_control_panel', 'arm_home', 'payload_arm_home'),
                                 ('vacuum', 'return_home', 'payload_return_to_base')]:
            model = deepcopy(MODELS[name])
            model['controls'][role]['payload'] = 'CUSTOM'
            self.assertEqual(ha_defaults(name, model)[key], 'CUSTOM')
        model = deepcopy(MODELS['lawn_mower'])
        model['controls']['start']['payload'] = 'CUSTOM'
        self.assertEqual(ha_defaults('lawn_mower', model)['start_mowing_command_template'], 'CUSTOM')
        model['controls']['start']['payload'] = '{{ user_literal }}'
        self.assertEqual(ha_defaults('lawn_mower', model)['start_mowing_command_template'], '{{ "{{ user_literal }}" }}')

    def test_properties_options_and_bounds_have_one_source(self):
        model = deepcopy(MODELS['water_heater'])
        control = model['controls']['temperature']
        control.update(key='setpoint', command='setpoint', min=5, max=95)
        model['controls']['mode']['options'] = ['off', 'eco']
        ha = ha_defaults('water_heater', model)
        self.assertEqual(ha['temperature_command_topic'], '$command:setpoint')
        self.assertEqual(ha['temperature_state_template'], '{{ value_json.setpoint }}')
        self.assertEqual((ha['min_temp'], ha['max_temp']), (5, 95))
        self.assertEqual(ha['modes'], ['off', 'eco'])
        model = deepcopy(MODELS['fan'])
        model['controls']['percentage']['command'] = ''
        self.assertEqual(ha_defaults('fan', model)['percentage_command_topic'], '$command')

    def test_capabilities_follow_available_controls(self):
        for name, roles, absent in [
            ('light', ['brightness', 'color'], ['brightness', 'supported_color_modes']),
            ('fan', ['oscillation'], ['oscillation_command_topic', 'oscillation_state_topic', 'oscillation_value_template']),
            ('cover', ['tilt'], ['tilt_command_topic', 'tilt_status_topic', 'tilt_status_template']),
            ('humidifier', ['mode'], ['mode_command_topic', 'mode_state_topic', 'mode_state_template', 'modes']),
            ('vacuum', ['fan_speed', 'send_command'], ['set_fan_speed_topic', 'fan_speed_list', 'send_command_topic']),
        ]:
            model = deepcopy(MODELS[name])
            for role in roles:
                del model['controls'][role]
            ha = ha_defaults(name, model)
            for key in absent:
                self.assertNotIn(key, ha)
            if name == 'vacuum':
                self.assertNotIn('fan_speed', ha['supported_features'])
                self.assertNotIn('send_command', ha['supported_features'])
        model = deepcopy(MODELS['siren'])
        del model['controls']['tone']
        self.assertFalse(ha_defaults('siren', model)['support_tones'])

    def test_semantic_names_do_not_depend_on_ui_order_or_enter_firmware(self):
        for name, model in MODELS.items():
            reordered = deepcopy(model)
            reordered['controls'] = dict(reversed(list(model['controls'].items())))
            self.assertEqual(ha_defaults(name, model), ha_defaults(name, reordered))
            self.assertEqual(runtime_model(model)['controls'], list(model['controls'].values()))
            runtime_model(model)['controls'].append({'key': 'extra'})
            self.assertEqual(len(runtime_model(model)['controls']), len(model['controls']))
        with self.assertRaises(ValueError):
            ha_defaults('unknown_control', {'controls': {}})


if __name__ == '__main__':
    unittest.main()
