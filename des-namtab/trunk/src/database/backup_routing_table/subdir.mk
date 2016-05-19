################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/backup_routing_table/batman_brt.c \
../src/database/backup_routing_table/batman_brt_nht.c 

OBJS += \
./src/database/backup_routing_table/batman_brt.o \
./src/database/backup_routing_table/batman_brt_nht.o 

C_DEPS += \
./src/database/backup_routing_table/batman_brt.d \
./src/database/backup_routing_table/batman_brt_nht.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/backup_routing_table/%.o: ../src/database/backup_routing_table/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


