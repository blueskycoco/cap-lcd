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

#define TEST_BUFFER_NUM 3

struct testbuffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};

struct testbuffer buffers[TEST_BUFFER_NUM];
int g_in_width = 640;
int g_in_height = 480;
int g_out_width = 640;
int g_out_height = 480;
int g_top = 0;
int g_left = 0;
int g_input = 0;
int g_capture_count = 100;
int g_cap_fmt = V4L2_PIX_FMT_YUYV;
int g_camera_framerate = 30;
int g_capture_mode = 0;
char g_v4l_device[100] = "/dev/video0";
static char *fbp=NULL;
static long screensize=0;
static int fbfd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
inline int clip(int value, int min, int max) {
	    return (value > max ? max : value < min ? min : value);
}
static void process_image (const void * p){


	unsigned char* in=(char*)p;
	int width=640;
	int height=480;
	int istride=1280;
	int x,y,j;
	int y0,u,y1,v,r,g,b;
	long location=0;
	int xpos = (vinfo.xres-width)/2;
	int ypos = (vinfo.yres-height)/2;

	for ( y = ypos; y < height + ypos; ++y) {
		for (j = 0, x=xpos; j < width * 2 ; j += 4,x +=2) {

			location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) +
				(y+vinfo.yoffset) * finfo.line_length;

			y0 = in[j];
			u = in[j + 1] - 128; 
			y1 = in[j + 2]; 
			v = in[j + 3] - 128; 

			r = (298 * y0 + 409 * v + 128) >> 8;
			g = (298 * y0 - 100 * u - 208 * v + 128) >> 8;
			b = (298 * y0 + 516 * u + 128) >> 8;

			fbp[ location + 0] = clip(b, 0, 255);
			fbp[ location + 1] = clip(g, 0, 255);
			fbp[ location + 2] = clip(r, 0, 255); 
			fbp[ location + 3] = 255; 

			r = (298 * y1 + 409 * v + 128) >> 8;
			g = (298 * y1 - 100 * u - 208 * v + 128) >> 8;
			b = (298 * y1 + 516 * u + 128) >> 8;

			fbp[ location + 4] = clip(b, 0, 255);
			fbp[ location + 5] = clip(g, 0, 255);
			fbp[ location + 6] = clip(r, 0, 255); 
			fbp[ location + 7] = 255; 
		}
		in +=istride;
	}

}
static void print_pixelformat(char *prefix, int val)
{
	printf("%s: %c%c%c%c\n", prefix ? prefix : "pixelformat",
			val & 0xff,
			(val >> 8) & 0xff,
			(val >> 16) & 0xff,
			(val >> 24) & 0xff);
}
int stop_capturing(int fd_v4l)
{
	enum v4l2_buf_type type;
	if (-1 == munmap(fbp, screensize)) {
		printf(" Error: framebuffer device munmap() failed.\n");
	} 
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return ioctl (fd_v4l, VIDIOC_STREAMOFF, &type);
}
int start_capturing(int fd_v4l)
{
	unsigned int i;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;
	fbp = (char *)mmap(NULL,screensize,PROT_READ | PROT_WRITE,MAP_SHARED ,fbfd, 0);
	if ((int)fbp == -1) {
		printf("Error: failed to map framebuffer device to memory.\n");
		exit (EXIT_FAILURE) ;
	}
	memset(fbp, 0, screensize);
	for (i = 0; i < TEST_BUFFER_NUM; i++)
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

		buffers[i].length = buf.length;
		buffers[i].offset = (size_t) buf.m.offset;
		buffers[i].start = mmap (NULL, buffers[i].length,
				PROT_READ | PROT_WRITE, MAP_SHARED,
				fd_v4l, buffers[i].offset);
		memset(buffers[i].start, 0xFF, buffers[i].length);
	}

	for (i = 0; i < TEST_BUFFER_NUM; i++)
	{
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		buf.m.offset = buffers[i].offset;

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
	return 0;
}
int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_crop crop;
	int fd_v4l = 0;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_fmtdesc ffmt;
	struct v4l2_requestbuffers req;

	if ((fd_v4l = open(g_v4l_device, O_RDWR, 0)) < 0)
	{
		printf("Unable to open %s\n", g_v4l_device);
		return 0;
	}
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd==-1) {
		printf("Error: cannot open framebuffer device.\n");
		return 0;
	}
	printf("sensor supported frame size:\n");
	fsize.index = 0;
	while (ioctl(fd_v4l, VIDIOC_ENUM_FRAMESIZES, &fsize) >= 0) {
		printf(" %dx%d\n", fsize.discrete.width,
				fsize.discrete.height);
		fsize.index++;
	}

	ffmt.index = 0;
	while (ioctl(fd_v4l, VIDIOC_ENUM_FMT, &ffmt) >= 0) {
		print_pixelformat("sensor frame format", ffmt.pixelformat);
		ffmt.index++;
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = g_camera_framerate;
	parm.parm.capture.capturemode = g_capture_mode;

	if (ioctl(fd_v4l, VIDIOC_S_PARM, &parm) < 0)
	{
		printf("VIDIOC_S_PARM failed\n");
		return -1;
	}

	if (ioctl(fd_v4l, VIDIOC_S_INPUT, &g_input) < 0)
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
	crop.c.width = g_in_width;
	crop.c.height = g_in_height;
	crop.c.top = g_top;
	crop.c.left = g_left;
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0)
	{
		printf("VIDIOC_S_CROP failed\n");
		return -1;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = g_cap_fmt;
	fmt.fmt.pix.width = g_out_width;
	fmt.fmt.pix.height = g_out_height;
	fmt.fmt.pix.bytesperline = g_out_width;
	fmt.fmt.pix.sizeimage = 0;

	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0)
	{
		printf("set format failed\n");
		return 0;
	}

	memset(&req, 0, sizeof (req));
	req.count = TEST_BUFFER_NUM;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_v4l, VIDIOC_REQBUFS, &req) < 0)
	{
		printf("v4l_capture_setup: VIDIOC_REQBUFS failed\n");
		return 0;
	}

	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
		return 0;
	}
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) {
		return 0;
	}
	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
	struct v4l2_buffer buf;
	FILE * fd_y_file = 0;
	int count = g_capture_count;
	if ((fd_y_file = fopen("./test.yuv", "wb")) ==NULL)
	{
		printf("Unable to create y frame recording file\n");
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
		printf("\t Width = %d", fmt.fmt.pix.width);
		printf("\t Height = %d", fmt.fmt.pix.height);
		printf("\t Image size = %d\n", fmt.fmt.pix.sizeimage);
		print_pixelformat(0, fmt.fmt.pix.pixelformat);
	}
	if (start_capturing(fd_v4l) < 0)
	{
		printf("start_capturing failed\n");
		return -1;
	}

	while (count-- > 0) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl (fd_v4l, VIDIOC_DQBUF, &buf) < 0)	{
			printf("VIDIOC_DQBUF failed.\n");
		}

		fwrite(buffers[buf.index].start, fmt.fmt.pix.sizeimage, 1, fd_y_file);
		process_image(buffers[buf.index].start);
		if (count >= TEST_BUFFER_NUM) {
			if (ioctl (fd_v4l, VIDIOC_QBUF, &buf) < 0) {
				printf("VIDIOC_QBUF failed\n");
				break;
			}
		}
		else
			printf("buf.index %d\n", buf.index);
	}

	if (stop_capturing(fd_v4l) < 0)
	{
		printf("stop_capturing failed\n");
		return -1;
	}

	fclose(fd_y_file);
	close(fbfd);
	close(fd_v4l);
	return 0;
}
