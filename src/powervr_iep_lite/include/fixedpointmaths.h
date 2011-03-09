/*!
******************************************************************************
 @file   : fixedpointmaths.h

 @brief

 @Author Tom Whalley

 @date   07/01/2005

         <b>Copyright 2003 by Imagination Technologies Limited.</b>\n
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
         Fixed point maths utility functions header file

 <b>Platform:</b>\n
	     Platform Independent

 @Version
	    -	1.0	1st Release

******************************************************************************/

/*
******************************************************************************
 Modifications :-

 $Log: fixedpointmaths.h,v $
 Revision 1.9  2007/12/13 11:23:40  thw
 Added macro 'FIXEDPT_CONVERT_FLOAT_TO_FIXED'.

 Revision 1.8  2007/12/11 15:44:08  thw
 Added definition 'FIXEDPT_ONE_DEGREE_IN_RADIANS'.

 Revision 1.7  2007/12/11 14:43:37  thw
 Removed hard coded references to number of fractional bits used.
 Added 'convert no of fractional bits' macros.

 Revision 1.6  2007/12/10 18:17:58  thw
 Added prototype for 'FIXEDPT_Cosine'.

 Revision 1.5  2007/12/10 10:56:36  thw
 - Changed name of downshift variable in 'FIXEDPT_64BitDivide' from 'ui32NumIntBitsInResult' to 'ui32PostShiftResult'.
 - Added prototype for function 'FIXEDPT_64BitDivide_Signed'.
 - Changed prototype for function 'FIXEDPT_ScalarMatrixMultiply' to reflect new operation - now multiplies two matrices (scalar multiply) rather than a single matrix by a single value.

 Revision 1.4  2007/12/06 16:38:36  thw
 Added header for new function 'FIXEDPT_ScalarMatrixMultiply'.

 Revision 1.3  2007/12/06 15:55:46  thw
 Added headers for new functions 'FIXEDPT_64BitMultiply_Signed' and 'FIXEDPT_MatrixMultiply'.

 Revision 1.2  2007/07/04 10:45:13  thw
 Fixed Linux warning.

 Revision 1.1  2005/01/07 11:11:44  thw
 New addition - fixed point maths utilities header file


******************************************************************************/

#if !defined (__FIXEDPOINTMATHS_H__)
#define __FIXEDPOINTMATHS_H__

#if (__cplusplus)
extern "C"
{
#endif

    /* Macros */
#define	FIXEDPT_CONVERT_FRACTIONAL_BITS(number,from,to)						(((from)>(to))?(((number)+((1<<((from)-(to)))>>1))>>((from)-(to))):((number)<<((to)-(from))))
#define	FIXEDPT_CONVERT_FRACTIONAL_BITS_NO_ROUNDING(number,from,to)			(((from)>(to))?(((number))>>((from)-(to))):((number)<<((to)-(from))))
#define FIXEDPT_CONVERT_FLOAT_TO_FIXED(float_num,fractional_bits)			(((img_float)(float_num)) * (1<<(fractional_bits)))

    /* Constants */
#define	FIXEDPT_SINE_TABLE_FRACTION_SIZE		8													/* Sine table represents values 0->PI/2 in 1.x format */
#define	FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS		29
#define FIXEDPT_TRIG_OUTPUT_FRACTIONAL_BITS		30

#define	FIXEDPT_PI								(img_uint32) ((img_float) (3.1415927f) * (1 << FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS))	/* 2.18 fixed point format */
#define FIXEDPT_NO_OF_SINE_TABLE_ENTRIES		(sizeof(FIXEDPT_aui32FixedPointSineTable)/sizeof(img_uint32))
#define FIXEDPT_DIVISION_PER_SINE_TABLE_ENTRY	(FIXEDPT_PI/FIXEDPT_NO_OF_SINE_TABLE_ENTRIES)
#define	FIXEDPT_ONE_DEGREE_IN_RADIANS			(FIXEDPT_PI/180)

    /* Function prototypes */

    /*!
    ******************************************************************************

     @Function				FIXEDPT_64BitMultiply

     @Description			This function takes two unsigned 32 bit inputs and
    						multiplies them together to give a 64 bit result.
    						It then shifts the result down by a user specified
    						amount in order to return a 32 bit value.

     @Input		ui32X :		The first of two unsigned 32 bit values to multiply
    						together.

     @Input		ui32Y :		The second of two unsigned 32 bit values to multiply
    						together.

     @Input		ui32PostShiftResult :
    						The number of bits to right shift the 64 bit result of
    						the multiplication in order to achieve a 32 bit result.
    						Note : If the post shift result is not sufficient to
    						ensure that no data remains in the upper 32 bit word of
    						the 64 bit result, the function will cause an assert to
    						indicate that data is being lost.

     @Return   img_uint32 :	This function returns an unsigned 32 bit value,
    						which is the downshifted 64 bit result of the
    						multiplication.

    ******************************************************************************/
    /*INLINE*/	img_uint32	FIXEDPT_64BitMultiply(img_uint32		ui32X,
            img_uint32		ui32Y,
            img_uint32		ui32PostShiftResult);

    /*!
    ******************************************************************************

     @Function				FIXEDPT_64BitDivide

     @Description			This function takes two unsigned 32 bit inputs: a
    						numerator and a denominator. The numerator is promoted
    						to a 64 bit value before being divided by the
    						denominator. The result of the division is then
    						shifted down according to the 'ui32PostShiftResult'
    						parameter.

     @Input		ui32Numerator :
    						The 32 bit numerator.

     @Input		ui32Denominator :
    						The	32 bit denominator.

     @Input		ui32PostShiftResult :
    						The number of bits to right shift the 64 bit result of
    						the division in order to achieve a 32 bit result.
    						Note : If the post shift result is not sufficient to
    						ensure that no data remains in the upper 32 bit word of
    						the 64 bit result, the function will cause an assert to
    						indicate that data is being lost.

     @Return   img_uint32 :	This function returns an unsigned 32 bit value,
    						which is the downshifted 64 bit result of the
    						division.

    ******************************************************************************/
    /*INLINE*/
    img_uint32	FIXEDPT_64BitDivide(img_uint32		ui32Numerator,
                                   img_uint32		ui32Denominator,
                                   img_uint32		ui32PostShiftResult);

    /*!
    ******************************************************************************

     @Function				FIXEDPT_64BitMultiply_Signed

     @Description			This function takes two signed 32 bit inputs and
    						multiplies them together to give a 64 bit result.
    						It then shifts the result down by a user specified
    						amount in order to return a 32 bit value.

     @Input		ui32X :		The first of two signed 32 bit values to multiply
    						together.

     @Input		ui32Y :		The second of two signed 32 bit values to multiply
    						together.

     @Input		ui32PostShiftResult :
    						The number of bits to right shift the 64 bit result of
    						the multiplication in order to achieve a 32 bit result.
    						Note : If the post shift result is not sufficient to
    						ensure that no data remains in the upper 32 bit word of
    						the 64 bit result, the function will cause an assert to
    						indicate that data is being lost.

     @Return   img_uint32 :	This function returns a signed 32 bit value,
    						which is the downshifted 64 bit result of the
    						multiplication.

    ******************************************************************************/
    img_int32	FIXEDPT_64BitMultiply_Signed(img_int32		i32X,
                                           img_int32		i32Y,
                                           img_uint32		ui32PostShiftResult);

    /*!
    ******************************************************************************

     @Function				FIXEDPT_64BitDivide_Signed

     @Description			This function takes two signed 32 bit inputs: a
    						numerator and a denominator. The numerator is promoted
    						to a 64 bit value before being divided by the
    						denominator. The result of the division is then
    						shifted down according to the 'ui32PostShiftResult'
    						parameter.

     @Input		i32Numerator :
    						The 32 bit numerator.

     @Input		i32Denominator :
    						The	32 bit denominator.

     @Input		ui32PostShiftResult :
    						The number of bits to right shift the 64 bit result of
    						the division in order to achieve a 32 bit result.
    						Note : If the post shift result is not sufficient to
    						ensure that no data remains in the upper 32 bit word of
    						the 64 bit result, the function will cause an assert to
    						indicate that data is being lost.

     @Return   img_int32 :	This function returns a signed 32 bit value,
    						which is the downshifted 64 bit result of the
    						division.

    ******************************************************************************/

    img_int32	FIXEDPT_64BitDivide_Signed(img_int32		ui32Numerator,
                                         img_int32		ui32Denominator,
                                         img_uint32		ui32PostShiftResult);

    /*!
    ******************************************************************************

     @Function				FIXEDPT_Sine

     @Description			This function calculates Sine(Theta) where
    						both Theta and the result of the Sine function are
    						specified in fixed point format.

     @Input		ui32Theta : The value of Theta for which Sine(Theta) is to be
    						established. Theta is specified in 3.29 unsigned fixed
    						point format and must fall in the range 0 -> 2*PI
    						radians. The number of fractional bits used in the
    						input to this function (29) is defined by the constant
    						'FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS'.

     @Return   img_int32  :	This function returns a signed 32 bit value, in signed
    						1.30 bit fixed point format. The value will be in the
    						range -1 -> 1. The number of fractional bits used in
    						the output from this function (30) is defined by the
    						constant 'FIXEDPT_TRIG_OUTPUT_FRACTIONAL_BITS'.

    ******************************************************************************/
    img_int32	FIXEDPT_Sine(img_uint32		ui32Theta);


    /*!
    ******************************************************************************

     @Function				FIXEDPT_Cosine

     @Description			This function calculates Cosine(Theta) where
    						both Theta and the result of the Cosine function are
    						specified in fixed point format.

     @Input		ui32Theta : The value of Theta for which Cosine(Theta) is to be
    						established. Theta is specified in 3.29 unsigned fixed
    						point format and must fall in the range 0 -> 2*PI
    						radians. The number of fractional bits used in the
    						input to this function (29) is defined by the constant
    						'FIXEDPT_TRIG_INPUT_FRACTIONAL_BITS'.

     @Return   img_int32  :	This function returns a signed 32 bit value, in signed
    						1.30 bit fixed point format. The value will be in the
    						range -1 -> 1. The number of fractional bits used in
    						the output from this function (30) is defined by the
    						constant 'FIXEDPT_TRIG_OUTPUT_FRACTIONAL_BITS'.

    ******************************************************************************/
    img_int32	FIXEDPT_Cosine(img_uint32		ui32Theta);


    /*!
    ******************************************************************************

     @Function				FIXEDPT_MatrixMultiply

     @Description			This function multiplies two matrices, A and B,to
    						produce a third matrix;

     @Input		ui32MatrixARows :
    						The number of rows in the first matrix (A)

     @Input		ui32MatrixAColumns :
    						The number of columns in the first matrix (A)

     @Input		ui32MatrixBRows :
    						The number of rows in the second matrix (B)

     @Input		ui32MatrixBColumns :
    						The number of columns in the second matrix (B)

     @Input		ui32ui32ResultRows :
    						The number of rows in the resultant matrix. This
    						value is provided as a safety check, and must match
    						the value expected by the function.

     @Input		ui32ui32ResultColumns :
    						The number of columns in the resultant matrix. This
    						value is provided as a safety check, and must match
    						the value expected by the function.

     @Input		pai32MatrixA :
    						A pointer to the first matrix, comprising
    						(Rows(A) x Columns(A)) signed 32 bit values.

     @Input		pai32MatrixB :
    						A pointer to the second matrix, comprising
    						(Rows(B) x Columns(C)) signed 32 bit values.

     @Input		pai32ResultMatrix :
    						A pointer to the resultant matrix, comprising
    						(Rows(result) x Columns(result)) signed 32 bit values.
    						This matrix is overwritten by the function.

     @Input		ui32PostShiftResults :
    						The number of bits to postshift the result of each
    						multiplication by. This must be set such that the
    						results do not overflow 32 bits. The function will
    						assert in the case of an overflow.

     @Return	img_void :	This function does not return a value

    ******************************************************************************/
    img_void	FIXEDPT_MatrixMultiply(img_uint32		ui32MatrixARows,
                                    img_uint32		ui32MatrixAColumns,
                                    img_uint32		ui32MatrixBRows,
                                    img_uint32		ui32MatrixBColumns,
                                    img_uint32		ui32ResultRows,
                                    img_uint32		ui32ResultColumns,
                                    img_int32 *		pai32MatrixA,
                                    img_int32 *		pai32MatrixB,
                                    img_int32 *		pai32ResultMatrix,
                                    img_uint32		ui32PostShiftResults);

    /*!
    ******************************************************************************

     @Function				FIXEDPT_ScalarMatrixMultiply

     @Description			This function multiplies each value in a matrix, A, by
    						the corresponding value in a second matrix, B, placing
    						the results in a third matrix. This is a scalar
    						multiplication - NOT a matrix multiplication. All
    						three matrices must be of identical dimensions.

     @Input		ui32MatrixRows :
    						The number of rows in the matrix

     @Input		ui32MatrixColumns :
    						The number of columns in the matrix

     @Input		pai32MatrixA :
    						A pointer to the first matrix, comprising
    						(Rows x Columns) signed 32 bit values.

     @Input		pai32MatrixB :
    						A pointer to the second matrix, comprising
    						(Rows x Columns) signed 32 bit values.

     @Input		pai32ResultantMatrix :
    						A pointer to the resultant matrix - this matrix must
    						occupy a separate area of memory from MatrixA and
    						MatrixB - it is not valid to use a single matrix
    						as both input and output from this function.

     @Input		ui32PostShiftResults :
    						The number of bits to postshift the result of each
    						multiplication by. This must be set such that the
    						results do not overflow 32 bits. The function will
    						assert in the case of an overflow.

     @Return	img_void :	This function does not return a value

    ******************************************************************************/
    img_void	FIXEDPT_ScalarMatrixMultiply(img_uint32		ui32MatrixRows,
                                          img_uint32		ui32MatrixColumns,
                                          img_int32 *		pai32MatrixA,
                                          img_int32 *		pai32MatrixB,
                                          img_int32 *		pai32ResultantMatrix,
                                          img_uint32		ui32PostShiftResults);

#if (__cplusplus)
}
#endif

#endif	/* __FIXEDPOINTMATHS_H__ */
