#ifndef _PTG_SLOTORDER_H_
#define _PTG_SLOTORDER_H_

#define SLOT_STAUS_OCCUPIED 1
#define SLOT_STAUS_EMPTY 0
#define FRAME_I 0
#define FRAME_P 1
#define FRAME_B 2
#define FRAME_IDR 3

typedef struct _FRAME_ORDER_INFO {
    short last_frame_type;
    short last_slot;
    int *slot_consume_dpy_order;
    int *slot_consume_enc_order;
    unsigned long long max_dpy_num;
} FRAME_ORDER_INFO;

/* Input, the encoding order, start from 0
 * Input, The number of B frames between P and I 
 * Input, Intra period
 * Input & Output. Reset to 0 on first call
 * Output. The displaying order
 * Output. Frame type. 1: I frame. 2: P frame. 3: B frame
 * Output. The corresponding slot index
 */

int getFrameDpyOrder(
    unsigned long long encoding_count, /*Input, the encoding order, start from 0*/ 
    int bframes, /*Input, The number of B frames between P and I */
    int intracnt, /*Input, Intra period*/
    int idrcnt, /*INput, IDR period. 0: only one IDR; */
    FRAME_ORDER_INFO *p_last_info, /*Input & Output. Reset to 0 on first call*/
    unsigned long long *displaying_order, /* Output. The displaying order */
    int *frame_type, /*Output. Frame type. 0: I frame. 1: P frame. 2: B frame*/
    int *slot); /*Output. The corresponding slot index */
#endif
