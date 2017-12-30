#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string.h>
#include <malloc.h>
#include <linux/fb.h>
#include "buf.h"
#include "in.h"
static void print_pixelformat(char *prefix, int val)
{
	printf("%s: %c%c%c%c\n", prefix ? prefix : "pixelformat",
			val & 0xff,
			(val >> 8) & 0xff,
			(val >> 16) & 0xff,
			(val >> 24) & 0xff);
}
int video_in_init(int width, int height, struct buffer **in_buf, int buf_num)
{
	char v4l_device[100] = "/dev/video0";
	int input = 0;
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_crop crop;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	unsigned int i;
	int fd_v4l = 0;
	struct v4l2_requestbuffers req;

	if ((fd_v4l = open(v4l_device, O_RDWR, 0)) < 0)
	{
		printf("Unable to open %s\n", v4l_device);
		return -1;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 30;
	parm.parm.capture.capturemode = 1;

	if (ioctl(fd_v4l, VIDIOC_S_PARM, &parm) < 0)
	{
		printf("VIDIOC_S_PARM failed\n");
		return -1;
	}

	if (ioctl(fd_v4l, VIDIOC_S_INPUT, &input) < 0)
	{
		printf("VIDIOC_S_INPUT failed\n");
		return -1;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_v4l, VIDIOC_G_CROP, &crop) < 0)
	{
		printf("VIDIOC_G_CROP failed\n");
		return -1;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c.width = width;
	crop.c.height = height;
	crop.c.top = 0;
	crop.c.left = 0;
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0)
	{
		printf("VIDIOC_S_CROP failed\n");
		return -1;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.bytesperline = width * 2; //?
	fmt.fmt.pix.sizeimage = 0;

	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0)
	{
		printf("set format failed\n");
		return -1;
	}

	memset(&req, 0, sizeof (req));
	req.count = buf_num;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0)
	{
		printf("v4l_capture_setup: VIDIOC_REQBUFS failed\n");
		return -1;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_v4l, VIDIOC_G_FMT, &fmt) < 0)
	{
		printf("get format failed\n");
		return -1;
	}
	else
	{
		printf("\t VIDEO IN Width = %d", fmt.fmt.pix.width);
		printf("\t Height = %d", fmt.fmt.pix.height);
		printf("\t Image size = %d\n", fmt.fmt.pix.sizeimage);
		print_pixelformat(0, fmt.fmt.pix.pixelformat);
	}

	in_buf = calloc(buf_num, sizeof(struct buffer *));

	if (!in_buf) {
		perror("insufficient buffer memory");
		return -1;
	}
	for (i = 0; i < buf_num; i++)
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf) < 0)
		{
			printf("VIDIOC_QUERYBUF error\n");
			return -1;
		}
		in_buf[i] = (struct buffer *)malloc(sizeof(struct buffer));
		in_buf[i]->length = buf.length;
		in_buf[i]->offset = (size_t) buf.m.offset;
		in_buf[i]->start = mmap (NULL, in_buf[i]->length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				fd_v4l, in_buf[i]->offset);
		memset(in_buf[i]->start, 0xFF, in_buf[i]->length);
	}

	for (i = 0; i < buf_num; i++)
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.m.offset = in_buf[i]->offset;

		if (ioctl (fd_v4l, VIDIOC_QBUF, &buf) < 0) {
			printf("VIDIOC_QBUF error\n");
			return -1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd_v4l, VIDIOC_STREAMON, &type) < 0) {
		printf("VIDIOC_STREAMON error\n");
		return -1;
	}
	return fd_v4l;
}
void video_in_deinit(int fd, struct buffer **in_buf, int buf_num)
{
	enum v4l2_buf_type type;
	int i;
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl (fd, VIDIOC_STREAMOFF, &type);
	for (i = 0; i < buf_num; i++)
	{
		munmap(in_buf[i]->start, in_buf[i]->length);
		free(in_buf[i]);
	}
	free(in_buf);
	close(fd);
}
