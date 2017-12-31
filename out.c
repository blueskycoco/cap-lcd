#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>

#include <linux/fb.h>
#include <linux/videodev2.h>
#include "buf.h"
#include "out.h"
int video_out_init(int width, int height, struct buffer **out_buf, int buf_num)
{
	char v4l_devname[20] = "/dev/video1";
	int fd_v4l;
	struct v4l2_requestbuffers req;
	int i = 0;
	//unsigned int out_addr;
	struct v4l2_output output;
	struct v4l2_format format;
	struct v4l2_framebuffer fb;
	struct v4l2_crop crop;
	struct v4l2_buffer buf;
	struct v4l2_control vc;
	int out_idx = 1;
	int tfd;
	char buf1[8];
	tfd = open("/dev/tty0", O_RDWR);
	sprintf(buf1, "%s", "\033[9;0]");
	write(tfd, buf1, 7);
	close(tfd);
	if ((fd_v4l = open(v4l_devname, O_RDWR, 0)) < 0) {
		printf("unable to open %s for output, continue searching "
				"device.\n", v4l_devname);
		return -1;
	}
	
	vc.id = V4L2_CID_PRIVATE_BASE;
	vc.value = 180;
	if (ioctl(fd_v4l, VIDIOC_S_CTRL, &vc) < 0) {
		perror("VIDIOC_S_CTRL");
		return -1;
	}
	*out_buf = (struct buffer *)calloc(buf_num, sizeof(struct buffer));

	if (!*out_buf) {
		perror("insufficient buffer memory");
		return -1;
	}

	if (ioctl(fd_v4l, VIDIOC_S_OUTPUT, &out_idx) < 0) {
		perror("failed to set output");
		return -1;
	}

	output.index = out_idx;

	if (ioctl(fd_v4l, VIDIOC_ENUMOUTPUT, &output) >= 0) {
		//out_addr = output.reserved[0];
		//printf("V4L output %d (0x%08x): %s\n",
		//       output.index, output.reserved[0], output.name);
	} else {
		perror("VIDIOC_ENUMOUTPUT");
		return -1;
	}

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	format.fmt.pix.width = width;
	format.fmt.pix.height = height;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &format) < 0) {
		perror("VIDIOC_S_FMT output");
		return -1;
	}

	//printf("Video input format: %dx%d\n", width, height);

	req.count = buf_num;
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory = V4L2_MEMORY_MMAP;
	//printf("request count %d\n", req.count);

	if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0) {
		perror("VIDIOC_REQBUFS");
		return -1;
	}

	if (req.count < buf_num) {
		perror("insufficient buffer control memory");
		return -1;
	}

	for (i = 0; i < buf_num; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf) < 0) {
			perror("VIDIOC_QUERYBUF");
			return -1;
		}

		//out_buf[i] = (struct buffer *)malloc(sizeof(struct buffer));
		(*out_buf)[i].length = buf.length;
		(*out_buf)[i].offset = (size_t) buf.m.offset;
		(*out_buf)[i].start = mmap(NULL /* start anywhere */ ,
				(*out_buf)[i].length,
				PROT_READ | PROT_WRITE,
				MAP_SHARED,
				fd_v4l,
				(*out_buf)[i].offset);

		if ((*out_buf)[i].start == MAP_FAILED) {
			perror("failed to mmap pxp buffer");
			return -1;
		}
	}

	/* Set FB overlay options */
	fb.flags = V4L2_FBUF_FLAG_OVERLAY;
	fb.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
	if (ioctl(fd_v4l, VIDIOC_S_FBUF, &fb) < 0) {
		perror("VIDIOC_S_FBUF");
		return -1;
	}

	/* Set overlay source window */
	memset(&format, 0, sizeof(struct v4l2_format));
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
	format.fmt.win.global_alpha = 0;
	format.fmt.win.chromakey = 0;
	format.fmt.win.w.left = 0;
	format.fmt.win.w.top = 0;
	format.fmt.win.w.width = width;
	format.fmt.win.w.height = height;
	//printf("win.w.l/t/w/h = %d/%d/%d/%d\n", format.fmt.win.w.left,
	//       format.fmt.win.w.top,
	//       format.fmt.win.w.width, format.fmt.win.w.height);
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &format) < 0) {
		perror("VIDIOC_S_FMT output overlay");
		return -1;
	}

	/* Set cropping window */
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
	crop.c.left = 80;
	crop.c.top = 0;
	crop.c.width = width;
	crop.c.height = height;
	//printf("crop.c.l/t/w/h = %d/%d/%d/%d\n", crop.c.left,
	//       crop.c.top, crop.c.width, crop.c.height);
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0) {
		perror("VIDIOC_S_CROP");
		return -1;
	}

	return fd_v4l;
}
void video_out_deinit(int fd, struct buffer **out_buf, int buf_num)
{
	int i=0;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (fd) { 
		if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
			perror("VIDIOC_STREAMOFF");
			return ;
		}
		close(fd);
	}
	if (*out_buf) {
		for (i = 0; i < buf_num; i++) {
			munmap((*out_buf)[i].start, (*out_buf)[i].length);
		}
		free(*out_buf);
	}
}
