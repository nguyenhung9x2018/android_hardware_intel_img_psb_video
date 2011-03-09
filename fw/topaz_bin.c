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

#include "H263Firmware_bin.h"
#include "H263FirmwareCBR_bin.h"
#include "H263FirmwareVBR_bin.h"
#include "H264Firmware_bin.h"
#include "H264FirmwareCBR_bin.h"
#include "H264FirmwareVBR_bin.h"
#include "H264FirmwareVCM_bin.h"
#include "MPG4Firmware_bin.h"
#include "MPG4FirmwareCBR_bin.h"
#include "MPG4FirmwareVBR_bin.h"

#define FW_VER 146 /* DDKv146 release */
#define FW_FILE_NAME "topaz_fw.bin"

struct topaz_fw_info_item_s {
    unsigned short ver;
    unsigned short codec;

    unsigned int  text_size;
    unsigned int data_size;
    unsigned int data_location;
};
typedef struct topaz_fw_info_item_s topaz_fw_info_item_t;

enum topaz_fw_codec_e {
    FW_JPEG = 0,
    FW_H264_NO_RC,
    FW_H264_VBR,
    FW_H264_CBR,
    FW_H264_VCM,
    FW_H263_NO_RC,
    FW_H263_VBR,
    FW_H263_CBR,
    FW_MPEG4_NO_RC,
    FW_MPEG4_VBR,
    FW_MPEG4_CBR,
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
    topaz_fw_codec_t iter = FW_H264_NO_RC;
    unsigned int size = 0;
    unsigned int i;

    fw_table_t topaz_fw_table[] = {
        /* index   header
         * { ver, codec, text_size, data_size, date_location }
         * fw_text fw_data */
        { 0, {0, 0, 0, 0, 0} },
        { FW_H264_NO_RC,
          { FW_VER,
            FW_H264_NO_RC,
            ui32H264_MTXTOPAZFWTextSize,
            ui32H264_MTXTOPAZFWDataSize,
            ui32H264_MTXTOPAZFWDataLocation
          },
          aui32H264_MTXTOPAZFWText, aui32H264_MTXTOPAZFWData
        },

        { FW_H264_VBR,
          { FW_VER,
            FW_H264_VBR,
            ui32H264VBR_MTXTOPAZFWTextSize,
            ui32H264VBR_MTXTOPAZFWDataSize,
            ui32H264VBR_MTXTOPAZFWDataLocation
          },
          aui32H264VBR_MTXTOPAZFWText, aui32H264VBR_MTXTOPAZFWData
        },

        { FW_H264_CBR,
          { FW_VER,
            FW_H264_CBR,
            ui32H264CBR_MTXTOPAZFWTextSize,
            ui32H264CBR_MTXTOPAZFWDataSize,
            ui32H264CBR_MTXTOPAZFWDataLocation
          },
          aui32H264CBR_MTXTOPAZFWText,
          aui32H264CBR_MTXTOPAZFWData
        },

        { FW_H264_VCM,
          { FW_VER,
            FW_H264_VCM,
            ui32H264VCM_MTXTOPAZFWTextSize,
            ui32H264VCM_MTXTOPAZFWDataSize,
            ui32H264VCM_MTXTOPAZFWDataLocation
          },
          aui32H264VCM_MTXTOPAZFWText,
          aui32H264VCM_MTXTOPAZFWData
        },

        { FW_H263_NO_RC,
          { FW_VER,
            FW_H263_NO_RC,
            ui32H263_MTXTOPAZFWTextSize,
            ui32H263_MTXTOPAZFWDataSize,
            ui32H263_MTXTOPAZFWDataLocation
          },
          aui32H263_MTXTOPAZFWText,
          aui32H263_MTXTOPAZFWData
        },

        { FW_H263_VBR,
          { FW_VER,
            FW_H263_VBR,
            ui32H263VBR_MTXTOPAZFWTextSize,
            ui32H263VBR_MTXTOPAZFWDataSize,
            ui32H263VBR_MTXTOPAZFWDataLocation
          },
          aui32H263VBR_MTXTOPAZFWText,
          aui32H263VBR_MTXTOPAZFWData
        },

        { FW_H263_CBR,
          { FW_VER,
            FW_H263_CBR,
            ui32H263CBR_MTXTOPAZFWTextSize,
            ui32H263CBR_MTXTOPAZFWDataSize,
            ui32H263CBR_MTXTOPAZFWDataLocation
          },
          aui32H263CBR_MTXTOPAZFWText,
          aui32H263CBR_MTXTOPAZFWData
        },

        { FW_MPEG4_NO_RC,
          { FW_VER,
            FW_MPEG4_NO_RC,
            ui32MPG4_MTXTOPAZFWTextSize,
            ui32MPG4_MTXTOPAZFWDataSize,
            ui32MPG4_MTXTOPAZFWDataLocation
          },
          aui32MPG4_MTXTOPAZFWText,
          aui32MPG4_MTXTOPAZFWData
        },

        { FW_MPEG4_VBR,
          { FW_VER,
            FW_MPEG4_VBR,
            ui32MPG4VBR_MTXTOPAZFWTextSize,
            ui32MPG4VBR_MTXTOPAZFWDataSize,
            ui32MPG4VBR_MTXTOPAZFWDataLocation
          },
          aui32MPG4VBR_MTXTOPAZFWText,
          aui32MPG4VBR_MTXTOPAZFWData
        },

        { FW_MPEG4_CBR,
          { FW_VER,
            FW_MPEG4_CBR,
            ui32MPG4CBR_MTXTOPAZFWTextSize,
            ui32MPG4CBR_MTXTOPAZFWDataSize,
            ui32MPG4CBR_MTXTOPAZFWDataLocation
          },
          aui32MPG4CBR_MTXTOPAZFWText,
          aui32MPG4CBR_MTXTOPAZFWData
        }
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
