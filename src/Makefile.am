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
pvr_drv_video_la_LTLIBRARIES = pvr_drv_video.la
pvr_drv_video_ladir = /usr/lib/dri
pvr_drv_video_la_LDFLAGS = -lwsbm -pthread -module -avoid-version -Wl,--no-undefined
pvr_drv_video_la_LIBADD = -ldrm -lX11 -lXv -lm -lXext -lva-x11 /usr/lib/libmemrar.a -lpvr2d -lGL  
AM_CFLAGS = -DDEBUG -DLINUX -I$(top_srcdir)/src/hwdefs $(DRM_CFLAGS) \
		-I$(top_srcdir)/src/powervr_iep_lite/include \
		-I$(top_srcdir)/src/powervr_iep_lite/include/win32 \
		-I$(top_srcdir)/src/powervr_iep_lite/iep_lite \
		-I$(top_srcdir)/src/powervr_iep_lite/fixedpointmaths \
		-I$(top_srcdir)/src/powervr_iep_lite/csc


pvr_drv_video_la_SOURCES = psb_drv_video.c object_heap.c psb_buffer.c psb_buffer_dm.c psb_cmdbuf.c psb_surface.c \
		psb_MPEG2.c psb_MPEG2MC.c psb_MPEG4.c psb_VC1.c psb_H264.c \
		vc1_vlc.c vc1_idx.c psb_deblock.c  \
		lnc_H263ES.c lnc_H264ES.c lnc_MPEG4ES.c lnc_cmdbuf.c \
		lnc_hostcode.c lnc_hostheader.c lnc_ospm.c psb_ws_driver.c \
		pnw_hostheader.c pnw_hostcode.c \
		pnw_cmdbuf.c pnw_H264ES.c pnw_H263ES.c pnw_MPEG4ES.c \
		pnw_H264.c pnw_MPEG2.c pnw_MPEG4.c pnw_hostjpeg.c pnw_jpeg.c \
		psb_output.c  psb_overlay.c psb_texture.c \
		x11/psb_x11.c x11/psb_coverlay.c x11/psb_xvva.c x11/psb_ctexture.c\
		powervr_iep_lite/csc/csc2.c \
		powervr_iep_lite/csc/csc2_data.c \
		powervr_iep_lite/fixedpointmaths/fixedpointmaths.c \
		powervr_iep_lite/iep_lite/iep_lite_api.c \
		powervr_iep_lite/iep_lite/iep_lite_hardware.c \
		powervr_iep_lite/iep_lite/iep_lite_utils.c
#		vc1_ap_i.c vc1_ap_p.c vc1_ap_utils.c vc1_bitplane.c \
#		vc1_shiftreg.c vc1_spmp.c vc1_utils.c 


CFLAGS = -O1 -Wall -ffloat-store -fvisibility=hidden

symbol_info:	pvr_drv_video.la
	objdump -T .libs/pvr_drv_video.so | grep UND | sort -k 5 > Linker_dependencies.txt
	objdump -T .libs/pvr_drv_video.so | grep -v UND | sort -k 5 > Linker_exports.txt	

#install-exec-am:
#	-rm -f $(pvr_drv_video_ladir)/psb_drv_video.so
#	-$(LN_S) $(pvr_drv_video_ladir)/pvr_drv_video.so $(pvr_drv_video_ladir)/psb_drv_video.so
