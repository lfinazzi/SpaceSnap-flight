################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/command.c \
../Core/Src/comms.c \
../Core/Src/fram.c \
../Core/Src/gpio.c \
../Core/Src/irq.c \
../Core/Src/main.c \
../Core/Src/photo.c \
../Core/Src/protection.c \
../Core/Src/sram.c \
../Core/Src/status.c \
../Core/Src/stm32f2xx_hal_msp.c \
../Core/Src/stm32f2xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f2xx.c \
../Core/Src/telemetry.c 

C_DEPS += \
./Core/Src/command.d \
./Core/Src/comms.d \
./Core/Src/fram.d \
./Core/Src/gpio.d \
./Core/Src/irq.d \
./Core/Src/main.d \
./Core/Src/photo.d \
./Core/Src/protection.d \
./Core/Src/sram.d \
./Core/Src/status.d \
./Core/Src/stm32f2xx_hal_msp.d \
./Core/Src/stm32f2xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f2xx.d \
./Core/Src/telemetry.d 

OBJS += \
./Core/Src/command.o \
./Core/Src/comms.o \
./Core/Src/fram.o \
./Core/Src/gpio.o \
./Core/Src/irq.o \
./Core/Src/main.o \
./Core/Src/photo.o \
./Core/Src/protection.o \
./Core/Src/sram.o \
./Core/Src/status.o \
./Core/Src/stm32f2xx_hal_msp.o \
./Core/Src/stm32f2xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f2xx.o \
./Core/Src/telemetry.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F217xx -c -I../Core/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc -I../Drivers/STM32F2xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F2xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/command.cyclo ./Core/Src/command.d ./Core/Src/command.o ./Core/Src/command.su ./Core/Src/comms.cyclo ./Core/Src/comms.d ./Core/Src/comms.o ./Core/Src/comms.su ./Core/Src/fram.cyclo ./Core/Src/fram.d ./Core/Src/fram.o ./Core/Src/fram.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/irq.cyclo ./Core/Src/irq.d ./Core/Src/irq.o ./Core/Src/irq.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/photo.cyclo ./Core/Src/photo.d ./Core/Src/photo.o ./Core/Src/photo.su ./Core/Src/protection.cyclo ./Core/Src/protection.d ./Core/Src/protection.o ./Core/Src/protection.su ./Core/Src/sram.cyclo ./Core/Src/sram.d ./Core/Src/sram.o ./Core/Src/sram.su ./Core/Src/status.cyclo ./Core/Src/status.d ./Core/Src/status.o ./Core/Src/status.su ./Core/Src/stm32f2xx_hal_msp.cyclo ./Core/Src/stm32f2xx_hal_msp.d ./Core/Src/stm32f2xx_hal_msp.o ./Core/Src/stm32f2xx_hal_msp.su ./Core/Src/stm32f2xx_it.cyclo ./Core/Src/stm32f2xx_it.d ./Core/Src/stm32f2xx_it.o ./Core/Src/stm32f2xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f2xx.cyclo ./Core/Src/system_stm32f2xx.d ./Core/Src/system_stm32f2xx.o ./Core/Src/system_stm32f2xx.su ./Core/Src/telemetry.cyclo ./Core/Src/telemetry.d ./Core/Src/telemetry.o ./Core/Src/telemetry.su

.PHONY: clean-Core-2f-Src

