#pragma once
struct esp_zb_apsde_data_ind_t {unsigned src_short_addr=0,src_endpoint=1,dst_endpoint=1,profile_id=0,cluster_id=0,asdu_length=0,status=0;int lqi=255;uint8_t* asdu=nullptr;};
inline bool(*apsCallback)(esp_zb_apsde_data_ind_t)=nullptr;
inline void esp_zb_aps_data_indication_handler_register(bool(*callback)(esp_zb_apsde_data_ind_t)){apsCallback=callback;}
