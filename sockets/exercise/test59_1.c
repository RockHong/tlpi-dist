#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "tlpi_hdr.h"
#include "read_line_buf.h"


int main(int argc, char** argv)
{
	if (argc < 3)
	{
		errExit("usage: %s, file-name max-line-length", argv[0]);
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1)
	{
		errExit("fail to open %s", argv[1]);
	}

	int max = atoi(argv[2]);
	if (max <= 0)
	{
		errExit("%d is less than 0", max);
	}

	struct ReadLineBuf rlb;
	readLineBufInit(fd, &rlb);

	char *buf = (char *)malloc(max);
	if (buf == NULL)
	{
		errExit("internal error");
	}

	while(readLineBuf(&rlb, buf, max -1))
	{
		printf("%s", buf);
		buf[max-1] = '\0';
		
	}

	return 0;
}
