
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
#define	V4L2_BUF_NUM	6

int g_num_buffers;

struct buffer {
	void *start;
	struct v4l2_buffer buf;
};
#define G_WIDTH		640
#define G_HEIGHT	480
int main(int argc, char **argv)
{
	char v4l_devname[20] = "/dev/video1";
	int fd_v4l;
	struct v4l2_requestbuffers req;
	int ibcnt = V4L2_BUF_NUM;
	int i = 0;
	unsigned int out_addr;
	struct v4l2_output output;
	struct v4l2_format format;
	struct v4l2_framebuffer fb;
	struct v4l2_crop crop;
	int out_idx = 1;
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	struct buffer *buffers;
	int cnt = 0;
	int fd;
	int s0_size;
	unsigned int total_time;
	struct timeval tv_start, tv_current;
	int ret = 0;
	if ((fd_v4l = open(v4l_devname, O_RDWR, 0)) < 0) {
		printf("unable to open %s for output, continue searching "
			"device.\n", v4l_devname);
		return -1;
	}
	
	buffers = calloc(V4L2_BUF_NUM, sizeof(struct buffer));

	if (!buffers) {
		perror("insufficient buffer memory");
		return -1;
	}
	
	if (ioctl(fd_v4l, VIDIOC_S_OUTPUT, &out_idx) < 0) {
		perror("failed to set output");
		return -1;
	}

	output.index = out_idx;

	if (ioctl(fd_v4l, VIDIOC_ENUMOUTPUT, &output) >= 0) {
		out_addr = output.reserved[0];
		printf("V4L output %d (0x%08x): %s\n",
		       output.index, output.reserved[0], output.name);
	} else {
		perror("VIDIOC_ENUMOUTPUT");
		return -1;
	}

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	format.fmt.pix.width = G_WIDTH;
	format.fmt.pix.height = G_HEIGHT;
	format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &format) < 0) {
		perror("VIDIOC_S_FMT output");
		return -1;
	}

	printf("Video input format: %dx%d\n", G_WIDTH, G_HEIGHT);
	
	req.count = ibcnt;
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory = V4L2_MEMORY_MMAP;
	printf("request count %d\n", req.count);
	g_num_buffers = req.count;

	if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0) {
		perror("VIDIOC_REQBUFS");
		return -1;
	}

	if (req.count < ibcnt) {
		perror("insufficient buffer control memory");
		return -1;
	}

	for (i = 0; i < ibcnt; i++) {
		buffers[i].buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buffers[i].buf.memory = V4L2_MEMORY_MMAP;
		buffers[i].buf.index = i;

		if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buffers[i].buf) < 0) {
			perror("VIDIOC_QUERYBUF");
			return -1;
		}

		buffers[i].start = mmap(NULL /* start anywhere */ ,
					     buffers[i].buf.length,
					     PROT_READ | PROT_WRITE,
					     MAP_SHARED,
					     fd_v4l,
					     buffers[i].buf.m.offset);

		if (buffers[i].start == MAP_FAILED) {
			perror("failed to mmap pxp buffer");
			return -1;
		}
	}

	/* Set FB overlay options */
	fb.flags = V4L2_FBUF_FLAG_OVERLAY;

	//if (pxp->global_alpha)
		fb.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
	//if (pxp->colorkey)
	//	fb.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
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
	format.fmt.win.w.width = G_WIDTH;
	format.fmt.win.w.height = G_HEIGHT;
	printf("win.w.l/t/w/h = %d/%d/%d/%d\n", format.fmt.win.w.left,
	       format.fmt.win.w.top,
	       format.fmt.win.w.width, format.fmt.win.w.height);
	if (ioctl(fd_v4l, VIDIOC_S_FMT, &format) < 0) {
		perror("VIDIOC_S_FMT output overlay");
		return -1;
	}

	/* Set cropping window */
	crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY;
	crop.c.left = 0;
	crop.c.top = 0;
	crop.c.width = G_WIDTH;
	crop.c.height = G_HEIGHT;
	printf("crop.c.l/t/w/h = %d/%d/%d/%d\n", crop.c.left,
	       crop.c.top, crop.c.width, crop.c.height);
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0) {
		perror("VIDIOC_S_CROP");
		return -1;
	}
	
	s0_size = G_WIDTH * G_HEIGHT * 2;
	if ((fd = open("./1.yuv", O_RDWR, 0)) < 0) {
		perror("s0 data open failed");
		return -1;
	}

	printf("PxP processing: start...\n");

	gettimeofday(&tv_start, NULL);

	for (i = 0;; i++) {

		struct v4l2_buffer buf;
		buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		if (i < g_num_buffers) {
			buf.index = i;
			if (ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf) < 0) {
				printf("VIDIOC_QUERYBUF failed\n");
				ret = -1;
				break;
			}
		} else {
			if (ioctl(fd_v4l, VIDIOC_DQBUF, &buf) < 0) {
				printf("VIDIOC_DQBUF failed\n");
				ret = -1;
				break;
			}
		}

		cnt = read(fd, buffers[buf.index].start, s0_size);
		if (cnt < s0_size)
			break;

		if (ioctl(fd_v4l, VIDIOC_QBUF, &buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
			ret = -1;
			break;
		}
		if (i == 2) {
			int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			if (ioctl(fd_v4l, VIDIOC_STREAMON, &type) < 0) {
				printf("Can't stream on\n");
				ret = -1;
				break;
			}
		}
	}
	if (ret == -1)
		return ret;

	gettimeofday(&tv_current, NULL);
	total_time = (tv_current.tv_sec - tv_start.tv_sec) * 1000000L;
	total_time += tv_current.tv_usec - tv_start.tv_usec;
	printf("total time for %u frames = %u us, %lld fps\n", i, total_time,
	       (i * 1000000ULL) / total_time);

	close(fd);

	sleep(5);
	printf("complete\n");

	/* Disable PxP */
	if (ioctl(fd_v4l, VIDIOC_STREAMOFF, &type) < 0) {
		perror("VIDIOC_STREAMOFF");
		return -1;
	}

	for (i = 0; i < g_num_buffers; i++)
		munmap(buffers[i].start, buffers[i].buf.length);
	close(fd_v4l);
	free(buffers);
	return 0;
}
