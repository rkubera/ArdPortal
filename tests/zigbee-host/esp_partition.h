#pragma once
constexpr int ESP_PARTITION_TYPE_DATA=1,ESP_PARTITION_SUBTYPE_ANY=0;
inline void* esp_partition_find_first(int,int,const char*){static int partition;return &partition;}
