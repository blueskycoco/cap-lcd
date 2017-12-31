#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/errno.h>
#include "buf.h"
#include "in.h"
#include "out.h"
#define WIDTH   640
#define HEIGHT  480
#define BUF_NUM 6
int main(int argc, char **argv)
{
	struct buffer *in_buf=NULL, *out_buf=NULL;
	int i=0,fd_in=0,fd_out=0;
	fd_in = video_in_init(WIDTH, HEIGHT, &in_buf, BUF_NUM); 
	if (fd_in > 0 && in_buf != NULL) {
		fd_out = video_out_init(WIDTH, HEIGHT, &out_buf, BUF_NUM);
	} else {
		if (fd_in <= 0)
			printf("video in fd invalid\r\n");
		else
			close(fd_in);
		if (in_buf == NULL)
			printf("video in buf invalid\r\n");
		return -1;
	}

	if (fd_out <= 0 || out_buf == NULL) {
		if (fd_out <= 0)
			printf("video out fd invalid\r\n");
		else
			close(fd_out);
		if (out_buf == NULL)
			printf("video out buf invalid\r\n");
		video_in_deinit(fd_in, &in_buf, BUF_NUM);
		return -1;
	}
	while (1) {
		struct v4l2_buffer buf_in,buf_out;
		fd_set fds;
		struct timeval tv;
		int r;  

		FD_ZERO(&fds);
		FD_SET(fd_in, &fds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd_in+1, &fds, NULL, NULL, &tv);
		if(-1 == r){  
			if(EINTR == errno){  
				printf("select erro! \n");  
			}  
		}  
		else if(0 == r){  
			printf("select in timeout! \n"); 
		}  

		memset(&buf_in, 0, sizeof (buf_in));
		buf_in.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf_in.memory = V4L2_MEMORY_MMAP;
		if (ioctl (fd_in, VIDIOC_DQBUF, &buf_in) < 0)	{
			printf("VIDIOC_DQBUF failed.\n");
			break;
		}

		FD_ZERO(&fds);
		FD_SET(fd_out, &fds);
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		r = select(fd_out+1, NULL, &fds, NULL, &tv);
		if(-1 == r){  
			if(EINTR == errno){  
				printf("select erro! \n");  
			}  
		}  
		else if(0 == r){  
			printf("select out timeout! \n"); 
		}  
		buf_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf_out.memory = V4L2_MEMORY_MMAP;
		if (i < BUF_NUM) {
			buf_out.index = i;
			if (ioctl(fd_out, VIDIOC_QUERYBUF, &buf_out) < 0) {
				printf("VIDIOC_QUERYBUF failed\n");
				break;
			}
		} else {
			if (ioctl(fd_out, VIDIOC_DQBUF, &buf_out) < 0) {
				printf("VIDIOC_DQBUF failed\n");
				break;
			}
		}

		memcpy(out_buf[buf_out.index].start, in_buf[buf_in.index].start, buf_in.length);
		if (ioctl(fd_out, VIDIOC_QBUF, &buf_out) < 0) {
			printf("VIDIOC_QBUF failed\n");
			break;
		}

		if (i == 2) {
			int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
			if (ioctl(fd_out, VIDIOC_STREAMON, &type) < 0) {
				printf("Can't stream on\n");
				break;
			}
		}

		if (i<=BUF_NUM)
			i++;

		if (ioctl (fd_in, VIDIOC_QBUF, &buf_in) < 0) {
			printf("VIDIOC_QBUF failed\n");
			break;
		}

	}

	video_out_deinit(fd_out, &out_buf, BUF_NUM);
	video_in_deinit(fd_in, &in_buf, BUF_NUM);
	return 0;	
}
