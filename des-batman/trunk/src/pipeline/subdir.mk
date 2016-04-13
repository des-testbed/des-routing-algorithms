################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/pipeline/batman_periodic.c \
../src/pipeline/batman_pipeline.c 

OBJS += \
./src/pipeline/batman_periodic.o \
./src/pipeline/batman_pipeline.o 

C_DEPS += \
./src/pipeline/batman_periodic.d \
./src/pipeline/batman_pipeline.d 


# Each subdirectory must supply rules for building sources it contributes
src/pipeline/%.o: ../src/pipeline/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


