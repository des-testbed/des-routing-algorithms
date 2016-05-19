################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/pipeline/olsr_periodic.c \
../src/pipeline/olsr_pipeline.c 

OBJS += \
./src/pipeline/olsr_periodic.o \
./src/pipeline/olsr_pipeline.o 

C_DEPS += \
./src/pipeline/olsr_periodic.d \
./src/pipeline/olsr_pipeline.d 


# Each subdirectory must supply rules for building sources it contributes
src/pipeline/%.o: ../src/pipeline/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


