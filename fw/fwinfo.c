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
#define TOPAZ_FW_FILE_NAME_ANDROID "/etc/firmware/topaz_fw.bin"
#define MSVDX_FW_FILE_NAME_ANDROID "/etc/firmware/msvdx_fw.bin"

#define TOPAZ_FW_FILE_NAME_MEEGO "/lib/firmware/topaz_fw.bin"
#define MSVDX_FW_FILE_NAME_MEEGO "/lib/firmware/msvdx_fw.bin"

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
    //  unsigned long *fw_text;
    //  unsigned long *fw_data;
};
typedef struct fw_table_s fw_table_t;


struct msvdx_fw {
    unsigned int ver;
    unsigned int  text_size;
    unsigned int data_size;
    unsigned int data_location;
};


static char *codec_to_string(int codec)
{
    switch (codec) {
    case FW_H264_NO_RC:
        return "H264_NO_RC";
    case FW_H264_VBR:
        return "H264_VBR";
    case FW_H264_CBR:
        return "H264_CBR";
    case FW_H264_VCM:
        return "H264_VCM";
    case FW_H263_NO_RC:
        return "H263_NO_RC";
    case FW_H263_VBR:
        return "H263_VBR";
    case FW_H263_CBR:
        return "H263_CBR";
    case FW_MPEG4_NO_RC:
        return "MPEG4_NO_RC";
    case FW_MPEG4_VBR:
        return "MPEG4_VBR";
    case FW_MPEG4_CBR:
        return "MPEG4_CBR";
    default:
        return "Undefined codec";
    }
    return "";
}

int main()
{
    FILE *fp = NULL;
    topaz_fw_codec_t iter = FW_H264_NO_RC;
    //  unsigned int read_data;
    unsigned int i, lseek;
    unsigned char system_id = 0;
    fw_table_t topaz_fw_table[FW_NUM + 1];
    struct msvdx_fw fw;


    /* open file
     * RRRdetermine Android or Meego
     * system_id = 0  Android
     * system_id = 1  Meego
     */
    fp = fopen(TOPAZ_FW_FILE_NAME_ANDROID, "r");

    if (NULL == fp) {
        fp = fopen(TOPAZ_FW_FILE_NAME_MEEGO, "r");
        if (NULL == fp) {
            printf("\nSystem isn't Android or Meego\n\n");
            printf("\nCan't open topaz_fw.bin\n");
            return -1;
        }
        system_id = 1;
        printf("\nSystem is Meego\n\n");
    } else {
        printf("\nSystem is Android\n\n");
    }

    //  fseek (fp, 0, SEEK_SET);

    printf("topza:Try to read and print topaz_fw_table...\n\n\n\n");

    /* read fw table into the topz_fw_table */
    while (iter < FW_NUM) {

        /* read header */
        fread(&(topaz_fw_table[iter].header), sizeof(topaz_fw_table[iter].header), 1, fp);

        /* print header */
        printf("topaz: index          : %s\n", codec_to_string(topaz_fw_table[iter].header.codec));
        printf("topaz: ver            : 0x%04x\n", topaz_fw_table[iter].header.ver);
        printf("topaz: Codec          : %s\n", codec_to_string(topaz_fw_table[iter].header.codec));
        printf("topaz: text_size      : %d\n", (topaz_fw_table[iter].header.text_size >> 2));
        printf("topaz: data_size      : %d\n", (topaz_fw_table[iter].header.data_size >> 2));
        printf("topaz: data_location  : 0x%08x\n\n", topaz_fw_table[iter].header.data_location);

        fseek(fp, topaz_fw_table[iter].header.text_size + topaz_fw_table[iter].header.data_size, SEEK_CUR);
#if 0
        /* read and print fw_text */
        printf("fw_text = {\n");
        for (i = 0; i < (topaz_fw_table[iter].header.text_size >> 2); i++) {
            fread(&read_data, 1, 4, fp);
            printf("            0x%08x\n", read_data);
        }
        printf("          }\n\n\n\n");

        /* read and print fw_data */
        printf("fw_data = {\n");
        for (i = 0; i < (topaz_fw_table[iter].header.data_size >> 2); i++) {
            fread(&read_data, 1, 4, fp);
            printf("            0x%08x\n", read_data);
        }
        printf("          }\n\n\n\n");
#endif

        ++iter;
    }

    /* close topaz_fw.bin file */
    fclose(fp);

    printf("\n\n\n\nmsvdx:Try to read and print msvdx_fw...\n\n\n\n");

    /* open msvdx_fw.bin */
    if (system_id == 0) {
        fp = fopen(MSVDX_FW_FILE_NAME_ANDROID, "r");
    } else {
        fp = fopen(MSVDX_FW_FILE_NAME_MEEGO, "r");
    }
    if (NULL == fp) {
        printf("Can't open msvdx_fw.bin\n");
        return -1;
    }

    //  fseek (fp, 0, SEEK_SET);

    /*read and print fw*/
    fread(&fw, sizeof(fw), 1, fp);

    printf("msvdx slice switch firmware: ver            : 0x%04x\n", fw.ver);
    printf("msvdx slice switch firmware: text_size      : %d\n", fw.text_size);
    printf("msvdx slice switch firmware: data_size      : %d\n", fw.data_size);
    printf("msvdx slice switch firmware: data_location  : 0x%08x\n\n", fw.data_location);

    lseek = ((sizeof(fw) + (fw.text_size + fw.data_size) * 4 + 0xfff) & ~0xfff);
    fseek(fp, lseek, SEEK_SET);

    /*read and print fw*/
    fread(&fw, sizeof(fw), 1, fp);

    printf("msvdx frame switch firmware: ver            : 0x%04x\n", fw.ver);
    printf("msvdx frame switch firmware: text_size      : %d\n", fw.text_size);
    printf("msvdx frame switch firmware: data_size      : %d\n", fw.data_size);
    printf("msvdx frame switch firmware: data_location  : 0x%08x\n\n", fw.data_location);

    /* close msvdx_fw.bin file */
    fclose(fp);

    return 0;
}



