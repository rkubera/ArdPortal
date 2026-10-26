# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
# ESP-IDF component; Arduino IDE continues to use library.properties.
idf_component_register(
    SRCS "src/ArdFS.cpp" "src/ArdFSVolume.cpp"
    INCLUDE_DIRS "src"
    REQUIRES arduino-esp32 esp_matter espressif__network_provisioning espressif__qrcode
)
if(CONFIG_ARDPORTAL_MATTER)
    if(CONFIG_ARDPORTAL_MATTER_PRODUCTION)
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDPORTAL_MATTER_PRODUCTION=1)
    endif()
    target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDPORTAL_ENABLE_MATTER=1)
    if(CONFIG_ARDPORTAL_MATTER_WIFI)
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDPORTAL_MATTER_THREAD=0)
    else()
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDPORTAL_MATTER_THREAD=1)
    endif()
    if(CONFIG_ARDPORTAL_MATTER_THREAD_ONLY)
        target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDPORTAL_ENABLE_WIFI=0)
    endif()
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        ARDPORTAL_MATTER_MAX_ENDPOINTS=${CONFIG_ARDPORTAL_MATTER_MAX_ENDPOINTS})
    # Arduino's precompiled C5 Thread override undefines Wi-Fi Kconfig.
    # The source-built mixed profile must retain its actual SDK settings.
    if(IDF_TARGET STREQUAL "esp32c5")
        if(CONFIG_ARDPORTAL_MATTER_WIFI)
            idf_build_set_property(COMPILE_DEFINITIONS "ARDUINO_MATTER_NETWORK_WIFI=1" APPEND)
        elseif(CONFIG_ARDPORTAL_MATTER_THREAD_ONLY)
            idf_build_set_property(COMPILE_DEFINITIONS "ARDUINO_MATTER_NETWORK_THREAD=1" APPEND)
        endif()
    endif()
endif()

# Attach the policy after all IDF component targets exist. Keep the override
# private to the SDK's CPU-only data-model allocator, never radio/DMA code.
if(CONFIG_ARDPORTAL_MATTER_THREAD_WIFI AND CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP AND NOT CONFIG_SPIRAM)
    function(ardportal_configure_matter_memory)
        idf_component_get_property(arduino_lib arduino-esp32 COMPONENT_LIB)
        get_target_property(arduino_sources ${arduino_lib} SOURCES)
        get_target_property(arduino_source_dir ${arduino_lib} SOURCE_DIR)
        set(policy_header "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/MatterMemoryPolicy.h")
        set(policy_attached FALSE)
        foreach(source IN LISTS arduino_sources)
            if(source MATCHES "(^|/)libraries/Matter/src/MatterMemWrap\.cpp$")
                if(NOT IS_ABSOLUTE "${source}")
                    set(source "${arduino_source_dir}/${source}")
                endif()
                set_property(SOURCE "${source}" TARGET_DIRECTORY ${arduino_lib} APPEND PROPERTY
                    COMPILE_OPTIONS -include "${policy_header}")
                set(policy_attached TRUE)
            endif()
        endforeach()
        if(NOT policy_attached)
            message(FATAL_ERROR "Cannot locate the pinned SDK Matter data-model allocator")
        endif()
    endfunction()
    cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL ardportal_configure_matter_memory)
endif()
