/*
 * INTEL CONFIDENTIAL
 * Copyright 2007 Intel Corporation. All Rights Reserved.
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
 *    Edward Lin <edward.lin@intel.com>
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include "psb_drv_video.h"
#include "ptg_hostheader.h"
#include "ptg_slotorder.h"

static unsigned long long displayingOrder2EncodingOrder(
    unsigned long long displaying_order,
    int bframes,
    int intracnt,
    int idrcnt)
{
    int poc;
    if (idrcnt != 0) 
        poc = displaying_order % (intracnt * idrcnt + 1);
    else
        poc = displaying_order;

    if (poc == 0) //IDR
        return displaying_order;
    else if ((poc % (bframes + 1)) == 0) //I or P 
        return (displaying_order - bframes);
    else 
        return (displaying_order + 1); //B
}


static int getSlotIndex(
    int bframes, int intracnt, int idrcnt,
    int displaying_order, int encoding_count,
    FRAME_ORDER_INFO *last_info)
{
    int i, slot_idx = 0;
    if (displaying_order == 0) {
	for (i = 0; i < (bframes + 2); i++)  {
	    //encoding order
	    if (i == 0)
			last_info->slot_consume_enc_order[0] = 0;
	    else if (i == 1)
			last_info->slot_consume_enc_order[bframes + 2 - 1] = 1;
	    else
			last_info->slot_consume_enc_order[i - 1] = i; 
	    last_info->slot_consume_dpy_order[i] = i; //displaying order
	}
	last_info->slot_consume_dpy_order[0] = bframes + 2;
        last_info->slot_consume_enc_order[0] = 	
	    displayingOrder2EncodingOrder(bframes + 2,
		    bframes, intracnt, idrcnt);
	last_info->max_dpy_num = bframes + 2; 
    } else {
	for (i = 0; i < (bframes + 2); i++) {
	    if (last_info->slot_consume_enc_order[i] == encoding_count) {
		slot_idx = i;
		break;
	    }
	}
	last_info->max_dpy_num++;
	last_info->slot_consume_dpy_order[slot_idx] = last_info->max_dpy_num;
	last_info->slot_consume_enc_order[slot_idx] =
	    displayingOrder2EncodingOrder(last_info->max_dpy_num,
		    bframes, intracnt, idrcnt);
    }
    
    return slot_idx;
}

int getFrameDpyOrder(
    unsigned long long encoding_count, /*Input, the encoding order, start from 0*/ 
    int bframes, /*Input, The number of B frames between P and I */
    int intracnt, /*Input, Intra period*/
    int idrcnt, /*INput, IDR period. 0: only one IDR; */
    FRAME_ORDER_INFO *p_last_info, /*Input & Output. Reset to 0 on first call*/
    unsigned long long *displaying_order, /* Output. The displaying order */
    int *frame_type, /*Output. Frame type. 0: I frame. 1: P frame. 2: B frame*/
    int *slot) /*Output. The corresponding slot index */
{
    int i;
    unsigned long long disp_index;
    unsigned long long val;

    if ((intracnt % (bframes + 1)) != 0 || bframes == 0)
	return -1;

    val = ((idrcnt == 0) ? encoding_count :
	    encoding_count % (intracnt * idrcnt + 1));
    if ((idrcnt == 0 && encoding_count == 0) ||
	    (idrcnt != 0 && (encoding_count % (intracnt * idrcnt + 1) == 0))) {
	   *frame_type = FRAME_IDR;
	   disp_index = encoding_count;
    } else if (((val - 1) % (bframes + 1)) != 0) {
	*frame_type = FRAME_B;
	 disp_index = encoding_count - 1;
    } else if (p_last_info->last_frame_type == FRAME_IDR ||
	    ((val - 1) / (bframes + 1) % (intracnt / (bframes + 1))) != 0) {
	*frame_type = FRAME_P;
	disp_index = encoding_count + bframes;
    } else {
	*frame_type = FRAME_I;
	disp_index = encoding_count + bframes;
    }	

    *displaying_order = disp_index;
    *slot = getSlotIndex(bframes, intracnt, idrcnt,
	    disp_index, encoding_count,
	    p_last_info);	
    p_last_info->last_frame_type = *frame_type; 
    p_last_info->last_slot = *slot;
    return 0;
}

#if 0
int main(int argc, char **argv) {
    int bframes, intracnt, frame_num;
    int i;
    int displaying_order, frame_type, slot;
    int j;
     char ac_frame_type[] = {'I', 'P', 'B'};
    FRAME_ORDER_INFO last_info;

    if (argc != 4)
    {
	    printf("%s [bframe_number] [intra period] [frame_number]\n", argv[0]);
	    return 0;
    }
    else {
		bframes = atoi(argv[1]);
		intracnt = atoi(argv[2]);
		frame_num = atoi(argv[3]);
    }
    if (intracnt % (bframes + 1) != 0) {
		printf(" intra count must be a muliple of (bframe_number + 1)\n");
		return 0;
    }

    memset(&last_info, 0, sizeof(FRAME_ORDER_INFO));
    last_info.slot_consume_dpy_order = (int *)malloc((bframes + 2)  * sizeof(int));
    last_info.slot_consume_enc_order = (int *)malloc((bframes + 2)  * sizeof(int));
    
    printf("encodingorder displaying order	frame_type	slot index\n");
    for (i = 0; i < frame_num; i++) {
		getFrameDpyOrder(i, bframes, intracnt, &last_info, &displaying_order, &frame_type, &slot);
		printf("%5d\t%5d\t%c\t%d\n", i, displaying_order, ac_frame_type[frame_type], slot);
    }
    free(last_info.slot_consume_dpy_order);
    free(last_info.slot_consume_enc_order);

    return 0; 
}
#endif //test routine
