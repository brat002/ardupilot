#!/bin/sh

#production binary for bootloader
#dfu-util -a 0 --dfuse-address 0x08010000 -D /tmp/ArduCopter.build/revomini_AirbotV2.bin

# bare metal binary

#dfu-util -a 0 --dfuse-address 0x08000000:unprotect:force -D /tmp/ArduCopter.build/revomini_MP32V1F4.bin
#dfu-util -a 0 --dfuse-address 0x08000000:leave -D ../../../../../ArduCopter/revomini_MP32V1F4.bin -R

dfu-util -a 0 --dfuse-address 0x08000000:unprotect:force -D ../../../../../ArduCopter/cl_racing.bin -R

