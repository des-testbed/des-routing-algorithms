################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/routing_table/batman_rt.c \
../src/database/routing_table/batman_sw.c 

OBJS += \
./src/database/routing_table/batman_rt.o \
./src/database/routing_table/batman_sw.o 

C_DEPS += \
./src/database/routing_table/batman_rt.d \
./src/database/routing_table/batman_sw.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/routing_table/%.o: ../src/database/routing_table/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


