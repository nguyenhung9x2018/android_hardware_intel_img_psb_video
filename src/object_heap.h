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

#ifndef _OBJECT_HEAP_H_
#define _OBJECT_HEAP_H_

#define OBJECT_HEAP_OFFSET_MASK		0x7F000000
#define OBJECT_HEAP_ID_MASK			0x00FFFFFF

typedef struct object_base_s *object_base_p;
typedef struct object_heap_s *object_heap_p;

struct object_base_s {
    int id;
    int next_free;
};

struct object_heap_s {
    int	object_size;
    int id_offset;
    object_base_p *heap_index;
    int next_free;
    int heap_size;
    int heap_increment;
};

typedef int object_heap_iterator;

/*
 * Return 0 on success, -1 on error
 */
int object_heap_init( object_heap_p heap, int object_size, int id_offset);

/*
 * Allocates an object
 * Returns the object ID on success, returns -1 on error
 */
int object_heap_allocate( object_heap_p heap );

/*
 * Lookup an allocated object by object ID
 * Returns a pointer to the object on success, returns NULL on error
 */
object_base_p object_heap_lookup( object_heap_p heap, int id );

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the first object on the heap, returns NULL if heap is empty.
 */
object_base_p object_heap_first( object_heap_p heap, object_heap_iterator *iter );

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the next object on the heap, returns NULL if heap is empty.
 */
object_base_p object_heap_next( object_heap_p heap, object_heap_iterator *iter );

/*
 * Frees an object
 */
void object_heap_free( object_heap_p heap, object_base_p obj );

/*
 * Destroys a heap, the heap must be empty.
 */
void object_heap_destroy( object_heap_p heap );

/*
 * Suspend an object
 * Suspended objects can not be looked up 
 */
void object_heap_suspend_object( object_base_p obj, int suspend);

#endif /* _OBJECT_HEAP_H_ */
