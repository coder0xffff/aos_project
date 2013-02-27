################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Aos_Project.cpp \
../src/Cornet.cpp \
../src/LamportClock.cpp 

OBJS += \
./src/Aos_Project.o \
./src/Cornet.o \
./src/LamportClock.o 

CPP_DEPS += \
./src/Aos_Project.d \
./src/Cornet.d \
./src/LamportClock.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


