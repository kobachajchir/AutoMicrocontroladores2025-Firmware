################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc.c \
../Core/Src/app_globals.c \
../Core/Src/dma.c \
../Core/Src/encoder.c \
../Core/Src/esp01_library.c \
../Core/Src/eventManagers.c \
../Core/Src/fonts.c \
../Core/Src/gpio.c \
../Core/Src/i2c.c \
../Core/Src/i2c_manager.c \
../Core/Src/main.c \
../Core/Src/menu_definition.c \
../Core/Src/menusystem.c \
../Core/Src/motor_control.c \
../Core/Src/mpu6050.c \
../Core/Src/notificationWrappers.c \
../Core/Src/oled_utils.c \
../Core/Src/permissions.c \
../Core/Src/screenWrappers.c \
../Core/Src/spi.c \
../Core/Src/ssd1306.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c \
../Core/Src/tim.c \
../Core/Src/ui_event_router.c \
../Core/Src/uner_app.c \
../Core/Src/uner_core.c \
../Core/Src/uner_handle.c \
../Core/Src/uner_router.c \
../Core/Src/uner_transport_nrf24_spi2.c \
../Core/Src/uner_transport_uart1_dma.c \
../Core/Src/uner_transport_usbcdc.c \
../Core/Src/usart.c \
../Core/Src/usb.c \
../Core/Src/user_button.c \
../Core/Src/utils.c 

OBJS += \
./Core/Src/adc.o \
./Core/Src/app_globals.o \
./Core/Src/dma.o \
./Core/Src/encoder.o \
./Core/Src/esp01_library.o \
./Core/Src/eventManagers.o \
./Core/Src/fonts.o \
./Core/Src/gpio.o \
./Core/Src/i2c.o \
./Core/Src/i2c_manager.o \
./Core/Src/main.o \
./Core/Src/menu_definition.o \
./Core/Src/menusystem.o \
./Core/Src/motor_control.o \
./Core/Src/mpu6050.o \
./Core/Src/notificationWrappers.o \
./Core/Src/oled_utils.o \
./Core/Src/permissions.o \
./Core/Src/screenWrappers.o \
./Core/Src/spi.o \
./Core/Src/ssd1306.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o \
./Core/Src/tim.o \
./Core/Src/ui_event_router.o \
./Core/Src/uner_app.o \
./Core/Src/uner_core.o \
./Core/Src/uner_handle.o \
./Core/Src/uner_router.o \
./Core/Src/uner_transport_nrf24_spi2.o \
./Core/Src/uner_transport_uart1_dma.o \
./Core/Src/uner_transport_usbcdc.o \
./Core/Src/usart.o \
./Core/Src/usb.o \
./Core/Src/user_button.o \
./Core/Src/utils.o 

C_DEPS += \
./Core/Src/adc.d \
./Core/Src/app_globals.d \
./Core/Src/dma.d \
./Core/Src/encoder.d \
./Core/Src/esp01_library.d \
./Core/Src/eventManagers.d \
./Core/Src/fonts.d \
./Core/Src/gpio.d \
./Core/Src/i2c.d \
./Core/Src/i2c_manager.d \
./Core/Src/main.d \
./Core/Src/menu_definition.d \
./Core/Src/menusystem.d \
./Core/Src/motor_control.d \
./Core/Src/mpu6050.d \
./Core/Src/notificationWrappers.d \
./Core/Src/oled_utils.d \
./Core/Src/permissions.d \
./Core/Src/screenWrappers.d \
./Core/Src/spi.d \
./Core/Src/ssd1306.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d \
./Core/Src/tim.d \
./Core/Src/ui_event_router.d \
./Core/Src/uner_app.d \
./Core/Src/uner_core.d \
./Core/Src/uner_handle.d \
./Core/Src/uner_router.d \
./Core/Src/uner_transport_nrf24_spi2.d \
./Core/Src/uner_transport_uart1_dma.d \
./Core/Src/uner_transport_usbcdc.d \
./Core/Src/usart.d \
./Core/Src/usb.d \
./Core/Src/user_button.d \
./Core/Src/utils.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/kobac/OneDrive/Escritorio/Facultad/Microcontroladores/Auto Proyecto/HAL Libs/TCRT5000/Inc" -I"C:/Users/kobac/OneDrive/Escritorio/Facultad/Microcontroladores/Auto Proyecto/HAL Libs/TCRT5000/Debug" -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7/Drivers/STM32F1xx_HAL_Driver/Inc -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7/Drivers/CMSIS/Device/ST/STM32F1xx/Include -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7/Drivers/CMSIS/Include -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc.cyclo ./Core/Src/adc.d ./Core/Src/adc.o ./Core/Src/adc.su ./Core/Src/app_globals.cyclo ./Core/Src/app_globals.d ./Core/Src/app_globals.o ./Core/Src/app_globals.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/encoder.cyclo ./Core/Src/encoder.d ./Core/Src/encoder.o ./Core/Src/encoder.su ./Core/Src/esp01_library.cyclo ./Core/Src/esp01_library.d ./Core/Src/esp01_library.o ./Core/Src/esp01_library.su ./Core/Src/eventManagers.cyclo ./Core/Src/eventManagers.d ./Core/Src/eventManagers.o ./Core/Src/eventManagers.su ./Core/Src/fonts.cyclo ./Core/Src/fonts.d ./Core/Src/fonts.o ./Core/Src/fonts.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/i2c.cyclo ./Core/Src/i2c.d ./Core/Src/i2c.o ./Core/Src/i2c.su ./Core/Src/i2c_manager.cyclo ./Core/Src/i2c_manager.d ./Core/Src/i2c_manager.o ./Core/Src/i2c_manager.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/menu_definition.cyclo ./Core/Src/menu_definition.d ./Core/Src/menu_definition.o ./Core/Src/menu_definition.su ./Core/Src/menusystem.cyclo ./Core/Src/menusystem.d ./Core/Src/menusystem.o ./Core/Src/menusystem.su ./Core/Src/motor_control.cyclo ./Core/Src/motor_control.d ./Core/Src/motor_control.o ./Core/Src/motor_control.su ./Core/Src/mpu6050.cyclo ./Core/Src/mpu6050.d ./Core/Src/mpu6050.o ./Core/Src/mpu6050.su ./Core/Src/notificationWrappers.cyclo ./Core/Src/notificationWrappers.d ./Core/Src/notificationWrappers.o ./Core/Src/notificationWrappers.su ./Core/Src/oled_utils.cyclo ./Core/Src/oled_utils.d ./Core/Src/oled_utils.o ./Core/Src/oled_utils.su ./Core/Src/permissions.cyclo ./Core/Src/permissions.d ./Core/Src/permissions.o ./Core/Src/permissions.su ./Core/Src/screenWrappers.cyclo ./Core/Src/screenWrappers.d ./Core/Src/screenWrappers.o ./Core/Src/screenWrappers.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/ssd1306.cyclo ./Core/Src/ssd1306.d ./Core/Src/ssd1306.o ./Core/Src/ssd1306.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/ui_event_router.cyclo ./Core/Src/ui_event_router.d ./Core/Src/ui_event_router.o ./Core/Src/ui_event_router.su ./Core/Src/uner_app.cyclo ./Core/Src/uner_app.d ./Core/Src/uner_app.o ./Core/Src/uner_app.su ./Core/Src/uner_core.cyclo ./Core/Src/uner_core.d ./Core/Src/uner_core.o ./Core/Src/uner_core.su ./Core/Src/uner_handle.cyclo ./Core/Src/uner_handle.d ./Core/Src/uner_handle.o ./Core/Src/uner_handle.su ./Core/Src/uner_router.cyclo ./Core/Src/uner_router.d ./Core/Src/uner_router.o ./Core/Src/uner_router.su ./Core/Src/uner_transport_nrf24_spi2.cyclo ./Core/Src/uner_transport_nrf24_spi2.d ./Core/Src/uner_transport_nrf24_spi2.o ./Core/Src/uner_transport_nrf24_spi2.su ./Core/Src/uner_transport_uart1_dma.cyclo ./Core/Src/uner_transport_uart1_dma.d ./Core/Src/uner_transport_uart1_dma.o ./Core/Src/uner_transport_uart1_dma.su ./Core/Src/uner_transport_usbcdc.cyclo ./Core/Src/uner_transport_usbcdc.d ./Core/Src/uner_transport_usbcdc.o ./Core/Src/uner_transport_usbcdc.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su ./Core/Src/usb.cyclo ./Core/Src/usb.d ./Core/Src/usb.o ./Core/Src/usb.su ./Core/Src/user_button.cyclo ./Core/Src/user_button.d ./Core/Src/user_button.o ./Core/Src/user_button.su ./Core/Src/utils.cyclo ./Core/Src/utils.d ./Core/Src/utils.o ./Core/Src/utils.su

.PHONY: clean-Core-2f-Src

