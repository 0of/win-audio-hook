#include <stdio.h>
#include <tchar.h>
#include "SharedMemory.h"


CSharedMemory::CSharedMemory(void)
	:mpBuf(NULL)
	,mBufSize(0)
	,mhMapFile(NULL)
{
}


CSharedMemory::~CSharedMemory(void)
{
	Close();
}

BOOL CSharedMemory::Open(LPCTSTR pName,DWORD procID,DWORD maxSize,int flag)
{
	TCHAR lszName[256] = {0};

	_stprintf(lszName,_T("%s_%d"),pName,procID);

	if (flag)
	{		
		mhMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS,\
			FALSE,
			lszName);		
	}
	else
	{		
		mhMapFile =  CreateFileMapping(INVALID_HANDLE_VALUE,NULL,\
			PAGE_READWRITE,\
			0,\
			maxSize,\
			lszName);		
	}

	if (mhMapFile == NULL)
	{		
		return FALSE;
	}

	mpBuf = (BYTE*)MapViewOfFile(mhMapFile,FILE_MAP_ALL_ACCESS,0,0,maxSize);
	if(mpBuf == NULL)
	{		
		CloseHandle(mhMapFile);
		return FALSE;
	}

	mBufSize = maxSize;
	return TRUE;
}


void CSharedMemory::Close()
{
	if (mpBuf)
	{
		UnmapViewOfFile(mpBuf);
		mpBuf = NULL;
	}

	if (mhMapFile) {
		CloseHandle(mhMapFile);
		mhMapFile = NULL;
	}
}

DWORD CSharedMemory::Write(BYTE *pBuf,DWORD iSize,DWORD offset)
{
	DWORD ldwRet = iSize;

	if (offset + iSize > mBufSize) {
		ldwRet = mBufSize - offset;		
	}
	memcpy(mpBuf + offset,pBuf,ldwRet);

	return ldwRet;
}

DWORD CSharedMemory::Read(BYTE *pBuf,DWORD iSize,DWORD offset)
{
	DWORD ldwRet = iSize;

	if (offset + iSize > mBufSize) {
		ldwRet = mBufSize - offset;		
	}
	memcpy(pBuf,mpBuf+offset,ldwRet);

	return ldwRet;
}

BYTE*  CSharedMemory::GetPtr(DWORD offset)
{
    return GetRawPtr(offset);
}

BYTE* CSharedMemory::GetRawPtr( DWORD offset )
{
	return mpBuf + offset;
}

