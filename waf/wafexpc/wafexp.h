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

#ifndef __WAF_EXP_H__
#define __WAF_EXP_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned long waf_size_t;

typedef struct waf_file waf_file;
typedef struct waf_archive waf_archive;

/*
open an archive
parameters:
	[in] filename - the archive's filename
	[in] offset - the archive's start offset
returns:
	pointer to the archive struct if success
	otherwise failed
*/
waf_archive* waf_archive_open(const char *filename, waf_size_t offset);

/*
close an opened archive
parameters:
	[in] arc - pointer to an opened archive
*/
void waf_archive_close(waf_archive *arc);

/*
open a file inside an archive
parameters:
	[in] arc - pointer to an opened archive
	[in] filename - name of the file to be opened
returns:
	pointer to the file inside the archive if success
	otherwise failed
*/
waf_file* waf_open(waf_archive *arc, const char *filename);

/*
close a file
parameters:
	[in] file - pointer to the file
*/
void waf_close(waf_file *file);

/*
read data from a file
parameters:
	[in] file - pointer to a file
	[in] buff - buffer to receive the data
	[in, out] readsize - number of bytes to read / number of bytes read
returns:
	= 0    success
	= 1    end of file
	< 0    failed
*/
int waf_read(waf_file *file, void *buff, waf_size_t *readsize);

/*
get a file's size
parameters:
	[in] file - pointer to a file
returns:
	size of the file
*/
waf_size_t waf_size(waf_file *file);

/*
seek file to a new position
parameters:
	[in] file - pointer to a file
	[in] offset - number of bytes to offset from origin
	[in] origin - position from where offset is added, same as fseek
returns:
	0 if success, otherwise failed
*/
int waf_seek(waf_file *file, int offset, int origin);

/*
return current position of a file
parameters:
	[in] file - pointer to a file
returns:
	position of the file
*/
waf_size_t waf_tell(waf_file *file);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* __WAF_EXP_H__ */
