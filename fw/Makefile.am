# Makefile to build MSVDX firmware 


firmware_DATA = msvdx_fw.bin topaz_fw.bin
firmwaredir = /lib/firmware
msvdx_fw_bin_DEPENDENCIES = msvdx_bin
topaz_fw_bin_DEPENDENCIES = topaz_bin

noinst_PROGRAMS = msvdx_bin topaz_bin
msvdx_bin_SOURCES = msvdx_bin.c thread0_bin.c
topaz_bin_SOURCES = topaz_bin.c H263Firmware_bin.c H263FirmwareCBR_bin.c H263FirmwareVBR_bin.c H264Firmware_bin.c H264FirmwareCBR_bin.c H264FirmwareVBR_bin.c MPG4Firmware_bin.c MPG4FirmwareCBR_bin.c MPG4FirmwareVBR_bin.c

msvdx_fw.bin: msvdx_bin
	./msvdx_bin

topaz_fw.bin: topaz_bin
	./topaz_bin

clean-generic:
	rm -f ./msvdx_fw.bin
