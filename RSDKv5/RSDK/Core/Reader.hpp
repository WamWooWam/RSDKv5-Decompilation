#ifndef READER_H
#define READER_H

#if RETRO_RENDERDEVICE_SDL2 || RETRO_AUDIODEVICE_SDL2 || RETRO_INPUTDEVICE_SDL2
#define FileIO                                          SDL_RWops
#define fOpen(path, mode)                               SDL_RWFromFile(path, mode)
#define fRead(buffer, elementSize, elementCount, file)  SDL_RWread(file, buffer, elementSize, elementCount)
#define fGetC(file)                                     SDL_ReadU8(file)
#define fSeek(file, offset, whence)                     SDL_RWseek(file, offset, whence)
#define fTell(file)                                     SDL_RWtell(file)
#define fClose(file)                                    SDL_RWclose(file)
#define fWrite(buffer, elementSize, elementCount, file) SDL_RWwrite(file, buffer, elementSize, elementCount)
#else
#define FileIO                                          FILE
#define fOpen(path, mode)                               fopen(path, mode)
#define fRead(buffer, elementSize, elementCount, file)  fread(buffer, elementSize, elementCount, file)
#define fGetC(file)                                     fgetc(file)
#define fSeek(file, offset, whence)                     fseek(file, offset, whence)
#define fTell(file)                                     ftell(file)
#define fClose(file)                                    fclose(file)
#define fWrite(buffer, elementSize, elementCount, file) fwrite(buffer, elementSize, elementCount, file)
#endif

#if RETRO_PLATFORM == RETRO_ANDROID || RETRO_PLATFORM == RETRO_WIIU
#undef fOpen
FileIO *fOpen(const char *path, const char *mode);
#endif

#include <miniz/miniz.h>

namespace RSDK
{
	
#define RSDK_SIGNATURE_RSDK (0x4B445352) // "RSDK"
#if RETRO_REV0U
#define RSDK_SIGNATURE_DATA (0x61746144) // "Data"
#endif
	
#define DATAFILE_COUNT (0x1000)
#define DATAPACK_COUNT (4)
	
	enum Scopes {
		SCOPE_NONE,
		SCOPE_GLOBAL,
		SCOPE_STAGE,
	};
	
	struct FileInfo {
		int32 fileSize;
		int32 externalFile;
		FileIO *file;
		uint8 *fileBuffer;
		int32 readPos;
		int32 fileOffset;
		uint8 usingFileBuffer;
		uint8 encrypted;
		uint8 eNybbleSwap;
		uint8 encryptionKeyA[0x10];
		uint8 encryptionKeyB[0x10];
		uint8 eKeyPosA;
		uint8 eKeyPosB;
		uint8 eKeyNo;
	};
	
	struct RSDKFileInfo {
		RETRO_HASH_MD5(hash);
		int32 size;
		int32 offset;
		uint8 encrypted;
		uint8 useFileBuffer;
		int32 packID;
	};
	
	struct RSDKContainer {
		char name[0x100];
		uint8 *fileBuffer;
		int32 fileCount;
	};
	
	extern RSDKFileInfo dataFileList[DATAFILE_COUNT];
	extern RSDKContainer dataPacks[DATAPACK_COUNT];
	
	extern uint8 dataPackCount;
	extern uint16 dataFileListCount;
	
	extern char gameLogicName[0x200];
	
	extern bool32 useDataPack;
	
#if RETRO_REV0U
	void DetectEngineVersion();
#endif
	bool32 LoadDataPack(const char *filename, size_t fileOffset, bool32 useBuffer);
	bool32 OpenDataFile(FileInfo *info, const char *filename);
	
	enum FileModes { FMODE_NONE, FMODE_RB, FMODE_WB, FMODE_RB_PLUS };
	
	static const char *openModes[3] = { "rb", "wb", "rb+" };
	
	inline void InitFileInfo(FileInfo *info)
	{
		info->file            = NULL;
		info->fileSize        = 0;
		info->externalFile    = false;
		info->usingFileBuffer = false;
		info->encrypted       = false;
		info->readPos         = 0;
		info->fileOffset      = 0;
	}
	
	bool32 LoadFile(FileInfo *info, const char *filename, uint8 fileMode);
	
	inline void CloseFile(FileInfo *info)
	{
		if (!info->usingFileBuffer && info->file)
			fClose(info->file);
		
		info->file = NULL;
	}
	
	void GenerateELoadKeys(FileInfo *info, const char *key1, int32 key2);
	void DecryptBytes(FileInfo *info, void *buffer, size_t size);
	void SkipBytes(FileInfo *info, int32 size);
	
	inline void Seek_Set(FileInfo *info, int32 count)
	{
		if (info->readPos != count) {
			if (info->encrypted) {
				info->eKeyNo      = (info->fileSize / 4) & 0x7F;
				info->eKeyPosA    = 0;
				info->eKeyPosB    = 8;
				info->eNybbleSwap = false;
				SkipBytes(info, count);
			}
			
			info->readPos = count;
			if (info->usingFileBuffer) {
				uint8 *fileBuffer = (uint8 *)info->file;
				info->fileBuffer  = &fileBuffer[info->readPos];
			}
			else {
				fSeek(info->file, info->fileOffset + info->readPos, SEEK_SET);
			}
		}
	}
	
	inline void Seek_Cur(FileInfo *info, int32 count)
	{
		info->readPos += count;
		
		if (info->encrypted)
			SkipBytes(info, count);
		
		if (info->usingFileBuffer) {
			info->fileBuffer += count;
		}
		else {
			fSeek(info->file, count, SEEK_CUR);
		}
	}
	
	inline size_t ReadBytes(FileInfo *info, void *data, int32 count)
	{
		size_t bytesRead = 0;
		
		if (info->usingFileBuffer) {
			bytesRead = MIN(count, info->fileSize - info->readPos);
			memcpy(data, info->fileBuffer, bytesRead);
			info->fileBuffer += bytesRead;
		}
		else {
			bytesRead = fRead(data, 1, count, info->file);
		}
		
		if (info->encrypted)
			DecryptBytes(info, data, bytesRead);
		
		info->readPos += bytesRead;
		return bytesRead;
	}
	
	inline uint8 ReadInt8(FileInfo *info)
	{
		int8 result      = 0;
		
		if (info->usingFileBuffer) {
			if (info->fileSize != info->readPos)
				result = *info->fileBuffer++;
		}
		else {
			result = fGetC(info->file);
		}
		
		if (info->encrypted)
			DecryptBytes(info, &result, 1);
		
		++info->readPos;
		return result;
	}
	
	inline int16 ReadInt16(FileInfo *info)
	{
		uint8 bytes[2];
		ReadBytes(info, bytes, sizeof(bytes));
		
		return ((unsigned int)bytes[0] << (8 * 0)) |
		((unsigned int)bytes[1] << (8 * 1));
	}
	
	inline int32 ReadInt32(FileInfo *info, bool32 swapEndian)
	{
		uint8 bytes[4];
		ReadBytes(info, bytes, sizeof(bytes));
		
		if (swapEndian)
			return ((unsigned long)bytes[0] << (8 * 3)) |
			((unsigned long)bytes[1] << (8 * 2)) |
			((unsigned long)bytes[2] << (8 * 1)) |
			((unsigned long)bytes[3] << (8 * 0));
		else
			return ((unsigned long)bytes[0] << (8 * 0)) |
			((unsigned long)bytes[1] << (8 * 1)) |
			((unsigned long)bytes[2] << (8 * 2)) |
			((unsigned long)bytes[3] << (8 * 3));
	}
	
	inline int64 ReadInt64(FileInfo *info)
	{
		uint8 bytes[8];
		ReadBytes(info, bytes, sizeof(bytes));
		
		return ((unsigned long long)bytes[0] << (8 * 0)) |
		((unsigned long long)bytes[1] << (8 * 1)) |
		((unsigned long long)bytes[2] << (8 * 2)) |
		((unsigned long long)bytes[3] << (8 * 3)) |
		((unsigned long long)bytes[4] << (8 * 4)) |
		((unsigned long long)bytes[5] << (8 * 5)) |
		((unsigned long long)bytes[6] << (8 * 6)) |
		((unsigned long long)bytes[7] << (8 * 7));
	}
	
	inline float ReadSingle(FileInfo *info)
	{
		union {
			float result;
			uint32 w;
		} buffer;
		
		buffer.w = ReadInt32(info, false);
		
		return buffer.result;
	}
	
	inline void ReadString(FileInfo *info, char *buffer)
	{
		uint8 size = ReadInt8(info);
		ReadBytes(info, buffer, size);
		buffer[size] = 0;
	}
	
	inline int32 Uncompress(uint8 **cBuffer, int32 cSize, uint8 **buffer, int32 size)
	{
		if (!buffer || !cBuffer)
			return 0;
		
		uLongf cLen    = cSize;
		uLongf destLen = size;
		
		int32 result = uncompress(*buffer, &destLen, *cBuffer, cLen);
		(void)result;
		
		return (int32)destLen;
	}
	
	// The buffer passed in parameter is allocated here, so it's up to the caller to free it once it goes unused
	inline int32 ReadCompressed(FileInfo *info, uint8 **buffer)
	{
		if (!buffer)
			return 0;
		
		uint32 cSize  = ReadInt32(info, false) - 4;
		uint32 sizeBE = ReadInt32(info, false);
		
		uint32 sizeLE = (uint32)((sizeBE << 24) | ((sizeBE << 8) & 0x00FF0000) | ((sizeBE >> 8) & 0x0000FF00) | (sizeBE >> 24));
		AllocateStorage((void **)buffer, sizeLE, DATASET_TMP, false);
		
		uint8 *cBuffer = NULL;
		AllocateStorage((void **)&cBuffer, cSize, DATASET_TMP, false);
		ReadBytes(info, cBuffer, cSize);
		
		uint32 newSize = Uncompress(&cBuffer, cSize, buffer, sizeLE);
		RemoveStorageEntry((void **)&cBuffer);
		
		return newSize;
	}
	
	inline void ClearDataFiles()
	{
		// Unload file list
		for (int32 f = 0; f < DATAFILE_COUNT; ++f) {
			HASH_CLEAR_MD5(dataFileList[f].hash);
		}
	}
	
} // namespace RSDK

#endif
