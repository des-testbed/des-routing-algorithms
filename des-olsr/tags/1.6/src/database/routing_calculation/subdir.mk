################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/routing_calculation/route_calculation.c 

OBJS += \
./src/database/routing_calculation/route_calculation.o 

C_DEPS += \
./src/database/routing_calculation/route_calculation.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/routing_calculation/%.o: ../src/database/routing_calculation/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


