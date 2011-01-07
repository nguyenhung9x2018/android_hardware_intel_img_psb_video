/*********************************************************************************
 *********************************************************************************
 **
 ** Name        : iep_lite_utils.c
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
 ** Description : Contains all general utility functions for iep lite api.
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

/*-------------------------------------------------------------------------------*/

/*
	Includes
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "img_iep_defs.h"
#include "csc2.h"

#define	EXTERNAL
#include "iep_lite_api.h"
#include "iep_lite_reg_defs.h"
#undef EXTERNAL

#include "iep_lite_utils.h"

inline void READ_REGISTER(void * p_iep_lite_context,unsigned int reg,unsigned int * pval)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
    *pval = *(img_uint32 *)(sIEP_LITE_Context->ui32RegBaseAddr + (img_uint32)reg);
    /*fprintf ( stderr, "Reading reg at addr %08x val %08x \n", reg, *pval );*/
}

/*!
******************************************************************************

 @Function				iep_lite_RenderCompleteCallback

******************************************************************************/

img_void iep_lite_RenderCompleteCallback (void * p_iep_lite_context)
{
    IEP_LITE_sContext * sIEP_LITE_Context = p_iep_lite_context;
	img_uint32 ui32MyReg;
	
	DEBUG_PRINT ( "Entering iep_lite_RenderCompleteCallback\n" );
    if (NULL == sIEP_LITE_Context) 
    {
        return;
    }

	if (( sIEP_LITE_Context->eBLEBlackMode != IEP_LITE_BLE_OFF ) ||
		( sIEP_LITE_Context->eBLEWhiteMode != IEP_LITE_BLE_OFF ))
	{			
		if ( sIEP_LITE_Context->bFirstLUTValuesWritten == IMG_FALSE )
		{
			/* The first table must be calculated using fixed values for YMinIn and YMaxIn. */
			/* This is because the BLE hardware will not return meaningful values for these	*/
			/* until it has been turned on and we can't turn it on without a valid table.	*/
			/* As such, we use fixed values to approximate a table for the first frame.		*/
			sIEP_LITE_Context->i32YMinIn		= IEP_LITE_BLE_INITIAL_MAVMIN_VALUE;
			sIEP_LITE_Context->i32YMaxIn	= IEP_LITE_BLE_INITIAL_MAVMAX_VALUE;
		}
		else
		{
			/* Back at start of "read vals / calc table / write table" cycle */
			READ_REGISTER ( sIEP_LITE_Context,
                            IEP_LITE_BLE_MAV_OFFSET,
							&ui32MyReg	);
			
			sIEP_LITE_Context->i32YMinIn = READ_BITFIELD ( IEP_LITE_BLE_MAV_BLE_MAVMIN, ui32MyReg );
			sIEP_LITE_Context->i32YMaxIn = READ_BITFIELD ( IEP_LITE_BLE_MAV_BLE_MAVMAX, ui32MyReg );							
		}
	
		/* The hardware can return unusable values for MinIn and MaxIn for two reasons : 	*/
		/* 1. 	The read has been performed while the hardware is calculating new values 	*/
		/*		(i.e.: mid frame). In this case, both readback values will be zero.		 	*/
		/* 2. 	The read has been performed during the first frame after the hardware has 	*/
		/*		been turned on, but before the first readback values have been calculated.	*/
		/*		In this case, the value for MinIn will be greater than the value for MaxIn.	*/
		/* 																					*/
		/* In both cases, we just ignore the read and try again next time.					*/
		if (
			(( sIEP_LITE_Context->i32YMinIn == 0 ) &&
			 ( sIEP_LITE_Context->i32YMaxIn == 0 ))
			||
			(( sIEP_LITE_Context->i32YMinIn > sIEP_LITE_Context->i32YMaxIn ))
			)
		{
			/* Discard values */
		}
		else
		{
			iep_lite_CalculateBLELookUpTable (	sIEP_LITE_Context,
                                                sIEP_LITE_Context->i32YMinIn,
												sIEP_LITE_Context->i32YMaxIn	);								
		
			/* All ready for writing to hardware */
			iep_lite_WriteBLELUTToHardware (sIEP_LITE_Context);																
		}			
	}

	DEBUG_PRINT ( "Leaving iep_lite_RenderCompleteCallback\n" );

	return;
}

/******************************************************************************
 * Function Name: iep_lite_CalculateBLELookUpTable
 *****************************************************************************/
img_void iep_lite_CalculateBLELookUpTable ( IEP_LITE_sContext * sIEP_LITE_Context,
                                            img_int32	i32YMinIn,
											img_int32	i32YMaxIn	)
{
	img_uint32	ui32x1;
	img_uint32 	ui32x2;
	img_uint32 	ui32x3;
	img_uint32 	ui32x4;
	img_uint32 	ui32y1;
	img_uint32 	ui32y2;
	img_uint32 	ui32y3;
	img_uint32 	ui32y4;	
	img_uint32	ui32lum0;
	img_uint32	ui32lum1;
	img_uint32 	ui32i;
	img_uint32	ui32DR;
	img_uint32  ui32DB;
	img_uint32  ui32DW;
	img_uint32  ui32BlackSlope;
	img_uint32  ui32WhiteSlope;
	img_uint32	ui32CompositeWord = 0;

	img_uint32 	ui32resolution	= (IEP_LITE_DATA_SIZE_10_BITS + 1);
	img_uint32 	ui32col_res 	= IEP_LITE_DATA_SIZE_10_BITS;		

	DEBUG_PRINT ( "Entering iep_lite_CalculateBLELookUpTable\n" );

	ui32DB 			=  	asBLEBlackModes [ sIEP_LITE_Context->eBLEBlackMode  ].ui32D;
	ui32DW 			=  	asBLEWhiteModes [ sIEP_LITE_Context->eBLEWhiteMode  ].ui32D;
	ui32BlackSlope	= 	asBLEBlackModes [ sIEP_LITE_Context->eBLEBlackMode  ].ui32Slope;	
	ui32WhiteSlope	= 	asBLEWhiteModes [ sIEP_LITE_Context->eBLEWhiteMode  ].ui32Slope;	
		
	ui32DR 			= 	i32YMaxIn - i32YMinIn;

	ui32lum0 		=  	i32YMinIn + ((ui32DB * ui32DR)>>(IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR));
	ui32lum1 		=  	i32YMaxIn - ((ui32DW * ui32DR)>>(IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR));
	
	ui32x1 			= 	(ui32lum0 - (ui32lum0 * ui32BlackSlope>>IEP_LITE_BLE_RES_PAR));
	ui32y1 			= 	0;
	ui32x2 			= 	ui32lum0;
	ui32y2 			= 	ui32lum0;
	ui32x3			= 	ui32lum1;
	ui32y3 			= 	ui32lum1;
	ui32x4 			= 	(ui32lum1 + ((ui32col_res - ui32lum1) * ui32WhiteSlope>>IEP_LITE_BLE_RES_PAR));
	ui32y4 			= 	ui32col_res;	
	
	/* initialization (we see black pixels if the resolution is not dense enough) */
	IMG_MEMSET (sIEP_LITE_Context->aui32BLELUT, 0, sizeof(img_uint32)*(ui32resolution));
	/*----------------------------------------------------------------------------*/
	for(ui32i = ui32x4; ui32i < ui32resolution; ui32i++)
	{
		sIEP_LITE_Context->aui32BLELUT [ui32i] = IEP_LITE_DATA_SIZE_10_BITS;
	}
	
	sIEP_LITE_Context->aui32BLELUT [ui32x1] = ui32y1;
	sIEP_LITE_Context->aui32BLELUT [ui32x2] = ui32y2;
	sIEP_LITE_Context->aui32BLELUT [ui32x3] = ui32y3;
	sIEP_LITE_Context->aui32BLELUT [ui32x4] = ui32y4;

	iep_lite_BLERecursiveLUTCalculation ( sIEP_LITE_Context,
                                          ui32x1, 
										  sIEP_LITE_Context->aui32BLELUT[ui32x1], 
										  ui32x2, 
										  sIEP_LITE_Context->aui32BLELUT[ui32x2], 
										  ui32x3, 
										  sIEP_LITE_Context->aui32BLELUT[ui32x3], 
										  ui32x4,
										  sIEP_LITE_Context->aui32BLELUT[ui32x4] );						
									 
	/* Compress table */
 	for(ui32i=0; ui32i<IEP_LITE_BLE_LUT_TABLE_SIZE_IN_ENTRIES; ui32i+=2)
	{
		WRITE_BITFIELD ( IEP_LITE_BLE_LUT_TABLE_LOW_ENTRY, 
						 sIEP_LITE_Context->aui32BLELUT [ ui32i ],
						 ui32CompositeWord );
										
		WRITE_BITFIELD ( IEP_LITE_BLE_LUT_TABLE_HIGH_ENTRY, 
						 sIEP_LITE_Context->aui32BLELUT [ (ui32i+1) ],
						 ui32CompositeWord );  
											
		sIEP_LITE_Context->aui32BLELUT [ ui32i ] = ui32CompositeWord;													      										        																	
	}

	DEBUG_PRINT ( "Leaving iep_lite_CalculateBLELookUpTable\n" );
}										

/******************************************************************************
 * Function Name: iep_lite_BLERecursiveLUTCalculation
 *****************************************************************************/
img_void iep_lite_BLERecursiveLUTCalculation ( IEP_LITE_sContext * sIEP_LITE_Context, img_int32 i32x1, img_int32 i32y1, img_int32 i32x2, img_int32 i32y2, img_int32 i32x3, img_int32 i32y3, img_int32 i32x4, img_int32 i32y4 )
{
	img_uint32 ui32id1; 
	img_uint32 ui32id2; 
	img_uint32 ui32id3; 
	img_uint32 ui32id4;
	img_uint32 ui32id5;
	img_uint32 ui32id6;
	img_uint32 ui32id7;
	img_uint32 ui32id8;	

	DEBUG_PRINT ( "Entering iep_lite_BLERecursiveLUTCalculation\n" );

	if( 
		i32x2 - i32x1 <= 1 && 
		i32x3 - i32x2 <= 1 && 
		i32x4 - i32x3 <= 1  
	  )
	{
		return;
	}
	else
	{		
		ui32id1 = i32x1;
		ui32id2 = (i32x1 + i32x2) >> 1;
		ui32id3 = (ui32id2 >> 1) + ((i32x2 + i32x3) >> 2);
		ui32id7 = (i32x3 + i32x4) >> 1;
		ui32id8 = i32x4;
		ui32id6 = ((i32x2 + i32x3) >> 2) + (ui32id7 >> 1);
		ui32id4 = ui32id5 = ((ui32id3 + ui32id6) >> 1);

		sIEP_LITE_Context->aui32BLELUT[ui32id1] = i32y1;
		sIEP_LITE_Context->aui32BLELUT[ui32id2] = (i32y1 + i32y2) >> 1;
		sIEP_LITE_Context->aui32BLELUT[ui32id3] = (sIEP_LITE_Context->aui32BLELUT[ui32id2] >> 1) + ((i32y2 + i32y3) >> 2);
		sIEP_LITE_Context->aui32BLELUT[ui32id7] = (i32y3 + i32y4) >> 1;
		sIEP_LITE_Context->aui32BLELUT[ui32id8] = i32y4;
		sIEP_LITE_Context->aui32BLELUT[ui32id6] = ((i32y2 + i32y3) >> 2) + (sIEP_LITE_Context->aui32BLELUT[ui32id7] >> 1);
		sIEP_LITE_Context->aui32BLELUT[ui32id4] = sIEP_LITE_Context->aui32BLELUT[ui32id5] = ((sIEP_LITE_Context->aui32BLELUT[ui32id3] + sIEP_LITE_Context->aui32BLELUT[ui32id6]) >> 1);

		iep_lite_BLERecursiveLUTCalculation (sIEP_LITE_Context, ui32id1, sIEP_LITE_Context->aui32BLELUT[ui32id1], ui32id2, sIEP_LITE_Context->aui32BLELUT[ui32id2], ui32id3, sIEP_LITE_Context->aui32BLELUT[ui32id3], ui32id4, sIEP_LITE_Context->aui32BLELUT[ui32id4]);
		iep_lite_BLERecursiveLUTCalculation (sIEP_LITE_Context, ui32id5, sIEP_LITE_Context->aui32BLELUT[ui32id5], ui32id6, sIEP_LITE_Context->aui32BLELUT[ui32id6], ui32id7, sIEP_LITE_Context->aui32BLELUT[ui32id7], ui32id8, sIEP_LITE_Context->aui32BLELUT[ui32id8]);
	}

	DEBUG_PRINT ( "Leaving iep_lite_BLERecursiveLUTCalculation\n" );
}


/******************************************************************************
 * Function Name: iep_lite_WriteBLELUTToHardware
 *****************************************************************************/
 
img_void iep_lite_WriteBLELUTToHardware (IEP_LITE_sContext * sIEP_LITE_Context )
{
	img_uint32	i;
		
	DEBUG_PRINT ( "Entering iep_lite_WriteBLELUTToHardware\n" );

	for(i=0; i<IEP_LITE_BLE_LUT_TABLE_SIZE_IN_ENTRIES; i+=2)
	{		
		WRITE_REGISTER( (IEP_LITE_BLE_LUT_TABLE_OFFSET + ((i/2) * 4)),
						 sIEP_LITE_Context->aui32BLELUT [ i ] );			
	}
	
	if ( sIEP_LITE_Context->bFirstLUTValuesWritten == IMG_FALSE )
	{
		/* Turn BLE on */
		WRITE_REGISTER ( IEP_LITE_BLE_CONF_STATUS_OFFSET,
						 IEP_LITE_BLE_CONF_STATUS_BLE_EN_MASK );
				
		sIEP_LITE_Context->bFirstLUTValuesWritten = IMG_TRUE;			
	}

	DEBUG_PRINT ( "Leaving iep_lite_WriteBLELUTToHardware\n" );
}

/*!
******************************************************************************

 @Function              iep_lite_StaticDataSafetyCheck

******************************************************************************/

img_void iep_lite_StaticDataSafetyCheck	(	img_void	)
{
	img_uint32	ui32i;
	
	DEBUG_PRINT ( "Entering iep_lite_StaticDataSafetyCheck\n" );

	/* asBLEBlackModes */
	for ( ui32i=0; ui32i<IEP_LITE_BLE_NO_OF_MODES; ui32i++ )
	{
		IMG_ASSERT ( asBLEBlackModes [ui32i].eSafetyCheck == (IEP_LITE_eBLEMode) ui32i );
	}	

	/* asBLEWhiteModes */
	for ( ui32i=0; ui32i<IEP_LITE_BLE_NO_OF_MODES; ui32i++ )
	{
		IMG_ASSERT ( asBLEWhiteModes [ui32i].eSafetyCheck == (IEP_LITE_eBLEMode) ui32i );
	}

	DEBUG_PRINT ( "Leaving iep_lite_StaticDataSafetyCheck\n" );
}

/*--------------------------- End of File --------------------------------*/



