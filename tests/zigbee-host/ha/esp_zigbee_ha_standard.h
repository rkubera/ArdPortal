#pragma once
struct esp_zb_configuration_tool_cfg_t {};
#define ESP_ZB_DEFAULT_CONFIGURATION_TOOL_CONFIG() {}
inline void* esp_zb_configuration_tool_ep_create(int,esp_zb_configuration_tool_cfg_t*){static int ep;return &ep;}

constexpr int ESP_ZB_ZCL_CLUSTER_ID_BASIC=0,ESP_ZB_ZCL_CLUSTER_SERVER_ROLE=1,ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID=4,ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID=5,ESP_ZB_AF_HA_PROFILE_ID=0x104,ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID=5;
struct esp_zb_endpoint_config_t{int endpoint=0,app_profile_id=0,app_device_id=0;};
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
inline esp_zb_cluster_list_t* esp_zb_configuration_tool_clusters_create(esp_zb_configuration_tool_cfg_t*){auto* c=esp_zb_zcl_cluster_list_create();c->clusters[0]=esp_zb_basic_cluster_create(nullptr);return c;}
inline esp_zb_ep_list_t* esp_zb_ep_list_create(){return &nativeEndpoints;}
inline int esp_zb_ep_list_add_ep(esp_zb_ep_list_t* p,esp_zb_cluster_list_t* c,esp_zb_endpoint_config_t cfg){p->endpoints[cfg.endpoint]=c;return ESP_OK;}
#else
inline void* esp_zb_configuration_tool_clusters_create(esp_zb_configuration_tool_cfg_t*){static int v;return &v;}
inline void* esp_zb_cluster_list_get_cluster(void* p,int,int){return p;}
inline int esp_zb_basic_cluster_add_attr(void*,int,void*){return ESP_OK;}
inline void* esp_zb_ep_list_create(){static int v;return &v;}
inline int esp_zb_ep_list_add_ep(void*,void*,esp_zb_endpoint_config_t){return ESP_OK;}
#endif

#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
inline esp_zb_attribute_list_t* esp_zb_zcl_attr_list_create(int id){auto* a=new esp_zb_attribute_list_t;a->cluster=id;return a;}
inline int esp_zb_cluster_list_add_time_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* a,int){c->clusters[a->cluster]=a;return ESP_OK;}
#else
constexpr int ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE=2;
inline void* esp_zb_zcl_attr_list_create(int){static int value;return &value;}
inline int esp_zb_cluster_list_add_time_cluster(void*,void*,int){return ESP_OK;}
#endif
