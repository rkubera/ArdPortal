#pragma once
#include <stdint.h>
#include <vector>
struct esp_partition_t {uint32_t size=4096;};
using esp_ota_handle_t=uint32_t;
constexpr int OTA_WITH_SEQUENTIAL_WRITES=0;
inline esp_partition_t otaPartition;
inline bool otaBeginFail=false,otaWriteFail=false,otaEndFail=false,otaBootFail=false;
inline unsigned otaAborts=0,otaBoots=0;
inline std::vector<uint8_t> otaBytes;
inline const esp_partition_t* esp_ota_get_next_update_partition(void*){return &otaPartition;}
inline int esp_ota_begin(const esp_partition_t*,int,esp_ota_handle_t* handle){if(otaBeginFail)return -1;*handle=1;otaBytes.clear();return 0;}
inline int esp_ota_abort(esp_ota_handle_t){++otaAborts;return 0;}
inline int esp_ota_write(esp_ota_handle_t,const uint8_t* bytes,size_t count){if(otaWriteFail)return -1;otaBytes.insert(otaBytes.end(),bytes,bytes+count);return 0;}
inline int esp_ota_end(esp_ota_handle_t){return otaEndFail?-1:0;}
inline int esp_ota_set_boot_partition(const esp_partition_t*){if(otaBootFail)return -1;++otaBoots;return 0;}
