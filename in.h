#ifndef _VIDEO_IN
#define _VIDEO_IN
struct buffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};
int video_in_init(int width, int height, struct buffer **in_buf, int buf_num);
void video_in_deinit(int fd, struct buffer *in_buf, int buf_num);
#endif
