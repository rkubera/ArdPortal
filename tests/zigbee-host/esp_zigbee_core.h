#pragma once
#ifndef ARDPORTAL_ENABLE_WIFI
#define ARDPORTAL_ENABLE_WIFI 1
#endif
#define CONFIG_ESP_COEX_SW_COEXIST_ENABLE 1
#define CONFIG_SOC_IEEE802154_SUPPORTED 1
#include <Arduino.h>
#include <deque>
#include <vector>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, pdTRUE = 1, pdPASS = 1;
struct Queue {
  size_t size, capacity;
  std::deque<std::vector<unsigned char>> items;
};
using QueueHandle_t = Queue*;
inline int queueBudget = -1, taskResult = pdPASS;
inline QueueHandle_t xQueueCreate(size_t n, size_t size) {
  if (queueBudget == 0) return nullptr;
  if (queueBudget > 0) --queueBudget;
  return new Queue{ size, n, {} };
}
inline void vQueueDelete(QueueHandle_t q) {
  delete q;
}
inline int xQueueSend(QueueHandle_t q, const void* p, int) {
  if (q->items.size() == q->capacity) return 0;
  auto b = static_cast<const unsigned char*>(p);
  q->items.emplace_back(b, b + q->size);
  return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void* p, int) {
  if (q->items.empty()) return 0;
  memcpy(p, q->items.front().data(), q->size);
  q->items.pop_front();
  return pdTRUE;
}
inline void (*queuedTask)(void*) = nullptr;
inline void* queuedTaskArg = nullptr;
inline int xTaskCreate(void (*entry)(void*), const char*, unsigned, void* arg, unsigned, void*) {
  if (taskResult == pdPASS) {
    queuedTask = entry;
    queuedTaskArg = arg;
  }
  return taskResult;
}
inline void vTaskDelete(void*) {}
constexpr unsigned ESP_ZB_BDB_MODE_INITIALIZATION = 0, ESP_ZB_BDB_MODE_NETWORK_STEERING = 2;
constexpr int ZB_RADIO_MODE_NATIVE = 0, ZB_HOST_CONNECTION_MODE_NONE = 0, ESP_ZB_DEVICE_TYPE_ROUTER = 1, ESP_ZB_DEVICE_TYPE_ED = 2, ESP_ZB_ED_AGING_TIMEOUT_64MIN = 6;
enum esp_zb_app_signal_type_t { ESP_ZB_COMMON_SIGNAL_CAN_SLEEP,
                                ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP,
                                ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START,
                                ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT,
                                ESP_ZB_BDB_SIGNAL_STEERING,
                                ESP_ZB_BDB_SIGNAL_TC_REJOIN_DONE,
                                ESP_ZB_ZDO_SIGNAL_LEAVE,
                                ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT };
struct esp_zb_app_signal_t {
  uint32_t* p_app_signal;
  int esp_err_status;
};
inline bool factoryNew = true, lockAvailable = true;
inline unsigned steering = 0, initialization = 0, cancelled = 0, resets = 0;
inline bool esp_zb_bdb_is_factory_new() {
  return factoryNew;
}
inline unsigned esp_zb_get_current_channel() {
  return 15;
}
inline unsigned esp_zb_get_pan_id() {
  return 0x1234;
}
inline bool esp_zb_lock_acquire(int) {
  return lockAvailable;
}
inline void esp_zb_lock_release() {}
inline int esp_zb_bdb_start_top_level_commissioning(unsigned mode) {
  if (mode == 2) ++steering;
  else ++initialization;
  return ESP_OK;
}
inline int esp_zb_bdb_cancel_steering() {
  ++cancelled;
  return ESP_OK;
}
inline void esp_zb_bdb_reset_via_local_action() {
  ++resets;
}
inline const char* esp_zb_zdo_signal_to_string(esp_zb_app_signal_type_t) {
  return "test";
}
struct esp_zb_platform_config_t {
  struct {
    int radio_mode;
  } radio_config;
  struct {
    int host_connection_mode;
  } host_config;
};
struct esp_zb_cfg_t {
  int esp_zb_role;
  bool install_code_policy;
  struct {
    struct {
      int max_children;
    } zczr_cfg;
    struct {
      int ed_timeout, keep_alive;
    } zed_cfg;
  } nwk_cfg;
};
inline int esp_zb_platform_config(esp_zb_platform_config_t*) {
  return ESP_OK;
}
inline void esp_zb_init(esp_zb_cfg_t*) {}
inline uint16_t schedulerSlots = 0;
inline int esp_zb_scheduler_queue_size_set(uint16_t slots) {
  schedulerSlots = slots;
  return ESP_OK;
}
inline int esp_zb_device_register(void*) {
  return ESP_OK;
}
inline void esp_zb_set_primary_network_channel_set(unsigned) {}
inline bool coexistEnabled = false;
inline int esp_zb_start(bool) {
  return coexistEnabled ? ESP_OK : ESP_FAIL;
}
inline void esp_zb_stack_main_loop() {}

#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "native-types.h"
#endif

#if !ARDPORTAL_ENABLE_DYNAMIC_PAGES
constexpr int ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT = 2;
#endif
struct esp_zb_zcl_read_attr_cmd_t {
  int address_mode;
  struct {
    uint8_t src_endpoint, dst_endpoint;
    struct {
      uint16_t addr_short;
    } dst_addr_u;
  } zcl_basic_cmd;
  uint16_t clusterID, attr_number;
  uint16_t* attr_field;
};
inline unsigned timeReads = 0;
inline uint8_t esp_zb_zcl_read_attr_cmd_req(esp_zb_zcl_read_attr_cmd_t*) {
  ++timeReads;
  return 77;
}

inline size_t uxQueueMessagesWaiting(QueueHandle_t q) {
  return q->items.size();
}
inline void esp_zb_sleep_enable(bool) {}
inline void esp_zb_sleep_now() {}
