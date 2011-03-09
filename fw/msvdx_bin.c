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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thread0_ss_bin.h" /* old sliceswitching firmware */
#include "thread0_bin.h" /* new frameswitching firmware for error concealment */
#include "thread1_bin.h"

#define MDFLD_SLICESWITCH_FIRMWARE 1

struct msvdx_fw {
    unsigned int ver;
    unsigned int  text_size;
    unsigned int data_size;
    unsigned int data_location;
};

int main()
{
    unsigned long i = 0;
    unsigned long lseek;
    FILE *ptr = NULL;

    /* Create msvdx firmware for mrst */

    struct msvdx_fw fw;
    fw.ver = 0x0187;
    fw.text_size = ui32MTXDXVAFWTextSize_ss;
    fw.data_size = ui32MTXDXVAFWDataSize_ss;
    fw.data_location = ui32MTXDXVAFWDataLocation_ss;

    ptr = fopen("msvdx_fw.bin", "w");
    if (ptr == NULL) {
        fprintf(stderr, "Create msvdx_fw_mrst.bin failed\n");
        exit(-1);
    }
    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&aui32MTXDXVAFWText_ss[i], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&aui32MTXDXVAFWData_ss[i], 4, 1, ptr);
    }

    lseek = ((sizeof(fw) + (fw.text_size + fw.data_size) * 4 + 0xfff) & ~0xfff);
    fseek(ptr, lseek, SEEK_SET);

    /* Create msvdx firmware for mrst error concealment*/
    /*	fw.ver = 0xec;
            fw.text_size = ui32sMiniSSLegacyTextSize;
            fw.data_size = ui32sMiniSSLegacyDataSize;
            fw.data_location = ui32sMiniSSLegacyDataLocation;

    	fwrite( &fw, sizeof(fw), 1, ptr);

    	for (i = 0; i < fw.text_size; i++)
    	{
    		fwrite( &aui32sMiniSSLegacyText[i],4, 1, ptr);
    	}
    	for (i = 0; i < fw.data_size; i++)
    	{
    		fwrite( &aui32sMiniSSLegacyData[i],4, 1, ptr);
    	}
    	fclose (ptr);
    */
    fw.ver = 0x4cd;
    fw.text_size = ui32MTXDXVAFWTextSize;
    fw.data_size = ui32MTXDXVAFWDataSize;
    fw.data_location = ui32MTXDXVAFWDataLocation;

    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&aui32MTXDXVAFWText[i], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&aui32MTXDXVAFWData[i], 4, 1, ptr);
    }
    fclose(ptr);

#if MDFLD_SLICESWITCH_FIRMWARE
    /* Create msvdx firmware for mfld */
    fw.ver = 0x2;
    fw.text_size = sSliceSwitchingFirmware.uiTextSize / 4;
    fw.data_size = sSliceSwitchingFirmware.uiDataSize / 4;;
    fw.data_location = sSliceSwitchingFirmware.DataOffset + 0x82880000;

    ptr = fopen("msvdx_fw_mfld.bin", "w");
    if (ptr == NULL) {
        fprintf(stderr, "Create msvdx_fw_mfld.bin failed\n");
        exit(-1);
    }
    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&sSliceSwitchingFirmware.pui8Text[i*4], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&sSliceSwitchingFirmware.pui8Data[i*4], 4, 1, ptr);
    }
    fclose(ptr);

#else
    fw.ver = 0x2;
    fw.text_size = sFrameSwitchingFirmware.uiTextSize / 4;
    fw.data_size = sFrameSwitchingFirmware.uiDataSize / 4;;
    fw.data_location = sFrameSwitchingFirmware.DataOffset + 0x82880000;

    ptr = fopen("msvdx_fw_mfld.bin", "w");
    if (ptr == NULL) {
        fprintf(stderr, "Create msvdx_fw_mfld.bin failed\n");
        exit(-1);
    }
    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&sFrameSwitchingFirmware.pui8Text[i*4], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&sFrameSwitchingFirmware.pui8Data[i*4], 4, 1, ptr);
    }
    fclose(ptr);
#endif

    /* Create msvdx firmware for mfld DE2.0 */

    FIRMWARE fw_DE2;

    /* fw_DE2 = sFirmware1133_SS; */
    fw_DE2 = sFirmware1163_SS;
    /* fw_DE2 = sFirmware1133_FS; */

    fw.ver = 0x0496;
    fw.text_size = fw_DE2.uiTextSize / 4;
    fw.data_size = fw_DE2.uiDataSize / 4;;
    fw.data_location = fw_DE2.DataOffset + 0x82880000;

    ptr = fopen("msvdx_fw_mfld_DE2.0.bin", "w");
    if (ptr == NULL) {
        fprintf(stderr, "Create msvdx_fw_mfld_DE2.0.bin failed\n");
        exit(-1);
    }
    fwrite(&fw, sizeof(fw), 1, ptr);

    for (i = 0; i < fw.text_size; i++) {
        fwrite(&fw_DE2.pui8Text[i*4], 4, 1, ptr);
    }
    for (i = 0; i < fw.data_size; i++) {
        fwrite(&fw_DE2.pui8Data[i*4], 4, 1, ptr);
    }
    fclose(ptr);

    return 0;
}

