// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "ArdPortalFeatures.h"
struct ArdHaSpec { const char* name; const char* json; const char* state; };
static constexpr size_t ARD_HA_SPEC_COUNT = ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR + ARDPORTAL_ENABLE_CONTROL_DATE + ARDPORTAL_ENABLE_CONTROL_TIME + ARDPORTAL_ENABLE_CONTROL_DATETIME + ARDPORTAL_ENABLE_CONTROL_BUTTON + ARDPORTAL_ENABLE_CONTROL_SCENE + ARDPORTAL_ENABLE_CONTROL_NOTIFY + ARDPORTAL_ENABLE_CONTROL_INFRARED + ARDPORTAL_ENABLE_CONTROL_EVENT + ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER + ARDPORTAL_ENABLE_CONTROL_TAG + ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER + ARDPORTAL_ENABLE_CONTROL_LIGHT + ARDPORTAL_ENABLE_CONTROL_FAN + ARDPORTAL_ENABLE_CONTROL_COVER + ARDPORTAL_ENABLE_CONTROL_VALVE + ARDPORTAL_ENABLE_CONTROL_LOCK + ARDPORTAL_ENABLE_CONTROL_ALARM_CONTROL_PANEL + ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER + ARDPORTAL_ENABLE_CONTROL_SIREN + ARDPORTAL_ENABLE_CONTROL_VACUUM + ARDPORTAL_ENABLE_CONTROL_LAWN_MOWER + ARDPORTAL_ENABLE_CONTROL_WATER_HEATER + ARDPORTAL_ENABLE_CONTROL_UPDATE;
extern const ArdHaSpec ARD_HA_SPECS[ARD_HA_SPEC_COUNT + 1] PROGMEM;
#ifdef ARDPORTAL_DEFINE_HA_DATA
// Original MQTT descriptors. Hardware behavior remains application-owned.
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
static const char ARD_HA_NAME_1[] PROGMEM = "binary_sensor";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_1[] PROGMEM = R"SPEC({"default":false,"ha":{"state_topic":"$state","payload_on":"ON","payload_off":"OFF"},"controls":[],"readonly":true})SPEC";
#else
#define ARD_HA_SPEC_1 ARD_HA_STATE_1
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
static const char ARD_HA_NAME_4[] PROGMEM = "date";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_4[] PROGMEM = R"SPEC({"default":"2000-01-01","ha":{"state_topic":"$state","command_topic":"$command"},"controls":[{"key":"","type":"date","label":"ui_049","command":""}]})SPEC";
#else
#define ARD_HA_SPEC_4 ARD_HA_STATE_4
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
static const char ARD_HA_NAME_5[] PROGMEM = "time";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_5[] PROGMEM = R"SPEC({"default":"00:00:00","ha":{"state_topic":"$state","command_topic":"$command"},"controls":[{"key":"","type":"time","label":"ui_049","command":""}]})SPEC";
#else
#define ARD_HA_SPEC_5 ARD_HA_STATE_5
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
static const char ARD_HA_NAME_6[] PROGMEM = "datetime";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_6[] PROGMEM = R"SPEC({"default":"2000-01-01T00:00:00Z","ha":{"state_topic":"$state","command_topic":"$command"},"controls":[{"key":"","type":"datetime","label":"ui_049","command":""}]})SPEC";
#else
#define ARD_HA_SPEC_6 ARD_HA_STATE_6
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_BUTTON
static const char ARD_HA_NAME_7[] PROGMEM = "button";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_7[] PROGMEM = R"SPEC({"default":null,"ha":{"command_topic":"$command","payload_press":"PRESS"},"controls":[{"key":"","type":"action","label":"ui_107","command":"","payload":"PRESS"}],"action_only":true})SPEC";
#else
#define ARD_HA_SPEC_7 ARD_HA_STATE_7
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_SCENE
static const char ARD_HA_NAME_8[] PROGMEM = "scene";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_8[] PROGMEM = R"SPEC({"default":null,"ha":{"command_topic":"$command","payload_on":"ON"},"controls":[{"key":"","type":"action","label":"ui_108","command":"","payload":"ON"}],"action_only":true})SPEC";
#else
#define ARD_HA_SPEC_8 ARD_HA_STATE_8
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
static const char ARD_HA_NAME_9[] PROGMEM = "notify";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_9[] PROGMEM = R"SPEC({"default":null,"ha":{"command_topic":"$command"},"controls":[{"key":"","type":"action_text","label":"ui_109","command":""}],"action_only":true})SPEC";
#else
#define ARD_HA_SPEC_9 ARD_HA_STATE_9
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
static const char ARD_HA_NAME_10[] PROGMEM = "infrared";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_10[] PROGMEM = R"SPEC({"default":null,"ha":{"schema":"emitter","command_topic":"$command"},"controls":[{"key":"","type":"action_json","label":"ui_110","command":""}],"action_only":true})SPEC";
#else
#define ARD_HA_SPEC_10 ARD_HA_STATE_10
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
static const char ARD_HA_NAME_11[] PROGMEM = "event";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_11[] PROGMEM = R"SPEC({"default":{"event_type":"press"},"ha":{"state_topic":"$state","event_types":["press","double_press","long_press"]},"controls":[],"readonly":true,"event_only":true})SPEC";
#else
#define ARD_HA_SPEC_11 ARD_HA_STATE_11
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
static const char ARD_HA_NAME_12[] PROGMEM = "device_trigger";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_12[] PROGMEM = R"SPEC({"default":"PRESS","ha":{"automation_type":"trigger","topic":"$state","type":"button_short_press","subtype":"button_1","payload":"PRESS"},"controls":[],"readonly":true,"event_only":true})SPEC";
#else
#define ARD_HA_SPEC_12 ARD_HA_STATE_12
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
static const char ARD_HA_NAME_13[] PROGMEM = "tag";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_13[] PROGMEM = R"SPEC({"default":"","ha":{"topic":"$state"},"controls":[],"readonly":true,"event_only":true})SPEC";
#else
#define ARD_HA_SPEC_13 ARD_HA_STATE_13
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER
static const char ARD_HA_NAME_16[] PROGMEM = "device_tracker";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_16[] PROGMEM = R"SPEC({"default":"not_home","ha":{"state_topic":"$state","payload_home":"home","payload_not_home":"not_home"},"controls":[],"readonly":true})SPEC";
#else
#define ARD_HA_SPEC_16 ARD_HA_STATE_16
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
static const char ARD_HA_NAME_17[] PROGMEM = "light";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_17[] PROGMEM = R"SPEC({"default":{"state":"OFF","brightness":128,"color":{"r":255,"g":255,"b":255},"color_mode":"rgb"},"ha":{"schema":"json","command_topic":"$command","state_topic":"$state","brightness":true,"supported_color_modes":["rgb"]},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"brightness","type":"slider","label":"ui_112","command":"","min":0,"max":255,"step":1},{"key":"color","type":"color","label":"ui_113","command":""}],"json_command":true})SPEC";
#else
#define ARD_HA_SPEC_17 ARD_HA_STATE_17
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
static const char ARD_HA_NAME_18[] PROGMEM = "fan";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_18[] PROGMEM = R"SPEC({"default":{"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"},"ha":{"command_topic":"$command","state_topic":"$state","state_value_template":"{{ value_json.state }}","percentage_command_topic":"$command:percentage","percentage_state_topic":"$state","percentage_value_template":"{{ value_json.percentage }}","oscillation_command_topic":"$command:oscillation","oscillation_state_topic":"$state","oscillation_value_template":"{{ value_json.oscillation }}","direction_command_topic":"$command:direction","direction_state_topic":"$state","direction_value_template":"{{ value_json.direction }}"},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"percentage","type":"slider","label":"ui_114","command":"percentage","min":0,"max":100,"step":1},{"key":"oscillation","type":"switch","label":"ui_115","command":"oscillation","on":"oscillate_on","off":"oscillate_off"},{"key":"direction","type":"select","label":"ui_116","command":"direction","options":["forward","reverse"]}]})SPEC";
#else
#define ARD_HA_SPEC_18 ARD_HA_STATE_18
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
static const char ARD_HA_NAME_19[] PROGMEM = "cover";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_19[] PROGMEM = R"SPEC({"default":{"state":"closed","position":0,"tilt":0},"ha":{"command_topic":"$command","state_topic":"$state","value_template":"{{ value_json.state }}","position_topic":"$state","position_template":"{{ value_json.position }}","set_position_topic":"$command:position","tilt_command_topic":"$command:tilt","tilt_status_topic":"$state","tilt_status_template":"{{ value_json.tilt }}"},"controls":[{"key":"","type":"action","label":"ui_117","command":"","payload":"OPEN"},{"key":"","type":"action","label":"ui_118","command":"","payload":"CLOSE"},{"key":"","type":"action","label":"ui_119","command":"","payload":"STOP"},{"key":"position","type":"slider","label":"ui_120","command":"position","min":0,"max":100,"step":1},{"key":"tilt","type":"slider","label":"ui_121","command":"tilt","min":0,"max":100,"step":1}]})SPEC";
#else
#define ARD_HA_SPEC_19 ARD_HA_STATE_19
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_VALVE
static const char ARD_HA_NAME_20[] PROGMEM = "valve";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_20[] PROGMEM = R"SPEC({"default":{"state":"closed","position":0},"ha":{"command_topic":"$command","state_topic":"$state","reports_position":true},"controls":[{"key":"","type":"action","label":"ui_117","command":"","payload":"100"},{"key":"","type":"action","label":"ui_118","command":"","payload":"0"},{"key":"position","type":"slider","label":"ui_120","command":"","min":0,"max":100,"step":1}]})SPEC";
#else
#define ARD_HA_SPEC_20 ARD_HA_STATE_20
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_LOCK
static const char ARD_HA_NAME_21[] PROGMEM = "lock";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_21[] PROGMEM = R"SPEC({"default":"LOCKED","ha":{"command_topic":"$command","state_topic":"$state","payload_lock":"LOCK","payload_unlock":"UNLOCK","payload_open":"OPEN"},"controls":[{"key":"","type":"action","label":"ui_122","command":"","payload":"LOCK"},{"key":"","type":"action","label":"ui_123","command":"","payload":"UNLOCK"},{"key":"","type":"action","label":"ui_117","command":"","payload":"OPEN"}]})SPEC";
#else
#define ARD_HA_SPEC_21 ARD_HA_STATE_21
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_ALARM_CONTROL_PANEL
static const char ARD_HA_NAME_22[] PROGMEM = "alarm_control_panel";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_22[] PROGMEM = R"SPEC({"default":"disarmed","ha":{"command_topic":"$command","state_topic":"$state","code_arm_required":false,"code_disarm_required":false,"code_trigger_required":false},"controls":[{"key":"","type":"action","label":"ui_124","command":"","payload":"ARM_HOME"},{"key":"","type":"action","label":"ui_125","command":"","payload":"ARM_AWAY"},{"key":"","type":"action","label":"ui_126","command":"","payload":"ARM_NIGHT"},{"key":"","type":"action","label":"ui_127","command":"","payload":"DISARM"},{"key":"","type":"action","label":"ui_128","command":"","payload":"TRIGGER"}]})SPEC";
#else
#define ARD_HA_SPEC_22 ARD_HA_STATE_22
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER
static const char ARD_HA_NAME_23[] PROGMEM = "humidifier";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_23[] PROGMEM = R"SPEC({"default":{"state":"OFF","humidity":50,"current_humidity":null,"mode":"normal"},"ha":{"command_topic":"$command","state_topic":"$state","state_value_template":"{{ value_json.state }}","target_humidity_command_topic":"$command:humidity","target_humidity_state_topic":"$state","target_humidity_state_template":"{{ value_json.humidity }}","current_humidity_topic":"$state","current_humidity_template":"{{ value_json.current_humidity | default('', true) }}","mode_command_topic":"$command:mode","mode_state_topic":"$state","mode_state_template":"{{ value_json.mode }}","modes":["normal","eco"]},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"humidity","type":"slider","label":"ui_129","command":"humidity","min":0,"max":100,"step":1},{"key":"mode","type":"select","label":"ui_103","command":"mode","options":["normal","eco"]}]})SPEC";
#else
#define ARD_HA_SPEC_23 ARD_HA_STATE_23
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_SIREN
static const char ARD_HA_NAME_24[] PROGMEM = "siren";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_24[] PROGMEM = R"SPEC({"default":{"state":"OFF","tone":"alarm","duration":10,"volume_level":0.5},"ha":{"command_topic":"$command","state_topic":"$state","available_tones":["alarm","bell"],"support_duration":true,"support_volume_set":true,"support_turn_on":true,"support_turn_off":true,"support_tones":true},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"tone","type":"select","label":"ui_130","command":"","options":["alarm","bell"]},{"key":"duration","type":"slider","label":"ui_131","command":"","min":1,"max":300,"step":1},{"key":"volume_level","type":"slider","label":"ui_132","command":"","min":0,"max":1,"step":0.1}],"json_command":true})SPEC";
#else
#define ARD_HA_SPEC_24 ARD_HA_STATE_24
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_VACUUM
static const char ARD_HA_NAME_25[] PROGMEM = "vacuum";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_25[] PROGMEM = R"SPEC({"default":{"state":"docked","fan_speed":"normal"},"ha":{"command_topic":"$command","state_topic":"$state","supported_features":["start","stop","pause","return_home","status","locate","clean_spot","fan_speed","send_command"],"set_fan_speed_topic":"$command:fan","fan_speed_list":["quiet","normal","turbo"],"send_command_topic":"$command:custom"},"controls":[{"key":"","type":"action","label":"ui_133","command":"","payload":"start"},{"key":"","type":"action","label":"ui_119","command":"","payload":"stop"},{"key":"","type":"action","label":"ui_134","command":"","payload":"pause"},{"key":"","type":"action","label":"ui_135","command":"","payload":"return_to_base"},{"key":"","type":"action","label":"ui_136","command":"","payload":"locate"},{"key":"","type":"action","label":"ui_137","command":"","payload":"clean_spot"},{"key":"fan_speed","type":"select","label":"ui_104","command":"fan","options":["quiet","normal","turbo"]},{"key":"","type":"action_json","label":"ui_138","command":"custom"}]})SPEC";
#else
#define ARD_HA_SPEC_25 ARD_HA_STATE_25
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_LAWN_MOWER
static const char ARD_HA_NAME_26[] PROGMEM = "lawn_mower";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_26[] PROGMEM = R"SPEC({"default":"docked","ha":{"activity_state_topic":"$state","start_mowing_command_topic":"$command:start","pause_command_topic":"$command:pause","dock_command_topic":"$command:dock"},"controls":[{"key":"","type":"action","label":"ui_139","command":"start","payload":"START"},{"key":"","type":"action","label":"ui_134","command":"pause","payload":"PAUSE"},{"key":"","type":"action","label":"ui_135","command":"dock","payload":"DOCK"}]})SPEC";
#else
#define ARD_HA_SPEC_26 ARD_HA_STATE_26
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_WATER_HEATER
static const char ARD_HA_NAME_27[] PROGMEM = "water_heater";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_27[] PROGMEM = R"SPEC({"default":{"temperature":50,"current_temperature":null,"mode":"off"},"ha":{"temperature_command_topic":"$command:temperature","temperature_state_topic":"$state","temperature_state_template":"{{ value_json.temperature }}","current_temperature_topic":"$state","current_temperature_template":"{{ value_json.current_temperature | default('', true) }}","mode_command_topic":"$command:mode","mode_state_topic":"$state","mode_state_template":"{{ value_json.mode }}","modes":["off","eco","electric","performance","heat_pump","high_demand"],"min_temp":30,"max_temp":80,"temperature_unit":"C"},"controls":[{"key":"temperature","type":"slider","label":"ui_101","command":"temperature","min":30,"max":80,"step":1},{"key":"mode","type":"select","label":"ui_103","command":"mode","options":["off","eco","electric","performance","heat_pump","high_demand"]}]})SPEC";
#else
#define ARD_HA_SPEC_27 ARD_HA_STATE_27
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_UPDATE
static const char ARD_HA_NAME_28[] PROGMEM = "update";
#if ARDPORTAL_ENABLE_HA
static const char ARD_HA_SPEC_28[] PROGMEM = R"SPEC({"default":{"installed_version":"0.0.0","latest_version":"0.0.0","in_progress":false},"ha":{"state_topic":"$state","command_topic":"$command","payload_install":"INSTALL"},"controls":[{"key":"","type":"action","label":"ui_140","command":"","payload":"INSTALL"}]})SPEC";
#else
#define ARD_HA_SPEC_28 ARD_HA_STATE_28
#endif
#endif
// BEGIN GENERATED STATE SCHEMAS
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
static const char ARD_HA_STATE_1[] PROGMEM = R"STATE({"default":false,"ha":{},"controls":[],"readonly":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
static const char ARD_HA_STATE_4[] PROGMEM = R"STATE({"default":"2000-01-01","ha":{},"controls":[{"key":"","type":"date","label":"ui_049","command":""}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
static const char ARD_HA_STATE_5[] PROGMEM = R"STATE({"default":"00:00:00","ha":{},"controls":[{"key":"","type":"time","label":"ui_049","command":""}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
static const char ARD_HA_STATE_6[] PROGMEM = R"STATE({"default":"2000-01-01T00:00:00Z","ha":{},"controls":[{"key":"","type":"datetime","label":"ui_049","command":""}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_BUTTON
static const char ARD_HA_STATE_7[] PROGMEM = R"STATE({"default":null,"ha":{},"controls":[{"key":"","type":"action","label":"ui_107","command":"","payload":"PRESS"}],"action_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_SCENE
static const char ARD_HA_STATE_8[] PROGMEM = R"STATE({"default":null,"ha":{},"controls":[{"key":"","type":"action","label":"ui_108","command":"","payload":"ON"}],"action_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
static const char ARD_HA_STATE_9[] PROGMEM = R"STATE({"default":null,"ha":{},"controls":[{"key":"","type":"action_text","label":"ui_109","command":""}],"action_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
static const char ARD_HA_STATE_10[] PROGMEM = R"STATE({"default":null,"ha":{},"controls":[{"key":"","type":"action_json","label":"ui_110","command":""}],"action_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
static const char ARD_HA_STATE_11[] PROGMEM = R"STATE({"default":{"event_type":"press"},"ha":{"event_types":["press","double_press","long_press"]},"controls":[],"readonly":true,"event_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
static const char ARD_HA_STATE_12[] PROGMEM = R"STATE({"default":"PRESS","ha":{},"controls":[],"readonly":true,"event_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
static const char ARD_HA_STATE_13[] PROGMEM = R"STATE({"default":"","ha":{},"controls":[],"readonly":true,"event_only":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER
static const char ARD_HA_STATE_16[] PROGMEM = R"STATE({"default":"not_home","ha":{},"controls":[],"readonly":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
static const char ARD_HA_STATE_17[] PROGMEM = R"STATE({"default":{"state":"OFF","brightness":128,"color":{"r":255,"g":255,"b":255},"color_mode":"rgb"},"ha":{},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"brightness","type":"slider","label":"ui_112","command":"","min":0,"max":255,"step":1},{"key":"color","type":"color","label":"ui_113","command":""}],"json_command":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
static const char ARD_HA_STATE_18[] PROGMEM = R"STATE({"default":{"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"},"ha":{},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"percentage","type":"slider","label":"ui_114","command":"percentage","min":0,"max":100,"step":1},{"key":"oscillation","type":"switch","label":"ui_115","command":"oscillation","on":"oscillate_on","off":"oscillate_off"},{"key":"direction","type":"select","label":"ui_116","command":"direction","options":["forward","reverse"]}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
static const char ARD_HA_STATE_19[] PROGMEM = R"STATE({"default":{"state":"closed","position":0,"tilt":0},"ha":{},"controls":[{"key":"","type":"action","label":"ui_117","command":"","payload":"OPEN"},{"key":"","type":"action","label":"ui_118","command":"","payload":"CLOSE"},{"key":"","type":"action","label":"ui_119","command":"","payload":"STOP"},{"key":"position","type":"slider","label":"ui_120","command":"position","min":0,"max":100,"step":1},{"key":"tilt","type":"slider","label":"ui_121","command":"tilt","min":0,"max":100,"step":1}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_VALVE
static const char ARD_HA_STATE_20[] PROGMEM = R"STATE({"default":{"state":"closed","position":0},"ha":{},"controls":[{"key":"","type":"action","label":"ui_117","command":"","payload":"100"},{"key":"","type":"action","label":"ui_118","command":"","payload":"0"},{"key":"position","type":"slider","label":"ui_120","command":"","min":0,"max":100,"step":1}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LOCK
static const char ARD_HA_STATE_21[] PROGMEM = R"STATE({"default":"LOCKED","ha":{},"controls":[{"key":"","type":"action","label":"ui_122","command":"","payload":"LOCK"},{"key":"","type":"action","label":"ui_123","command":"","payload":"UNLOCK"},{"key":"","type":"action","label":"ui_117","command":"","payload":"OPEN"}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_ALARM_CONTROL_PANEL
static const char ARD_HA_STATE_22[] PROGMEM = R"STATE({"default":"disarmed","ha":{},"controls":[{"key":"","type":"action","label":"ui_124","command":"","payload":"ARM_HOME"},{"key":"","type":"action","label":"ui_125","command":"","payload":"ARM_AWAY"},{"key":"","type":"action","label":"ui_126","command":"","payload":"ARM_NIGHT"},{"key":"","type":"action","label":"ui_127","command":"","payload":"DISARM"},{"key":"","type":"action","label":"ui_128","command":"","payload":"TRIGGER"}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER
static const char ARD_HA_STATE_23[] PROGMEM = R"STATE({"default":{"state":"OFF","humidity":50,"current_humidity":null,"mode":"normal"},"ha":{},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"humidity","type":"slider","label":"ui_129","command":"humidity","min":0,"max":100,"step":1},{"key":"mode","type":"select","label":"ui_103","command":"mode","options":["normal","eco"]}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_SIREN
static const char ARD_HA_STATE_24[] PROGMEM = R"STATE({"default":{"state":"OFF","tone":"alarm","duration":10,"volume_level":0.5},"ha":{},"controls":[{"key":"state","type":"switch","label":"ui_111","command":"","on":"ON","off":"OFF"},{"key":"tone","type":"select","label":"ui_130","command":"","options":["alarm","bell"]},{"key":"duration","type":"slider","label":"ui_131","command":"","min":1,"max":300,"step":1},{"key":"volume_level","type":"slider","label":"ui_132","command":"","min":0,"max":1,"step":0.1}],"json_command":true})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_VACUUM
static const char ARD_HA_STATE_25[] PROGMEM = R"STATE({"default":{"state":"docked","fan_speed":"normal"},"ha":{},"controls":[{"key":"","type":"action","label":"ui_133","command":"","payload":"start"},{"key":"","type":"action","label":"ui_119","command":"","payload":"stop"},{"key":"","type":"action","label":"ui_134","command":"","payload":"pause"},{"key":"","type":"action","label":"ui_135","command":"","payload":"return_to_base"},{"key":"","type":"action","label":"ui_136","command":"","payload":"locate"},{"key":"","type":"action","label":"ui_137","command":"","payload":"clean_spot"},{"key":"fan_speed","type":"select","label":"ui_104","command":"fan","options":["quiet","normal","turbo"]},{"key":"","type":"action_json","label":"ui_138","command":"custom"}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LAWN_MOWER
static const char ARD_HA_STATE_26[] PROGMEM = R"STATE({"default":"docked","ha":{},"controls":[{"key":"","type":"action","label":"ui_139","command":"start","payload":"START"},{"key":"","type":"action","label":"ui_134","command":"pause","payload":"PAUSE"},{"key":"","type":"action","label":"ui_135","command":"dock","payload":"DOCK"}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_WATER_HEATER
static const char ARD_HA_STATE_27[] PROGMEM = R"STATE({"default":{"temperature":50,"current_temperature":null,"mode":"off"},"ha":{},"controls":[{"key":"temperature","type":"slider","label":"ui_101","command":"temperature","min":30,"max":80,"step":1},{"key":"mode","type":"select","label":"ui_103","command":"mode","options":["off","eco","electric","performance","heat_pump","high_demand"]}]})STATE";
#endif
#if ARDPORTAL_ENABLE_CONTROL_UPDATE
static const char ARD_HA_STATE_28[] PROGMEM = R"STATE({"default":{"installed_version":"0.0.0","latest_version":"0.0.0","in_progress":false},"ha":{},"controls":[{"key":"","type":"action","label":"ui_140","command":"","payload":"INSTALL"}]})STATE";
#endif
// END GENERATED STATE SCHEMAS
const ArdHaSpec ARD_HA_SPECS[ARD_HA_SPEC_COUNT + 1] PROGMEM = {
  
  
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
  {ARD_HA_NAME_1,ARD_HA_SPEC_1,ARD_HA_STATE_1},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_DATE
  {ARD_HA_NAME_4,ARD_HA_SPEC_4,ARD_HA_STATE_4},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_TIME
  {ARD_HA_NAME_5,ARD_HA_SPEC_5,ARD_HA_STATE_5},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
  {ARD_HA_NAME_6,ARD_HA_SPEC_6,ARD_HA_STATE_6},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_BUTTON
  {ARD_HA_NAME_7,ARD_HA_SPEC_7,ARD_HA_STATE_7},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_SCENE
  {ARD_HA_NAME_8,ARD_HA_SPEC_8,ARD_HA_STATE_8},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
  {ARD_HA_NAME_9,ARD_HA_SPEC_9,ARD_HA_STATE_9},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
  {ARD_HA_NAME_10,ARD_HA_SPEC_10,ARD_HA_STATE_10},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  {ARD_HA_NAME_11,ARD_HA_SPEC_11,ARD_HA_STATE_11},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  {ARD_HA_NAME_12,ARD_HA_SPEC_12,ARD_HA_STATE_12},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_TAG
  {ARD_HA_NAME_13,ARD_HA_SPEC_13,ARD_HA_STATE_13},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER
  {ARD_HA_NAME_16,ARD_HA_SPEC_16,ARD_HA_STATE_16},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  {ARD_HA_NAME_17,ARD_HA_SPEC_17,ARD_HA_STATE_17},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_FAN
  {ARD_HA_NAME_18,ARD_HA_SPEC_18,ARD_HA_STATE_18},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_COVER
  {ARD_HA_NAME_19,ARD_HA_SPEC_19,ARD_HA_STATE_19},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_VALVE
  {ARD_HA_NAME_20,ARD_HA_SPEC_20,ARD_HA_STATE_20},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_LOCK
  {ARD_HA_NAME_21,ARD_HA_SPEC_21,ARD_HA_STATE_21},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_ALARM_CONTROL_PANEL
  {ARD_HA_NAME_22,ARD_HA_SPEC_22,ARD_HA_STATE_22},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER
  {ARD_HA_NAME_23,ARD_HA_SPEC_23,ARD_HA_STATE_23},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_SIREN
  {ARD_HA_NAME_24,ARD_HA_SPEC_24,ARD_HA_STATE_24},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_VACUUM
  {ARD_HA_NAME_25,ARD_HA_SPEC_25,ARD_HA_STATE_25},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_LAWN_MOWER
  {ARD_HA_NAME_26,ARD_HA_SPEC_26,ARD_HA_STATE_26},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_WATER_HEATER
  {ARD_HA_NAME_27,ARD_HA_SPEC_27,ARD_HA_STATE_27},
#endif
  
#if ARDPORTAL_ENABLE_CONTROL_UPDATE
  {ARD_HA_NAME_28,ARD_HA_SPEC_28,ARD_HA_STATE_28},
#endif
  {nullptr,nullptr,nullptr}
};

#endif // ARDPORTAL_DEFINE_HA_DATA
