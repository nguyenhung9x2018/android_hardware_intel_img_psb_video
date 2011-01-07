/*! 
******************************************************************************
 @file   : iep_lite_api.h

 @Author Imagination Technologies 

         <b>Copyright 2008 by Imagination Technologies Limited.</b>\n
         All rights reserved.  No part of this software, either
         material or conceptual may be copied or distributed,
         transmitted, transcribed, stored in a retrieval system
         or translated into any human or computer language in any
         form by any means, electronic, mechanical, manual or
         other-wise, or disclosed to third parties without the
         express written permission of Imagination Technologies
         Limited, Unit 8, HomePark Industrial Estate,
         King's Langley, Hertfordshire, WD4 8LZ, U.K.

 \n<b>Description:</b>\n
		This file contains the headers & definitions for the top level
		IEP_Lite control functions.

******************************************************************************/

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

#if !defined (__IEP_LITE_API_H__)
#define __IEP_LITE_API_H__

#if (__cplusplus)
extern "C" {
#endif

extern FILE * pfDebugOutput;

#define DEBUG_PRINT(msg)													\
{																			\
	if ( pfDebugOutput == IMG_NULL )										\
	{																		\
		/*pfDebugOutput = fopen ( "/tmp/iep_lite_demo_output.txt", "w" );*/			\
		/*IMG_ASSERT ( pfDebugOutput != IMG_NULL );*/							\
	}																		\
																			\
	/*fprintf ( pfDebugOutput, (msg) );*/										\
}

/*-------------------------------------------------------------------------------*/

/*
	Enumerations
*/

/*!
******************************************************************************

 This type defines how the mode in which the black level expander should 
 operate.

******************************************************************************/
typedef enum
{
	/*! Black level expander off */
	IEP_LITE_BLE_OFF								= 0x00,
	/*! Black level expander low */
	IEP_LITE_BLE_LOW,
	/*! Black level expander medium */
	IEP_LITE_BLE_MEDIUM,
	/*! Black level expander high */
	IEP_LITE_BLE_HIGH,	
	
	/* PLACE HOLDER ONLY - NOT A VALID MODE */
	IEP_LITE_BLE_NO_OF_MODES

} IEP_LITE_eBLEMode;

/*-------------------------------------------------------------------------------*/

/*
	Function prototypes
*/

/* API command functions */

/*!
******************************************************************************

 @Function				IEP_LITE_Initialise
 
 @Description 
 This function initialises the IEP Lite hardware I must be called prior to
 any other IEP Lite functions.
 
 @Input    None 

 @Return   img_result   : This function will always return IMG_SUCCESS

******************************************************************************/

extern	img_result	IEP_LITE_Initialise	(void * p_iep_lite_context,	img_uint32 ui32RegBaseAddr	);
/*!
******************************************************************************

 @Function				IEP_LITE_BlackLevelExpanderConfigure
 
 @Description

 This function is used to enable/disable and configure the black level 
 expander (note : The Black Level Expander can be used to independently
 control the levels of Black AND White level expansion).
 
 @Input    eBLEBlackMode  : Selects the level of black level expansion to be
 							applied. If the black level mode is set to
 							'IEP_LITE_BLE_OFF' then no black level expansion will
 							be applied.

 @Input    eBLEWhiteMode  : Selects the level of white level expansion to be
 							applied. If the white level mode is set to
 							'IEP_LITE_BLE_OFF' then no white level expansion will
 							be applied. 	
 							
 							Note : If both 'eBLEBlackMode' and 'eBLEWhiteMode'
 							are set to 'IEP_LITE_BLE_OFF' the BLE hardware will be
 							disabled.				 							

 @Return   img_result   : 	This function will always return IMG_SUCCESS

******************************************************************************/

extern img_result	IEP_LITE_BlackLevelExpanderConfigure	(	
                                                                void * p_iep_lite_context,															
																IEP_LITE_eBLEMode							eBLEBlackMode,
																IEP_LITE_eBLEMode							eBLEWhiteMode
															);

/*!
******************************************************************************


 @Function				IEP_LITE_BlueStretchConfigure
 
 @Description

 This function is used to enable/disable and configure the blue stretch.
  
 @Input	   ui8Gain		 : Specifies the extent to which blue stretch should
						   be applied. When set to zero, the blue stretch block
						   is turned off, when set to 0xFF, maximum blue stretch
						   is applied.

 @Return   img_result   : This function will always return IMG_SUCCESS

******************************************************************************/

extern	img_result	IEP_LITE_BlueStretchConfigure			(	
                                                                void * p_iep_lite_context,			
																img_uint8								ui8Gain
															);

/*!
******************************************************************************

 @Function				IEP_LITE_SkinColourCorrectionConfigure
 
 @Description

 This function is used to enable/disable and configure the skin colour 
 correction block.

 @Input	   ui8Gain		 : Specifies the extent to which skin colour correction
						   should be applied. When set to zero, the skin colour
						   correction block is turned off, when set to 0xFF, 
						   maximum skin colour correction is applied.

 @Return   img_result   : This function will always return IMG_SUCCESS

******************************************************************************/

extern	img_result	IEP_LITE_SkinColourCorrectionConfigure	(
                                                                void * p_iep_lite_context,	
																img_uint8							ui8Gain
															);
/*!
******************************************************************************

 @Function				IEP_LITE_CSCConfigure
 
 @Description 
 This function is used to configure the colour space converter

 @Return   img_result   : This function will always return IMG_SUCCESS

******************************************************************************/

extern	img_result	IEP_LITE_CSCConfigure	(
                                                void * p_iep_lite_context,	
												CSC_eColourSpace	eInputColourSpace,
												CSC_eColourSpace	eOutputColourSpace,	
												CSC_psHSBCSettings	psHSBCSettings	
											);

#if (__cplusplus)
}
#endif

#endif	/* __IEP_LITE_API_H__ */


/*--------------------------- End of File --------------------------------*/
