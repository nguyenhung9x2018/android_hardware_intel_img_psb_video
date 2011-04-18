# INTEL CONFIDENTIAL
# Copyright 2007 Intel Corporation. All Rights Reserved.
#
# The source code contained or described herein and all documents related to
# the source code ("Material") are owned by Intel Corporation or its suppliers
# or licensors. Title to the Material remains with Intel Corporation or its
# suppliers and licensors. The Material may contain trade secrets and
# proprietary and confidential information of Intel Corporation and its
# suppliers and licensors, and is protected by worldwide copyright and trade
# secret laws and treaty provisions. No part of the Material may be used,
# copied, reproduced, modified, published, uploaded, posted, transmitted,
# distributed, or disclosed in any way without Intel's prior express written
# permission. 
# 
# No license under any patent, copyright, trade secret or other intellectual
# property right is granted to or conferred upon you by disclosure or delivery
# of the Materials, either expressly, by implication, inducement, estoppel or
# otherwise. Any license under such intellectual property rights must be
# express and approved by Intel in writing.
#
# Makefile to build MSVDX firmware 

CFLAGS = -DFRAME_SWITCHING_VARIANT=1 -DSLICE_SWITCHING_VARIANT=1
firmware_DATA = msvdx_fw.bin msvdx_fw_mfld.bin msvdx_fw_mfld_DE2.0.bin topaz_fw.bin topazsc_fw.bin 
firmwaredir = /lib/firmware
msvdx_fw_bin_DEPENDENCIES = msvdx_bin
topaz_fw_bin_DEPENDENCIES = topaz_bin
topazsc_fw_bin_DEPENDENCIES = topazsc_bin

noinst_PROGRAMS = msvdx_bin topaz_bin topazsc_bin
msvdx_bin_SOURCES = msvdx_bin.c thread0_ss_bin.c thread0_bin.c thread1_bin.c thread2_bin.c thread3_bin.c
topaz_bin_SOURCES = topaz_bin.c H263Firmware_bin.c H263FirmwareCBR_bin.c H263FirmwareVBR_bin.c H264Firmware_bin.c H264FirmwareCBR_bin.c H264FirmwareVBR_bin.c MPG4Firmware_bin.c MPG4FirmwareCBR_bin.c MPG4FirmwareVBR_bin.c H264FirmwareVCM_bin.c

topazsc_bin_SOURCES = topazsc_bin.c H263MasterFirmwareCBR_bin.c H263MasterFirmwareVBR_bin.c H263MasterFirmware_bin.c \
		    H264MasterFirmwareCBR_bin.c H264MasterFirmwareVBR_bin.c H264MasterFirmware_bin.c \
		    JPEGMasterFirmware_bin.c MPG4MasterFirmwareCBR_bin.c MPG4MasterFirmwareVBR_bin.c \
		    MPG4MasterFirmware_bin.c H263SlaveFirmwareCBR_bin.c H263SlaveFirmwareVBR_bin.c \
		    H263SlaveFirmware_bin.c H264SlaveFirmwareCBR_bin.c H264SlaveFirmwareVBR_bin.c \
		    H264SlaveFirmware_bin.c JPEGSlaveFirmware_bin.c MPG4SlaveFirmwareCBR_bin.c \
		    MPG4SlaveFirmwareVBR_bin.c MPG4SlaveFirmware_bin.c \
		    H264SlaveFirmwareVCM_bin.c H264MasterFirmwareVCM_bin.c

msvdx_fw.bin: msvdx_bin
	./msvdx_bin

msvdx_fw_mfld.bin: msvdx_bin
	./msvdx_bin

msvdx_fw_mfld_DE2.0.bin: msvdx_bin
	./msvdx_bin

topaz_fw.bin: topaz_bin
	./topaz_bin

topazsc_fw.bin: topazsc_bin
	./topazsc_bin

clean-generic:
	rm -f ./msvdx_fw.bin ./msvdx_fw_mfld.bin
