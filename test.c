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

#define ipu_fourcc(a,b,c,d)\
	(((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))
#define IPU_PIX_FMT_RGB565  ipu_fourcc('R','G','B','P') /*!< 16  RGB-5-6-5     */

int g_display_lcd = 0;
int g_in_width = 640;
int g_in_height = 480;
int g_out_width = 640;
int g_out_height = 480;
int g_top = 0;
int g_left = 0;
int g_camera_framerate = 30;
int g_capture_mode = 0;
char g_v4l_device[100] = "/dev/video0";
int main(int argc, char **argv)
{
	int fbfd = -1;
	int overlay=1;	
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_crop crop;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	int fd_v4l = 0;

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
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
		return 0;
	}
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) {
		return 0;
	}
	if (ioctl(fd_v4l, VIDIOC_S_OUTPUT, &g_display_lcd) < 0)
	{
		printf("VIDIOC_S_OUTPUT failed\n");
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

	crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	crop.c.width = g_in_width;
	crop.c.height = g_in_height;
	crop.c.top = g_top;
	crop.c.left = g_left;
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0)
	{
		printf("VIDIOC_S_CROP failed\n");
		return -1;
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.top=  g_top ;
	fmt.fmt.win.w.left= g_left;
	fmt.fmt.win.w.width=g_out_width;
	fmt.fmt.win.w.height=g_out_height;

	if (ioctl(fd_v4l, VIDIOC_S_FMT, &fmt) < 0)
	{
		printf("set format failed\n");
		return 0;
	}
	struct v4l2_framebuffer fb_v4l2;
	memset(&fb_v4l2, 0, sizeof(fb_v4l2));
	fb_v4l2.fmt.width = vinfo.xres;
	fb_v4l2.fmt.height = vinfo.yres;
	fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_RGB565;
	fb_v4l2.fmt.bytesperline = 2 * fb_v4l2.fmt.width;
	fb_v4l2.flags = V4L2_FBUF_FLAG_PRIMARY;
	fb_v4l2.base = (void *) finfo.smem_start +
		finfo.line_length*vinfo.yoffset;

	close(fbfd);
	if (ioctl(fd_v4l, VIDIOC_S_FBUF, &fb_v4l2) < 0)
	{
		printf("set framebuffer failed\n");

	}
	if (ioctl(fd_v4l, VIDIOC_G_FBUF, &fb_v4l2) < 0) {
		printf("get framebuffer failed\n");

	}

	if (ioctl(fd_v4l, VIDIOC_OVERLAY, &overlay) < 0) {
		printf("VIDIOC_OVERLAY start failed\n");
	}
	sleep(3600);

	overlay = 0;
	if (ioctl(fd_v4l, VIDIOC_OVERLAY, &overlay) < 0)
	{
		printf("VIDIOC_OVERLAY stop failed\n");
	}
	close(fd_v4l);
	return 0;
}
