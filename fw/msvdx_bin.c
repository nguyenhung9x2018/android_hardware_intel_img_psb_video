/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
 * Copyright (c) Imagination Technologies Limited, UK 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include "thread0_bin.h"
#include "thread1_bin.h"

struct msvdx_fw
{
	unsigned int ver;
	unsigned int  text_size;
	unsigned int data_size;
	unsigned int data_location;
};

int main()
{
	unsigned long i = 0;
	FILE *ptr = NULL;

	/* Create msvdx firmware for mrst */
	struct msvdx_fw fw;
	fw.ver = 0x2;
        fw.text_size = ui32MTXDXVAFWTextSize;
        fw.data_size = ui32MTXDXVAFWDataSize;
        fw.data_location = ui32MTXDXVAFWDataLocation;

	ptr = fopen("msvdx_fw.bin", "w");
	if (ptr == NULL) {
            fprintf(stderr,"Create msvdx_fw_mrst.bin failed\n");
            exit(-1);
        }
	fwrite( &fw, sizeof(fw), 1, ptr);
	
	for (i = 0; i < fw.text_size; i++)
	{
		fwrite( &aui32MTXDXVAFWText[i],4, 1, ptr);
	}
	for (i = 0; i < fw.data_size; i++)
	{
		fwrite( &aui32MTXDXVAFWData[i],4, 1, ptr);
	}
	fclose (ptr);


	/* Create msvdx firmware for mfld */
	fw.ver = 0x2;
	fw.text_size = sFrameSwitchingFirmware.uiTextSize / 4;
	fw.data_size = sFrameSwitchingFirmware.uiDataSize / 4;;
	fw.data_location = sFrameSwitchingFirmware.DataOffset + 0x82880000;

	ptr = fopen("msvdx_fw_mfld.bin", "w");
	if (ptr == NULL) {
            fprintf(stderr,"Create msvdx_fw_mfld.bin failed\n");
            exit(-1);
        }
	fwrite( &fw, sizeof(fw), 1, ptr);
	
	for (i = 0; i < fw.text_size; i++)
	{
		fwrite( &sFrameSwitchingFirmware.pui8Text[i*4],4, 1, ptr);
	}
	for (i = 0; i < fw.data_size; i++)
	{
		fwrite( &sFrameSwitchingFirmware.pui8Data[i*4],4, 1, ptr);
	}
	fclose (ptr);
	

	return 0;
}

