/*
 * PUAE - The Un*x Amiga Emulator
 *
 * Windows 2 Linux
 *
 * Copyright 2012-2013 Mustafa 'GnoStiC' Tufan
 */

typedef int HANDLE;
typedef unsigned int DWORD;
typedef long long LONGLONG;
typedef int BOOL;
typedef int64_t off64_t;

#define INVALID_HANDLE_VALUE		((HANDLE)~0U)
#define INVALID_FILE_ATTRIBUTES		((DWORD) -1)
#define ERROR_ACCESS_DENIED			5
#define INVALID_SET_FILE_POINTER	((DWORD)-1)
#define NO_ERROR					0

#define FILE_BEGIN		0
#define FILE_CURRENT	1
#define FILE_END		2

#define FILE_FLAG_WRITE_THROUGH			0x80000000
#define FILE_FLAG_OVERLAPPED			0x40000000
#define FILE_FLAG_NO_BUFFERING			0x20000000
#define FILE_FLAG_RANDOM_ACCESS			0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN		0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE		0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS		0x02000000
#define FILE_FLAG_POSIX_SEMANTICS		0x01000000
#define FILE_FLAG_OPEN_REPARSE_POINT	0x00200000
#define FILE_FLAG_OPEN_NO_RECALL		0x00100000
#define FILE_FLAG_FIRST_PIPE_INSTANCE	0x00080000

#define CREATE_NEW			1
#define CREATE_ALWAYS		2
#define OPEN_EXISTING		3
#define OPEN_ALWAYS			4
#define TRUNCATE_EXISTING	5

#define FILE_ATTRIBUTE_NORMAL		0x00000080
#define FILE_ATTRIBUTE_READONLY		0x00000001
#define FILE_ATTRIBUTE_HIDDEN		0x00000002
#define FILE_ATTRIBUTE_SYSTEM		0x00000004
#define FILE_ATTRIBUTE_DIRECTORY	0x00000010

#define FILE_READ_DATA		0x0001
#define FILE_WRITE_DATA		0x0002
#define FILE_APPEND_DATA	0x0004

#define GENERIC_READ		FILE_READ_DATA
#define GENERIC_WRITE		FILE_WRITE_DATA
#define FILE_SHARE_READ		0x00000001
#define FILE_SHARE_WRITE	0x00000002
#define FILE_SHARE_DELETE	0x00000004

struct my_opendir_s {
//	HANDLE h;
//	WIN32_FIND_DATA fd;
//	int first;
};

struct my_openfile_s {
	HANDLE h;
};

typedef struct {
	DWORD LowPart;
	int32_t HighPart;
	LONGLONG QuadPart;
} LARGE_INTEGER;

DWORD GetLastError()
{
	return errno;
}

struct my_opendir_s *my_opendir (const TCHAR *name)
{
	struct my_opendir_s *mod;
	TCHAR tmp[MAX_DPATH];

	tmp[0] = 0;
/*
	if (currprefs.win32_filesystem_mangle_reserved_names == false)
		_tcscpy (tmp, PATHPREFIX);
	_tcscat (tmp, name);
	_tcscat (tmp, _T("\\"));
	_tcscat (tmp, mask);
	mod = xmalloc (struct my_opendir_s, 1);
	if (!mod)
		return NULL;
	mod->h = FindFirstFile(tmp, &mod->fd);
	if (mod->h == INVALID_HANDLE_VALUE) {
		xfree (mod);
		return NULL;
	}
	mod->first = 1;
	return mod;
*/
	return opendir(name);
}

void my_closedir (struct my_opendir_s *mod) {
	if (mod)
		closedir(mod);
//	xfree (mod);
}

int my_readdir (struct my_opendir_s *mod, TCHAR *name) {
/*
	if (mod->first) {
		_tcscpy (name, mod->fd.cFileName);
		mod->first = 0;
		return 1;
	}
	if (!FindNextFile (mod->h, &mod->fd))
		return 0;
	_tcscpy (name, mod->fd.cFileName);
*/
	return readdir(mod);
	return 1;
}

bool CloseHandle(HANDLE hObject) {
	if (!hObject)
		return false;

	if (hObject == INVALID_HANDLE_VALUE || hObject == (HANDLE)-1)
		return true;

	return true;
}

void my_close (struct my_openfile_s *mos)
{
	close (mos->h);
	xfree (mos);
}

DWORD SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, int32_t *lpDistanceToMoveHigh, DWORD dwMoveMethod) {
	if (hFile == NULL)
		return 0;

	LONGLONG offset = lDistanceToMove;
	if (lpDistanceToMoveHigh) {
		LONGLONG helper = *lpDistanceToMoveHigh;
		helper <<= 32;
		offset &= 0xFFFFFFFF;   // Zero out the upper half (sign ext)
		offset |= helper;
	}

	int nMode = SEEK_SET;
	if (dwMoveMethod == FILE_CURRENT)
		nMode = SEEK_CUR;
	else if (dwMoveMethod == FILE_END)
		nMode = SEEK_END;

	off64_t currOff;
	currOff = lseek(hFile, offset, nMode);

	if (lpDistanceToMoveHigh) {
		*lpDistanceToMoveHigh = (int32_t)(currOff >> 32);
	}

	return (DWORD)currOff;
}

uae_s64 my_lseek (struct my_openfile_s *mos, uae_s64 offset, int whence) {
	off_t result = lseek(mos->h, offset, whence);
	return result;
    
	LARGE_INTEGER li, old;

	old.QuadPart = 0;
	old.LowPart = SetFilePointer (mos->h, 0, &old.HighPart, FILE_CURRENT);
	if (old.LowPart == INVALID_SET_FILE_POINTER && GetLastError () != NO_ERROR)
		return -1;
	if (offset == 0 && whence == SEEK_CUR)
		return old.QuadPart;
	li.QuadPart = offset;
	li.LowPart = SetFilePointer (mos->h, li.LowPart, &li.HighPart,
		whence == SEEK_SET ? FILE_BEGIN : (whence == SEEK_END ? FILE_END : FILE_CURRENT));
	if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError () != NO_ERROR)
		return -1;
	return old.QuadPart;
}

HANDLE CreateFile(const TCHAR *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
	int flags = 0, mode = S_IRUSR | S_IRGRP | S_IROTH;
	if (dwDesiredAccess & FILE_WRITE_DATA) {
		flags = O_RDWR;
		mode |= S_IWUSR;
	} else if ( (dwDesiredAccess & FILE_READ_DATA) == FILE_READ_DATA)
		flags = O_RDONLY;
	else {
		return INVALID_HANDLE_VALUE;
	}

	switch (dwCreationDisposition) {
		case OPEN_ALWAYS:
			flags |= O_CREAT;
			break;
		case TRUNCATE_EXISTING:
			flags |= O_TRUNC;
			mode  |= S_IWUSR;
			break;
		case CREATE_ALWAYS:
			flags |= O_CREAT|O_TRUNC;
			mode  |= S_IWUSR;
			break;
		case CREATE_NEW:
			flags |= O_CREAT|O_TRUNC|O_EXCL;
			mode  |= S_IWUSR;
			break;
		case OPEN_EXISTING:
			break;
	}

	int fd = 0;
//	mode = S_IREAD | S_IWRITE;

	if (dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING)
		flags |= O_SYNC;

	flags |= O_NONBLOCK;

	fd = open(lpFileName, flags, mode);

	if (fd == -1 && errno == ENOENT) {
		write_log("FS: error %d opening file <%s>, flags:%x, mode:%x.\n", errno, lpFileName, flags, mode);
		return INVALID_HANDLE_VALUE;
	} else {
		write_log ("FS: '%s' open successful\n", lpFileName);
	}

	// turn of nonblocking reads/writes
	fcntl(fd, F_GETFL, &flags);
	fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

	return fd;
}

struct my_openfile_s *my_open (const TCHAR *name, int flags) {
	errno = 0;

	struct my_openfile_s *mos;
	HANDLE h;
	DWORD DesiredAccess = GENERIC_READ;
	DWORD ShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	DWORD CreationDisposition = OPEN_EXISTING;
	DWORD FlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	DWORD attr;
	const TCHAR *namep;
	TCHAR path[MAX_DPATH];
	
/*	if (currprefs.win32_filesystem_mangle_reserved_names == false) {
		_tcscpy (path, PATHPREFIX);
		_tcscat (path, name);
		namep = path;
	} else {*/
		namep = name;
//	}
	mos = xmalloc (struct my_openfile_s, 1);
	if (!mos)
		return NULL;
//	attr = GetFileAttributesSafe (name);
	if (flags & O_TRUNC)
		CreationDisposition = CREATE_ALWAYS;
	else if (flags & O_CREAT)
		CreationDisposition = OPEN_ALWAYS;
	if (flags & O_WRONLY)
		DesiredAccess = GENERIC_WRITE;
	if (flags & O_RDONLY) {
		DesiredAccess = GENERIC_READ;
		CreationDisposition = OPEN_EXISTING;
	}
	if (flags & O_RDWR)
		DesiredAccess = GENERIC_READ | GENERIC_WRITE;
//	if (CreationDisposition == CREATE_ALWAYS && attr != INVALID_FILE_ATTRIBUTES && (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
//		SetFileAttributesSafe (name, FILE_ATTRIBUTE_NORMAL);
	h = CreateFile (namep, DesiredAccess, ShareMode, NULL, CreationDisposition, FlagsAndAttributes, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		if (err == ERROR_ACCESS_DENIED && (DesiredAccess & GENERIC_WRITE)) {
			DesiredAccess &= ~GENERIC_WRITE;
			h = CreateFile (namep, DesiredAccess, ShareMode, NULL, CreationDisposition, FlagsAndAttributes, NULL);
			if (h == INVALID_HANDLE_VALUE)
				err = GetLastError();
		}
		if (h == INVALID_HANDLE_VALUE) {
			write_log (_T("FS: failed to open '%s' %x %x err=%d\n"), namep, DesiredAccess, CreationDisposition, err);
			xfree (mos);
			mos = NULL;
			goto err;
		}
	}
	mos->h = h;
err:
//	write_log (_T("open '%s' | flags: %d | FS: %x | ERR: %s\n"), namep, flags, mos ? mos->h : 0, strerror(errno));
/*char buffer[65];
int gotten;
gotten = read(mos->h, buffer, 10);
buffer[gotten] = '\0';
write_log("*** %s ***\n",buffer);
lseek(mos->h, 0, SEEK_SET);*/
	return mos;
}

BOOL SetEndOfFile(HANDLE hFile) {
	if (hFile == NULL)
		return false;

	off64_t currOff = lseek(hFile, 0, SEEK_CUR);
	if (currOff >= 0)
		return (ftruncate(hFile, currOff) == 0);

	return false;
}

int my_truncate (const TCHAR *name, uae_u64 len) {
	HANDLE hFile;
	BOOL bResult = FALSE;
	int result = -1;
	const TCHAR *namep;
	TCHAR path[MAX_DPATH];
	
/*	if (currprefs.win32_filesystem_mangle_reserved_names == false) {
		_tcscpy (path, PATHPREFIX);
		_tcscat (path, name);
		namep = path;
	} else {*/
		namep = name;
//	}
	if ((hFile = CreateFile (namep, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ) != INVALID_HANDLE_VALUE ) {
		LARGE_INTEGER li;
		li.QuadPart = len;
		li.LowPart = SetFilePointer (hFile, li.LowPart, &li.HighPart, FILE_BEGIN);
		if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError () != NO_ERROR) {
			write_log (_T("FS: truncate seek failure for %s to pos %d\n"), namep, len);
		} else {
			if (SetEndOfFile (hFile) == TRUE)
				result = 0;
		}
		CloseHandle (hFile);
	} else {
		write_log (_T("FS: truncate failed to open %s\n"), namep);
	}
	return result;
}

uae_s64 my_fsize (struct my_openfile_s *mos) {
	uae_s64 cur, filesize;

	cur = lseek (mos->h, 0, SEEK_CUR);
	filesize = lseek (mos->h, 0, SEEK_END);
	lseek (mos->h, cur, SEEK_SET);
//	write_log (_T("FS: filesize <%d>\n"), filesize);
	return filesize;
}

int my_read (struct my_openfile_s *mos, void *b, unsigned int size) {
//        DWORD read = 0;
//        ReadFile (mos->h, b, size, &read, NULL);
	ssize_t bytesRead = read(mos->h, b, size);
//	write_log (_T("read <%d> | FS: %x | size: %d | ERR: %s\n"), bytesRead, mos->h, size, strerror(errno));

	return bytesRead;
}

int my_write (struct my_openfile_s *mos, void *b, unsigned int size) {
//        DWORD written = 0;
//        WriteFile (mos->h, b, size, &written, NULL);
	ssize_t written = write (mos->h, b, size);
//	write_log (_T("wrote <%d> | FS: %x | ERR: %s\n"), written, mos->h, strerror(errno));

	return written;
}

