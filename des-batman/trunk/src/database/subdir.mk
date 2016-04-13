################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/batman_database.c \
../src/database/timeslot.c 

OBJS += \
./src/database/batman_database.o \
./src/database/timeslot.o 

C_DEPS += \
./src/database/batman_database.d \
./src/database/timeslot.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/%.o: ../src/database/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


