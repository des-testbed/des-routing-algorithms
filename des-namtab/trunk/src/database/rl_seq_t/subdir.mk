################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/database/rl_seq_t/rl_seq.c 

OBJS += \
./src/database/rl_seq_t/rl_seq.o 

C_DEPS += \
./src/database/rl_seq_t/rl_seq.d 


# Each subdirectory must supply rules for building sources it contributes
src/database/rl_seq_t/%.o: ../src/database/rl_seq_t/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


