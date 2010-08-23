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
/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

/* Most of the codes are copied from  V4L2 sample capture.c, see
 * Video for Linux Two API Specification
 * http://v4l2spec.bytesex.org/spec-single/v4l2.html#CAPTURE-EXAMPLE
 */

/* for buffer sharing between CI and VA */

#ifndef _CI_VA_H
#define _CI_VA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>		/* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

struct ci_frame_info
{
	unsigned long frame_id; /* in */
	unsigned int width; /* out */
	unsigned int height; /* out */
	unsigned int stride; /* out */
	unsigned int fourcc; /* out */
	unsigned int offset; /* out */
};

#define ISP_IOCTL_GET_FRAME_INFO _IOWR('V', 192 + 5, struct ci_frame_info)


static int
open_device (char *dev_name)
{
  struct stat st;
  int fd;
  
  if (-1 == stat (dev_name, &st))
    {
      fprintf (stderr, "Cannot identify '%s': %d, %s\n",
	       dev_name, errno, strerror (errno));
      return -1;
    }

  if (!S_ISCHR (st.st_mode))
    {
      fprintf (stderr, "%s is no device\n", dev_name);
      return -1;
    }

  fd = open (dev_name, O_RDWR /* required */  | O_NONBLOCK, 0);

  if (-1 == fd)
    {
      fprintf (stderr, "Cannot open '%s': %d, %s\n",
	       dev_name, errno, strerror (errno));
      return -1;
    }

  return fd;
}


static int
close_device (int fd)
{
  if (-1 == close (fd)) {
    fprintf (stderr, "close device failed\n");
    return -1;
  }

  return 0;
}

static int
xioctl (int fd, int request, void *arg)
{
  int r;

  do
    r = ioctl (fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}

static int 
ci_get_frame_info (int fd, struct ci_frame_info *frame_info)
{
  if (-1 == xioctl (fd, ISP_IOCTL_GET_FRAME_INFO, frame_info))
    {
      if (EINVAL == errno)
	{
	  fprintf (stderr, "Camear IOCTL ISP_IOCTL_GET_FRAME_INFO failed\n");
	  return -1;
	}
      else
	{
	  fprintf (stderr, "Camear IOCTL ISP_IOCTL_GET_FRAME_INFO failed\n");
          return -1;
	}
    }
  return 0;
}


#endif

