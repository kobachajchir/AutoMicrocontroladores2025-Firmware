# WifiBridge backend selection.
#
# Use one build variable to select both the compile define and the .c files.
# Example:
#   make -B WIFI_BRIDGE_BACKEND=UNER
#   make -B WIFI_BRIDGE_BACKEND=ESP_AT
#
# Use -B or clean when switching backends in the same build directory so common
# objects are rebuilt with the new compile define.
#
# This matters because #ifdef inside C prevents code paths inside a compiled file,
# but it does not stop Make from compiling/linking an unused backend object.

WIFI_BRIDGE_BACKEND ?= UNER

SRCS += Core/Src/wifi_bridge.c
SRCS += Core/Src/wifi_bridge_ui_adapter.c

ifeq ($(WIFI_BRIDGE_BACKEND),UNER)
CFLAGS += -DWIFI_BRIDGE_USE_UNER
SRCS += Core/Src/wifi_bridge_uner.c
SRCS += Core/Src/uner_app.c
SRCS += Core/Src/uner_core.c
SRCS += Core/Src/uner_handle.c
SRCS += Core/Src/uner_router.c
SRCS += Core/Src/uner_transport_nrf24_spi2.c
SRCS += Core/Src/uner_transport_uart1_dma.c
SRCS += Core/Src/uner_transport_usbcdc.c
else ifeq ($(WIFI_BRIDGE_BACKEND),ESP_AT)
CFLAGS += -DWIFI_BRIDGE_USE_ESP_AT
# Optional: only initializes the runtime global password.
# CFLAGS += -DWIFI_BRIDGE_AT_DEFAULT_PASSWORD=\"your_password\"
SRCS += Core/Src/wifi_bridge_at.c
else
$(error WIFI_BRIDGE_BACKEND must be UNER or ESP_AT)
endif
