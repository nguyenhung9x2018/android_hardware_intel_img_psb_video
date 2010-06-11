/*
** psb_texture.h
** Login : <brady@luna.bj.intel.com>
** Started on  Wed Mar 31 14:40:52 2010 brady
** $Id$
** 
** Copyright (C) 2010 brady
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef   	PSB_TEXTURE_H_
# define   	PSB_TEXTURE_H_



void init_test_texture(VADriverContextP ctx);

void deinit_test_texture(VADriverContextP ctx);

void blit_texture_to_buf(VADriverContextP ctx, unsigned char * data, int src_x, int src_y, int src_w,
			 int src_h, int dst_x, int dst_y, int dst_w, int dst_h,
			 int width, int height, int src_pitch, struct _WsbmBufferObject * src_buf,
			 unsigned int placement);


#endif 	    /* !PSB_TEXTURE_H_ */
