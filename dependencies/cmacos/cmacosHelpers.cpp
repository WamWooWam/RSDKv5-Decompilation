#include "cmacosHelpers.hpp"

#include <string.h>

#include <Dialogs.h>
#include <Files.h>
#include <TextUtils.h>
#include <Timer.h>

namespace Classic
{

// RSDK paths use '/' and are relative to the game folder. Retro68's open() hands the name
// to HOpenDF relative to the default directory (the application's folder), so a relative
// HFS path needs a leading ':' and ':' separators.
static void ToHFSPath(const char *path, char *out, size_t outSize)
{
    size_t o = 0;

    while (path[0] == '.' && path[1] == '/') path += 2;
    while (path[0] == '/') path++;

    bool hasDir = strchr(path, '/') != NULL;
    if (hasDir && o + 1 < outSize)
        out[o++] = ':';

    for (; *path && o + 1 < outSize; ++path) {
        char c = *path;
        if (c == '/') {
            // collapse "//" and skip "./"
            if (o > 0 && out[o - 1] == ':')
                continue;
            c = ':';
        }
        else if (c == ':') {
            c = '-'; // not allowed in HFS names
        }
        out[o++] = c;
    }
    out[o] = 0;
}

// The application's folder
static short appVRefNum = 0;
static long appDirID    = 0;
static bool appDirKnown = false;

static void RememberAppDir()
{
    if (appDirKnown)
        return;
    if (HGetVol(NULL, &appVRefNum, &appDirID) != noErr) {
        appVRefNum = 0;
        appDirID   = 0;
    }
    appDirKnown = true;
}

static bool ToPascalPath(const char *path, Str255 out)
{
    char hfsPath[0x200];
    ToHFSPath(path, hfsPath, sizeof(hfsPath));
    size_t len = strlen(hfsPath);
    if (len > 255)
        return false;
    out[0] = (unsigned char)len;
    memcpy(out + 1, hfsPath, len);
    return true;
}

FILE *OpenStdFile(const char *path, const char *mode)
{
    char hfsPath[0x200];
    RememberAppDir();
    ToHFSPath(path, hfsPath, sizeof(hfsPath));
    return fopen(hfsPath, mode);
}

enum { FILE_CACHE_SIZE = 32 * 1024 };

struct File {
    short refNum;
    bool writable;
    long size;     // logical EOF
    long position; // logical position
    long cacheStart;
    long cacheLength;
    uint8_t cache[FILE_CACHE_SIZE];
};

static OSType FileTypeFor(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext && (!strcmp(ext, ".ini") || !strcmp(ext, ".txt") || !strcmp(ext, ".cfg")))
        return 'TEXT';
    return 'BINA';
}

File *FileOpen(const char *path, const char *mode)
{
    RememberAppDir();

    Str255 name;
    if (!ToPascalPath(path, name))
        return NULL;

    bool read   = mode[0] == 'r';
    bool plus   = strchr(mode, '+') != NULL;
    bool append = mode[0] == 'a';
    bool write  = !read || plus;

    short refNum = 0;
    OSErr err;
    if (!write) {
        err = HOpenDF(appVRefNum, appDirID, name, fsRdPerm, &refNum);
    }
    else {
        if (!read) {
            OSErr cerr = HCreate(appVRefNum, appDirID, name, 'SMan', FileTypeFor(path));
            (void)cerr; // dupFNErr when it already exists
        }
        err = HOpenDF(appVRefNum, appDirID, name, fsRdWrPerm, &refNum);
        if (err == noErr && mode[0] == 'w')
            SetEOF(refNum, 0);
    }
    if (err != noErr)
        return NULL;

    File *file = (File *)NewPtr(sizeof(File));
    if (!file) {
        FSClose(refNum);
        return NULL;
    }

    file->refNum      = refNum;
    file->writable    = write;
    file->size        = 0;
    file->position    = 0;
    file->cacheStart  = 0;
    file->cacheLength = 0;
    GetEOF(refNum, &file->size);
    if (append)
        file->position = file->size;
    return file;
}

size_t FileRead(void *buffer, size_t size, size_t count, File *file)
{
    if (!file || !size || !count)
        return 0;

    long want = (long)(size * count);
    if (file->position + want > file->size)
        want = file->size - file->position;
    if (want <= 0)
        return 0;

    uint8_t *out = (uint8_t *)buffer;
    long done    = 0;

    while (done < want) {
        long pos = file->position;

        // served from the cache
        if (pos >= file->cacheStart && pos < file->cacheStart + file->cacheLength) {
            long avail = file->cacheStart + file->cacheLength - pos;
            long n     = want - done < avail ? want - done : avail;
            memcpy(out + done, file->cache + (pos - file->cacheStart), n);
            done += n;
            file->position += n;
            continue;
        }

        long remaining = want - done;
        SetFPos(file->refNum, fsFromStart, pos);
        if (remaining >= FILE_CACHE_SIZE) {
            // big reads go straight into the destination
            long n = remaining;
            FSRead(file->refNum, &n, out + done);
            if (n <= 0)
                break;
            done += n;
            file->position += n;
        }
        else {
            long n = FILE_CACHE_SIZE;
            if (pos + n > file->size)
                n = file->size - pos;
            FSRead(file->refNum, &n, file->cache);
            if (n <= 0)
                break;
            file->cacheStart  = pos;
            file->cacheLength = n;
        }
    }

    return (size_t)done / size;
}

int FileGetC(File *file)
{
    uint8_t c;
    return FileRead(&c, 1, 1, file) == 1 ? c : EOF;
}

int FileSeek(File *file, long offset, int whence)
{
    if (!file)
        return -1;

    long pos = offset;
    if (whence == SEEK_CUR)
        pos = file->position + offset;
    else if (whence == SEEK_END)
        pos = file->size + offset;
    if (pos < 0)
        return -1;

    file->position = pos;
    return 0;
}

long FileTell(File *file) { return file ? file->position : -1; }

int FileClose(File *file)
{
    if (!file)
        return EOF;

    short vRefNum = 0;
    bool flush    = file->writable && GetVRefNum(file->refNum, &vRefNum) == noErr;
    FSClose(file->refNum);
    if (flush)
        FlushVol(NULL, vRefNum);
    DisposePtr((Ptr)file);
    return 0;
}

size_t FileWrite(const void *buffer, size_t size, size_t count, File *file)
{
    if (!file || !file->writable || !size || !count)
        return 0;

    long n = (long)(size * count);
    SetFPos(file->refNum, fsFromStart, file->position);
    FSWrite(file->refNum, &n, buffer);
    if (n < 0)
        n = 0;

    // keep the cache coherent
    if (file->position < file->cacheStart + file->cacheLength && file->position + n > file->cacheStart)
        file->cacheLength = 0;

    file->position += n;
    if (file->position > file->size)
        file->size = file->position;
    return (size_t)n / size;
}

static FILE *logFile = NULL;

void LogWrite(const char *text)
{
    if (!logFile) {
        RememberAppDir();
        logFile = fopen("RSDK Log.txt", "w");
        if (!logFile)
            return;
    }

    fputs(text, logFile);
    fflush(logFile);
}

uint64_t GetMicroseconds()
{
    UnsignedWide t;
    Microseconds(&t);
    return ((uint64_t)t.hi << 32) | t.lo;
}

void Alert(const char *text)
{
    Str255 msg;
    size_t len = strlen(text);
    if (len > 255)
        len = 255;
    msg[0] = (unsigned char)len;
    memcpy(msg + 1, text, len);

    ParamText(msg, "\p", "\p", "\p");
    StopAlert(128, NULL);
}

} // namespace Classic
