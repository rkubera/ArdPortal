#pragma once
constexpr int ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE=2;
#include <map>
#include <array>
constexpr int ESP_ZB_ZCL_ATTR_TYPE_CHAR_STRING=0x42;
constexpr int ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID=0x41,ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID=0x45;
constexpr int ESP_ERR_INVALID_ARG=0x102,ESP_ERR_NO_MEM=0x101;
enum esp_zb_core_action_callback_id_t {ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,ESP_ZB_CORE_DOOR_LOCK_LOCK_DOOR_CB_ID,ESP_ZB_CORE_WINDOW_COVERING_MOVEMENT_CB_ID,ESP_ZB_CORE_IAS_WD_START_WARNING_CB_ID,ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID};
constexpr int ESP_ZB_ZCL_STATUS_SUCCESS=0,ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY=1,ESP_ZB_ZCL_ATTR_ACCESS_REPORTING=4,ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT=2,ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI=1,ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC=0;
constexpr int ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID=12,ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID=2,ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID=0x302,ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID=0x102,ESP_ZB_HA_THERMOSTAT_DEVICE_ID=0x301,ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID=0x202,ESP_ZB_HA_DOOR_LOCK_DEVICE_ID=0xa;
struct Attr {uint8_t access=ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY|ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;std::array<unsigned char,64> bytes{};void* data_p=bytes.data();};
struct esp_zb_attribute_list_t {uint16_t cluster;std::map<uint16_t,Attr> attributes;};
struct esp_zb_cluster_list_t {std::map<uint16_t,esp_zb_attribute_list_t*> clusters;};
struct esp_zb_ep_list_t {std::map<uint8_t,esp_zb_cluster_list_t*> endpoints;};
inline esp_zb_ep_list_t nativeEndpoints;
inline bool nativeFailure=false,reportFailure=false;
inline unsigned reports=0;
inline int failedReportCluster=-1,failedReportAttribute=-1;
inline esp_zb_cluster_list_t* esp_zb_zcl_cluster_list_create(){return nativeFailure?nullptr:new esp_zb_cluster_list_t;}
inline esp_zb_attribute_list_t* esp_zb_cluster_list_get_cluster(esp_zb_cluster_list_t* l,int id,int){return l->clusters.count(id)?l->clusters[id]:nullptr;}
inline int esp_zb_cluster_update_attr(esp_zb_attribute_list_t* l,int id,void* value){if(!l->attributes.count(id))return ESP_FAIL;memcpy(l->attributes[id].bytes.data(),value,(l->cluster==19||l->cluster==18)&&(id==0x55||id==0x4a)?2:4);return ESP_OK;}
inline int esp_zb_cluster_add_attr(esp_zb_attribute_list_t* l,int,int id,int,int access,void* value){auto& a=l->attributes[id];a.access=uint8_t(access);memcpy(a.bytes.data(),value,4);return ESP_OK;}
struct esp_zb_color_cluster_cfg_t {uint16_t color_capabilities;uint8_t color_mode,enhanced_color_mode;};
struct esp_zb_thermostat_cluster_cfg_t {int16_t local_temperature,occupied_cooling_setpoint,occupied_heating_setpoint;uint8_t control_sequence_of_operation;};
struct esp_zb_window_covering_cluster_cfg_t {uint8_t covering_type;};
struct esp_zb_door_lock_cluster_cfg_t {bool actuator_enabled;uint8_t lock_state;};
struct esp_zb_ias_wd_cluster_cfg_t {uint16_t max_duration;};
inline esp_zb_attribute_list_t* esp_zb_basic_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=0;return l;}
inline int esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_basic_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure||id==0x0e)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,(id==4||id==5||id==0x4000)?size_t(*static_cast<uint8_t*>(value))+1:1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_identify_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=3;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_identify_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_groups_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=4;return l;}
inline int esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_groups_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_scenes_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=5;return l;}
inline int esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_scenes_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_on_off_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=6;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_on_off_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_analog_output_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=13;l->attributes[85];return l;}
inline int esp_zb_cluster_list_add_analog_output_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_analog_output_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure||id==0x46)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,id==0x1c?1:4);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_multistate_output_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=19;l->attributes[85];l->attributes[74];return l;}
inline int esp_zb_cluster_list_add_multistate_output_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_multistate_output_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_analog_input_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=12;l->attributes[85];return l;}
inline int esp_zb_cluster_list_add_analog_input_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_analog_input_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_temperature_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1026;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_temperature_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_temperature_meas_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_humidity_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1029;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_humidity_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_humidity_meas_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_binary_input_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=15;l->attributes[85];return l;}
inline int esp_zb_cluster_list_add_binary_input_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_binary_input_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_level_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=8;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_level_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_color_control_cluster_create(esp_zb_color_cluster_cfg_t*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=768;l->attributes[8].access=ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY;l->attributes[0x4001].access=ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY;return l;}
inline int esp_zb_cluster_list_add_color_control_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_color_control_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure||l->attributes.count(id))return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_thermostat_cluster_create(esp_zb_thermostat_cluster_cfg_t* cfg){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=513;l->attributes[0];l->attributes[17];l->attributes[18];l->attributes[28];l->attributes[0x1b].bytes[0]=cfg->control_sequence_of_operation;return l;}
inline int esp_zb_cluster_list_add_thermostat_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_thermostat_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,(id==0x19||id==0x1b||id==0x1c)?1:2);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_fan_control_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=514;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_fan_control_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_fan_control_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_window_covering_cluster_create(esp_zb_window_covering_cluster_cfg_t*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=258;return l;}
inline int esp_zb_cluster_list_add_window_covering_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_window_covering_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_door_lock_cluster_create(esp_zb_door_lock_cluster_cfg_t*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=257;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_door_lock_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_door_lock_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_ias_wd_cluster_create(esp_zb_ias_wd_cluster_cfg_t*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1282;return l;}
inline int esp_zb_cluster_list_add_ias_wd_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline int esp_zb_ias_wd_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
struct esp_zb_zcl_set_attr_value_message_t {struct {int status;uint8_t dst_endpoint;uint16_t cluster;} info;struct {uint16_t id;struct {uint8_t type;uint16_t size;void* value;} data;} attribute;};
struct esp_zb_zcl_door_lock_lock_door_message_t {decltype(esp_zb_zcl_set_attr_value_message_t::info) info;uint16_t cmd_id;};
struct esp_zb_zcl_window_covering_movement_message_t {decltype(esp_zb_zcl_set_attr_value_message_t::info) info;uint16_t command;struct{uint8_t percentage_lift_value;} payload;};
struct esp_zb_zcl_ias_wd_start_warning_t {uint8_t warning_mode,siren_level;uint16_t duration;};
struct esp_zb_zcl_ias_wd_start_warning_message_t {decltype(esp_zb_zcl_set_attr_value_message_t::info) info;esp_zb_zcl_ias_wd_start_warning_t* msg_in;};
inline Attr* esp_zb_zcl_get_attribute(uint8_t ep,uint16_t cluster,int,uint16_t id){if(!nativeEndpoints.endpoints.count(ep))return nullptr;auto* c=nativeEndpoints.endpoints[ep];if(!c->clusters.count(cluster))return nullptr;auto& a=c->clusters[cluster]->attributes;if(!a.count(id))return nullptr;return &a[id];}
inline int esp_zb_zcl_set_attribute_val(uint8_t ep,uint16_t c,int role,uint16_t id,void* data,bool){auto* a=esp_zb_zcl_get_attribute(ep,c,role,id);if(!a||nativeFailure)return ESP_FAIL;
 if(c==0x201&&(id==0x11||id==0x12)&&nativeEndpoints.endpoints[ep]->clusters[c]->attributes[0x1b].bytes[0]==4){
   int16_t target=0,other=0;memcpy(&target,data,2);memcpy(&other,esp_zb_zcl_get_attribute(ep,c,role,id==0x11?0x12:0x11)->data_p,2);
   if((id==0x11?target-other:other-target)<250)return 0x87;
 }
 memcpy(a->bytes.data(),data,c==0x201&&id!=0x1b&&id!=0x1c?2:1);return ESP_OK;}
struct esp_zb_zcl_report_attr_cmd_t {int address_mode;struct {uint8_t src_endpoint,dst_endpoint;struct{uint16_t addr_short;}dst_addr_u;}zcl_basic_cmd;uint16_t clusterID,attributeID,manuf_code;uint8_t direction;};
inline int esp_zb_zcl_report_attr_cmd_req(esp_zb_zcl_report_attr_cmd_t* cmd){++reports;return reportFailure||(cmd->clusterID==failedReportCluster&&cmd->attributeID==failedReportAttribute)?ESP_FAIL:ESP_OK;}
inline void esp_zb_core_action_handler_register(esp_err_t(*)(esp_zb_core_action_callback_id_t,const void*)){}

#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
constexpr int ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF=1,ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID=0xfff1,ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID=0xfff2,ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID=0xfff3;
struct esp_zb_ota_cluster_cfg_t {uint32_t ota_upgrade_file_version;uint16_t ota_upgrade_manufacturer,ota_upgrade_image_type;};
struct esp_zb_zcl_ota_upgrade_client_variable_t {uint16_t timer_query,hw_version;uint8_t max_data_size;};
inline esp_zb_attribute_list_t* esp_zb_ota_cluster_create(esp_zb_ota_cluster_cfg_t*){auto* l=new esp_zb_attribute_list_t;l->cluster=0x19;return l;}
inline int esp_zb_ota_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void*){l->attributes[id];return ESP_OK;}
inline int esp_zb_cluster_list_add_ota_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline unsigned otaQueries=0;
inline int esp_zb_ota_upgrade_client_query_interval_set(uint8_t,unsigned){return ESP_OK;}
inline int esp_zb_ota_upgrade_client_query_image_req(unsigned,unsigned){++otaQueries;return ESP_OK;}
enum {ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT,ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ERROR};
struct esp_zb_zcl_ota_upgrade_value_message_t {decltype(esp_zb_zcl_set_attr_value_message_t::info) info;int upgrade_status;struct{}ota_header;uint16_t payload_size;uint8_t* payload;};
#endif

inline esp_zb_attribute_list_t* esp_zb_pressure_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1027;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_pressure_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_illuminance_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1024;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_illuminance_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_flow_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1028;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_flow_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_wind_speed_measurement_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1035;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_wind_speed_measurement_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_carbon_dioxide_measurement_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1037;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_pm2_5_measurement_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=1066;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_pm2_5_measurement_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
inline esp_zb_attribute_list_t* esp_zb_electrical_meas_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=2820;l->attributes[0];return l;}
inline int esp_zb_cluster_list_add_electrical_meas_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}

inline esp_zb_attribute_list_t* esp_zb_multistate_input_cluster_create(void*){if(nativeFailure)return nullptr;auto* l=new esp_zb_attribute_list_t;l->cluster=0x12;l->attributes[0x55];l->attributes[0x4a];return l;}
inline int esp_zb_multistate_input_cluster_add_attr(esp_zb_attribute_list_t* l,int id,void* value){if(nativeFailure)return ESP_FAIL;auto& a=l->attributes[id];memcpy(a.bytes.data(),value,1);return ESP_OK;}
inline int esp_zb_cluster_list_add_multistate_input_cluster(esp_zb_cluster_list_t* c,esp_zb_attribute_list_t* l,int){c->clusters[l->cluster]=l;return ESP_OK;}
