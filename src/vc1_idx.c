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

/* format is: inital_opcode, initial_width, address */
/* for each table in gui16vc1VlcTableData[] */

#include <img_types.h>

IMG_UINT16    gaui16vc1VlcIndexData[83][3] = {
    {0,    3,    0},        /* vc1DEC_Code_3x2_2x3_tiles.out */
    {0,    2,    91},        /* vc1DEC_FourMV_Pattern_0.out */
    {0,    3,    110},        /* vc1DEC_FourMV_Pattern_1.out */
    {0,    3,    130},        /* vc1DEC_FourMV_Pattern_2.out */
    {0,    3,    148},        /* vc1DEC_FourMV_Pattern_3.out */
    {0,    3,    168},        /* vc1DEC_High_Mot_Chroma_DC_Diff_VLC.out */
    {0,    4,    245},        /* vc1DEC_High_Mot_Inter_VLC.out */
    {0,    4,    742},        /* vc1DEC_High_Mot_Intra_VLC.out */
    {0,    3,    1010},        /* vc1DEC_High_Mot_Luminance_DC_Diff_VLC.out */
    {0,    4,    1121},        /* vc1DEC_High_Rate_Inter_VLC.out */
    {0,    4,    1367},        /* vc1DEC_High_Rate_Intra_VLC.out */
    {0,    4,    1837},        /* vc1DEC_High_Rate_SUBBLKPAT.out */
    {0,    1,    1871},        /* vc1DEC_High_Rate_TTBLK.out */
    {0,    2,    1879},        /* vc1DEC_High_Rate_TTMB.out */
    {0,    3,    1901},        /* vc1DEC_I_Picture_CBPCY_VLC.out */
    {0,    1,    1994},        /* vc1DEC_Interlace_2_MVP_Pattern_0.out */
    {2,    1,    1998},        /* vc1DEC_Interlace_2_MVP_Pattern_1.out */
    {2,    1,    2001},        /* vc1DEC_Interlace_2_MVP_Pattern_2.out */
    {2,    1,    2006},        /* vc1DEC_Interlace_2_MVP_Pattern_3.out */
    {0,    4,    2011},        /* vc1DEC_Interlace_4MV_MB_0.out */
    {0,    4,    2054},        /* vc1DEC_Interlace_4MV_MB_1.out */
    {0,    4,    2091},        /* vc1DEC_Interlace_4MV_MB_2.out */
    {0,    4,    2126},        /* vc1DEC_Interlace_4MV_MB_3.out */
    {0,    1,    2169},        /* vc1DEC_Interlace_Non_4MV_MB_0.out */
    {0,    2,    2180},        /* vc1DEC_Interlace_Non_4MV_MB_1.out */
    {0,    1,    2193},        /* vc1DEC_Interlace_Non_4MV_MB_2.out */
    {2,    1,    2204},        /* vc1DEC_Interlace_Non_4MV_MB_3.out */
    {0,    2,    2215},        /* vc1DEC_Interlaced_CBPCY_0.out */
    {0,    4,    2281},        /* vc1DEC_Interlaced_CBPCY_1.out */
    {0,    4,    2360},        /* vc1DEC_Interlaced_CBPCY_2.out */
    {0,    4,    2441},        /* vc1DEC_Interlaced_CBPCY_3.out */
    {0,    4,    2511},        /* vc1DEC_Interlaced_CBPCY_4.out */
    {0,    4,    2588},        /* vc1DEC_Interlaced_CBPCY_5.out */
    {0,    4,    2656},        /* vc1DEC_Interlaced_CBPCY_6.out */
    {0,    4,    2734},        /* vc1DEC_Interlaced_CBPCY_7.out */
    {0,    4,    2813},        /* vc1DEC_Low_Mot_Chroma_DC_Diff_VLC.out */
    {0,    4,    2905},        /* vc1DEC_Low_Mot_Inter_VLC.out */
    {0,    4,    3335},        /* vc1DEC_Low_Mot_Intra_VLC.out */
    {2,    5,    3655},        /* vc1DEC_Low_Mot_Luminance_DC_Diff_VLC.out */
    {0,    3,    3735},        /* vc1DEC_Low_Rate_SUBBLKPAT.out */
    {0,    1,    3757},        /* vc1DEC_Low_Rate_TTBLK.out */
    {0,    2,    3764},        /* vc1DEC_Low_Rate_TTMB.out */
    {0,    3,    3784},        /* vc1DEC_Medium_Rate_SUBBLKPAT.out */
    {0,    2,    3806},        /* vc1DEC_Medium_Rate_TTBLK.out */
    {0,    3,    3814},        /* vc1DEC_Medium_Rate_TTMB.out */
    {0,    4,    3838},        /* vc1DEC_Mid_Rate_Inter_VLC.out */
    {0,    4,    3989},        /* vc1DEC_Mid_Rate_Intra_VLC.out */
    {0,    4,    4146},        /* vc1DEC_Mixed_MV_MB_0.out */
    {0,    4,    4180},        /* vc1DEC_Mixed_MV_MB_1.out */
    {0,    4,    4212},        /* vc1DEC_Mixed_MV_MB_2.out */
    {0,    4,    4246},        /* vc1DEC_Mixed_MV_MB_3.out */
    {0,    4,    4280},        /* vc1DEC_Mixed_MV_MB_4.out */
    {0,    4,    4314},        /* vc1DEC_Mixed_MV_MB_5.out */
    {0,    4,    4348},        /* vc1DEC_Mixed_MV_MB_6.out */
    {0,    4,    4380},        /* vc1DEC_Mixed_MV_MB_7.out */
    {0,    4,    4414},        /* vc1DEC_Mot_Vector_Diff_VLC_0.out */
    {0,    4,    4503},        /* vc1DEC_Mot_Vector_Diff_VLC_1.out */
    {0,    4,    4588},        /* vc1DEC_Mot_Vector_Diff_VLC_2.out */
    {0,    4,    4640},        /* vc1DEC_Mot_Vector_Diff_VLC_3.out */
    {0,    4,    4736},        /* vc1DEC_One_Field_Ref_Ilace_MV_0.out */
    {0,    4,    4834},        /* vc1DEC_One_Field_Ref_Ilace_MV_1.out */
    {0,    4,    4932},        /* vc1DEC_One_Field_Ref_Ilace_MV_2.out */
    {0,    4,    5033},        /* vc1DEC_One_Field_Ref_Ilace_MV_3.out */
    {2,    4,    5128},        /* vc1DEC_One_MV_MB_0.out */
    {2,    4,    5134},        /* vc1DEC_One_MV_MB_1.out */
    {0,    4,    5140},        /* vc1DEC_One_MV_MB_2.out */
    {0,    4,    5172},        /* vc1DEC_One_MV_MB_3.out */
    {0,    3,    5204},        /* vc1DEC_One_MV_MB_4.out */
    {0,    3,    5220},        /* vc1DEC_One_MV_MB_5.out */
    {2,    4,    5236},        /* vc1DEC_One_MV_MB_6.out */
    {0,    4,    5242},        /* vc1DEC_One_MV_MB_7.out */
    {0,    4,    5274},        /* vc1DEC_P_Picture_CBPCY_VLC_0.out */
    {0,    4,    5369},        /* vc1DEC_P_Picture_CBPCY_VLC_1.out */
    {0,    4,    5457},        /* vc1DEC_P_Picture_CBPCY_VLC_2.out */
    {0,    3,    5547},        /* vc1DEC_P_Picture_CBPCY_VLC_3.out */
    {0,    4,    5627},        /* vc1DEC_Two_Field_Ref_Ilace_MV_0.out */
    {0,    4,    5803},        /* vc1DEC_Two_Field_Ref_Ilace_MV_1.out */
    {0,    4,    5983},        /* vc1DEC_Two_Field_Ref_Ilace_MV_2.out */
    {0,    4,    6156},        /* vc1DEC_Two_Field_Ref_Ilace_MV_3.out */
    {0,    4,    6339},        /* vc1DEC_Two_Field_Ref_Ilace_MV_4.out */
    {0,    4,    6518},        /* vc1DEC_Two_Field_Ref_Ilace_MV_5.out */
    {0,    3,    6696},        /* vc1DEC_Two_Field_Ref_Ilace_MV_6.out */
    {0,    4,    6869},        /* vc1DEC_Two_Field_Ref_Ilace_MV_7.out */
};

const IMG_UINT8    gui8vc1VlcIndexSize = 83;

/* EOF */
