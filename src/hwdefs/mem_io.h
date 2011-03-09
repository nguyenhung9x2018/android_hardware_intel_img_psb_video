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

/*!
******************************************************************************
 @file   : mem_io.h

 @brief		Memory structure access macros.

 @Author	Ray Livesley

 @date		12/09/05

 <b>Description:</b>\n

 This file contains a set of memory access macros for accessing packed memory
 structures.

 <b>Platform:</b>\n
 Platform Independent

 @Version	1.0

******************************************************************************/

/*
******************************************************************************
 Modifications :-

 $Log: mem_io.h $

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---

  --- Revision Logs Removed ---


*****************************************************************************/

#if !defined (__MEM_IO_H__)
#define __MEM_IO_H__

#if (__cplusplus)
extern "C"
{
#endif

#include "img_types.h"

#ifdef DOXYGEN_WILL_SEE_THIS
    /*!
    ******************************************************************************

     @Function	MEMIO_READ_FIELD

     @Description

     This macro is used to extract a field from a packed memory based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be extracted.

     @Return	IMG_UINT32:	The value of the field - right aligned.

    ******************************************************************************/
    IMG_UINT32 MEMIO_READ_FIELD(IMG_VOID *	vpMem, field);

    /*!
    ******************************************************************************

     @Function	MEMIO_READ_TABLE_FIELD

     @Description

     This macro is used to extract the value of a field in a table in a packed
     memory based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be extracted.

     @Input		ui32TabIndex: 		The table index of the field to be extracted.

     @Return	IMG_UINT32:	The value of the field - right aligned.

    ******************************************************************************/
    IMG_UINT32 MEMIO_READ_TABLE_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32TabIndex);

    /*!
    ******************************************************************************

     @Function	MEMIO_READ_REPEATED_FIELD

     @Description

     This macro is used to extract the value of a repeated field in a packed
     memory based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be extracted.

     @Input		ui32RepIndex: 		The repeat index of the field to be extracted.

     @Return	IMG_UINT32:	The value of the field - right aligned.

    ******************************************************************************/
    IMG_UINT32 MEMIO_READ_REPEATED_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32RepIndex);

    /*!
    ******************************************************************************

     @Function	MEMIO_READ_TABLE_REPEATED_FIELD

     @Description

     This macro is used to extract the value of a repeated field in a table
     in a packed memory based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be extracted.

     @Input		ui32TabIndex: 		The table index of the field to be extracted.

     @Input		ui32RepIndex: 		The repeat index of the field to be extracted.

     @Return	IMG_UINT32:	The value of the field - right aligned.

    ******************************************************************************/
    IMG_UINT32 MEMIO_READ_TABLE_REPEATED_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32TabIndex, IMG_UINT32 ui32RepIndex);

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_FIELD

     @Description

     This macro is used to update the value of a field in a packed memory based
     structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be updated.

     @Input		ui32Value: 	The value to be writtem to the field - right aligned.

     @Return	None.

    ******************************************************************************/
    IMG_VOID MEMIO_WRITE_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32	ui32Value);

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_TABLE_FIELD

     @Description

     This macro is used to update the field in a table in a packed memory
     based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be updated.

     @Input		ui32TabIndex: 		The table index of the field to be updated.

     @Input		ui32Value: 	The value to be writtem to the field - right aligned.

     @Return	None.

    ******************************************************************************/
    IMG_VOID MEMIO_WRITE_TABLE_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32TabIndex, IMG_UINT32	ui32Value);

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_REPEATED_FIELD

     @Description

     This macro is used to update a repeated field in a packed memory
     based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be updated.

     @Input		ui32RepIndex: 		The repeat index of the field to be updated.

     @Input		ui32Value: 	The value to be writtem to the field - right aligned.

     @Return	None.

    ******************************************************************************/
    IMG_VOID MEMIO_WRITE_REPEATED_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32RepIndex, IMG_UINT32	ui32Value);


    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_TABLE_REPEATED_FIELD

     @Description

     This macro is used to update a repeated field in a table in a packed memory
     based structure.

     @Input		vpMem: 		A pointer to the memory structure.

     @Input		field: 		The name of the field to be updated.

     @Input		ui32TabIndex: 		The table index of the field to be updated.

     @Input		ui32RepIndex: 		The repeat index of the field to be updated.

     @Input		ui32Value: 	The value to be writtem to the field - right aligned.

     @Return	None.

    ******************************************************************************/
    IMG_VOID MEMIO_WRITE_TABLE_REPEATED_FIELD(IMG_VOID *	vpMem, field, IMG_UINT32 ui32TabIndex, IMG_UINT32 ui32RepIndex, IMG_UINT32	ui32Value);

#else

#if WIN32
#define MEMIO_CHECK_ALIGNMENT(vpMem)

#else
#define MEMIO_CHECK_ALIGNMENT(vpMem)		\
	IMG_ASSERT(((IMG_UINT32)vpMem & 0x3) == 0)
#endif

    /*!
    ******************************************************************************

     @Function	MEMIO_READ_FIELD

    ******************************************************************************/


#if defined __RELEASE_DEBUG__

#define MEMIO_READ_FIELD(vpMem, field)																				\
	( MEMIO_CHECK_ALIGNMENT(vpMem),																					\
	((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & field##_MASK) >> field##_SHIFT)) )

#else

#if 1
#define MEMIO_READ_FIELD(vpMem, field)																				    \
	    ((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & field##_MASK) >> field##_SHIFT))

#else

#define MEMIO_READ_FIELD(vpMem, field)																				    \
	    ((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) >> field##_SHIFT) & field##_LSBMASK) )

#endif

#endif



    /*!
    ******************************************************************************

     @Function	MEMIO_READ_TABLE_FIELD

    ******************************************************************************/
#if defined __RELEASE_DEBUG__

#define MEMIO_READ_TABLE_FIELD(vpMem, field, ui32TabIndex)																								\
	( MEMIO_CHECK_ALIGNMENT(vpMem), IMG_ASSERT((ui32TabIndex < field##_NO_ENTRIES) || (field##_NO_ENTRIES == 0)),										\
	((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * ui32TabIndex)))) & field##_MASK) >> field##_SHIFT)) )	\
 
#else

#define MEMIO_READ_TABLE_FIELD(vpMem, field, ui32TabIndex)																								\
	((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * ui32TabIndex)))) & field##_MASK) >> field##_SHIFT)) 	\
 
#endif


    /*!
    ******************************************************************************

     @Function	MEMIO_READ_REPEATED_FIELD

    ******************************************************************************/
#if defined __RELEASE_DEBUG__

#define MEMIO_READ_REPEATED_FIELD(vpMem, field, ui32RepIndex)																																\
	( MEMIO_CHECK_ALIGNMENT(vpMem),	IMG_ASSERT(ui32RepIndex < field##_NO_REPS),																											\
	((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & (field##_MASK >> (ui32RepIndex * field##_SIZE))) >> (field##_SHIFT - (ui32RepIndex * field##_SIZE)))) )	\
 
#else

#define MEMIO_READ_REPEATED_FIELD(vpMem, field, ui32RepIndex)																																\
	( (IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & (field##_MASK >> (ui32RepIndex * field##_SIZE))) >> (field##_SHIFT - (ui32RepIndex * field##_SIZE))) )	\
 
#endif
    /*!
    ******************************************************************************

     @Function	MEMIO_READ_TABLE_REPEATED_FIELD

    ******************************************************************************/
#if defined __RELEASE_DEBUG__

#define MEMIO_READ_TABLE_REPEATED_FIELD(vpMem, field, ui32TabIndex, ui32RepIndex)																																		\
    ( MEMIO_CHECK_ALIGNMENT(vpMem), IMG_ASSERT((ui32TabIndex < field##_NO_ENTRIES) || (field##_NO_ENTRIES == 0)), IMG_ASSERT(ui32RepIndex < field##_NO_REPS), \
    ((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * ui32TabIndex)))) & (field##_MASK >> (ui32RepIndex * field##_SIZE))) >> (field##_SHIFT - (ui32RepIndex * field##_SIZE)))) )	\
 
#else

#define MEMIO_READ_TABLE_REPEATED_FIELD(vpMem, field, ui32TabIndex, ui32RepIndex)																																		\
    ((IMG_UINT32)(((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * ui32TabIndex)))) & (field##_MASK >> (ui32RepIndex * field##_SIZE))) >> (field##_SHIFT - (ui32RepIndex * field##_SIZE))))	\
 
#endif

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_FIELD

    ******************************************************************************/
#define MEMIO_WRITE_FIELD(vpMem, field, ui32Value)														\
	MEMIO_CHECK_ALIGNMENT(vpMem);																		\
	(*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) =										\
	((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & (field##_TYPE)~field##_MASK) |		\
		(field##_TYPE)(( (IMG_UINT32) (ui32Value) << field##_SHIFT) & field##_MASK);

#define MEMIO_WRITE_FIELD_LITE(vpMem, field, ui32Value)													\
	MEMIO_CHECK_ALIGNMENT(vpMem);																		\
	 (*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) =										\
	((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) |		                                \
		(field##_TYPE) (( (IMG_UINT32) (ui32Value) << field##_SHIFT)) );

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_TABLE_FIELD
    ******************************************************************************/
#define MEMIO_WRITE_TABLE_FIELD(vpMem, field, ui32TabIndex, ui32Value)																		\
	MEMIO_CHECK_ALIGNMENT(vpMem); IMG_ASSERT(((ui32TabIndex) < field##_NO_ENTRIES) || (field##_NO_ENTRIES == 0));							\
	(*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * (ui32TabIndex))))) =										\
		((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * (ui32TabIndex))))) & (field##_TYPE)~field##_MASK) |	\
		(field##_TYPE)(( (IMG_UINT32) (ui32Value) << field##_SHIFT) & field##_MASK);

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_REPEATED_FIELD

    ******************************************************************************/
#define MEMIO_WRITE_REPEATED_FIELD(vpMem, field, ui32RepIndex, ui32Value)																	\
	MEMIO_CHECK_ALIGNMENT(vpMem); IMG_ASSERT((ui32RepIndex) < field##_NO_REPS);																\
	(*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) =																			\
	((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET))) & (field##_TYPE)~(field##_MASK >> ((ui32RepIndex) * field##_SIZE)) |			\
		(field##_TYPE)(( (IMG_UINT32) (ui32Value) << (field##_SHIFT - ((ui32RepIndex) * field##_SIZE))) & (field##_MASK >> ((ui32RepIndex) * field##_SIZE))));

    /*!
    ******************************************************************************

     @Function	MEMIO_WRITE_TABLE_REPEATED_FIELD

    ******************************************************************************/
#define MEMIO_WRITE_TABLE_REPEATED_FIELD(vpMem, field, ui32TabIndex, ui32RepIndex, ui32Value)																						\
	MEMIO_CHECK_ALIGNMENT(vpMem); IMG_ASSERT(((ui32TabIndex) < field##_NO_ENTRIES) || (field##_NO_ENTRIES == 0)); IMG_ASSERT((ui32RepIndex) < field##_NO_REPS);						\
	(*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * (ui32TabIndex))))) =																				\
		((*((field##_TYPE *)(((IMG_UINT32)vpMem) + field##_OFFSET + (field##_STRIDE * (ui32TabIndex))))) & (field##_TYPE)~(field##_MASK >> ((ui32RepIndex) * field##_SIZE))) |		\
		(field##_TYPE)(( (IMG_UINT32) (ui32Value) << (field##_SHIFT - ((ui32RepIndex) * field##_SIZE))) & (field##_MASK >> ((ui32RepIndex) * field##_SIZE)));

#endif


#if (__cplusplus)
}
#endif

#endif

