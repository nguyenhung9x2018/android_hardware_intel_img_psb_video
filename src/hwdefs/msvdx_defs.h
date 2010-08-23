/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
 * Copyright 2005-2007 Imagination Technologies Limited. All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used,
 * copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed, or disclosed in any way without Intel's prior express written
 * permission. 
 * 
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 */

/*!****************************************************************************
@File			msvdx_defs.h

@Title			System Description Header

@Author			Imagination Technologies

@date   		20 Decemner 2006

@Platform		generic

@Description	This header provides hardware-specific declarations and macros

@DoxygenVer		

******************************************************************************/

/******************************************************************************
Modifications :-

$Log: msvdx_defs.h $
*/

#ifndef _MSVDX_DEFS_H_
#define _MSVDX_DEFS_H_

#define MSVDX_REG_SIZE 	0x4000

/* MSVDX Register base definitions														*/
#define REG_MSVDX_MTX_OFFSET		0x00000000
#define REG_MSVDX_VDMC_OFFSET		0x00000400
#define REG_MSVDX_VDEB_OFFSET		0x00000480
#define REG_MSVDX_DMAC_OFFSET		0x00000500
#define REG_MSVDX_SYS_OFFSET		0x00000600
#define REG_MSVDX_VEC_IQRAM_OFFSET	0x00000700
#define REG_MSVDX_VEC_OFFSET		0x00000800
#define REG_MSVDX_CMD_OFFSET		0x00001000
#define REG_MSVDX_VEC_RAM_OFFSET	0x00002000
#define REG_MSVDX_VEC_VLC_OFFSET	0x00003000

#define REG_MSVDX_MTX_SIZE			0x00000400
#define REG_MSVDX_VDMC_SIZE			0x00000080
#define REG_MSVDX_VDEB_SIZE			0x00000080
#define REG_MSVDX_DMAC_SIZE			0x00000100
#define REG_MSVDX_SYS_SIZE			0x00000100
#define REG_MSVDX_VEC_IQRAM_SIZE	0x00000100
#define REG_MSVDX_VEC_SIZE			0x00000800
#define REG_MSVDX_CMD_SIZE			0x00001000
#define REG_MSVDX_VEC_RAM_SIZE		0x00001000
#define REG_MSVDX_VEC_VLC_SIZE		0x00002000

#endif /* _MSVDX_DEFS_H_ */
