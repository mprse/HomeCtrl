cmake_minimum_required(VERSION 3.12)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

project(HomeCtrl C CXX ASM)

execute_process(
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040
    COMMAND patch -p5 < ${CMAKE_CURRENT_SOURCE_DIR}/freertos_smp.patch
    OUTPUT_VARIABLE patch_output
    RESULT_VARIABLE patch_result
)

if(NOT ${patch_result} EQUAL 0)
    message(WARNING "Failed to apply patch: ${patch_output}")
endif()

add_subdirectory("FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040" FREERTOS_KERNEL)

pico_sdk_init()

if(DEFINED SYSPROGS_FRAMEWORKS_FILE)
include(${SYSPROGS_FRAMEWORKS_FILE})
endif()
SET(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


add_executable(HomeCtrl
        tcp_server.c
        smart_home.c
        dht22.c)

target_compile_definitions(HomeCtrl PRIVATE
        WIFI_SSID=\"NibyPolinezja\"
        WIFI_PASSWORD=\"dupa2713\"
        DHT_ENABLED=1
        CONT_ENABLED=0
        HEAT_ENABLED=1
        GATE_ENABLED=0
        BEEPER_ENABLED=0
        TCP_SERVER_PORT=1235
        NO_SYS=0
        PICO_DEFAULT_UART_TX_PIN=28)
target_include_directories(HomeCtrl PRIVATE
        ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(HomeCtrl
        pico_cyw43_arch_lwip_sys_freertos
        pico_stdlib
        pico_lwip_iperf
        FreeRTOS-Kernel-Heap4
        hardware_rtc
        hardware_sleep)

pico_add_extra_outputs(HomeCtrl)
