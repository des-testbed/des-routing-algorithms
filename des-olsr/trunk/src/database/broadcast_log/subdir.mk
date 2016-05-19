################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/broadcast_log/broadcast_log.c 

OBJS += \
./src/database/broadcast_log/broadcast_log.o 

C_DEPS += \
./src/database/broadcast_log/broadcast_log.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/broadcast_log/%.o: ../src/database/broadcast_log/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


