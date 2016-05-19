################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/inv_routing_table/batman_invrt.c \
../src/database/inv_routing_table/batman_irt_nht.c 

OBJS += \
./src/database/inv_routing_table/batman_invrt.o \
./src/database/inv_routing_table/batman_irt_nht.o 

C_DEPS += \
./src/database/inv_routing_table/batman_invrt.d \
./src/database/inv_routing_table/batman_irt_nht.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/inv_routing_table/%.o: ../src/database/inv_routing_table/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


