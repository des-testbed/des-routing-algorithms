################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/topology_set/topology_set.c 

OBJS += \
./src/database/topology_set/topology_set.o 

C_DEPS += \
./src/database/topology_set/topology_set.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/topology_set/%.o: ../src/database/topology_set/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


