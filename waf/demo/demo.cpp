#include <stdio.h>
#include <stdlib.h>
#include "../wafexpc/wafexp.h"

int main(void)
{
	waf_archive *arc;
	waf_file *fp;
	
	arc = waf_archive_open("../demodata.waf", 0);
	if (arc)
	{
		fp = waf_open(arc, "hello.txt");
		if (fp)
		{
			waf_size_t size;
			char *buf;

			size = waf_size(fp);
			buf = new char[size + 1];
			waf_read(fp, buf, &size);
			buf[size] = '\0';

			printf("Read %d bytes.\n", size);
			printf("%s\n", buf);

			waf_close(fp);
		}
		else
		{
			printf("Can't open content file.\n");
		}

		waf_archive_close(arc);
	}
	else
	{
		printf("Can't open archive.\n");
	}

	return 0;
}
