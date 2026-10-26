// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageLiterals.h"
#include <esp_zigbee_attribute.h>
#include <esp_zigbee_cluster.h>
#include <ha/esp_zigbee_ha_standard.h>
#include <zcl/esp_zigbee_zcl_command.h>
#include "HeapMemory.h"
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
#include "ZigbeeOtaImpl.h"
#endif
/** @brief Format a scalar ZCL payload without allocating JSON or Strings. @param out Destination buffer. @param capacity Buffer bytes. @param data Borrowed scalar bytes. @param type ZCL data type. @param bytes Scalar size. @return No value. */
static void ardZigbeePayload(char* out, size_t capacity, const void* data, uint8_t type, size_t bytes) {
  uint32_t value = 0;
  memcpy(&value, data, std::min(bytes, sizeof(value)));
  if (type == 0x39 && bytes == 4) {
    float number;
    memcpy(&number, data, 4);
    snprintf(out, capacity, "%.7g", double(number));
  } else if (type == 0x29 && bytes == 2) snprintf(out, capacity, "%d", int(int16_t(value)));
  else if (type == 0x10) snprintf(out, capacity, "%s", value ? "true" : "false");
  else snprintf(out, capacity, "%lu", (unsigned long)value);
}
/** @brief Retain compact endpoint metadata without copying a page. @param spec Mapping. @param initial Initial attributes. @param label Description. @return True if staged. */
bool ArdZigbee::addControl(ArdZigbeeFields::Spec spec, const ArdZigbeeFields::Values& initial, const String& label) {
  if (_begun) {
    _error = ARD_LANG_NATIVE_ENDPOINTS_ARE_FROZEN_RESTART_REQUIRED;
    return false;
  }
  auto failed = [&](const char* reason) {
    _error = reason;
    setState(State::Failed);
    return false;
  };
  if (spec.kind == ArdZigbeeFields::Kind::None || _controlCount >= 239) return failed(ARD_LANG_NATIVE_UNSUPPORTED_MAPPING_OR_ZIGBEE_ENDPOINT_LIMIT_239);
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  if (spec.kind == ArdZigbeeFields::Kind::Update)
    for (auto* entry = _controls.get(); entry; entry = entry->next.get())
      if (entry->spec.kind == spec.kind) return failed(ARD_LANG_NATIVE_ONLY_ONE_UPDATE_FIELD_MAY_OWN_ZIGBEE_OTA);
#endif
  auto* raw = new (std::nothrow) Control();
  if (!raw) return failed(ARD_LANG_NATIVE_OUT_OF_MEMORY_ALLOCATING_ENDPOINT_METADATA);
  std::unique_ptr<Control> entry(raw);
  entry->spec = spec;
  entry->spec.endpoint = _controlCount + 2;
  entry->initialCount = initial.count;
  if (initial.count) {
    entry->initial.reset(static_cast<ArdZigbeeFields::Attribute*>(std::malloc(initial.count * sizeof(ArdZigbeeFields::Attribute))));
    if (!entry->initial) return failed(ARD_LANG_NATIVE_OUT_OF_MEMORY_STAGING_ENDPOINT_ATTRIBUTES);
    memcpy(entry->initial.get(), initial.attributes, initial.count * sizeof(ArdZigbeeFields::Attribute));
  }
  size_t labelBytes = std::min(size_t(63), label.length());
  if (labelBytes < label.length())
    while (labelBytes && (uint8_t(label[labelBytes]) & 0xc0) == 0x80) --labelBytes;
  if (labelBytes) {
    entry->label.reset(static_cast<char*>(std::malloc(labelBytes + 1)));
    if (!entry->label) return failed(ARD_LANG_NATIVE_OUT_OF_MEMORY_COPYING_ENDPOINT_LABEL);
    entry->label.get()[0] = char(labelBytes);
    memcpy(entry->label.get() + 1, label.c_str(), labelBytes);
  }
  auto* tail = &_controls;
  while (*tail) tail = &(*tail)->next;
  *tail = std::move(entry);
  ++_controlCount;
  return true;
}
/** @brief Create every staged standard server endpoint. @param endpoints SDK endpoint list. @return False on any SDK allocation/registration failure. */
bool ArdZigbee::createControls(esp_zb_ep_list_t* endpoints) {
  using namespace ArdZigbeeFields;
  for (auto* entry = _controls.get(); entry; entry = entry->next.get()) {
    auto stage = [&](const char* name) {
      snprintf(_startupError, sizeof(_startupError), ARD_LANG_NATIVE_ENDPOINT_U_KIND_U_S_FAILED, unsigned(entry->spec.endpoint), unsigned(entry->spec.kind), name);
    };
    stage(ARD_LANG_NATIVE_MEMORY_RESERVE_CLUSTER_ALLOCATION);
    if (!ArdHeap::permits(2048, 1024)) return false;
    auto* clusters = esp_zb_zcl_cluster_list_create();
    if (!clusters) return false;
    auto add = [&](esp_zb_attribute_list_t* list, auto function) {
      return list && function(clusters, list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) == ESP_OK;
    };
    auto* basic = esp_zb_basic_cluster_create(nullptr);
    static char manufacturer[] = "\x09"
                                 "ArdPortal";
    auto* model = ArdZigbeeIdentity::Model;
    stage("Basic/Identify");
    if (!basic || esp_zb_basic_cluster_add_attr(basic, 4, manufacturer) != ESP_OK || esp_zb_basic_cluster_add_attr(basic, 5, model) != ESP_OK || esp_zb_basic_cluster_add_attr(basic, 0x4000, const_cast<char*>(_firmwareVersion)) != ESP_OK || !add(basic, esp_zb_cluster_list_add_basic_cluster) || !add(esp_zb_identify_cluster_create(nullptr), esp_zb_cluster_list_add_identify_cluster)) return false;
    // Register the standard optional ProductLabel with its explicit type:
    // the SDK's typed Basic helper does not accept it in every version.
    char text[65];
    text[0] = entry->label ? entry->label.get()[0] : 0;
    if (text[0]) memcpy(text + 1, entry->label.get() + 1, uint8_t(text[0]));
    stage("ProductLabel");
    if ((entry->label ? uint8_t(entry->label.get()[0]) : 0)) {
      if (esp_zb_cluster_add_attr(basic, 0, 0x0e, ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING, ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY, text) != ESP_OK) return false;
    }
    uint16_t device = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;
    auto& s = entry->spec;
    stage(ARD_LANG_NATIVE_CONTROL_CLUSTER);
    switch (s.kind) {
      case Kind::Action:
      case Kind::Switch:
        device = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID;
        // Standard HA profile identities; the wire command stays On/Off.
        if (s.kind == Kind::Switch && s.classId == ArdControl::Class::Outlet) device = 0x0051;
        else if (s.kind == Kind::Switch && s.classId == ArdControl::Class::Light) device = 0x0100;
        if (!add(esp_zb_on_off_cluster_create(nullptr), esp_zb_cluster_list_add_on_off_cluster)) return false;
        break;
      case Kind::Slider:
        {
          auto* list = esp_zb_analog_output_cluster_create(nullptr);
          if (!list) return false;

          if (esp_zb_analog_output_cluster_add_attr(list, 0x1c, text) != ESP_OK || esp_zb_analog_output_cluster_add_attr(list, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, &s.max) != ESP_OK || esp_zb_analog_output_cluster_add_attr(list, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, &s.min) != ESP_OK || esp_zb_analog_output_cluster_add_attr(list, 0x6a, &s.step) != ESP_OK || !add(list, esp_zb_cluster_list_add_analog_output_cluster)) return false;
          break;
        }
      case Kind::Select:
        {
          auto* list = esp_zb_multistate_output_cluster_create(nullptr);
          if (!list) return false;
          uint16_t state = 1;
          if (esp_zb_cluster_update_attr(list, 0x4a, &s.states) != ESP_OK || esp_zb_cluster_update_attr(list, 0x55, &state) != ESP_OK || esp_zb_multistate_output_cluster_add_attr(list, 0x1c, text) != ESP_OK || !add(list, esp_zb_cluster_list_add_multistate_output_cluster)) return false;
          break;
        }
      case Kind::State:
        {
          auto* list = esp_zb_multistate_input_cluster_create(nullptr);
          if (!list) return false;
          uint16_t state = 1;
          if (esp_zb_cluster_update_attr(list, 0x4a, &s.states) != ESP_OK || esp_zb_cluster_update_attr(list, 0x55, &state) != ESP_OK || esp_zb_multistate_input_cluster_add_attr(list, 0x1c, text) != ESP_OK || !add(list, esp_zb_cluster_list_add_multistate_input_cluster)) return false;
          break;
        }
#if !defined(ARDPORTAL_ENABLE_CONTROL_TEXT) || ARDPORTAL_ENABLE_CONTROL_TEXT
      case Kind::Number:
        {
          auto* list = esp_zb_analog_input_cluster_create(nullptr);
          if (!list) return false;
          if (esp_zb_analog_input_cluster_add_attr(list, 0x1c, text) != ESP_OK || !add(list, esp_zb_cluster_list_add_analog_input_cluster)) return false;
          break;
        }
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_TEXT) || ARDPORTAL_ENABLE_CONTROL_TEXT || (defined(ARDPORTAL_ENABLE_CONTROL_WATER_HEATER) && ARDPORTAL_ENABLE_CONTROL_WATER_HEATER)
      case Kind::Temperature:
        device = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID;
        if (!add(esp_zb_temperature_meas_cluster_create(nullptr), esp_zb_cluster_list_add_temperature_meas_cluster)) return false;
        break;
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_TEXT) || ARDPORTAL_ENABLE_CONTROL_TEXT || (defined(ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER) && ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER)
      case Kind::Humidity:
        if (!add(esp_zb_humidity_meas_cluster_create(nullptr), esp_zb_cluster_list_add_humidity_meas_cluster)) return false;
        break;
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_TEXT) || ARDPORTAL_ENABLE_CONTROL_TEXT
      case Kind::Pressure:
        if (!add(esp_zb_pressure_meas_cluster_create(nullptr), esp_zb_cluster_list_add_pressure_meas_cluster)) return false;
        break;
      case Kind::Illuminance:
        if (!add(esp_zb_illuminance_meas_cluster_create(nullptr), esp_zb_cluster_list_add_illuminance_meas_cluster)) return false;
        break;
      case Kind::Flow:
        if (!add(esp_zb_flow_meas_cluster_create(nullptr), esp_zb_cluster_list_add_flow_meas_cluster)) return false;
        break;
      case Kind::WindSpeed:
        if (!add(esp_zb_wind_speed_measurement_cluster_create(nullptr), esp_zb_cluster_list_add_wind_speed_measurement_cluster)) return false;
        break;
      case Kind::CarbonDioxide:
        if (!add(esp_zb_carbon_dioxide_measurement_cluster_create(nullptr), esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster)) return false;
        break;
      case Kind::PM25:
        if (!add(esp_zb_pm2_5_measurement_cluster_create(nullptr), esp_zb_cluster_list_add_pm2_5_measurement_cluster)) return false;
        break;
      case Kind::Voltage:
      case Kind::Current:
      case Kind::Power:
        if (!add(esp_zb_electrical_meas_cluster_create(nullptr), esp_zb_cluster_list_add_electrical_meas_cluster)) return false;
        break;
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR) || ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
      case Kind::Binary:
        if (!add(esp_zb_binary_input_cluster_create(nullptr), esp_zb_cluster_list_add_binary_input_cluster)) return false;
        break;
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_LIGHT) || ARDPORTAL_ENABLE_CONTROL_LIGHT
      case Kind::Light:
        {
          device = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;
          esp_zb_color_cluster_cfg_t color = {};
          color.color_capabilities = 11;
          color.color_mode = 0;
          color.enhanced_color_mode = 0;
          // Initial values below update existing attributes and add missing optional
          // attributes with explicit types; typed helpers may reject duplicates.
          if (!add(esp_zb_on_off_cluster_create(nullptr), esp_zb_cluster_list_add_on_off_cluster) || !add(esp_zb_level_cluster_create(nullptr), esp_zb_cluster_list_add_level_cluster) || !add(esp_zb_color_control_cluster_create(&color), esp_zb_cluster_list_add_color_control_cluster)) return false;
          break;
        }
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_CLIMATE) || ARDPORTAL_ENABLE_CONTROL_CLIMATE
      case Kind::Climate:
        {
          device = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;
          esp_zb_thermostat_cluster_cfg_t cfg = {};
          cfg.local_temperature = 0x8000;
          cfg.occupied_cooling_setpoint = cfg.occupied_heating_setpoint = 2000;
          cfg.control_sequence_of_operation = s.sequence;
          auto* list = esp_zb_thermostat_cluster_create(&cfg);
          int16_t min = std::lround(s.min * 100), max = std::lround(s.max * 100);
          uint16_t running = 0;
          int16_t heatMin = min - (s.sequence == 4 ? 250 : 0), coolMax = max + (s.sequence == 4 ? 250 : 0);
          if (!list || esp_zb_thermostat_cluster_add_attr(list, 0x03, &heatMin) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x04, &max) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x05, &min) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x06, &coolMax) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x15, &heatMin) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x16, &max) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x17, &min) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x18, &coolMax) != ESP_OK || esp_zb_thermostat_cluster_add_attr(list, 0x29, &running) != ESP_OK || !add(list, esp_zb_cluster_list_add_thermostat_cluster)) return false;
          if (s.fan && !add(esp_zb_fan_control_cluster_create(nullptr), esp_zb_cluster_list_add_fan_control_cluster)) return false;
          break;
        }
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_COVER) || ARDPORTAL_ENABLE_CONTROL_COVER
      case Kind::Cover:
        {
          device = ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID;
          esp_zb_window_covering_cluster_cfg_t cfg = {};
          cfg.covering_type = 8;  // Lift and tilt blind when no class refines the preset.
          switch (s.classId) {
            case ArdControl::Class::Awning: cfg.covering_type = 5; break;
            case ArdControl::Class::Curtain: cfg.covering_type = 4; break;
            case ArdControl::Class::Shutter: cfg.covering_type = 6; break;
            case ArdControl::Class::Shade: cfg.covering_type = 0; break;
            default: break;
          }
          auto* list = esp_zb_window_covering_cluster_create(&cfg);
          uint8_t position = 100;
          if (!list || esp_zb_window_covering_cluster_add_attr(list, 8, &position) != ESP_OK || esp_zb_window_covering_cluster_add_attr(list, 9, &position) != ESP_OK || !add(list, esp_zb_cluster_list_add_window_covering_cluster)) return false;
          break;
        }
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_LOCK) || ARDPORTAL_ENABLE_CONTROL_LOCK
      case Kind::Lock:
        {
          device = ESP_ZB_HA_DOOR_LOCK_DEVICE_ID;
          esp_zb_door_lock_cluster_cfg_t cfg = {};
          cfg.actuator_enabled = true;
          cfg.lock_state = 1;
          if (!add(esp_zb_door_lock_cluster_create(&cfg), esp_zb_cluster_list_add_door_lock_cluster)) return false;
          break;
        }
#endif
#if !defined(ARDPORTAL_ENABLE_CONTROL_SIREN) || ARDPORTAL_ENABLE_CONTROL_SIREN
      case Kind::Siren:
        {
          device = 0x0403;
          esp_zb_ias_wd_cluster_cfg_t cfg = {};
          cfg.max_duration = 300;
          if (!add(esp_zb_ias_wd_cluster_create(&cfg), esp_zb_cluster_list_add_ias_wd_cluster)) return false;
          break;
        }
#endif
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
      case Kind::Update:
        {
          if (_otaEndpoint) return false;  // One physical firmware image, one OTA client.
          device = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID;
          esp_zb_ota_cluster_cfg_t cfg = {};
          cfg.ota_upgrade_file_version = ARDPORTAL_ZIGBEE_OTA_FILE_VERSION;
          cfg.ota_upgrade_manufacturer = ARDPORTAL_ZIGBEE_OTA_MANUFACTURER;
          cfg.ota_upgrade_image_type = ARDPORTAL_ZIGBEE_OTA_IMAGE_TYPE;
          auto* list = esp_zb_ota_cluster_create(&cfg);
          esp_zb_zcl_ota_upgrade_client_variable_t variable = {};
          variable.timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF;
          variable.max_data_size = 48;
          uint16_t address = 0xffff;
          uint8_t ep = 0xff;
          if (!list || esp_zb_ota_cluster_add_attr(list, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &variable) != ESP_OK || esp_zb_ota_cluster_add_attr(list, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, &address) != ESP_OK || esp_zb_ota_cluster_add_attr(list, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, &ep) != ESP_OK || esp_zb_cluster_list_add_ota_cluster(clusters, list, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE) != ESP_OK) return false;
          _otaEndpoint = s.endpoint;
          break;
        }
#endif
      default: return false;
    }
    if (s.kind == Kind::Light || s.kind == Kind::Cover) {
      if (!add(esp_zb_groups_cluster_create(nullptr), esp_zb_cluster_list_add_groups_cluster) || !add(esp_zb_scenes_cluster_create(nullptr), esp_zb_cluster_list_add_scenes_cluster)) return false;
    }
    for (uint8_t i = 0; i < entry->initialCount; ++i) {
      auto& a = entry->initial.get()[i];
      snprintf(_startupError, sizeof(_startupError), ARD_LANG_NATIVE_ENDPOINT_U_CLUSTER_0X_04X_ATTRIBUTE_0X_04X_FAILED, unsigned(s.endpoint), unsigned(a.cluster), unsigned(a.id));
      auto* list = esp_zb_cluster_list_get_cluster(clusters, a.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
      if (!list) return false;
      const uint8_t access = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ((a.cluster == 0x300 && a.id >= 0x4000) ? 0 : ESP_ZB_ZCL_ATTR_ACCESS_REPORTING);
      if (esp_zb_cluster_update_attr(list, a.id, &a.data) != ESP_OK && esp_zb_cluster_add_attr(list, a.cluster, a.id, a.type, access, &a.data) != ESP_OK) return false;
    }
    esp_zb_endpoint_config_t endpoint = {};
    endpoint.endpoint = s.endpoint;
    endpoint.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    endpoint.app_device_id = device;
    stage(ARD_LANG_NATIVE_ENDPOINT_REGISTRATION);
    if (esp_zb_ep_list_add_ep(endpoints, clusters, endpoint) != ESP_OK) return false;
    // The SDK copies attribute values; startup buffers have no runtime purpose.
    entry->initial.reset();
    entry->initialCount = 0;
    entry->label.reset();
  }
  _startupError[0] = 0;
  return true;
}
/** @brief Preserve native callback payloads in a bounded queue. @param id Callback kind. @param message Borrowed SDK object. @return Error if malformed or the command queue is full. */
esp_err_t ArdZigbee::action(esp_zb_core_action_callback_id_t id, const void* message) {
  if (!_instance || !message) return ESP_ERR_INVALID_ARG;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  if (id == ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID) return _instance->otaAction(static_cast<const esp_zb_zcl_ota_upgrade_value_message_t*>(message));
#endif
  using namespace ArdZigbeeFields;
  Command c;
  if (id == ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID) {
    auto* m = static_cast<const esp_zb_zcl_set_attr_value_message_t*>(message);
    if (m->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return ESP_FAIL;
    c.endpoint = m->info.dst_endpoint;
    c.cluster = m->info.cluster;
    c.id = m->attribute.id;

    const Control* owner = nullptr;
    for (auto* entry = _instance->_controls.get(); entry; entry = entry->next.get())
      if (entry->spec.endpoint == c.endpoint) {
        owner = entry;
        break;
      }
    if (!owner) return ESP_OK;
    auto kind = owner->spec.kind;
    bool supported = ((kind == Kind::Switch || kind == Kind::Action || kind == Kind::Light) && c.cluster == 6 && c.id == 0) || (kind == Kind::Slider && c.cluster == 0xd && c.id == 0x55) || (kind == Kind::Select && c.cluster == 0x13 && c.id == 0x55) || (kind == Kind::Light && ((c.cluster == 8 && c.id == 0) || (c.cluster == 0x300 && (c.id == 0 || c.id == 1 || c.id == 3 || c.id == 4 || c.id == 0x4000)))) || (kind == Kind::Climate && ((c.cluster == 0x201 && (c.id == 0x11 || c.id == 0x12 || c.id == 0x1c)) || (c.cluster == 0x202 && c.id == 0)));
    if (!supported) return ESP_OK;
    auto& data = m->attribute.data;
    if (!data.value || !data.size || data.size > 4) return ESP_ERR_INVALID_ARG;
    uint8_t expected = c.cluster == 0x300 && (c.id == 0x4000 || c.id == 3 || c.id == 4) ? 0x21 : c.cluster == 0xd                                     ? 0x39
                                                                                               : c.cluster == 0x13                                    ? 0x21
                                                                                               : c.cluster == 0x201 && (c.id == 0x11 || c.id == 0x12) ? 0x29
                                                                                               : c.cluster == 6                                       ? 0x10
                                                                                               : c.cluster == 0x201 || c.cluster == 0x202             ? 0x30
                                                                                                                                                      : 0x20;
    size_t expectedSize = expected == 0x39 || expected == 0x23 ? 4 : expected == 0x29 || expected == 0x21 ? 2
                                                                                                          : 1;
    if (data.type != expected || data.size != expectedSize) return ESP_ERR_INVALID_ARG;
    char payload[32], line[128];
    ardZigbeePayload(payload, sizeof(payload), data.value, data.type, data.size);
    snprintf(line, sizeof(line), ARD_LANG_NATIVE_RX_ATTRIBUTE, c.endpoint, c.cluster, c.id, data.type, payload);
    _instance->traffic(line);
    memcpy(&c.value, data.value, data.size);
    if (c.cluster == 0x300) {
      auto* hue = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0);
      auto* saturation = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 1);
      if (!hue || !saturation || !hue->data_p || !saturation->data_p) return ESP_FAIL;
      c.hue = *static_cast<uint8_t*>(hue->data_p);
      c.saturation = *static_cast<uint8_t*>(saturation->data_p);
      auto* enhanced = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0x4000);
      auto* colorMode = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0x4001);
      if (enhanced && enhanced->data_p && colorMode && colorMode->data_p && *static_cast<uint8_t*>(colorMode->data_p) == 3) c.hue = uint8_t(std::lround(*static_cast<uint16_t*>(enhanced->data_p) * 254.0 / 65535));
      if (c.id == 3 || c.id == 4) {
        auto* x = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 3);
        auto* y = esp_zb_zcl_get_attribute(c.endpoint, 0x300, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 4);
        if (!x || !y || !x->data_p || !y->data_p) return ESP_FAIL;
        c.x = *static_cast<uint16_t*>(x->data_p);
        c.y = *static_cast<uint16_t*>(y->data_p);
        if (c.id == 3) c.x = c.value;
        else c.y = c.value;
      }
      if (c.id == 0x4000) {
        c.value = uint8_t(std::lround(c.value * 254.0 / 65535));
        c.id = 0;
      }
      if (c.id == 0) c.hue = c.value;
      else if (c.id == 1) c.saturation = c.value;
      auto* level = esp_zb_zcl_get_attribute(c.endpoint, 8, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0);
      if (!level || !level->data_p) return ESP_FAIL;
      c.level = *static_cast<uint8_t*>(level->data_p);
    }

  } else if (id == ESP_ZB_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID) {
    auto* m = static_cast<const esp_zb_zcl_door_lock_lock_door_message_t*>(message);
    if (m->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return ESP_FAIL;
    c.kind = 1;
    c.endpoint = m->info.dst_endpoint;
    c.cluster = 0x101;
    c.id = m->cmd_id;
  } else if (id == ESP_ZB_CORE_WINDOW_COVERING_MOVEMENT_CB_ID) {
    auto* m = static_cast<const esp_zb_zcl_window_covering_movement_message_t*>(message);
    if (m->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return ESP_FAIL;
    c.kind = 1;
    c.endpoint = m->info.dst_endpoint;
    c.cluster = 0x102;
    c.id = m->command;
    c.value = m->payload.percentage_lift_value;
  } else if (id == ESP_ZB_CORE_IAS_WD_START_WARNING_CB_ID) {
    auto* m = static_cast<const esp_zb_zcl_ias_wd_start_warning_message_t*>(message);
    if (m->info.status != ESP_ZB_ZCL_STATUS_SUCCESS || !m->msg_in) return ESP_FAIL;
    c.kind = 1;
    c.endpoint = m->info.dst_endpoint;
    c.cluster = 0x502;
    c.id = m->msg_in->warning_mode;
    c.duration = m->msg_in->duration;
    c.volume = m->msg_in->siren_level;
  } else return ESP_OK;
  if (c.kind == 1) {
    char line[128];
    snprintf(line, sizeof(line), ARD_LANG_NATIVE_RX_COMMAND, c.endpoint, c.cluster, c.id, (unsigned long)c.value, unsigned(c.duration), unsigned(c.volume));
    _instance->traffic(line);
  }
  for (auto* entry = _instance->_controls.get(); entry; entry = entry->next.get())
    if (entry->spec.endpoint == c.endpoint) {
      if (!acceptsCommands(entry->spec)) return ESP_OK;
      if (!_instance->_commands || xQueueSend(_instance->_commands, &c, 0) != pdTRUE) {
        _instance->traffic(ARD_LANG_NATIVE_RX_ZCL_COMMAND_QUEUE_FULL_COMMAND_REJECTED);
        return ESP_ERR_NO_MEM;
      }
      return ESP_OK;
    }
  return ESP_OK;
}
/** @brief Write attributes and send reports to the coordinator. @param endpoint Endpoint. @param values Desired attributes. @return False when deferred or any SDK operation fails. */
bool ArdZigbee::publish(uint8_t endpoint, const ArdZigbeeFields::Values& values, bool force) {
  Control* entry = _controls.get();
  while (entry && entry->spec.endpoint != endpoint) entry = entry->next.get();
  if (!entry) return false;
  entry->reportIncomplete = false;
  if (!_ready || _state != State::Connected || !ArdHeap::permits(1024, 512) || !esp_zb_lock_acquire(0)) return false;
  _powerIdleAt.store(0);  // Require fresh SDK sleep readiness after this submission.
  const uint16_t previous = entry->pendingReports;
  // A retry retains only attributes not yet queued successfully.
  if (force && !previous) entry->pendingReports = uint16_t((1u << values.count) - 1u);
  bool ok = true;
  bool submitted = false;
  bool achromatic = false;
  for (uint8_t i = 0; i < values.count; ++i)
    if (values.attributes[i].cluster == 0x300 && values.attributes[i].id == 1 && values.attributes[i].data.u == 0) achromatic = true;
  bool heatFirst = false;
  if (entry->spec.kind == ArdZigbeeFields::Kind::Climate && values.count >= 3) {
    auto* heating = esp_zb_zcl_get_attribute(endpoint, 0x201, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0x12);
    int16_t stored = 0;
    if (heating && heating->data_p) {
      memcpy(&stored, heating->data_p, sizeof(stored));
      heatFirst = int16_t(values.attributes[2].data.u) < stored;
    }
  }
  for (uint8_t order = 0; order < values.count; ++order) {
    uint8_t i = heatFirst && (order == 1 || order == 2) ? 3 - order : order;
    const auto& a = values.attributes[i];
    const uint16_t bit = uint16_t(1u << i);
    auto* current = esp_zb_zcl_get_attribute(endpoint, a.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, a.id);
    size_t size = a.type == 0x39 || a.type == 0x23 || a.type == 0x1b ? 4 : a.type == 0x21 || a.type == 0x29 || a.type == 0x19 ? 2
                                                                                                                              : 1;
    if (achromatic && a.cluster == 0x300 && (a.id == 0 || a.id == 0x4000)) {
      entry->pendingReports &= ~bit;
      continue;
    }
    if (!current || !current->data_p) {
      entry->pendingReports |= bit;
      ok = false;
      char message[112];
      snprintf(message, sizeof(message), ARD_LANG_NATIVE_TX_ATTRIBUTE_MISSING_EP_U_CLUSTER_04X_ATTR_04X, endpoint, a.cluster, a.id);
      traffic(message);
      continue;
    }
    const bool changed = memcmp(current->data_p, &a.data, size) != 0;
    if (changed) entry->pendingReports |= bit;
    if (!(entry->pendingReports & bit)) continue;
    const auto stored = changed ? esp_zb_zcl_set_attribute_val(endpoint, a.cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, a.id, const_cast<void*>(static_cast<const void*>(&a.data)), false) : ESP_ZB_ZCL_STATUS_SUCCESS;
    if (stored != ESP_ZB_ZCL_STATUS_SUCCESS) {
      ok = false;
      char message[112];
      snprintf(message, sizeof(message), ARD_LANG_NATIVE_TX_ATTRIBUTE_REJECTED_EP_U_CLUSTER_04X_ATTR_04X_STATUS_0, endpoint, a.cluster, a.id, unsigned(stored));
      traffic(message);
      continue;
    }
    // Enhanced color metadata is readable; ordinary color mode is reported.
    if (a.cluster == 0x300 && a.id >= 0x4000) {
      entry->pendingReports &= ~bit;
      continue;
    }
    if (submitted) continue;  // Release the stack lock between outgoing attributes.
    submitted = true;
    // A standard global Report Attributes frame does not require the SDK's
    // per-attribute reporting table (writable thermostat values lack entries).
    // APS copies the bounded frame and supplies delivery acknowledgement/retries.
    uint8_t frame[10] = { 0x18, _reportSequence++, 0x0a, uint8_t(a.id), uint8_t(a.id >> 8), a.type };
    uint32_t wire = 0;
    memcpy(&wire, &a.data, size);
    for (size_t byte = 0; byte < size; ++byte) frame[6 + byte] = uint8_t(wire >> (8 * byte));
    esp_zb_apsde_data_req_t report = {};
    report.dst_addr_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    report.dst_addr.addr_short = 0;
    report.dst_endpoint = 1;
    report.src_endpoint = endpoint;
    report.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    report.cluster_id = a.cluster;
    report.asdu = frame;
    report.asdu_length = 6 + size;
    report.radius = 30;
    report.tx_options = ESP_ZB_APSDE_TX_OPT_ACK_TX;
    const auto result = esp_zb_aps_data_request(&report);
    if (result != ESP_OK) {
      ok = false;
      {
        char message[128];
        snprintf(message, sizeof(message), ARD_LANG_NATIVE_TX_REPORT_DEFERRED_EP_U_CLUSTER_04X_ATTR_04X_STATUS_0X_L, endpoint, a.cluster, a.id, (unsigned long)result);
        traffic(message);
      }
    } else {
      entry->pendingReports &= ~bit;
      // Float32 formatted with %.7g and all supported integer/boolean values fit in 16 bytes.
      // This also keeps the complete JSON record inside the 128-byte traffic queue slot.
      char payload[16], message[128];
      ardZigbeePayload(payload, sizeof(payload), &a.data, a.type, size);
      snprintf(message, sizeof(message), ARD_LANG_NATIVE_TX_REPORT, endpoint, a.cluster, a.id, a.type, payload);
      traffic(message);
    }
  }
  esp_zb_lock_release();
  entry->reportIncomplete = ok && entry->pendingReports;
  return ok && !entry->pendingReports;
}
