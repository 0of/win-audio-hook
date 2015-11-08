#pragma once

#include "AudioStreamGlobals.h"

#include <Windows.h>

class AUDIOSTREAM_EXPORT CSharedMemory
{
public:
	CSharedMemory(void);
	~CSharedMemory(void);	
	
    BOOL Open(LPCTSTR pName, DWORD procID, DWORD maxSize, int flag = 0);
	void Close();

	DWORD Write(BYTE *pBuf,DWORD iSize,DWORD offset);
	DWORD Read(BYTE *pBuf,DWORD iSize,DWORD offset);	

	BYTE*  GetPtr(DWORD offset);
	BYTE*  GetRawPtr(DWORD offset);

    BOOL IsOpen() CONST{ return NULL != mhMapFile; }
private:
	BYTE	*mpBuf;
	DWORD	mBufSize;
	HANDLE	mhMapFile;
	HANDLE	mhMutex;
};

