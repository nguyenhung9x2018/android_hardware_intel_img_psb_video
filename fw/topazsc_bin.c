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

#include "JPEGMasterFirmware_bin.h"
#include "JPEGSlaveFirmware_bin.h"

#include "H263MasterFirmware_bin.h"
#include "H263MasterFirmwareCBR_bin.h"
#include "H263MasterFirmwareVBR_bin.h"
#include "H264MasterFirmware_bin.h"
#include "H264MasterFirmwareCBR_bin.h"
#include "H264MasterFirmwareVBR_bin.h"
#include "MPG4MasterFirmware_bin.h"
#include "MPG4MasterFirmwareCBR_bin.h"
#include "MPG4MasterFirmwareVBR_bin.h"

#include "H263SlaveFirmware_bin.h"
#include "H263SlaveFirmwareCBR_bin.h"
#include "H263SlaveFirmwareVBR_bin.h"
#include "H264SlaveFirmware_bin.h"
#include "H264SlaveFirmwareCBR_bin.h"
#include "H264SlaveFirmwareVBR_bin.h"
#include "MPG4SlaveFirmware_bin.h"
#include "MPG4SlaveFirmwareCBR_bin.h"
#include "MPG4SlaveFirmwareVBR_bin.h"


#define FW_VER 0x5B
#define FW_FILE_NAME "topazsc_fw.bin"

#define FW_MASTER_INFO(codec,prefix) \
	{ FW_MASTER_##codec,\
	  { FW_VER,\
	    FW_MASTER_##codec,\
	    ui32##prefix##_MasterMTXTOPAZFWTextSize,\
	    ui32##prefix##_MasterMTXTOPAZFWDataSize,\
	    ui32##prefix##_MasterMTXTOPAZFWDataOrigin\
	  },\
	  aui32##prefix##_MasterMTXTOPAZFWText, aui32##prefix##_MasterMTXTOPAZFWData \
	}

#define FW_SLAVE_INFO(codec,prefix) \
	{ FW_SLAVE_##codec,\
	  { FW_VER,\
	    FW_SLAVE_##codec,\
	    ui32##prefix##_SlaveMTXTOPAZFWTextSize,\
	    ui32##prefix##_SlaveMTXTOPAZFWDataSize,\
	    ui32##prefix##_SlaveMTXTOPAZFWDataOrigin\
	  },\
	  aui32##prefix##_SlaveMTXTOPAZFWText, aui32##prefix##_SlaveMTXTOPAZFWData \
	}



struct topaz_fw_info_item_s
{
	unsigned short ver;
	unsigned short codec;
    
	unsigned int  text_size;
	unsigned int data_size;
	unsigned int data_location;
};
typedef struct topaz_fw_info_item_s topaz_fw_info_item_t;

enum topaz_fw_codec_e {
    FW_MASTER_JPEG = 0,
    FW_SLAVE_JPEG,
    FW_MASTER_H264_NO_RC,
    FW_SLAVE_H264_NO_RC,
    FW_MASTER_H264_VBR,
    FW_SLAVE_H264_VBR,
    FW_MASTER_H264_CBR,
    FW_SLAVE_H264_CBR,
    FW_MASTER_H263_NO_RC,
    FW_SLAVE_H263_NO_RC,
    FW_MASTER_H263_VBR,
    FW_SLAVE_H263_VBR,
    FW_MASTER_H263_CBR,
    FW_SLAVE_H263_CBR,
    FW_MASTER_MPEG4_NO_RC,
    FW_SLAVE_MPEG4_NO_RC,
    FW_MASTER_MPEG4_VBR,
    FW_SLAVE_MPEG4_VBR,
    FW_MASTER_MPEG4_CBR,
    FW_SLAVE_MPEG4_CBR,
    FW_NUM
};
typedef enum topaz_fw_codec_e topaz_fw_codec_t;

struct fw_table_s
{
    topaz_fw_codec_t index;
    topaz_fw_info_item_t header;
    unsigned long *fw_text;
    unsigned long *fw_data;
};
typedef struct fw_table_s fw_table_t;

int main ()
{
    FILE *fp = NULL;
    topaz_fw_codec_t iter = FW_MASTER_JPEG;
    unsigned int size = 0;
    unsigned int i;

    fw_table_t topaz_fw_table[] = {
        /* index   header
         * { ver, codec, text_size, data_size, date_location }
         * fw_text fw_data */
	FW_MASTER_INFO(JPEG,JPEG),
	FW_SLAVE_INFO(JPEG,JPEG),

	FW_MASTER_INFO(H264_NO_RC,H264),
	FW_SLAVE_INFO(H264_NO_RC,H264),
	FW_MASTER_INFO(H264_VBR,H264VBR),
	FW_SLAVE_INFO(H264_VBR,H264VBR),
	FW_MASTER_INFO(H264_CBR,H264CBR),
	FW_SLAVE_INFO(H264_CBR,H264CBR),

	FW_MASTER_INFO(H263_NO_RC,H263),
	FW_SLAVE_INFO(H263_NO_RC,H263),
	FW_MASTER_INFO(H263_VBR,H263VBR),
	FW_SLAVE_INFO(H263_VBR,H263VBR),
	FW_MASTER_INFO(H263_CBR,H263CBR),
	FW_SLAVE_INFO(H263_CBR,H263CBR),

	FW_MASTER_INFO(MPEG4_NO_RC,MPG4),
	FW_SLAVE_INFO(MPEG4_NO_RC,MPG4),
	FW_MASTER_INFO(MPEG4_VBR,MPG4VBR),
	FW_SLAVE_INFO(MPEG4_VBR,MPG4VBR),
	FW_MASTER_INFO(MPEG4_CBR,MPG4CBR),
	FW_SLAVE_INFO(MPEG4_CBR,MPG4CBR),
    };
    
    /* open file  */
    fp = fopen (FW_FILE_NAME, "w");

    if (NULL == fp)
	return -1;

    /* write fw table into the file */
    while (iter < FW_NUM) {
	/* record the size use bytes */
	topaz_fw_table[iter].header.data_size *= 4;
	topaz_fw_table[iter].header.text_size *= 4;

	/* write header */
	fwrite (&(topaz_fw_table[iter].header), sizeof (topaz_fw_table[iter].header), 1, fp);

	/* write text */
	size = topaz_fw_table[iter].header.text_size;
	fwrite (topaz_fw_table[iter].fw_text, 1, size, fp);

	/* write data */
	size = topaz_fw_table[iter].header.data_size;
        fwrite (topaz_fw_table[iter].fw_data, 1, size, fp);

        ++iter;
    }
    
    /* close file */
    fclose (fp);

    return 0;
}
