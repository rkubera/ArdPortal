#pragma once
#include <cassert>
struct esp_zb_apsde_data_ind_t {
  unsigned src_short_addr = 0, src_endpoint = 1, dst_endpoint = 1, profile_id = 0, cluster_id = 0, asdu_length = 0, status = 0;
  int lqi = 255;
  uint8_t* asdu = nullptr;
};
inline bool (*apsCallback)(esp_zb_apsde_data_ind_t) = nullptr;
inline void esp_zb_aps_data_indication_handler_register(bool (*callback)(esp_zb_apsde_data_ind_t)) {
  apsCallback = callback;
}

#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
constexpr int ESP_ZB_APSDE_TX_OPT_ACK_TX = 4;
struct esp_zb_apsde_data_req_t {
  uint8_t dst_addr_mode = 0;
  struct {
    uint16_t addr_short = 0;
  } dst_addr;
  uint8_t dst_endpoint = 0, src_endpoint = 0;
  uint16_t profile_id = 0, cluster_id = 0;
  uint8_t* asdu = nullptr;
  uint32_t asdu_length = 0;
  uint8_t radius = 0, tx_options = 0;
};
inline std::vector<std::vector<uint8_t>> apsReports;
inline int esp_zb_aps_data_request(esp_zb_apsde_data_req_t* request) {
  ++reports;
  if (reportFailure || (failedReportCluster == request->cluster_id && failedReportAttribute == (request->asdu[3] | request->asdu[4] << 8))) return ESP_FAIL;
  assert(request->dst_addr.addr_short == 0 && request->dst_endpoint == 1 && request->profile_id == 260 && request->asdu_length >= 7 && request->asdu_length <= 10 && request->tx_options == 4);
  apsReports.emplace_back(request->asdu, request->asdu + request->asdu_length);
  return ESP_OK;
}

#endif
