################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/link_set/link_set.c \
../src/database/link_set/sliding_window.c 

OBJS += \
./src/database/link_set/link_set.o \
./src/database/link_set/sliding_window.o 

C_DEPS += \
./src/database/link_set/link_set.d \
./src/database/link_set/sliding_window.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/link_set/%.o: ../src/database/link_set/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


