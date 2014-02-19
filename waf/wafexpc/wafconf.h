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

#ifndef __WAF_CONF_H__
#define __WAF_CONF_H__

/* waf signature */
#define WAF_SIGNATURE 0x00666177UL

/* max filename size in archive */
#define WAF_FILENAME_SIZE 260

/* buffer size. use same size as the archive maker */
#define WAF_BUFF_SIZE (64 * 1024)
#define WAF_RAW_SIZE (68 * 1024)

/* decompress procedure */
#include "../zlib/zlib.h"
#define WAF_DECOMPRESS(inbuf,insize,outbuf,outsize) (uncompress((outbuf), (uLongf*)&(outsize), (inbuf), (insize)) != Z_OK)

#endif  /* __WAF_CONF_H__ */
