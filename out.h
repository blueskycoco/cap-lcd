#ifndef _VIDEO_OUT
#define _VIDEO_OUT
int video_out_init(int width, int height, struct buffer **out_buf, int buf_num);
void video_out_deinit(int fd, struct buffer **out_buf, int buf_num);
#endif
