#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Test the generated Python module against installed zha/zigpy/zha-quirks APIs."""
import ast
import asyncio
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock


def main():
    """Compile/register a generated fixture and verify metadata and wire conversions."""
    root = Path(__file__).resolve().parent.parent
    tail = root / 'src/ZhaQuirkAssets.h'
    fragment = tail.read_text().split('R"PY(', 1)[1].split(')PY";', 1)[0]
    fields = [
        dict(kind='switch', ep=2, endpoint='relay_ep2', label='Relay'),
        dict(kind='select', ep=3, endpoint='mode_ep3', label='Mode', options=['off', 'eco', 'comfort', '__init__', 'a_b', 'a b']),
        dict(kind='state', ep=4, endpoint='presence_ep4', label='Presence', options=['not_home', 'home']),
        dict(kind='slider', ep=5, endpoint='value_ep5', label='Value', min=-10, max=80, step=0.25),
        dict(kind='climate', ep=6, endpoint='climate_ep6', label='Climate', min=5, max=45, step=0.25, modes=['off', 'heat', 'cool']),
        dict(kind='action', ep=7, endpoint='button_ep7', label='Button'),
    ]
    for ep, kind in enumerate(['number', 'temperature', 'humidity', 'pressure', 'illuminance', 'flow', 'wind_speed', 'carbon_dioxide', 'pm25', 'voltage', 'current', 'power', 'binary', 'light', 'cover', 'lock', 'siren', 'update'], 8):
        fields.append(dict(kind=kind, ep=ep, endpoint=f'{kind}_ep{ep}', label=kind))
    model = 'ArdPortal-0123456789abcdef'
    prefix = 'import json\nMODEL = ' + repr(model) + '\nFIELDS = [\n'
    body = ',\n'.join('json.loads(' + repr(json.dumps(field, ensure_ascii=False)) + ')' for field in fields)
    program = prefix + body + '\n' + fragment
    ast.parse(program)
    namespace = {'__name__': 'ardportal_test_quirk', '__file__': str(Path('/private/tmp/ardportal_test_quirk.py'))}
    exec(compile(program, namespace['__file__'], 'exec'), namespace)
    builder = namespace['builder']
    entry = namespace['ARDPORTAL_QUIRK_ENTRY']
    assert tuple(entry.device_match.applies_to)[0] == ('ArdPortal', model)
    assert len(builder.entity_metadata) > 10
    # Native ZHA entities already have generic translated names (e.g. "Light").
    # Override their translation key too, otherwise it takes priority over our label.
    from zhaquirks.builder.device import QuirkV2Device
    from zha.application import Platform
    for field in fields:
        if field['kind'] not in ('switch', 'light', 'climate', 'cover', 'lock', 'siren'):
            continue
        entity = SimpleNamespace(PLATFORM=Platform(field['kind']), endpoint=SimpleNamespace(id=field['ep']),
                                 targets_cluster=lambda *args, **kwargs: True,
                                 _attr_translation_key='light', _attr_fallback_name=None)
        QuirkV2Device._apply_entity_metadata_changes(
            SimpleNamespace(_quirk_definition=SimpleNamespace(
                changed_entity_metadata=builder.changed_entity_metadata)), entity)
        assert entity._attr_translation_key == 'ardportal_' + field['endpoint']
        assert entity._attr_fallback_name == field['label']
    removal_device = SimpleNamespace(_quirk_definition=SimpleNamespace(
        disabled_default_entities=builder.disabled_default_entities))
    for metadata in builder.entity_metadata:
        ep, cluster = metadata.endpoint_id, metadata.cluster_id
        entity = SimpleNamespace(PLATFORM=Platform(metadata.entity_platform.value),
                                 endpoint=SimpleNamespace(id=ep),
                                 targets_cluster=lambda *args: True,
                                 unique_id=f'test-{ep}-{cluster}-{metadata.unique_id_suffix}')
        assert not QuirkV2Device._is_entity_removed_by_quirk(removal_device, entity)
        entity.unique_id = f'test-{ep}-{cluster}'
        assert QuirkV2Device._is_entity_removed_by_quirk(removal_device, entity)
    choices = namespace['_choices'](fields[1])
    assert [choice.value for choice in choices] == list(range(1, 7))
    assert len(set(choice.name for choice in choices)) == 6
    assert choices(3).name == 'comfort'
    assert choices(4).name.replace('_', ' ') == '  init  '
    measurement = namespace['_measurement']
    assert measurement(2234, 'temperature') == 22.34
    assert measurement(-32768, 'temperature') is None
    assert measurement(65535, 'humidity') is None
    assert measurement(0.0004, 'carbon_dioxide') == 400
    assert measurement(float('nan'), 'pm25') is None
    from zigpy.device import Device
    from zigpy.types import EUI64, NWK
    device = Device(MagicMock(), EUI64.convert('44:bd:8d:fe:ff:c8:c9:34'), NWK(0x1234))
    device.manufacturer = 'ArdPortal';device.model = model
    assert builder.registry.match_entry(device) is entry
    second = Device(MagicMock(), EUI64.convert('44:bd:8d:fe:ff:c8:c9:35'), NWK(0x1235))
    second.manufacturer = 'ArdPortal';second.model = model
    assert builder.registry.match_entry(second) is entry
    second.model = 'ArdPortal-different'
    assert builder.registry.match_entry(second) is None
    endpoint = device.add_endpoint(6)
    operation = builder.replaces_ops[0]
    cluster = operation.cluster(endpoint, is_server=True)
    cluster._update_attribute(0x0000, 2234)
    assert cluster.get('local_temperature') == 2234
    cluster._update_attribute(0x0000, -32768)
    assert cluster.get('local_temperature') is None
    cluster._update_attribute(0x0012, 2250)
    assert cluster.get('occupied_heating_setpoint') == 2250
    async def limits():
        """Read local thermostat limits without sending a network request."""
        result, failed = await cluster.read_attributes(['min_heat_setpoint_limit', 'max_heat_setpoint_limit'])
        assert not failed and result == {'min_heat_setpoint_limit': 500, 'max_heat_setpoint_limit': 4500}
    asyncio.run(limits())
    from zigpy.zcl.clusters.general import MultistateOutput
    endpoint = device.add_endpoint(3)
    select = MultistateOutput(endpoint, is_server=True)
    select._write_attributes = AsyncMock(return_value=[[SimpleNamespace(status=0, attrid=None)]])
    async def select_write():
        """Verify the selected display option is written as its original one-based ordinal."""
        from zha.application.platforms.select import ZCLEnumSelectEntity
        entity = SimpleNamespace(_cluster=select, _attribute_name='present_value', _enum=choices,
                                 maybe_emit_state_changed_event=lambda: None)
        for ordinal, choice in enumerate(choices, 1):
            await ZCLEnumSelectEntity.async_select_option(entity, choice.name.replace('_', ' '))
            args = select._write_attributes.call_args.args[0]
            assert int(args[0].value.value) == ordinal
    asyncio.run(select_write())
    print('PASS: actual ZHA API registration, shared model match, named choices, measurements, thermostat limits and ordinal writes')


if __name__ == '__main__':
    main()
