/*********************************************************************************
 *********************************************************************************
 **
 ** Name        : iep_lite_hardware.c
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
 ** Description : This module contains hardware specific data for the
 **				  IEP LITE module.
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

#include "img_iep_defs.h"
#include "csc2.h"

#define EXTERNAL
#include "iep_lite_api.h"
#include "iep_lite_utils.h"
#undef EXTERNAL

/*-------------------------------------------------------------------------------*/

/*
	Data
*/

const IEP_LITE_sBLEModeSpecificSettings	asBLEBlackModes [ IEP_LITE_BLE_NO_OF_MODES ] = {
    /*  Safety check			D 																					Slope											*/
    {	IEP_LITE_BLE_OFF,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(1.0 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Off		*/

    {	IEP_LITE_BLE_LOW,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.9 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Low		*/
    {	IEP_LITE_BLE_MEDIUM,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.6 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Medium	*/
    {	IEP_LITE_BLE_HIGH,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.4 * (1ul << IEP_LITE_BLE_RES_PAR))	}	/* High		*/
};

const IEP_LITE_sBLEModeSpecificSettings	asBLEWhiteModes [ IEP_LITE_BLE_NO_OF_MODES ] = {
    /*  Safety check			D 																					Slope											*/
    {	IEP_LITE_BLE_OFF,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(1.0 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Off		*/

    {	IEP_LITE_BLE_LOW,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.9 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Low		*/
    {	IEP_LITE_BLE_MEDIUM,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.6 * (1ul << IEP_LITE_BLE_RES_PAR))	},	/* Medium	*/
    {	IEP_LITE_BLE_HIGH,	(img_uint32)(0.3f * (1ul << (IEP_LITE_BLE_RES_PAR + IEP_LITE_BLE_RES_PAR_CORR))),	(img_uint32)(0.4 * (1ul << IEP_LITE_BLE_RES_PAR))	}	/* High		*/
};

/*--------------------------- End of File --------------------------------*/
