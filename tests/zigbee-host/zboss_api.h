#pragma once
#include <nwk/esp_zigbee_nwk.h>
#define ZB_TRUE true
#define ZB_FALSE false
inline void zb_set_rx_on_when_idle(bool enabled){esp_zb_set_rx_on_when_idle(enabled);}
extern "C" inline void zb_set_ed_node_descriptor(bool,bool,bool){}
