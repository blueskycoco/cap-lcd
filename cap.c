/*
 * Copyright 2004-2012 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * @file mxc_v4l2_still.c
 *
 * @brief Mxc Video For Linux 2 driver test application
 *
 */

#ifdef __cplusplus
extern "C"{
#endif

/*=======================================================================
  INCLUDE FILES
  =======================================================================*/
/* Standard Include Files */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string.h>
#include <malloc.h>
struct buffer {
	        void *                  start;
	                size_t                  length;
};
#define ipu_fourcc(a,b,c,d)\
	(((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))

#define IPU_PIX_FMT_YUYV    ipu_fourcc('Y','U','Y','V') /*!< 16 YUV 4:2:2 */
struct buffer *bufs = NULL;
static int g_width = 2592;
static int g_height = 1944;
static int g_top = 0;
static int g_left = 0;
static unsigned long g_pixelformat = IPU_PIX_FMT_YUYV;
static int g_bpp = 16;
static int g_camera_framerate = 30;
static int g_capture_mode = 0;
static char g_v4l_device[100] = "/dev/video0";

int v4l_capture_setup(int * fd_v4l)
{
	struct v4l2_streamparm parm;
	struct v4l2_format fmt;
	struct v4l2_crop crop;
	int ret = 0;

	if ((*fd_v4l = open(g_v4l_device, O_RDWR, 0)) < 0)
	{
		printf("Unable to open %s\n", g_v4l_device);
		return -1;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = g_camera_framerate;
	parm.parm.capture.capturemode = g_capture_mode;

	if ((ret = ioctl(*fd_v4l, VIDIOC_S_PARM, &parm)) < 0)
	{
		printf("VIDIOC_S_PARM failed\n");
		return ret;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c.left = g_left;
	crop.c.top = g_top;
	crop.c.width = g_width;
	crop.c.height = g_height;
	if ((ret = ioctl(*fd_v4l, VIDIOC_S_CROP, &crop)) < 0)
	{
		printf("set cropping failed\n");
		return ret;
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = g_pixelformat;
	fmt.fmt.pix.width = g_width;
	fmt.fmt.pix.height = g_height;
	fmt.fmt.pix.sizeimage = fmt.fmt.pix.width * fmt.fmt.pix.height * g_bpp / 8;
	fmt.fmt.pix.bytesperline = g_width * 2;

	if ((ret = ioctl(*fd_v4l, VIDIOC_S_FMT, &fmt)) < 0)
	{
		printf("set format failed\n");
		return ret;
	}

	return ret;
}

int v4l_capture_test(int fd_v4l)
{
	struct v4l2_format fmt;
	int fd_still = 0, ret = 0;
	//char *buf1;
	int i;
	char still_file[100] = "./test.jpg";

	if ((fd_still = open(still_file, O_RDWR | O_CREAT | O_TRUNC, 0x0666)) < 0)
	{
		printf("Unable to create y frame recording file\n");
		return -1;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ((ret = ioctl(fd_v4l, VIDIOC_G_FMT, &fmt)) < 0) {
		printf("get format failed\n");
		return ret;
	} else {
		printf("\t Width = %d\n", fmt.fmt.pix.width);
		printf("\t Height = %d\n", fmt.fmt.pix.height);
		printf("\t Image size = %d\n", fmt.fmt.pix.sizeimage);
		printf("\t Pixel format = %c%c%c%c\n",
				(char)(fmt.fmt.pix.pixelformat & 0xFF),
				(char)((fmt.fmt.pix.pixelformat & 0xFF00) >> 8),
				(char)((fmt.fmt.pix.pixelformat & 0xFF0000) >> 16),
				(char)((fmt.fmt.pix.pixelformat & 0xFF000000) >> 24));
	}

#if 0
	char *buf1 = (char *)malloc(fmt.fmt.pix.sizeimage);
	if (!buf1)
		goto exit0;

	memset(buf1, 0, fmt.fmt.pix.sizeimage);
	if (read(fd_v4l, buf1, fmt.fmt.pix.sizeimage) != fmt.fmt.pix.sizeimage) {
		printf("v4l2 read error.\n");
		goto exit0;
	}

	write(fd_still, buf1, fmt.fmt.pix.sizeimage);
exit0:
	if (buf1)
		free(buf1);
#else
	struct v4l2_requestbuffers req;
	memset(&req, 0, sizeof (req));
	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0)
	{
		printf("v4l_capture_setup: VIDIOC_REQBUFS failed\n");
		return -1;
	}
	struct v4l2_buffer buf;
	bufs = calloc (3, sizeof (struct buffer));
	for (i=0; i<3; i++) {
	memset(&buf, 0, sizeof (buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = i;
	if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf) < 0)
	{
		printf("VIDIOC_QUERYBUF error\n");
		return -1;
	}
	printf("begin mmap\r\n");
	bufs[i].length = buf.length;
	bufs[i].start = mmap (NULL, buf.length,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			fd_v4l, buf.m.offset);
	memset(bufs[i].start, 0xFF, buf.length);
	}
	for (i=0;i<3;i++) {
	memset(&buf, 0, sizeof (buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = i;
	if (ioctl (fd_v4l, VIDIOC_QBUF, &buf) < 0) {
		printf("VIDIOC_QBUF error\n");
		return -1;
	}
	}
	printf("begin stream on\r\n");
	enum v4l2_buf_type type;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd_v4l, VIDIOC_STREAMON, &type) < 0) {
		printf("VIDIOC_STREAMON error\n");
		return -1;
	}
	printf("begin dqbuf\r\n");
	memset(&buf, 0, sizeof (buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (ioctl (fd_v4l, VIDIOC_DQBUF, &buf) < 0)	{
		printf("VIDIOC_DQBUF failed.\n");
	}
	printf("begin write\r\n");
	write(fd_still, bufs[buf.index].start, bufs[buf.index].length);
	printf("begin stream off\r\n");
	ioctl(fd_v4l, VIDIOC_QBUF, &buf);
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl (fd_v4l, VIDIOC_STREAMOFF, &type);
	printf("begin munmap\r\n");
	for (i=0;i<3;i++)
	munmap(bufs[i].start, bufs[i].length);
#endif
	close(fd_still);
	close(fd_v4l);
	free(bufs);
	return ret;
}

int main(int argc, char **argv)
{
	int fd_v4l;
	int ret;

	ret = v4l_capture_setup(&fd_v4l);
	if (ret)
		return ret;

	ret = v4l_capture_test(fd_v4l);

	return ret;
}
