/*

WANE's Archive File Builder
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

*/

#include <Windows.h>

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <list>
#include <vector>
#include <algorithm>
#include <functional>
#include <stdexcept>

using namespace std;

#include "../zlib/zlib.h"

struct archive_info
{
	vector<string> filename;
	DWORD size;
	DWORD offset;

	unsigned char md5[16];  // md5 value used to eliminate duplicated files
};

enum
{
	waf_src_size = 64 * 1024,
	waf_raw_size = 68 * 1024,
};

typedef list<archive_info*> waf_archive;

static waf_archive _waf_info;

// arguments
static string _srcdir;
static string _outname;
static string _pathadd;
static bool _include_hidden = false;

struct md5_context
{
	ULONG i[2];
	ULONG buf[4];
	unsigned char in[64];
	unsigned char digest[16];
};

typedef void (WINAPI *MD5INITPROC)(md5_context*);
typedef void (WINAPI *MD5UPDATEPROC)(md5_context*, unsigned char*, unsigned int);
typedef void (WINAPI *MD5FINALPROC)(md5_context*);

static HINSTANCE md5lib = NULL;
static MD5INITPROC md5_init;
static MD5UPDATEPROC md5_update;
static MD5FINALPROC md5_final;

bool md5_initsys(void)
{
	md5lib = LoadLibrary("Cryptdll.dll");
	
	if (!md5lib)
		return false;

	md5_init = (MD5INITPROC)GetProcAddress(md5lib, "MD5Init");
	md5_update = (MD5UPDATEPROC)GetProcAddress(md5lib, "MD5Update");
	md5_final = (MD5FINALPROC)GetProcAddress(md5lib, "MD5Final");

	return md5_init && md5_update && md5_final;
}

void md5_freesys(void)
{
	if (md5lib)
		FreeLibrary(md5lib);
}

unsigned char* md5_file(const string &filename, unsigned char *digest)
{
	HANDLE fp = INVALID_HANDLE_VALUE;
	unsigned char buf[512];
	DWORD size;

	try
	{
		fp = CreateFile(filename.c_str(), FILE_GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fp == INVALID_HANDLE_VALUE)
			throw runtime_error("Can't open file.");

		md5_context md5;
		
		md5_init(&md5);

		while (1)
		{
			if (!ReadFile(fp, buf, 512, &size, NULL))
				throw runtime_error("Can't read file.");

			if (size == 0)
				break;  // finish

			md5_update(&md5, buf, size);
		}

		md5_final(&md5);

		CloseHandle(fp);

		memcpy(digest, md5.digest, 16);
	}
	catch (runtime_error&)
	{
		if (fp != INVALID_HANDLE_VALUE)
			CloseHandle(fp);

		return NULL;
	}

	return digest;
}

bool comparefile(const string &one, const string &two)
{
	HANDLE fp1 = INVALID_HANDLE_VALUE;
	HANDLE fp2 = INVALID_HANDLE_VALUE;
	unsigned char buffone[512];
	unsigned char bufftwo[512];
	DWORD size;
	bool same = true;

	try
	{
		fp1 = CreateFile(one.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fp1 == INVALID_HANDLE_VALUE)
			throw runtime_error("Can't open file one.");

		fp2 = CreateFile(two.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fp2 == INVALID_HANDLE_VALUE)
			throw runtime_error("Can't open file two.");

		while (1)
		{
			if (!ReadFile(fp1, buffone, 512, &size, NULL))
				throw runtime_error("Can't read file one.");

			if (!ReadFile(fp2, bufftwo, 512, &size, NULL))
				throw runtime_error("Can't read file two.");

			if (size == 0)
				break;  // finish

			if (memcmp(buffone, bufftwo, size) != 0)
			{
				same = false;
				break;
			}
		}

		CloseHandle(fp1);
		CloseHandle(fp2);
	}
	catch (runtime_error&)
	{
		if (fp1 != INVALID_HANDLE_VALUE)
			CloseHandle(fp1);
		if (fp2 != INVALID_HANDLE_VALUE)
			CloseHandle(fp1);

		return false;
	}

	return same;
}

struct duplicate_file_finder
{
	string filename;
	unsigned char md5[16];

	bool operator()(archive_info *info)
	{
		if (memcmp(md5, info->md5, 16) != 0)
			return false;

		// since md5 is not 100% accurate, we have to compare binary data again
		return comparefile(_srcdir + "/" + filename, _srcdir + "/" + info->filename[0]);
	}
};

void scandir(const string &root, const string &sub)
{
	WIN32_FIND_DATA wfd;
	HANDLE hFind;

	string findwhat = root + "/" + sub + "/*";

	hFind = FindFirstFile(findwhat.c_str(), &wfd);

	while (hFind != INVALID_HANDLE_VALUE)
	{
		string found = wfd.cFileName;

		if (!(found == "." || found == ".."))
		{
			found = sub;
			if (!found.empty())
				found += "/";
			found += wfd.cFileName;

            if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) && !_include_hidden)
            {
                // hidden files are skipped
            }
			else if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// recursive search sub-directory
				scandir(root, found);
			}
			else
			{
				duplicate_file_finder finder;
				finder.filename = found;
				md5_file(_srcdir + "/" + found, finder.md5);

				waf_archive::iterator it = find_if(_waf_info.begin(), _waf_info.end(), finder);

				if (it != _waf_info.end())
				{
					// duplicated file
					(*it)->filename.push_back(found);
				}
				else
				{
					// add this file to file list
					archive_info *inf = new archive_info();

					inf->filename.push_back(found);
					inf->size = 0;
					inf->offset = 0;
					memcpy(inf->md5, finder.md5, 16);

					_waf_info.push_back(inf);
				}
			}
		}

		if (!FindNextFile(hFind, &wfd))
		{
			FindClose(hFind);
			hFind = INVALID_HANDLE_VALUE;
		}
	}
}

void waf_saveinfo(HANDLE hFile, archive_info *inf)
{
	DWORD written;
	BOOL result = true;

	for (vector<string>::iterator it = inf->filename.begin(); it != inf->filename.end(); ++it)
	{
		string name = _pathadd + *it;
		int len = name.length();

		// write archive info
		result = result && WriteFile(hFile, &len, sizeof(int), &written, NULL);
		result = result && WriteFile(hFile, name.c_str(), len, &written, NULL);
		result = result && WriteFile(hFile, &inf->size, sizeof(DWORD), &written, NULL);
		result = result && WriteFile(hFile, &inf->offset, sizeof(DWORD), &written, NULL);

		if (!result)
			throw runtime_error("An error was occurred when storing archive info.");
	}
}

void waf_append(HANDLE hFile, archive_info *inf)
{
	for (vector<string>::iterator it = inf->filename.begin(); it != inf->filename.end(); ++it)
	{
		printf("Compressing %s...\n", it->c_str());
	}

	HANDLE src = INVALID_HANDLE_VALUE;
	unsigned char srcbuff[waf_src_size];
	unsigned char outbuff[waf_raw_size];
	DWORD datasize;
	uLongf outsize;

	try
	{
		string fullpath = _srcdir + "/" + inf->filename[0];

		src = CreateFile(fullpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (src == INVALID_HANDLE_VALUE)
			throw runtime_error("Can't open source file.");

		inf->offset = GetFileSize(hFile, NULL);
		inf->size = GetFileSize(src, NULL);
		
		while (1)
		{
			if (!ReadFile(src, srcbuff, waf_src_size, &datasize, NULL))
				throw runtime_error("An error was occurred when reading from source file.");

			if (datasize == 0)
				break;

			// compress
			outsize = waf_raw_size;
			if (compress(outbuff, &outsize, srcbuff, datasize) != Z_OK)
				throw runtime_error("An error was occurred when compressing data.");

			// save block size and block data
			BOOL result = TRUE;
			result = result && WriteFile(hFile, &outsize, sizeof(DWORD), &datasize, NULL);
			result = result && WriteFile(hFile, outbuff, outsize, &datasize, NULL);

			if (!result)
				throw runtime_error("An error was occurred when storing data block.");
		}

		// a zero size block to indicate end of a file
		outsize = 0;
		if (!WriteFile(hFile, &outsize, sizeof(DWORD), &datasize, NULL))
			throw runtime_error("An error was occurred when storing data block.");

		CloseHandle(src);
	}
	catch (runtime_error &e)
	{
		if (src)
			CloseHandle(src);

		throw e;
	}
}

bool waf_build(void)
{
	HANDLE hFile;
	DWORD written;

	printf("Scanning for files...\n");
	scandir(_srcdir, "");

	printf("\n");
	hFile = CreateFile(_outname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Can't create output file.\n");
		return false;
	}

	try
	{
		// signature
		unsigned char buff[4 + sizeof(DWORD) * 2];

		buff[0] = 'w';
		buff[1] = 'a';
		buff[2] = 'f';
		buff[3] = 0;

		DWORD *p = (DWORD*)&buff[4];

		*p++ = waf_src_size;
		
		*p = 0;
		for (waf_archive::iterator it = _waf_info.begin(); it != _waf_info.end(); ++it)
		{
			*p += (*it)->filename.size();
		}

		if (!WriteFile(hFile, buff, sizeof(buff), &written, NULL))
			throw runtime_error("An error was occurred when storing archive signature.");

		// archive info
		for_each(_waf_info.begin(), _waf_info.end(), bind1st(ptr_fun(waf_saveinfo), hFile));
		
		// archive file data
		for_each(_waf_info.begin(), _waf_info.end(), bind1st(ptr_fun(waf_append), hFile));

		// update archive info
		SetFilePointer(hFile, sizeof(buff), NULL, FILE_BEGIN);
		for_each(_waf_info.begin(), _waf_info.end(), bind1st(ptr_fun(waf_saveinfo), hFile));

		SetFilePointer(hFile, 0, NULL, FILE_END);

		CloseHandle(hFile);

		printf("\n");
		printf("Build archive '%s' success.\n", _outname.c_str());
	}
	catch (runtime_error &e)
	{
		CloseHandle(hFile);
		DeleteFile(_outname.c_str());

		printf("%s\n", e.what());

		return false;
	}

	return true;
}

string& replace_str(string &str, const string &os, const string &ns)
{
	string::size_type pos = 0;

	while ((pos = str.find(os, pos)) != string::npos)
	{
		str.replace(pos, os.length(), ns);
		pos += ns.length();
	}

	return str;
}

bool parse_args(int argc, char *argv[])
{
	enum parse_status
	{
		ps_normal,
		ps_path,
	};

	if (argc < 3)
		return false;

	_srcdir = argv[1];
	_outname = argv[2];

	parse_status status = ps_normal;

	for (int i = 3; i < argc; i++)
	{
		string arg(argv[i]);

		if (status == ps_normal)
		{
			if (arg == "-h")
			{
				_include_hidden = true;
			}
			else if (arg == "-p")
			{
				status = ps_path;
			}
		}
		else if (status == ps_path)
		{
			replace_str(arg, "\\", "/");
			if (arg.length() > 0 && arg[arg.length() - 1] != '/')
				arg += '/';

			_pathadd = arg;

			status = ps_normal;
		}
	}

	if (status != ps_normal)
		return false;

	return true;
}

void show_usage(void)
{
	printf("Usage: waf <src path> <outfile> [options]\n");
	printf("\n");
	printf("Options:\n");
	printf("\n");
	printf("  -h           Include hidden files.\n");
	printf("  -p <path>    Add a relative path before filename.\n");
}

int main(int argc, char *argv[])
{
	printf("WANE's Archive File Maker\n");
	printf("Copyright (c) 2010-2011 wane. All rights reserved.\n\n");

	if (!parse_args(argc, argv))
	{
		show_usage();
		return 1;
	}

	if (!md5_initsys())
	{
		printf("Can't initialize MD5 function.\n"
			"Microsoft Windows 2000 or later Windows system is required.\n");
		return 2;
	}

	bool result = waf_build();

	md5_freesys();

	return result ? 0 : 3;
}
