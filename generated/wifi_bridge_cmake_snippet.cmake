# WifiBridge backend selection examples.

option(WIFI_BRIDGE_USE_ESP_AT "Use ESP AT backend" OFF)
option(WIFI_BRIDGE_USE_UNER "Use UNER backend" ON)

if(WIFI_BRIDGE_USE_ESP_AT AND WIFI_BRIDGE_USE_UNER)
    message(FATAL_ERROR "Only one WifiBridge backend can be enabled")
endif()

if(NOT WIFI_BRIDGE_USE_ESP_AT AND NOT WIFI_BRIDGE_USE_UNER)
    message(FATAL_ERROR "Select one WifiBridge backend")
endif()

target_sources(${PROJECT_NAME} PRIVATE
    Core/Src/wifi_bridge.c
    Core/Src/wifi_bridge_ui_adapter.c
)

if(WIFI_BRIDGE_USE_ESP_AT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIFI_BRIDGE_USE_ESP_AT)
    # Optional for the current "connect selected network" flow:
    # target_compile_definitions(${PROJECT_NAME} PRIVATE WIFI_BRIDGE_AT_DEFAULT_PASSWORD="your_password")
    target_sources(${PROJECT_NAME} PRIVATE Core/Src/wifi_bridge_at.c)
    # AT-only builds should not add legacy UNER sources:
    # uner_app.c, uner_core.c, uner_handle.c, uner_router.c, uner_transport_*.c
endif()

if(WIFI_BRIDGE_USE_UNER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIFI_BRIDGE_USE_UNER)
    target_sources(${PROJECT_NAME} PRIVATE Core/Src/wifi_bridge_uner.c)
endif()
