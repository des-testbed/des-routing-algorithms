################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/neighbor_set/neighbor_set.c 

OBJS += \
./src/database/neighbor_set/neighbor_set.o 

C_DEPS += \
./src/database/neighbor_set/neighbor_set.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/neighbor_set/%.o: ../src/database/neighbor_set/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


