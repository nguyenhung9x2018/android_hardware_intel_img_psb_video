# Makefile to build MSVDX firmware 


firmware_DATA = msvdx_fw.bin msvdx_fw_mfld.bin topaz_fw.bin topazsc_fw.bin 
firmwaredir = /lib/firmware
msvdx_fw_bin_DEPENDENCIES = msvdx_bin
topaz_fw_bin_DEPENDENCIES = topaz_bin
topazsc_fw_bin_DEPENDENCIES = topazsc_bin

noinst_PROGRAMS = msvdx_bin topaz_bin topazsc_bin
msvdx_bin_SOURCES = msvdx_bin.c thread0_bin.c thread1_bin.c
topaz_bin_SOURCES = topaz_bin.c H263Firmware_bin.c H263FirmwareCBR_bin.c H263FirmwareVBR_bin.c H264Firmware_bin.c H264FirmwareCBR_bin.c H264FirmwareVBR_bin.c MPG4Firmware_bin.c MPG4FirmwareCBR_bin.c MPG4FirmwareVBR_bin.c

topazsc_bin_SOURCES = topazsc_bin.c H263MasterFirmwareCBR_bin.c H263MasterFirmwareVBR_bin.c H263MasterFirmware_bin.c \
		    H264MasterFirmwareCBR_bin.c H264MasterFirmwareVBR_bin.c H264MasterFirmware_bin.c \
		    JPEGMasterFirmware_bin.c MPG4MasterFirmwareCBR_bin.c MPG4MasterFirmwareVBR_bin.c \
		    MPG4MasterFirmware_bin.c H263SlaveFirmwareCBR_bin.c H263SlaveFirmwareVBR_bin.c \
		    H263SlaveFirmware_bin.c H264SlaveFirmwareCBR_bin.c H264SlaveFirmwareVBR_bin.c \
		    H264SlaveFirmware_bin.c JPEGSlaveFirmware_bin.c MPG4SlaveFirmwareCBR_bin.c \
		    MPG4SlaveFirmwareVBR_bin.c MPG4SlaveFirmware_bin.c

msvdx_fw.bin: msvdx_bin
	./msvdx_bin

msvdx_fw_mfld.bin: msvdx_bin
	./msvdx_bin

topaz_fw.bin: topaz_bin
	./topaz_bin

topazsc_fw.bin: topazsc_bin
	./topazsc_bin

clean-generic:
	rm -f ./msvdx_fw.bin ./msvdx_fw_mfld.bin
