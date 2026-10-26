cmake_minimum_required(VERSION 3.20)

set(ARDPORTAL_PROFILE "wifi" CACHE STRING "wifi, thread, or thread-wifi")
set_property(CACHE ARDPORTAL_PROFILE PROPERTY STRINGS wifi thread thread-wifi)
if(NOT ARDPORTAL_PROFILE MATCHES "^(wifi|thread|thread-wifi)$")
    message(FATAL_ERROR "ARDPORTAL_PROFILE must be wifi, thread, or thread-wifi")
endif()
set(ARDPORTAL_CREDENTIALS "development" CACHE STRING "development or production")
set_property(CACHE ARDPORTAL_CREDENTIALS PROPERTY STRINGS development production)
if(NOT ARDPORTAL_CREDENTIALS MATCHES "^(development|production)$")
    message(FATAL_ERROR "ARDPORTAL_CREDENTIALS must be development or production")
endif()
set(ARDPORTAL_POWER "off" CACHE STRING "off, connected, or icd")
set_property(CACHE ARDPORTAL_POWER PROPERTY STRINGS off connected icd)
if(NOT ARDPORTAL_POWER MATCHES "^(off|connected|icd)$")
    message(FATAL_ERROR "ARDPORTAL_POWER must be off, connected, or icd")
endif()
if(ARDPORTAL_POWER STREQUAL "icd" AND ARDPORTAL_PROFILE STREQUAL "wifi")
    message(FATAL_ERROR "The pinned SDK supports ICD only over Thread")
endif()
if(ARDPORTAL_POWER STREQUAL "connected" AND NOT ARDPORTAL_PROFILE STREQUAL "wifi")
    message(FATAL_ERROR "Thread power saving requires ARDPORTAL_POWER=icd")
endif()
# Thread role selects the SDK device type, independently of portal Wi-Fi.
set(ARDPORTAL_THREAD_ROLE "mtd" CACHE STRING "mtd or router")
set_property(CACHE ARDPORTAL_THREAD_ROLE PROPERTY STRINGS mtd router)
if(NOT ARDPORTAL_THREAD_ROLE MATCHES "^(mtd|router)$")
    message(FATAL_ERROR "ARDPORTAL_THREAD_ROLE must be mtd or router")
endif()
if(ARDPORTAL_THREAD_ROLE STREQUAL "router" AND
   (ARDPORTAL_PROFILE STREQUAL "wifi" OR ARDPORTAL_POWER STREQUAL "icd"))
    message(FATAL_ERROR "Thread router requires a Thread transport without ICD")
endif()
get_filename_component(ARDPORTAL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(ARDPORTAL_COMPONENT_NAME "${ARDPORTAL_ROOT}" NAME)
# Pin official sources and avoid a full Git history download in Component Manager.
set(arduino_commit "94afccf35fb1e401facddbcf9e13bcf7c76a31d8")
set(arduino_source "${ARDPORTAL_ROOT}/.build-tools/arduino-esp32")
if(EXISTS "${arduino_source}/idf_component.yml")
    if(NOT EXISTS "${arduino_source}/.ardportal-commit")
        message(FATAL_ERROR "Unverified Arduino source cache; remove .build-tools/arduino-esp32 and reconfigure")
    endif()
    file(READ "${arduino_source}/.ardportal-commit" cached_arduino_commit)
    string(STRIP "${cached_arduino_commit}" cached_arduino_commit)
    if(NOT cached_arduino_commit STREQUAL arduino_commit)
        message(FATAL_ERROR "Arduino source version changed; remove .build-tools/arduino-esp32 and reconfigure")
    endif()
else()
    set(arduino_archive "${CMAKE_BINARY_DIR}/arduino-${arduino_commit}.tar.gz")
    file(DOWNLOAD
        "https://codeload.github.com/espressif/arduino-esp32/tar.gz/${arduino_commit}"
        "${arduino_archive}"
        EXPECTED_HASH SHA256=e447de8af8255244d1c65a6985255cdcad131f753261b73fe4c25b2ca218ade1
        TLS_VERIFY ON)
    file(MAKE_DIRECTORY "${ARDPORTAL_ROOT}/.build-tools")
    file(ARCHIVE_EXTRACT INPUT "${arduino_archive}" DESTINATION "${ARDPORTAL_ROOT}/.build-tools")
    file(RENAME "${ARDPORTAL_ROOT}/.build-tools/arduino-esp32-${arduino_commit}" "${arduino_source}")
    file(WRITE "${arduino_source}/.ardportal-commit" "${arduino_commit}\n")
endif()
set(EXTRA_COMPONENT_DIRS "${ARDPORTAL_ROOT}")
if(NOT DEFINED SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.${ARDPORTAL_PROFILE}"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.credentials.${ARDPORTAL_CREDENTIALS}")
if(NOT ARDPORTAL_PROFILE STREQUAL "wifi")
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.thread-role.${ARDPORTAL_THREAD_ROLE}")
endif()
if(NOT ARDPORTAL_POWER STREQUAL "off")
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.power.connected")
endif()
if(ARDPORTAL_POWER STREQUAL "icd")
    list(APPEND SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.power.icd")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ArdPortalMatter)

# Use the same native USB Serial/JTAG port for flashing and the example console.
if(CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED)
    idf_component_get_property(arduino_lib arduino-esp32 COMPONENT_LIB)
    target_compile_definitions(${arduino_lib} PUBLIC ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1)
endif()

# Arduino ships this GCC 14/C++20 compatibility header. Apply it to the SDK
# ClosureControl implementation too, without editing downloaded SDK sources.
idf_component_get_property(matter_lib espressif__esp_matter COMPONENT_LIB)
get_target_property(matter_sources ${matter_lib} SOURCES)
include("${CMAKE_CURRENT_LIST_DIR}/MatterWiFiBands.cmake")
foreach(source IN LISTS matter_sources)
    if(source MATCHES "/closure-control-server/[^/]+\\.cpp$")
        set_property(SOURCE "${source}" TARGET_DIRECTORY ${matter_lib} APPEND PROPERTY
            COMPILE_OPTIONS -include "${arduino_source}/libraries/Matter/src/matter_closure_patch.h")
    endif()
endforeach()
idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++2a;-DCHIP_HAVE_CONFIG_H" APPEND)

# Stop instead of silently using a stale sdkconfig from another profile.
if(ARDPORTAL_PROFILE STREQUAL "wifi")
    if(NOT CONFIG_ARDPORTAL_MATTER_WIFI OR NOT CONFIG_ENABLE_WIFI_STATION OR
       CONFIG_ENABLE_MATTER_OVER_THREAD OR CONFIG_OPENTHREAD_ENABLED)
        message(FATAL_ERROR "Wi-Fi profile requires OpenThread disabled; use a fresh build directory")
    endif()
else()
    if(NOT CONFIG_ENABLE_MATTER_OVER_THREAD OR NOT CONFIG_OPENTHREAD_ENABLED)
        message(FATAL_ERROR "Thread profile requires the SDK Thread stack")
    endif()
endif()
if(ARDPORTAL_PROFILE STREQUAL "thread" AND
   (CONFIG_ENABLE_WIFI_STATION OR NOT CONFIG_ARDPORTAL_MATTER_THREAD_ONLY))
    message(FATAL_ERROR "Thread-only profile requires Wi-Fi disabled")
endif()
if(ARDPORTAL_PROFILE STREQUAL "thread-wifi" AND
   (CONFIG_ENABLE_WIFI_STATION OR NOT CONFIG_ESP_WIFI_ENABLED OR NOT CONFIG_ARDPORTAL_MATTER_THREAD_WIFI))
    message(FATAL_ERROR "Thread with Wi-Fi portal requires Matter Wi-Fi disabled and the portal Wi-Fi driver enabled")
endif()
if(ARDPORTAL_PROFILE STREQUAL "thread-wifi" AND NOT CONFIG_ESP_COEX_SW_COEXIST_ENABLE)
    message(FATAL_ERROR "Thread with Wi-Fi portal requires Wi-Fi/802.15.4 radio coexistence")
endif()
if(NOT CONFIG_ENABLE_CHIPOBLE)
    message(FATAL_ERROR "These profiles require BLE commissioning")
endif()
if(CONFIG_COMPILER_CXX_EXCEPTIONS)
    message(FATAL_ERROR "These profiles disable C++ exceptions; disable CONFIG_COMPILER_CXX_EXCEPTIONS in menuconfig or use a fresh build directory")
endif()
if(CONFIG_ESP_WIFI_ENABLED AND
   (CONFIG_ESP_WIFI_IRAM_OPT OR CONFIG_ESP_WIFI_EXTRA_IRAM_OPT OR CONFIG_ESP_WIFI_RX_IRAM_OPT OR CONFIG_ESP_WIFI_SLP_IRAM_OPT))
    message(FATAL_ERROR "These profiles reserve SRAM for BLE, AP authentication and HTTP; disable ESP_WIFI_IRAM_OPT, ESP_WIFI_EXTRA_IRAM_OPT, ESP_WIFI_RX_IRAM_OPT and ESP_WIFI_SLP_IRAM_OPT or use a fresh build directory")
endif()
math(EXPR required_endpoints "${CONFIG_ARDPORTAL_MATTER_MAX_ENDPOINTS} + 1")
if(CONFIG_ESP_MATTER_MAX_DYNAMIC_ENDPOINT_COUNT LESS required_endpoints)
    message(FATAL_ERROR "SDK endpoint capacity must include the Matter root endpoint")
endif()

# Credential selection is independent of transport. Never accept a stale SDK
# configuration that would fall back to development attestation in production.
if(ARDPORTAL_CREDENTIALS STREQUAL "production")
    if(NOT CONFIG_ARDPORTAL_MATTER_PRODUCTION OR
       NOT CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER OR
       NOT CONFIG_ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER OR
       NOT CONFIG_FACTORY_PARTITION_DAC_PROVIDER OR
       NOT CONFIG_FACTORY_COMMISSIONABLE_DATA_PROVIDER OR
       NOT CONFIG_FACTORY_DEVICE_INSTANCE_INFO_PROVIDER OR
       CONFIG_ENABLE_TEST_SETUP_PARAMS OR CONFIG_EXAMPLE_DAC_PROVIDER OR
       CONFIG_EXAMPLE_COMMISSIONABLE_DATA_PROVIDER OR CONFIG_EXAMPLE_DEVICE_INSTANCE_INFO_PROVIDER)
        message(FATAL_ERROR "Production requires factory credential providers without test setup parameters; use a fresh build directory")
    endif()
    if(NOT CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL STREQUAL "fctry" OR
       NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "partitions.production.csv" OR
       CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH)
        message(FATAL_ERROR "Production requires the dedicated fctry partition at 0x3F0000 (no flash coredump)")
    endif()
else()
    if(NOT CONFIG_ARDPORTAL_MATTER_DEVELOPMENT OR CONFIG_ARDPORTAL_MATTER_PRODUCTION OR
       CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER OR
       NOT CONFIG_EXAMPLE_DAC_PROVIDER OR NOT CONFIG_EXAMPLE_COMMISSIONABLE_DATA_PROVIDER OR
       NOT CONFIG_EXAMPLE_DEVICE_INSTANCE_INFO_PROVIDER OR NOT CONFIG_ENABLE_TEST_SETUP_PARAMS OR
       NOT CONFIG_PARTITION_TABLE_CUSTOM_FILENAME STREQUAL "partitions.csv")
        message(FATAL_ERROR "Development requires the SDK test providers and development partition table; use a fresh build directory")
    endif()
endif()

# Thread role guards: SDK defaults never override a saved device-type choice.
if(NOT ARDPORTAL_PROFILE STREQUAL "wifi")
    if(ARDPORTAL_THREAD_ROLE STREQUAL "router")
        if(NOT CONFIG_OPENTHREAD_FTD OR CONFIG_OPENTHREAD_MTD OR
           CONFIG_ENABLE_ICD_SERVER OR CONFIG_IEEE802154_SLEEP_ENABLE)
            message(FATAL_ERROR "Thread router requires FTD without ICD or radio sleep; use a fresh build directory")
        endif()
    elseif(NOT CONFIG_OPENTHREAD_MTD OR CONFIG_OPENTHREAD_FTD)
        message(FATAL_ERROR "Thread MTD requires the MTD SDK; use a fresh build directory")
    endif()
endif()

# SDK defaults do not override an existing sdkconfig: fail closed on stale builds.
if(NOT ARDPORTAL_POWER STREQUAL "off" AND
   (NOT CONFIG_PM_ENABLE OR NOT CONFIG_FREERTOS_USE_TICKLESS_IDLE))
    message(FATAL_ERROR "Low power requires PM and tickless idle; use a fresh build directory")
endif()
if(ARDPORTAL_POWER STREQUAL "icd")
    if(NOT CONFIG_ENABLE_ICD_SERVER OR NOT CONFIG_OPENTHREAD_MTD OR NOT CONFIG_IEEE802154_SLEEP_ENABLE)
        message(FATAL_ERROR "ICD requires the SDK ICD server, sleepy Thread MTD and radio sleep")
    endif()
elseif(CONFIG_ENABLE_ICD_SERVER)
    message(FATAL_ERROR "ICD configuration requires ARDPORTAL_POWER=icd; use a fresh build directory")
endif()
