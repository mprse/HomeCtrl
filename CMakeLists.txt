cmake_minimum_required(VERSION 3.12)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

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
add_subdirectory(dht)


if(DEFINED SYSPROGS_FRAMEWORKS_FILE)
include(${SYSPROGS_FRAMEWORKS_FILE})
endif()
SET(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_executable(HomeCtrl
        tcp_server.c
        smart_home.c)

target_compile_definitions(HomeCtrl PRIVATE
        WIFI_SSID=\"Moja_Polinezja\"
        WIFI_PASSWORD=\"dupa1212\"
        NO_SYS=0
        PICO_DEFAULT_UART_TX_PIN=28)
target_include_directories(HomeCtrl PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/dht/include)

target_link_libraries(HomeCtrl
        pico_cyw43_arch_lwip_sys_freertos
        pico_stdlib
        pico_lwip_iperf
        FreeRTOS-Kernel-Heap4
        dht)

pico_add_extra_outputs(HomeCtrl)
