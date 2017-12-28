/*
 * Copyright 2004-2013 Freescale Semiconductor, Inc. All rights reserved.
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
 * @file mxc_v4l2_overlay.c
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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* Verification Test Environment Include Files */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <signal.h>

//#include <linux/mxcfb.h>
#define MXCFB_SET_LOC_ALP_BUF    _IOW('F', 0x27, unsigned long)
#define MXCFB_SET_LOC_ALPHA     _IOWR('F', 0x26, struct mxcfb_loc_alpha)
#define MXCFB_SET_GBL_ALPHA     _IOW('F', 0x21, struct mxcfb_gbl_alpha)
#define MXCFB_SET_CLR_KEY       _IOW('F', 0x22, struct mxcfb_color_key)
struct mxcfb_loc_alpha {
	int enable;
	int alpha_in_pixel;
	unsigned long alpha_phy_addr0;
	unsigned long alpha_phy_addr1;
};
struct mxcfb_color_key {
	int enable;
	__u32 color_key;
};
struct mxcfb_gbl_alpha {
	int enable;
	int alpha;
};

#define TFAIL -1
#define TPASS 0

#define ipu_fourcc(a,b,c,d)\
	(((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))

#define IPU_PIX_FMT_RGB332  ipu_fourcc('R','G','B','1') /*!<  8  RGB-3-3-2     */
#define IPU_PIX_FMT_RGB555  ipu_fourcc('R','G','B','O') /*!< 16  RGB-5-5-5     */
#define IPU_PIX_FMT_RGB565  ipu_fourcc('R','G','B','P') /*!< 16  RGB-5-6-5     */
#define IPU_PIX_FMT_RGB666  ipu_fourcc('R','G','B','6') /*!< 18  RGB-6-6-6     */
#define IPU_PIX_FMT_BGR24   ipu_fourcc('B','G','R','3') /*!< 24  BGR-8-8-8     */
#define IPU_PIX_FMT_RGB24   ipu_fourcc('R','G','B','3') /*!< 24  RGB-8-8-8     */
#define IPU_PIX_FMT_BGR32   ipu_fourcc('B','G','R','4') /*!< 32  BGR-8-8-8-8   */
#define IPU_PIX_FMT_BGRA32  ipu_fourcc('B','G','R','A') /*!< 32  BGR-8-8-8-8   */
#define IPU_PIX_FMT_RGB32   ipu_fourcc('R','G','B','4') /*!< 32  RGB-8-8-8-8   */
#define IPU_PIX_FMT_RGBA32  ipu_fourcc('R','G','B','A') /*!< 32  RGB-8-8-8-8   */
#define IPU_PIX_FMT_ABGR32  ipu_fourcc('A','B','G','R') /*!< 32  ABGR-8-8-8-8  */

char v4l_device[100] = "/dev/video0";
int fd_v4l = 0;
int g_sensor_width = 640;
int g_sensor_height = 480;
int g_sensor_top = 0;
int g_sensor_left = 0;
int g_display_width = 1024;
int g_display_height = 768;
int g_display_top = 0;
int g_display_left = 0;
int g_rotate = 0;
int g_timeout = 3600;
int g_display_lcd = 0;
int g_overlay = 0;
int g_camera_color = 0;
int g_camera_framerate = 30;
int g_capture_mode = 0;
int g_alpha_mode = 0;
char *alpha_buf0 = NULL;
char *alpha_buf1 = NULL;
int alpha_fb_w = 0, alpha_fb_h = 0;
unsigned long loc_alpha_phy_addr0;
unsigned long loc_alpha_phy_addr1;
int alpha_buf_size = 0;
int g_fd_fb_fg = 0;

static void print_pixelformat(char *prefix, int val)
{
	printf("%s: %c%c%c%c\n", prefix ? prefix : "pixelformat",
			val & 0xff,
			(val >> 8) & 0xff,
			(val >> 16) & 0xff,
			(val >> 24) & 0xff);
}

	int
mxc_v4l_overlay_test(int timeout)
{
	int i;
	int overlay = 1;
	int retval = 0;
	struct v4l2_control ctl;
#ifdef BUILD_FOR_ANDROID
	char fb_device_0[100] = "/dev/graphics/fb0";
#else
	char fb_device_0[100] = "/dev/fb0";
#endif
	int fd_graphic_fb = 0;
	struct fb_var_screeninfo fb0_var;
	struct mxcfb_loc_alpha l_alpha;

	if (ioctl(fd_v4l, VIDIOC_OVERLAY, &overlay) < 0) {
		printf("VIDIOC_OVERLAY start failed\n");
		retval = TFAIL;
		goto out1;
	}

	if (g_alpha_mode) {
	}

	for (i = 0; i < 3 ; i++) {
		// flash a frame
		ctl.id = V4L2_CID_PRIVATE_BASE + 1;
		if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl) < 0)
		{
			printf("set ctl failed\n");
			retval = TFAIL;
			goto out2;
		}
		sleep(1);
	}

		sleep(timeout);

out2:
	overlay = 0;
	if (ioctl(fd_v4l, VIDIOC_OVERLAY, &overlay) < 0)
	{
		printf("VIDIOC_OVERLAY stop failed\n");
		retval = TFAIL;
		goto out1;
	}
out1:
	if (g_alpha_mode) {
	}

	close(fd_graphic_fb);
	return retval;
}

	int
mxc_v4l_overlay_setup(struct v4l2_format *fmt)
{
	struct v4l2_streamparm parm;
	v4l2_std_id id;
	struct v4l2_control ctl;
	struct v4l2_crop crop;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_fmtdesc ffmt;

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
		return TFAIL;
	}

	parm.parm.capture.timeperframe.numerator = 0;
	parm.parm.capture.timeperframe.denominator = 0;

	if (ioctl(fd_v4l, VIDIOC_G_PARM, &parm) < 0)
	{
		printf("get frame rate failed\n");
		return TFAIL;
	}

	printf("frame_rate is %d\n",
			parm.parm.capture.timeperframe.denominator);

	ctl.id = V4L2_CID_PRIVATE_BASE + 2;
	ctl.value = g_rotate;
	if (ioctl(fd_v4l, VIDIOC_S_CTRL, &ctl) < 0)
	{
		printf("set control failed\n");
		return TFAIL;
	}

	crop.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	crop.c.left = g_sensor_left;
	crop.c.top = g_sensor_top;
	crop.c.width = g_sensor_width;
	crop.c.height = g_sensor_height;
	if (ioctl(fd_v4l, VIDIOC_S_CROP, &crop) < 0)
	{
		printf("set cropping failed\n");
		return TFAIL;
	}

	if (ioctl(fd_v4l, VIDIOC_S_FMT, fmt) < 0)
	{
		printf("set format failed\n");
		return TFAIL;
	}

	if (ioctl(fd_v4l, VIDIOC_G_FMT, fmt) < 0)
	{
		printf("get format failed\n");
		return TFAIL;
	}

	if (ioctl(fd_v4l, VIDIOC_G_STD, &id) < 0)
	{
		printf("VIDIOC_G_STD failed\n");
		return TFAIL;
	}

	return TPASS;
}

	int
main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_framebuffer fb_v4l2;
#ifdef BUILD_FOR_ANDROID
	char fb_device_0[100] = "/dev/graphics/fb0";
#else
	char fb_device_0[100] = "/dev/fb0";
#endif
	char *fb_device_fg;
	int fd_fb_0 = 0;
	struct fb_fix_screeninfo fb0_fix, fb_fg_fix;
	struct fb_var_screeninfo fb0_var, fb_fg_var;
	struct mxcfb_color_key color_key;
	struct mxcfb_gbl_alpha g_alpha;
	struct mxcfb_loc_alpha l_alpha;
	int ret = 0;
	struct v4l2_dbg_chip_ident chip;

	if ((fd_v4l = open(v4l_device, O_RDWR, 0)) < 0) {
		printf("Unable to open %s\n", v4l_device);
		return TFAIL;
	}

	if (ioctl(fd_v4l, VIDIOC_DBG_G_CHIP_IDENT, &chip))
	{
		printf("VIDIOC_DBG_G_CHIP_IDENT failed.\n");
		return TFAIL;
	}
	printf("sensor chip is %s\n", chip.match.name);

	if ((fd_fb_0 = open(fb_device_0, O_RDWR )) < 0)	{
		printf("Unable to open frame buffer 0\n");
		return TFAIL;
	}

	if (ioctl(fd_fb_0, FBIOGET_VSCREENINFO, &fb0_var) < 0) {
		close(fd_fb_0);
		return TFAIL;
	}
	if (ioctl(fd_fb_0, FBIOGET_FSCREENINFO, &fb0_fix) < 0) {
		close(fd_fb_0);
		return TFAIL;
	}

	if (strcmp(fb0_fix.id, "DISP3 BG - DI1") == 0)
		g_display_lcd = 1;
	else if (strcmp(fb0_fix.id, "DISP3 BG") == 0)
		g_display_lcd = 0;
	else if (strcmp(fb0_fix.id, "DISP4 BG") == 0)
		g_display_lcd = 3;
	else if (strcmp(fb0_fix.id, "DISP4 BG - DI1") == 0)
		g_display_lcd = 4;
printf("S_OUTPUT %s %d\r\n", fb0_fix.id, g_display_lcd);
	if (ioctl(fd_v4l, VIDIOC_S_OUTPUT, &g_display_lcd) < 0)
	{
		printf("VIDIOC_S_OUTPUT failed\n");
		return TFAIL;
	}


	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.top=  g_display_top ;
	fmt.fmt.win.w.left= g_display_left;
	fmt.fmt.win.w.width=g_display_width;
	fmt.fmt.win.w.height=g_display_height;

	if (mxc_v4l_overlay_setup(&fmt) < 0) {
		printf("Setup overlay failed.\n");
		return TFAIL;
	}

	memset(&fb_v4l2, 0, sizeof(fb_v4l2));

	if (!g_overlay) {
		g_alpha.alpha = 255;
		g_alpha.enable = 1;
		if (ioctl(fd_fb_0, MXCFB_SET_GBL_ALPHA, &g_alpha) < 0) {
			printf("Set global alpha failed\n");
			close(fd_fb_0);
			return TFAIL;
		}

		if (g_alpha_mode) {
		}

		fb_v4l2.fmt.width = fb0_var.xres;
		fb_v4l2.fmt.height = fb0_var.yres;

		if (fb0_var.bits_per_pixel == 32) {
			fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_BGR32;
			fb_v4l2.fmt.bytesperline = 4 * fb_v4l2.fmt.width;
		}
		else if (fb0_var.bits_per_pixel == 24) {
			fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_BGR24;
			fb_v4l2.fmt.bytesperline = 3 * fb_v4l2.fmt.width;
		}
		else if (fb0_var.bits_per_pixel == 16) {
			fb_v4l2.fmt.pixelformat = IPU_PIX_FMT_RGB565;
			fb_v4l2.fmt.bytesperline = 2 * fb_v4l2.fmt.width;
		}

		fb_v4l2.flags = V4L2_FBUF_FLAG_PRIMARY;
		fb_v4l2.base = (void *) fb0_fix.smem_start +
			fb0_fix.line_length*fb0_var.yoffset;
	} else {
	}

	close(fd_fb_0);

	if (ioctl(fd_v4l, VIDIOC_S_FBUF, &fb_v4l2) < 0)
	{
		printf("set framebuffer failed\n");
		return TFAIL;
	}

	if (ioctl(fd_v4l, VIDIOC_G_FBUF, &fb_v4l2) < 0) {
		printf("set framebuffer failed\n");
		return TFAIL;
	}

	printf("\n frame buffer width %d, height %d, bytesperline %d\n",
			fb_v4l2.fmt.width, fb_v4l2.fmt.height, fb_v4l2.fmt.bytesperline);
	ret = mxc_v4l_overlay_test(g_timeout);

	close(fd_v4l);
	return ret;
}

