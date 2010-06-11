/*! 
******************************************************************************
 @file   : fixedpointmaths.c

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

 <b>Description:</b>\n
         This module contains various utility functions to assist in
		 performing fixed point calculations.

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

#include "img_iep_defs.h"
#include "fixedpointmaths.h"
#include "fixedpoint_sinetable.h"

/*!
******************************************************************************

 @Function					FIXEDPT_64BitMultiply

******************************************************************************/

img_uint32	FIXEDPT_64BitMultiply	(	img_uint32		ui32X,
										img_uint32		ui32Y,
										img_uint32		ui32PostShiftResult		)
{

	/* Note :																				*/
	/*																						*/
	/* Works on the principle that a 32 bit value, X, can be split into two 16 bit parts,	*/
	/* X0 (the lower 16	bits of X) and X1 (the upper sixteen bits of X), where X = X0 + X1.	*/
	/* Hence TWO 32 bit numbers, X and Y, can be expanded in this way and the				*/
	/* multiplication expanded :															*/
	/*	X * Y = (X0 + X1)(Y0 + Y1)															*/
	/*		  = X0.Y0 + X0.Y1 + X1.Y0 + X1.Y1												*/

	img_uint32	ui32X0, ui32X1, ui32Y0, ui32Y1;
	img_uint32	ui32C0, ui32C1, ui32C2, ui32C3;

	img_uint32  ui32Temp;

	/* Extract 16 bit portions */
	ui32X0		= ui32X & 0xFFFF;
	ui32Y0		= ui32Y & 0xFFFF;

	ui32X1		= (ui32X >> 16) & 0xFFFF;
	ui32Y1		= (ui32Y >> 16) & 0xFFFF;

	ui32Temp	= ui32X0 * ui32Y0;

	ui32C0		= ui32Temp & 0xFFFF;
	ui32C1		= ui32Temp >> 16;

	ui32Temp	=  ui32X1 * ui32Y0;

	ui32C1		+= ui32Temp & 0xFFFF;
	ui32C2		= ui32Temp >> 16;

	ui32Temp	=  ui32X0 * ui32Y1;

	ui32C1		+= ui32Temp & 0xFFFF;
	ui32C2		+= ui32Temp >> 16;

	ui32Temp	=  ui32X1 * ui32Y1;

	ui32C2		+= ui32Temp & 0xFFFF;
	ui32C3		= ui32Temp >> 16;

	/* Now ripple up the overflows */
	ui32C2		+= ui32C1 >> 16;
	ui32C3		+= ui32C2 >> 16;

	/* Combine 16 bit pairs into 2 32 bit values */
	ui32C0		= ui32C0 | (ui32C1 << 16);
	ui32C2		= (ui32C2 & 0xFFFF) | (ui32C3 << 16);

	/* Pick the required 32 bits */
	if(ui32PostShiftResult >= 32)
	{
		ui32C0	= ui32C2;
		ui32C2	= 0;
		ui32PostShiftResult -= 32;
	}

	ui32Temp = (ui32C0 >> ui32PostShiftResult) | (ui32C2 << (32 - ui32PostShiftResult));

	/* Ensure nothing is left in the upper 32 bit word following the downshift */
	IMG_ASSERT ( (ui32C2 >> ui32PostShiftResult) == 0 );

	return ui32Temp;
}

/*!
******************************************************************************

 @Function					FIXEDPT_64BitDivide

******************************************************************************/

img_uint32	FIXEDPT_64BitDivide	 (	img_uint32		ui32Numerator,
									img_uint32		ui32Denominator,
									img_uint32		ui32PostShiftResult	)
{
	img_uint32	ui32Remainder;
	img_uint32	ui32Divisor;
	img_uint32	ui32ResultTop;
	img_uint32	ui32ResultBot;
	img_uint32	ui32FinalResult;
	img_uint32	ui32DivBit;

	img_int32	i32DivisionShift; 

	/* No divisions by zero thankyou												*/
	IMG_ASSERT ( ui32Denominator );

	/* special case of zero numerator												*/
	if(!ui32Numerator)
	{
		return 0;
	}

	/* Set Numerator/Remainder to be "A * 2^32"										*/
	ui32Remainder = ui32Numerator;
	ui32Divisor	  = ui32Denominator;

	i32DivisionShift = 32;
	ui32ResultTop	  = 0;
	ui32ResultBot	  = 0;

	/* Do a pre-normalising step so that at least one of A or B has the MSB set		*/
	while(((ui32Remainder | ui32Divisor) & 0x80000000) == 0)
	{
		ui32Remainder <<= 1;
		ui32Divisor   <<= 1;
	}

	/* Now make adjust either remainder or numerator so that the other also has	the	*/
	/* top bit set																	*/
	if((ui32Remainder & 0x80000000) == 0)
	{
		do
		{
			ui32Remainder <<= 1;
			i32DivisionShift--;

		}
		while((ui32Remainder & 0x80000000) == 0);
	}
	else if((ui32Divisor & 0x80000000) == 0)
	{
		do
		{
			ui32Divisor <<= 1;
			i32DivisionShift++;

		}
		while((ui32Divisor & 0x80000000) == 0);
	}

	/* Set the "divbit" to access the correct bit in the 64 bit result				*/
	if(i32DivisionShift >= 32)
	{
		ui32DivBit =  1 << (i32DivisionShift -32);
	}
	else
	{
		ui32DivBit =  1 << i32DivisionShift;
	}

	/* Start doing the division														*/
	while(ui32Remainder && (i32DivisionShift >= 0))
	{
		/* Shift the remainder until the MSB is set									*/
		while((ui32Remainder & 0x80000000) == 0)
		{
			ui32Remainder <<= 1;
			i32DivisionShift --;

			ui32DivBit >>= 1;
			if(ui32DivBit == 0)
			{
				ui32DivBit = 0x80000000;
			}
		}

		/* If we've run out of sig digits, exit										*/
		if(i32DivisionShift < 0)
		{
			break;
		}


		/* If we can subtract the divisor from the current remainder...				*/
		if(ui32Remainder >= ui32Divisor)
		{
			ui32Remainder -= ui32Divisor;

			IMG_ASSERT (ui32Remainder < ui32Divisor);

			if(i32DivisionShift >= 32)
			{
				ui32ResultTop |= ui32DivBit;
			}
			else
			{
				ui32ResultBot |= ui32DivBit;
			}

			/* Move to the next bit...												*/
			ui32Remainder <<= 1;

			i32DivisionShift --;
			ui32DivBit >>= 1;
			if(i32DivisionShift == 31)
			{
				ui32DivBit = 0x80000000;
			}

		}
		/* else we MUST be able to subtract 1/2 of the divisor from the remainder..	*/
		/* Multiply the remainder by 2 (which loses the top bit which we know to be	*/
		/* set) and subtract the divisor.											*/
		else
		{
			ui32Remainder = (ui32Remainder << 1) - ui32Divisor;

			/* Because we are subtracting 1/2 the divisor pre-adjust our position.	*/
			/*																		*/
			/* Note that it doesnt matter if division shift goes negative here		*/
			/* because the DivBit will hit zero as well.							*/
			i32DivisionShift --;
			ui32DivBit >>= 1;
			if(i32DivisionShift == 31)
			{
				ui32DivBit = 0x80000000;
			}

			if(i32DivisionShift >= 32)
			{
				ui32ResultTop |= ui32DivBit;
			}
			else
			{
				ui32ResultBot |= ui32DivBit;
			}
		}/*end if/else*/

	}/*end while*/

	ui32FinalResult = (ui32ResultBot >> ui32PostShiftResult) | 
					  (ui32ResultTop << (32 - ui32PostShiftResult));

	IMG_ASSERT ( (ui32ResultTop >> ui32PostShiftResult) == 0 );

	return ui32FinalResult;
}

/*!
******************************************************************************

 @Function					FIXEDPT_64BitMultiply_Signed

******************************************************************************/

img_int32	FIXEDPT_64BitMultiply_Signed	(	img_int32		i32X,
												img_int32		i32Y,
												img_uint32		ui32PostShiftResult		)
{
	img_int64	i64IntermediateResult;

	i64IntermediateResult = (img_int64) i32X * (img_int64) i32Y;
	i64IntermediateResult >>= ui32PostShiftResult;

	/* After the shift, the signed bit in the resultant 32 bit value (i.e.: bit 31)	*/
	/* should be replicated in every bit above it in the 64 bit value. As such, we	*/
	/* can compare bits 31 and 63 in the intermediate result and if they do not		*/
	/* match we can assume we have overflowed.										*/
	IMG_ASSERT (( i64IntermediateResult >> 63 ) == ( i64IntermediateResult >> 31 ));

	return ((img_int32) i64IntermediateResult);
}

/*!
******************************************************************************

 @Function					FIXEDPT_64BitDivide_Signed

******************************************************************************/

/*INLINE*/	img_int32	FIXEDPT_64BitDivide_Signed	 (	img_int32		ui32Numerator,
														img_int32		ui32Denominator,
														img_uint32		ui32PostShiftResult	)
{
	img_int64	i64IntermediateResult;

	i64IntermediateResult = (img_int64) ui32Numerator << 32;
	i64IntermediateResult = i64IntermediateResult / (img_int64) ui32Denominator;
	i64IntermediateResult >>= ui32PostShiftResult;

	/* After the shift, the signed bit in the resultant 32 bit value (i.e.: bit 31)	*/
	/* should be replicated in every bit above it in the 64 bit value. As such, we	*/
	/* can compare bits 31 and 63 in the intermediate result and if they do not		*/
	/* match we can assume we have overflowed.										*/
	IMG_ASSERT (( i64IntermediateResult >> 63 ) == ( i64IntermediateResult >> 31 ));	

	return ((img_int32) i64IntermediateResult);
}

/*!
******************************************************************************

 @Function					FIXEDPT_Sine

******************************************************************************/

img_int32	FIXEDPT_Sine	( img_uint32 ui32Theta )
{
	/* Note : This function uses a lookup table for basic reference, then improves on the	*/
	/* result by interpolating linearly. A possible future enhancement would be to use		*/
	/* quadratic interpolation instead of linear.											*/

	img_uint32	ui32LookupRef;
	img_uint32	ui32TableRef;
	img_uint32	ui32TableValueBelow, ui32TableValueAbove;
	img_uint32	ui32Offset;
	img_uint32	ui32Difference;
	img_uint32	ui32LinearAddition;
	img_int32	i32InterpolatedValue;

	/* Theta must be no greater than 2PI */
	IMG_ASSERT ( ui32Theta <= (FIXEDPT_PI << 1) );

	if ( ui32Theta > (FIXEDPT_PI >> 1) )
	{			
		if ( ui32Theta > FIXEDPT_PI )
		{
			if ( ui32Theta > (FIXEDPT_PI + (FIXEDPT_PI >> 1)) )
			{
				/* Between 3PI/2 and 2PI				*/
				
				ui32LookupRef = (ui32Theta - (FIXEDPT_PI + (FIXEDPT_PI >> 1)));		/* Fit into range of table, 0 to PI/2					*/				
				ui32LookupRef = (FIXEDPT_PI >> 1) - ui32LookupRef;					/* Reference table 'backwards'							*/
			}
			else
			{
				/* Between PI and 3PI/2					*/
				ui32LookupRef = (ui32Theta - FIXEDPT_PI);
			}
		}
		else
		{
			/* Between PI/2 and PI */

			/* Fit into range of table, 0 to PI/2		*/
			ui32LookupRef = (ui32Theta - (FIXEDPT_PI >> 1));	
			/* Reference table 'backwards'				*/
			ui32LookupRef = (FIXEDPT_PI >> 1) - ui32LookupRef;	
		}
	}
	else
	{
		/* Between 0 and PI/2 */
		ui32LookupRef = ui32Theta;
	}	

	/********************************/
	/* Perform linear interpolation */
	/********************************/

	/* Lookup reference is a 1.29 bit unsigned fixed point value between 0 and PI/2	*/
	ui32TableRef = FIXEDPT_CONVERT_FRACTIONAL_BITS_NO_ROUNDING(ui32LookupRef,FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS,FIXEDPT_SINE_TABLE_FRACTION_SIZE);	/* Adjust to granularity of table.						*/

	ui32TableValueBelow = FIXEDPT_aui32FixedPointSineTable [ ui32TableRef ];
	ui32TableValueAbove = FIXEDPT_aui32FixedPointSineTable [ (ui32TableRef+1) ];

	ui32Difference		= ui32TableValueAbove - ui32TableValueBelow;								/* (Difference between two nearest points in table)		*/

	ui32Offset			= ui32LookupRef - (FIXEDPT_CONVERT_FRACTIONAL_BITS_NO_ROUNDING(ui32TableRef,FIXEDPT_SINE_TABLE_FRACTION_SIZE,FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS));	/* Horizontal difference between nearest point below	*/
																																								/* and actual requested point. Result is in 1.29 fmt	*/

	/* y=mx+c.  m=(diff y)/(diff x). therefore linear addition = (diff y * offset)/(diff x)		*/
	/* We know that (diff x) is one increment of an x.8 number. As such, a divide by (diff x)	*/
	/* is the same as multiplying by 256 (which we can achieve by not downshifting the multiply */
	/* as much as we otherwise would).															*/
	ui32LinearAddition	= FIXEDPT_64BitMultiply (	ui32Difference,									/* (1.30 * 1.29)/(2^-FIXEDPT_SINE_TABLE_FRACTION_SIZE)	*/
													ui32Offset, 
													(FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS-FIXEDPT_SINE_TABLE_FRACTION_SIZE)	);

	i32InterpolatedValue = ui32TableValueBelow + ui32LinearAddition;								/* Result is 1.30										*/

	if ( ui32Theta > FIXEDPT_PI )
	{
		i32InterpolatedValue = -1 * i32InterpolatedValue;
	}

	return i32InterpolatedValue;	
}

/*!
******************************************************************************

 @Function					FIXEDPT_Cosine

******************************************************************************/

img_int32	FIXEDPT_Cosine	(	img_uint32		ui32Theta	)
{
	img_uint32	ui32SinTheta;

	/* Theta must be no greater than 2PI */
	IMG_ASSERT ( ui32Theta <= (FIXEDPT_PI << 1) );

	/* Cos(Theta) = Sin(Theta+(PI/2 radians)) */
	if ( ui32Theta >= (FIXEDPT_PI + (FIXEDPT_PI >> 1)) )
	{
		ui32SinTheta = ui32Theta - (FIXEDPT_PI + (FIXEDPT_PI >> 1));
	}
	else
	{
		ui32SinTheta = ui32Theta + (FIXEDPT_PI >> 1);
	}

	return (FIXEDPT_Sine(ui32SinTheta));
}

/*!
******************************************************************************

 @Function					FIXEDPT_MatrixMultiply

******************************************************************************/

img_void	FIXEDPT_MatrixMultiply	(	img_uint32		ui32MatrixARows,
										img_uint32		ui32MatrixAColumns,
										img_uint32		ui32MatrixBRows,
										img_uint32		ui32MatrixBColumns,
										img_uint32		ui32ResultRows,
										img_uint32		ui32ResultColumns,
										img_int32 *		pai32MatrixA,
										img_int32 *		pai32MatrixB,
										img_int32 *		pai32ResultMatrix,
										img_uint32		ui32PostShiftResults	)
{
	img_uint32	ui32m;
	img_uint32	ui32n;
	img_uint32	ui32p;
	img_int64	i64Sum;

	/* Check that matrix pointers are valid */
	IMG_ASSERT ( pai32MatrixA != IMG_NULL );
	IMG_ASSERT ( pai32MatrixB != IMG_NULL );
	IMG_ASSERT ( pai32ResultMatrix != IMG_NULL );

	/* Check matrix dimensions are sensible */
	IMG_ASSERT ( ui32MatrixARows	!= 0 );
	IMG_ASSERT ( ui32MatrixAColumns != 0 );
	IMG_ASSERT ( ui32MatrixBRows	!= 0 );
	IMG_ASSERT ( ui32MatrixBColumns != 0 );
	IMG_ASSERT ( ui32ResultRows		!= 0 );
	IMG_ASSERT ( ui32ResultColumns	!= 0 );

	/* Can only multiply matrices where the number of columns in the left hand matrix	*/
	/* (matrix A) is the same as the number of rows in the right hand matrix (matrix B)	*/
	IMG_ASSERT ( ui32MatrixAColumns == ui32MatrixBRows );

	/* If matrix A has m rows and n columns, and matrix B has n rows and p columns,		*/
	/* then the matrix resulting from multiplying A by B will have m rows and p columns	*/
	IMG_ASSERT ( ui32ResultRows == ui32MatrixARows );
	IMG_ASSERT ( ui32ResultColumns == ui32MatrixBColumns );

	for ( ui32m=0; ui32m<ui32MatrixARows; ui32m++ )
	{
		for ( ui32p=0; ui32p<ui32MatrixBColumns; ui32p++ )
		{
			i64Sum = 0;
			for ( ui32n=0; ui32n<ui32MatrixAColumns; ui32n++ )
			{
				/* Sum += MatrixA[m,n] * MatrixB[n,p] */
				i64Sum += FIXEDPT_64BitMultiply_Signed ( pai32MatrixA[((ui32m*ui32MatrixAColumns)+ui32n)],
														 pai32MatrixB[((ui32n*ui32MatrixBColumns)+ui32p)],
														 ui32PostShiftResults );

				/* Check that the result still fits into 32 bits */
				IMG_ASSERT (( i64Sum >> 63 ) == ( i64Sum >> 31 ));
			}

			/* Set appropriate element in result */
			/* ResultMatrix [m,p] = Sum */
			pai32ResultMatrix [((ui32m*ui32ResultColumns)+ui32p)] = (img_int32) i64Sum;
		}
	}
}

/*!
******************************************************************************

 @Function					FIXEDPT_ScalarMatrixMultiply

******************************************************************************/

img_void	FIXEDPT_ScalarMatrixMultiply	(	img_uint32		ui32MatrixRows,
												img_uint32		ui32MatrixColumns,												
												img_int32 *		pai32MatrixA,
												img_int32 *		pai32MatrixB,
												img_int32 *		pai32ResultantMatrix,
												img_uint32		ui32PostShiftResults	)
{
	img_uint32	ui32m;
	img_uint32	ui32n;

	/* Check that matrix pointer is valid */
	IMG_ASSERT ( pai32MatrixA != IMG_NULL );
	IMG_ASSERT ( pai32MatrixB != IMG_NULL );
	IMG_ASSERT ( pai32ResultantMatrix != IMG_NULL );

	/* Resultant matrix cannot be the same as either of the input matrices, as this	*/
	/* will result in an incorrect result (as the result matrix is written as the	*/
	/* calculation proceeds, so it cannot be read from as well as written to as		*/
	/* of the same calculation).													*/
	IMG_ASSERT ( pai32ResultantMatrix != pai32MatrixA );
	IMG_ASSERT ( pai32ResultantMatrix != pai32MatrixB );

	/* Check matrix dimensions are sensible */
	IMG_ASSERT ( ui32MatrixRows		!= 0 );
	IMG_ASSERT ( ui32MatrixColumns	!= 0 );

	for ( ui32m=0; ui32m<ui32MatrixRows; ui32m++ )
	{
		for ( ui32n=0; ui32n<ui32MatrixColumns; ui32n++ )
		{
			/* Multiply each value in the matrix by the scalar value */
			pai32ResultantMatrix [((ui32m*ui32MatrixColumns)+ui32n)] = 
				FIXEDPT_64BitMultiply_Signed (	pai32MatrixA [((ui32m*ui32MatrixColumns)+ui32n)],
												pai32MatrixB [((ui32m*ui32MatrixColumns)+ui32n)],
												ui32PostShiftResults );
		}
	}
}
