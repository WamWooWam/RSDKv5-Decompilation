#ifndef CMACOS_HELPERS_H
#define CMACOS_HELPERS_H

#include <stdio.h>
#include <stdint.h>
#define FileInfo ToolboxFileInfo
#include <Mw/Milsko.h>
#undef FileInfo

namespace Classic
{

// handrolled open for iniparser because Retro68's open always asks for write permission and also it needs an actual FILE pointer
FILE *OpenStdFile(const char *path, const char *mode);

struct File;
File *FileOpen(const char *path, const char *mode);
size_t FileRead(void *buffer, size_t size, size_t count, File *file);
int FileGetC(File *file);
int FileSeek(File *file, long offset, int whence);
long FileTell(File *file);
int FileClose(File *file);
size_t FileWrite(const void *buffer, size_t size, size_t count, File *file);
void LogWrite(const char *text);
uint64_t GetMicroseconds();

void Alert(const char *text);

} // namespace Classic

#endif // CMACOS_HELPERS_H
