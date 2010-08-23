/*********************************************************************************
 *********************************************************************************
 **
 ** Author      : Imagination Technologies
 ** 
 ** Copyright   : 2008 by Imagination Technologies Limited. All rights reserved
 **             : No part of this software, either material or conceptual
 **             : may be copied or distributed, transmitted, transcribed,
 **             : stored in a retrieval system or translated into any
 **             : human or computer language in any form by any means,
 **             : electronic, mechanical, manual or other-wise, or
 **             : disclosed to third parties without the express written
 **             : permission of Imagination Technologies Limited, Unit 8,
 **             : HomePark Industrial Estate, King's Langley, Hertfordshire,
 **             : WD4 8LZ, U.K.
 **
 ** Description : This header file contains internal definitions required
 **				  by the IEP lite API.
 **
 *********************************************************************************
 *********************************************************************************/

/********************************************************************
	DISCLAIMER:
	This code is provided for demonstration purposes only. It has
	been ported from other projects and has not been tested with
	real hardware. It is not provided in a state that can be run
	with real hardware - this is not intended as the basis for a
	production driver. This code should only be used as an example 
	of the algorithms to be used for setting up the IEP lite 
	hardware.
 ********************************************************************/

#if !defined (__IEP_LITE_UTILS_H__)
#define __IEP_LITE_UTILS_H__

/************************************************************************************************************/
/* The following defines should be replaced with real system equivalent - these are placeholders only, and	*/
/* provide no actual functionality.																			*/
#define RENDER_COMPLETE		0										
#define	REGISTER_CALLBACK(event_type,pfnCallbackFunction) 			

#define WRITE_REGISTER(reg,val)																					\
{																												\
	/*fprintf ( stderr, "Writing reg at addr %08x : %08x\n", reg, val );*/											\
	*(img_uint32 *)(sIEP_LITE_Context->ui32RegBaseAddr + (img_uint32)reg) = val;	\
}

inline void READ_REGISTER(void * p_iep_lite_context, unsigned int reg,unsigned int * pval);
/************************************************************************************************************/

#define READ_BITFIELD(field,regval)																				\
	((regval & field##_MASK) >> field##_SHIFT)

#define WRITE_BITFIELD(field,val,reg)																			\
{																												\
	(reg) =																										\
	((reg) & ~(field##_MASK)) |																					\
		(((IMG_UINT32) (val) << (field##_SHIFT)) & (field##_MASK));												\
}

#define IEP_LITE_DATA_SIZE_10_BITS						0x000003FF
#define IEP_LITE_BLE_LUT_TABLE_SIZE_IN_ENTRIES			1024
#define IEP_LITE_BLE_RES_PAR 							10 
#define IEP_LITE_BLE_RES_PAR_CORR						3 
#define IEP_LITE_BLE_INITIAL_MAVMIN_VALUE				0
#define IEP_LITE_BLE_INITIAL_MAVMAX_VALUE				((1 << IEP_LITE_BLE_MAV_BLE_MAVMAX_LENGTH) - 1)

#define	IEP_LITE_BS_R4_DEFAULT_VALUE					0		/* 0 -> 1024	*/
#define IEP_LITE_BS_R3_DEFAULT_VALUE					150		/* 0 -> 1024	*/
#define	IEP_LITE_BS_THETA4_DEFAULT_VALUE				25		/* 0 -> 360		*/
#define IEP_LITE_BS_THETA3_DEFAULT_VALUE				170		/* 0 -> 360		*/
#define IEP_LITE_BS_YMIN_DEFAULT_VALUE					0x2D0	/* 0 -> 1024	*/

#define	IEP_LITE_BS_OFFSET_MAX_VALUE					((img_uint32) (0.8f * (1 << 23)))	/* 0.23 fixed point format - max val 0.99999 */

#define	IEP_LITE_SCC_R2_DEFAULT_VALUE					0		/* 0 -> 1024	*/
#define IEP_LITE_SCC_R1_DEFAULT_VALUE					0x3E8	/* 0 -> 1024	*/
#define	IEP_LITE_SCC_THETA2_DEFAULT_VALUE				28		/* 0 -> 360		*/
#define IEP_LITE_SCC_THETA1_DEFAULT_VALUE				112		/* 0 -> 360 	*/

#define	IEP_LITE_SCC_OFFSET_MAX_VALUE					((img_uint32) (0.5f * (1 << 23)))	/* 0.23 fixed point format - max val 0.99999 */

#define IEP_LITE_CSC_FRACTIONAL_BITS_IN_COEFFICIENTS	7
#define IEP_LITE_CSC_FRACTIONAL_BITS_IN_OUTPUT_OFFSETS	1

/*-------------------------------------------------------------------------------*/

/*
	Typedefs
*/

typedef struct
{
	IEP_LITE_eBLEMode	eSafetyCheck;
	
	img_uint32			ui32D;							
	img_uint32			ui32Slope;		

} IEP_LITE_sBLEModeSpecificSettings;

typedef struct
{
	/************************************************************************************************/
	/* Initialised and configured flags MUST come first in the structure as it is the only part		*/
	/* of the structure which is initialised statically.											*/
	img_bool			bInitialised;
	/************************************************************************************************/

	/* Blue stretch */
	img_uint8			ui8BSGain;
	
	/* Skin colour conversion */
	img_uint8			ui8SCCGain;

	/* Black level expander */
	img_bool			bFirstLUTValuesWritten;
	img_int32			i32YMinIn;
	img_int32			i32YMaxIn;

	IEP_LITE_eBLEMode	eBLEBlackMode;	
	IEP_LITE_eBLEMode	eBLEWhiteMode;	
        img_uint32                      ui32RegBaseAddr;
    img_uint32							aui32BLELUT								[ IEP_LITE_BLE_LUT_TABLE_SIZE_IN_ENTRIES ];	
} IEP_LITE_sContext;
/*-------------------------------------------------------------------------------*/

/*
	Externs
*/
#define EXTERNAL
#if defined EXTERNAL
#define	EXTERN			extern
#define INITIALISE		0
#else
#define	EXTERN			
#define INITIALISE		1
#endif

#if INITIALISE					
					IEP_LITE_sContext				sIEP_LITE_Context						= { IMG_FALSE };									
#else										
	EXTERN			IEP_LITE_sContext				sIEP_LITE_Context;	
#endif

/* Non pre-initialised declarations											
EXTERN			img_uint32							aui32BLELUT								[ IEP_LITE_BLE_LUT_TABLE_SIZE_IN_ENTRIES ];						
*/																				
extern	const 	IEP_LITE_sBLEModeSpecificSettings	asBLEBlackModes 						[ IEP_LITE_BLE_NO_OF_MODES ];
extern	const 	IEP_LITE_sBLEModeSpecificSettings	asBLEWhiteModes 						[ IEP_LITE_BLE_NO_OF_MODES ];																																								

#undef EXTERN
#undef INITIALISE

/*-------------------------------------------------------------------------------*/

/*
	Function prototypes
*/

img_void iep_lite_CalculateBLELookUpTable		(	IEP_LITE_sContext * sIEP_LITE_Context,
                                                    img_int32	i32YMinIn,
													img_int32	i32YMaxIn		);
img_void iep_lite_BLERecursiveLUTCalculation	(	IEP_LITE_sContext * sIEP_LITE_Context,
                                                    img_int32	i32x1, 
													img_int32	i32y1, 
													img_int32	i32x2, 
													img_int32	i32y2, 
													img_int32	i32x3, 
													img_int32	i32y3, 
													img_int32	i32x4, 
													img_int32	i32y4			);
img_void iep_lite_WriteBLELUTToHardware			(	IEP_LITE_sContext * sIEP_LITE_Context   );
img_void iep_lite_StaticDataSafetyCheck			(	img_void					);

#endif	/* __IEP_LITE_UTILS_H__ */														

/*--------------------------- End of File --------------------------------*/
