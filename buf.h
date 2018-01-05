#ifndef _BUF_IN
#define _BUF_IN
struct buffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};
#endif
