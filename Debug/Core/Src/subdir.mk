################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/encoder.c \
../Core/Src/eventManagers.c \
../Core/Src/fonts.c \
../Core/Src/i2c_manager.c \
../Core/Src/main.c \
../Core/Src/menusystem.c \
../Core/Src/motor_control.c \
../Core/Src/mpu6050.c \
../Core/Src/oled_ssd1306_dma.c \
../Core/Src/oled_utils.c \
../Core/Src/screenWrappers.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c \
../Core/Src/uner_protocol.c \
../Core/Src/utils.c 

OBJS += \
./Core/Src/encoder.o \
./Core/Src/eventManagers.o \
./Core/Src/fonts.o \
./Core/Src/i2c_manager.o \
./Core/Src/main.o \
./Core/Src/menusystem.o \
./Core/Src/motor_control.o \
./Core/Src/mpu6050.o \
./Core/Src/oled_ssd1306_dma.o \
./Core/Src/oled_utils.o \
./Core/Src/screenWrappers.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o \
./Core/Src/uner_protocol.o \
./Core/Src/utils.o 

C_DEPS += \
./Core/Src/encoder.d \
./Core/Src/eventManagers.d \
./Core/Src/fonts.d \
./Core/Src/i2c_manager.d \
./Core/Src/main.d \
./Core/Src/menusystem.d \
./Core/Src/motor_control.d \
./Core/Src/mpu6050.d \
./Core/Src/oled_ssd1306_dma.d \
./Core/Src/oled_utils.d \
./Core/Src/screenWrappers.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d \
./Core/Src/uner_protocol.d \
./Core/Src/utils.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I"C:/Users/kobac/OneDrive/Escritorio/Facultad/Microcontroladores/Auto Proyecto/HAL Libs/TCRT5000/Inc" -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Drivers/STM32F1xx_HAL_Driver/Inc -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Drivers/CMSIS/Device/ST/STM32F1xx/Include -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Middlewares/ST/STM32_USB_Device_Library/Core/Inc -IC:/Users/kobac/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/encoder.cyclo ./Core/Src/encoder.d ./Core/Src/encoder.o ./Core/Src/encoder.su ./Core/Src/eventManagers.cyclo ./Core/Src/eventManagers.d ./Core/Src/eventManagers.o ./Core/Src/eventManagers.su ./Core/Src/fonts.cyclo ./Core/Src/fonts.d ./Core/Src/fonts.o ./Core/Src/fonts.su ./Core/Src/i2c_manager.cyclo ./Core/Src/i2c_manager.d ./Core/Src/i2c_manager.o ./Core/Src/i2c_manager.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/menusystem.cyclo ./Core/Src/menusystem.d ./Core/Src/menusystem.o ./Core/Src/menusystem.su ./Core/Src/motor_control.cyclo ./Core/Src/motor_control.d ./Core/Src/motor_control.o ./Core/Src/motor_control.su ./Core/Src/mpu6050.cyclo ./Core/Src/mpu6050.d ./Core/Src/mpu6050.o ./Core/Src/mpu6050.su ./Core/Src/oled_ssd1306_dma.cyclo ./Core/Src/oled_ssd1306_dma.d ./Core/Src/oled_ssd1306_dma.o ./Core/Src/oled_ssd1306_dma.su ./Core/Src/oled_utils.cyclo ./Core/Src/oled_utils.d ./Core/Src/oled_utils.o ./Core/Src/oled_utils.su ./Core/Src/screenWrappers.cyclo ./Core/Src/screenWrappers.d ./Core/Src/screenWrappers.o ./Core/Src/screenWrappers.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su ./Core/Src/uner_protocol.cyclo ./Core/Src/uner_protocol.d ./Core/Src/uner_protocol.o ./Core/Src/uner_protocol.su ./Core/Src/utils.cyclo ./Core/Src/utils.d ./Core/Src/utils.o ./Core/Src/utils.su

.PHONY: clean-Core-2f-Src

