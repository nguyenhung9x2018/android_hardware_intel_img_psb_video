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


/*
 * Authors:
 *    Elaine Wang <zhaohan.ren@intel.com>
 *
 */

#include <stdio.h>

#include "H263Firmware_bin.h"
#include "H263FirmwareCBR_bin.h"
#include "H263FirmwareVBR_bin.h"
#include "H263MasterFirmware_bin.h"
#include "H263MasterFirmwareCBR_bin.h"
#include "H263MasterFirmwareERC_bin.h"
#include "H263MasterFirmwareLLRC_bin.h"
#include "H263MasterFirmwareVBR_bin.h"
#include "H263SlaveFirmware_bin.h"
#include "H263SlaveFirmwareCBR_bin.h"
#include "H263SlaveFirmwareVBR_bin.h"
#include "H264Firmware_bin.h"
#include "H264FirmwareCBR_bin.h"
#include "H264FirmwareVBR_bin.h"
#include "H264MasterFirmware_bin.h"
#include "H264MasterFirmwareCBR_bin.h"
#include "H264MasterFirmwareERC_bin.h"
#include "H264MasterFirmwareLLRC_bin.h"
#include "H264MasterFirmwareVBR_bin.h"
#include "H264MasterFirmwareVCM_bin.h"
#include "H264MVCMasterFirmware_bin.h"
#include "H264MVCMasterFirmwareCBR_bin.h"
#include "H264MVCMasterFirmwareERC_bin.h"
#include "H264MVCMasterFirmwareLLRC_bin.h"
#include "H264MVCMasterFirmwareVBR_bin.h"
#include "H264SlaveFirmware_bin.h"
#include "H264SlaveFirmwareCBR_bin.h"
#include "H264SlaveFirmwareVBR_bin.h"
#include "JPEGFirmware_bin.h"
#include "JPEGMasterFirmware_bin.h"
#include "JPEGSlaveFirmware_bin.h"
#include "MPG2MasterFirmware_bin.h"
#include "MPG2MasterFirmwareCBR_bin.h"
#include "MPG2MasterFirmwareERC_bin.h"
#include "MPG2MasterFirmwareLLRC_bin.h"
#include "MPG2MasterFirmwareVBR_bin.h"
#include "MPG4Firmware_bin.h"
#include "MPG4FirmwareCBR_bin.h"
#include "MPG4FirmwareVBR_bin.h"
#include "MPG4MasterFirmware_bin.h"
#include "MPG4MasterFirmwareCBR_bin.h"
#include "MPG4MasterFirmwareERC_bin.h"
#include "MPG4MasterFirmwareLLRC_bin.h"
#include "MPG4MasterFirmwareVBR_bin.h"
#include "MPG4SlaveFirmware_bin.h"
#include "MPG4SlaveFirmwareCBR_bin.h"
#include "MPG4SlaveFirmwareVBR_bin.h"
#include "H264MasterFirmwareALL_bin.h"
#include "thread0_bin.h"


#define FW_VER 0x5D
#define FW_FILE_NAME "topazhp_fw.bin"

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

struct topaz_fw_info_item_s {
    unsigned short ver;
    unsigned short codec;

    unsigned int  text_size;
    unsigned int data_size;
    unsigned int data_location;
};

typedef struct topaz_fw_info_item_s topaz_fw_info_item_t;

/* Commented out all the SLAVE structure according to DDK */
enum topaz_fw_codec_e {
        FW_MASTER_JPEG = 0,                         //!< JPEG
        FW_MASTER_H264_NO_RC,           //!< H264 with no rate control
        FW_MASTER_H264_VBR,                     //!< H264 variable bitrate
        FW_MASTER_H264_CBR,                     //!< H264 constant bitrate
        FW_MASTER_H264_VCM,                     //!< H264 video conferance mode
        FW_MASTER_H263_NO_RC,           //!< H263 with no rate control
        FW_MASTER_H263_VBR,                     //!< H263 variable bitrate
        FW_MASTER_H263_CBR,                     //!< H263 constant bitrate
        FW_MASTER_MPEG4_NO_RC,          //!< MPEG4 with no rate control
        FW_MASTER_MPEG4_VBR,            //!< MPEG4 variable bitrate
        FW_MASTER_MPEG4_CBR,            //!< MPEG4 constant bitrate
        FW_MASTER_MPEG2_NO_RC,          //!< MPEG2 with no rate control
        FW_MASTER_MPEG2_VBR,            //!< MPEG2 variable bitrate
        FW_MASTER_MPEG2_CBR,            //!< MPEG2 constant bitrate
        FW_MASTER_H264_ERC,                     //!< H264 example rate control
        FW_MASTER_H263_ERC,                     //!< H263 example rate control
        FW_MASTER_MPEG4_ERC,            //!< MPEG4 example rate control
        FW_MASTER_MPEG2_ERC,            //!< MPEG2 example rate control
        FW_MASTER_H264_LLRC,            //!< H264 low-latency rate control
        FW_MASTER_H264MVC_NO_RC,        //!< MVC H264 with no rate control
        FW_MASTER_H264MVC_CBR,          //!< MVC H264 constant bitrate
        FW_MASTER_H264MVC_VBR,          //!< MVC H264 variable bitrate
        FW_MASTER_H264MVC_ERC,          //!< MVC H264 example rate control
        FW_MASTER_H264MVC_LLRC,         //!< MVC H264 low-latency rate control
        FW_MASTER_H264ALL,
	FW_NUM
};

typedef enum topaz_fw_codec_e topaz_fw_codec_t;

struct fw_table_s {
    topaz_fw_codec_t index;
    topaz_fw_info_item_t header;
    unsigned long *fw_text;
    unsigned long *fw_data;
};

typedef struct fw_table_s fw_table_t;

int main()
{
    FILE *fp = NULL;
    topaz_fw_codec_t iter = FW_MASTER_JPEG;
    unsigned int size = 0;
    unsigned int i;

    fw_table_t topaz_fw_table[] = {
        FW_MASTER_INFO(JPEG, JPEG),	//FW_MASTER_JPEG = 0,                         //!< JPEG
        FW_MASTER_INFO(H264_NO_RC, H264),//FW_MASTER_H264_NO_RC,           //!< H264 with no rate control
        FW_MASTER_INFO(H264_VBR, H264VBR),//FW_MASTER_H264_VBR,                     //!< H264 variable bitrate
        FW_MASTER_INFO(H264_CBR, H264CBR),//FW_MASTER_H264_CBR,                     //!< H264 constant bitrate
        FW_MASTER_INFO(H264_VCM, H264VCM),//FW_MASTER_H264_VCM,                     //!< H264 video conferance mode
        FW_MASTER_INFO(H263_NO_RC, H263),//FW_MASTER_H263_NO_RC,           //!< H263 with no rate control
        FW_MASTER_INFO(H263_VBR, H263VBR),//FW_MASTER_H263_VBR,                     //!< H263 variable bitrate
        FW_MASTER_INFO(H263_CBR, H263CBR),//FW_MASTER_H263_CBR,                     //!< H263 constant bitrate
        FW_MASTER_INFO(MPEG4_NO_RC, MPG4),//FW_MASTER_MPEG4_NO_RC,          //!< MPEG4 with no rate control
        FW_MASTER_INFO(MPEG4_VBR, MPG4VBR),//FW_MASTER_MPEG4_VBR,            //!< MPEG4 variable bitrate
        FW_MASTER_INFO(MPEG4_CBR, MPG4CBR),//FW_MASTER_MPEG4_CBR,            //!< MPEG4 constant bitrate
        FW_MASTER_INFO(MPEG2_NO_RC, MPG2),//FW_MASTER_MPEG2_NO_RC,          //!< MPEG2 with no rate control
        FW_MASTER_INFO(MPEG2_VBR, MPG2VBR),//FW_MASTER_MPEG2_VBR,            //!< MPEG2 variable bitrate
        FW_MASTER_INFO(MPEG2_CBR, MPG2CBR),//FW_MASTER_MPEG2_CBR,            //!< MPEG2 constant bitrate
        FW_MASTER_INFO(H264_ERC, H264ERC),//FW_MASTER_H264_ERC,                     //!< H264 example rate control
        FW_MASTER_INFO(H263_ERC, H263ERC),//FW_MASTER_H263_ERC,                     //!< H263 example rate control
        FW_MASTER_INFO(MPEG4_ERC, MPG4ERC),//FW_MASTER_MPEG4_ERC,            //!< MPEG4 example rate control
        FW_MASTER_INFO(MPEG2_ERC, MPG2ERC),//FW_MASTER_MPEG2_ERC,            //!< MPEG2 example rate control
        FW_MASTER_INFO(H264_LLRC, H264LLRC),//FW_MASTER_H264_LLRC,            //!< H264 low-latency rate control
        FW_MASTER_INFO(H264MVC_NO_RC, H264MVC),//FW_MASTER_H264MVC_NO_RC,        //!< MVC H264 with no rate control
        FW_MASTER_INFO(H264MVC_CBR, H264MVCCBR),//FW_MASTER_H264MVC_CBR,          //!< MVC H264 constant bitrate
        FW_MASTER_INFO(H264MVC_VBR, H264MVCVBR),//FW_MASTER_H264MVC_VBR,          //!< MVC H264 variable bitrate
        FW_MASTER_INFO(H264MVC_ERC, H264MVCERC),//FW_MASTER_H264MVC_ERC,          //!< MVC H264 example rate control
        FW_MASTER_INFO(H264MVC_LLRC, H264MVCLLRC),//FW_MASTER_H264MVC_LLRC,         //!< MVC H264 low-latency rate control
        FW_MASTER_INFO(H264ALL, H264ALL),
    };
    /* open file  */
    fp = fopen(FW_FILE_NAME, "w");

    if (NULL == fp)
        return -1;

    /* write fw table into the file */
    while (iter < FW_NUM) {
        /* record the size use bytes */
        topaz_fw_table[iter].header.data_size *= 4;
        topaz_fw_table[iter].header.text_size *= 4;

        /* write header */
        fwrite(&(topaz_fw_table[iter].header), sizeof(topaz_fw_table[iter].header), 1, fp);

        /* write text */
        size = topaz_fw_table[iter].header.text_size;
        fwrite(topaz_fw_table[iter].fw_text, 1, size, fp);

        /* write data */
        size = topaz_fw_table[iter].header.data_size;
        fwrite(topaz_fw_table[iter].fw_data, 1, size, fp);

        ++iter;
    }

    /* close file */
    fclose(fp);
    return 0;
}
