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
/******************************************************************************

 @File         thread0_bin.h

 @Title        

 @Copyright    Copyright (C)  Imagination Technologies Limited. All Rights Reserved. Strictly Confidential.

 @Platform     

 @Description  

******************************************************************************/
#ifdef DE_ENV 
#include "global.h" 
#endif 
//#if SLICE_SWITCHING_VARIANT 
// This file was automatically generated from ../release/thread0.dnl using dnl2c.

/*
extern unsigned long aui32MTXDXVAFWText[];
extern unsigned long ui32MTXDXVAFWTextSize;

extern unsigned long aui32MTXDXVAFWData[];
extern unsigned long ui32MTXDXVAFWDataSize;

extern unsigned long ui32MTXDXVAFWDataLocation;
*/
typedef struct
{
    const char* psVersion;
    const char* psBuildDate;
    unsigned int uiTextSize;
    unsigned int uiDataSize;
    unsigned int DataOffset;
    const unsigned char* pui8Text;
    const unsigned char* pui8Data;
} FIRMWARE;

//extern const FIRMWARE sSliceSwitchingFirmware;
extern const FIRMWARE sFrameSwitchingFirmware;
extern const FIRMWARE sSliceSwitchingFirmware;
extern const FIRMWARE sFirmware1100_SS;
extern const FIRMWARE sFirmware1133_SS;
extern const FIRMWARE sFirmware1133_FS;
extern const FIRMWARE sFirmware1163_SS; 
#define FIRMWARE_VERSION_DEFINED 
#define FIRMWARE_BUILDDATE_DEFINED 
//#endif /* SLICE_SWITCHING_VARIANT */
