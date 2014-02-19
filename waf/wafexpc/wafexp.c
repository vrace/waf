/*

WANE's Archive File Explorer
Copyright (c) 2010-2011 wane. All rights reserved.

This software is provided 'as-is', without any express or
implied warranty. In no event will the authors be held liable
for any damages arising from the use of this software. 

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software. 

3. This notice may not be removed or altered from any source
distribution.

wane <newsheep@gmail.com>

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include "wafexp.h"
#include "wafconf.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)  /* ignore some secure warnings */
#endif

#define WAF_MIN(a,b) ((a) < (b) ? (a) : (b))
#define WAF_U32(arr) (((arr)[0]) + ((waf_size_t)(arr)[1] << 8) + ((waf_size_t)(arr)[2] << 16) + ((waf_size_t)(arr)[3] << 24))

/* read next block result */
#define READ_STATUS_SUCCESS 0
#define READ_STATUS_FAILED 1
#define READ_STATUS_EOF 2

/* archive file info */
struct waf_inf
{
	char name[WAF_FILENAME_SIZE];
	waf_size_t hash;    /* hash for the filename */
	waf_size_t size;    /* uncompressed size */
	waf_size_t offset;  /* offset in archive file */
};

/* archive file struct */
struct waf_file
{
	FILE *fp;
	waf_size_t cur;  /* current position */
	waf_size_t cp;  /* current block offset */
	waf_size_t np;  /* next block offset */
	struct waf_inf *inf;

	unsigned char cdata[WAF_BUFF_SIZE];  /* buffered data */
	waf_size_t coff;  /* current buffer position */
	waf_size_t csize;  /* current buffer size */

	waf_size_t *fast_offset;  /* fast seek offsets */
};

/* archive struct */
struct waf_archive
{
	FILE *fp;  /* pointer to the archive file */
	waf_size_t count;  /* file count */
	struct waf_inf **infs;  /* file info array */
};

/* string hash (borrowed from bkdr hash) */
static waf_size_t waf_strhash(const char *str)
{
	waf_size_t seed = 131;
	waf_size_t hash = 0;

	assert(str != NULL);

	while (*str)
	{
		hash = hash * seed + (*str++);
	}

	return hash & 0x7fffffff;
}

/* read a waf_size_t from file */
static int waf_readsize(FILE *fp, waf_size_t *data)
{
	unsigned char buff[sizeof(waf_size_t)];

	if (fread(buff, 1, sizeof(waf_size_t), fp) != sizeof(waf_size_t) || ferror(fp))
		return -1;

	*data = WAF_U32(buff);

	return 0;
}

struct waf_archive* waf_archive_open(const char *filename, waf_size_t offset)
{
	struct waf_archive *arc = NULL;
	unsigned char signature[sizeof(waf_size_t) * 3] = {0};
	waf_size_t i;

	assert(filename != NULL);
	assert(offset >= 0);

	arc = (struct waf_archive*)malloc(sizeof(struct waf_archive));
	if (!arc)
		goto __error;

	arc->fp = NULL;
	arc->infs = NULL;

	arc->fp = fopen(filename, "rb");
	if (!arc->fp)
		goto __error;

	fseek(arc->fp, (long)offset, SEEK_SET);

	/* read signature */
	if (fread(signature, 1, sizeof(signature), arc->fp) != sizeof(signature) ||
		ferror(arc->fp))
		goto __error;

	if (WAF_U32(signature) != WAF_SIGNATURE)
		goto __error;  /* bad tag */
	if (WAF_U32(&signature[sizeof(waf_size_t)]) != WAF_BUFF_SIZE)
		goto __error;  /* bad block size */
	
	arc->count = WAF_U32(&signature[sizeof(waf_size_t) * 2]);

	if (arc->count > 0)
		arc->infs = (struct waf_inf**)malloc(sizeof(struct waf_inf*) * arc->count);
	if (!arc->infs)
		goto __error;  /* out of memory? */
	memset(arc->infs, 0, sizeof(struct waf_inf*) * arc->count);

	for (i = 0; i < arc->count; i++)
	{
		waf_size_t size;
		struct waf_inf *inf;
		
		arc->infs[i] = (struct waf_inf*)malloc(sizeof(struct waf_inf));
		inf = arc->infs[i];
		if (!inf)
			goto __error;

		/* size of filename */
		if (waf_readsize(arc->fp, &size) != 0 || size > WAF_FILENAME_SIZE)
			goto __error;

		/* read filename */
		if (fread(inf->name, 1, size, arc->fp) != size || ferror(arc->fp))
			goto __error;
		inf->name[size] = 0;
		inf->hash = waf_strhash(inf->name);

		/* read uncompressed file size */
		if (waf_readsize(arc->fp, &inf->size) != 0)
			goto __error;

		/* read file offset */
		if (waf_readsize(arc->fp, &inf->offset) != 0)
			goto __error;
		inf->offset += offset;
	}

	goto __finish;

__error:
	if (arc)
	{
		waf_archive_close(arc);
		arc = NULL;
	}
	
__finish:
	return arc;
}

void waf_archive_close(struct waf_archive *arc)
{
	waf_size_t i;

	if (!arc)
		return;

	if (arc->fp)
	{
		fclose(arc->fp);
		arc->fp = NULL;
	}
	
	if (arc->infs)
	{
		for (i = 0; i < arc->count; i++)
		{
			if (arc->infs[i])
			{
				free(arc->infs[i]);
			}
		}
		free(arc->infs);
		arc->infs = NULL;
	}

	free(arc);
}

struct waf_file* waf_open(struct waf_archive *arc, const char *filename)
{
	waf_size_t i;
	waf_size_t hash;

	assert(arc != NULL);
	assert(filename != NULL);

	hash = waf_strhash(filename);

	for (i = 0; i < arc->count; i++)
	{
		if (arc->infs[i]->hash == hash && strcmp(arc->infs[i]->name, filename) == 0)
		{
			struct waf_file *fp = NULL;
			struct waf_inf *inf = arc->infs[i];
			waf_size_t size;

			/* prepare waf_file struct */
			fp = (struct waf_file*)malloc(sizeof(struct waf_file));
			if (!fp)
				goto __error;
			memset(fp, 0, sizeof(struct waf_file));

			fp->fp = arc->fp;
			fp->cur = 0;
			fp->cp = ~0;  /* should never have any block at this position */
			fp->np = inf->offset;
			fp->inf = inf;
			fp->coff = 0;
			fp->csize = 0;

			/* prepare fast offset */
			size = sizeof(waf_size_t) * ((inf->size + WAF_BUFF_SIZE - 1) / WAF_BUFF_SIZE);
			fp->fast_offset = (waf_size_t*)malloc(size);
			if (!fp->fast_offset)
				goto __error;
			memset(fp->fast_offset, 0, size);
			fp->fast_offset[0] = inf->offset;

			goto __finish;

__error:
			if (fp)
			{
				if (fp->fast_offset)
					free(fp->fast_offset);

				free(fp);
				fp = NULL;
			}

__finish:

			return fp;
		}
	}

	return NULL;
}

void waf_close(struct waf_file *file)
{
	if (file)
	{
		if (file->fast_offset)
		{
			free(file->fast_offset);
			file->fast_offset = NULL;
		}

		memset(file, 0, sizeof(struct waf_file));
		free(file);
	}
}

waf_size_t waf_size(struct waf_file *file)
{
	if (file)
		return file->inf->size;
	return 0;
}

static int waf_next_block(struct waf_file *file)
{
	unsigned char raw[WAF_RAW_SIZE];
	waf_size_t bs;

	fseek(file->fp, file->np, SEEK_SET);

	if (waf_readsize(file->fp, &bs) != 0)
		return READ_STATUS_FAILED;

	if (bs == 0)
		return READ_STATUS_EOF;

	if (bs > WAF_RAW_SIZE)
		return READ_STATUS_FAILED;

	if (fread(raw, 1, bs, file->fp) != bs || ferror(file->fp))
		return READ_STATUS_FAILED;

	file->csize = WAF_BUFF_SIZE;
	if (WAF_DECOMPRESS(raw, bs, file->cdata, file->csize) != 0)
		return READ_STATUS_FAILED;

	file->coff = 0;
	file->cp = file->np;
	file->np += sizeof(waf_size_t);
	file->np += bs;

	return READ_STATUS_SUCCESS;
}

int waf_read(struct waf_file *file, void *buff, waf_size_t *readsize)
{
	waf_size_t datasize = 0;
	unsigned char *buf = (unsigned char*)buff;

	assert(buff != NULL);
	assert(readsize != NULL);

	if (!file)
		return -1;

	while (1)
	{
		waf_size_t copysize;

		if (file->coff >= file->csize)
		{
			int read_status = waf_next_block(file);

			if (read_status == READ_STATUS_FAILED)
			{
				return -1;
			}
			else if (read_status == READ_STATUS_EOF)
			{
				*readsize = datasize;
				return 1;
			}
		}

		copysize = WAF_MIN(*readsize - datasize, file->csize - file->coff);
		memcpy(&buf[datasize], &file->cdata[file->coff], copysize);

		datasize += copysize;
		file->coff += copysize;
		file->cur += copysize;

		if (datasize >= *readsize)
			break;
	}

	*readsize = datasize;
	return 0;
}

int waf_seekabs(struct waf_file *file, waf_size_t position)
{
	waf_size_t block;
	waf_size_t start;

	assert(position >= 0);

	if (!file)
		return -1;

	position = WAF_MIN(position, waf_size(file));

	block = position / WAF_BUFF_SIZE;
	start = file->inf->offset;

	if (file->fast_offset[block] > 0)
	{
		/* use the pre-calculated offset */
		start = file->fast_offset[block];
	}
	else
	{
		/* no pre-calculated offset found, we have to calculate it */
		waf_size_t i;
		waf_size_t bs;

		fseek(file->fp, start, SEEK_SET);

		for (i = 0; i < block; i++)
		{
			if (waf_readsize(file->fp, &bs) != 0 || bs > WAF_RAW_SIZE)
				return -1;

			fseek(file->fp, bs, SEEK_CUR);

			start += sizeof(waf_size_t);
			start += bs;
			
			/* save next block's offset */
			file->fast_offset[i + 1] = start;
		}
	}

	/* read data block if necessary */
	if (file->cp < 0 || file->cp != start)
	{
		waf_size_t prev = file->np;
		file->np = start;

		if (waf_next_block(file) == READ_STATUS_FAILED)
		{
			file->np = prev;
			return -1;
		}
	}

	file->coff = position % WAF_BUFF_SIZE;
	file->cur = position;

	return 0;
}

int waf_seek(struct waf_file *file, int offset, int origin)
{
	if (!file)
		return -1;

	switch (origin)
	{
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset = file->cur + offset;
		break;
	case SEEK_END:
		offset = waf_size(file) + offset;
		break;
	default:
		return -1;
	}

	return waf_seekabs(file, offset);
}

waf_size_t waf_tell(struct waf_file *file)
{
	if (!file)
		return ~0;

	return file->cur;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
