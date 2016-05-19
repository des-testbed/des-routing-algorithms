################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/pipeline/aodv_periodic.c \
../src/pipeline/aodv_pipeline.c 

OBJS += \
./src/pipeline/aodv_periodic.o \
./src/pipeline/aodv_pipeline.o 

C_DEPS += \
./src/pipeline/aodv_periodic.d \
./src/pipeline/aodv_pipeline.d 


# Each subdirectory must supply rules for building sources it contributes
src/pipeline/%.o: ../src/pipeline/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -DHASH_DEBUG=1 -Wall -c -fmessage-length=0 -DHASH_FUNCTION=HASH_BER -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


