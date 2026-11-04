# Apply roaming flags to Matter's credential-install and restore paths as well
# as ArdPortal's Arduino connection path. Never edit downloaded SDK sources.
if(CONFIG_ENABLE_WIFI_STATION AND (CONFIG_ESP_WIFI_11KV_SUPPORT OR CONFIG_ESP_WIFI_11R_SUPPORT))
    set(roaming_drivers_found 0)
    set(roaming_call "esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);")
    set(roaming_setup "
#if CONFIG_ESP_WIFI_11KV_SUPPORT
    wifiConfig.sta.rm_enabled = 1;
    wifiConfig.sta.btm_enabled = 1;
#if CONFIG_ESP_WIFI_MBO_SUPPORT
    wifiConfig.sta.mbo_enabled = 1;
#endif
#endif
#if CONFIG_ESP_WIFI_11R_SUPPORT
    wifiConfig.sta.ft_enabled = 1;
#endif
    wifiConfig.sta.bssid_set = false;
    wifiConfig.sta.channel = 0;
    ${roaming_call}")
    foreach(source IN LISTS matter_sources)
        if(source MATCHES "/(NetworkCommissioningDriver|ESP32Utils)\\.cpp$")
            file(READ "${source}" roaming_cpp)
            string(FIND "${roaming_cpp}" "${roaming_call}" roaming_at)
            if(roaming_at EQUAL -1)
                message(FATAL_ERROR "Matter Wi-Fi driver changed; review the roaming overlay: ${source}")
            endif()
            string(REPLACE "${roaming_call}" "${roaming_setup}" roaming_cpp "${roaming_cpp}")
            get_filename_component(roaming_name "${source}" NAME)
            set(roaming_output "${CMAKE_BINARY_DIR}/ardportal-matter-roaming/${roaming_name}")
            file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ardportal-matter-roaming")
            file(CONFIGURE OUTPUT "${roaming_output}" CONTENT "${roaming_cpp}" @ONLY)
            list(REMOVE_ITEM matter_sources "${source}")
            list(APPEND matter_sources "${roaming_output}")
            math(EXPR roaming_drivers_found "${roaming_drivers_found} + 1")
        endif()
    endforeach()
    if(NOT roaming_drivers_found EQUAL 2)
        message(FATAL_ERROR "Matter Wi-Fi commissioning/provision drivers not found for roaming overlay")
    endif()
    set_property(TARGET ${matter_lib} PROPERTY SOURCES "${matter_sources}")
endif()
