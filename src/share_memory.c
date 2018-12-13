#include <stdio.h>
#include <windows.h>
#include "./lib_svr_common_def.h"
#include "../include/share_memory.h"

typedef struct st_share_memory_info
{
    HANDLE	handle;
    void*	mem;
    size_t	size;
}share_memory_info;

void* shm_alloc(const TCHAR* shm_name, unsigned int size)
{
    void* pTr = NULL;
    HANDLE hHandle = NULL;
    TCHAR key_name[64];
    SECURITY_ATTRIBUTES sa = { 0 };
    SECURITY_DESCRIPTOR sd = { 0 };

    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    _stprintf_s(key_name, 64, _T("Global\\%s"), shm_name);

    hHandle = CreateFileMapping(INVALID_HANDLE_VALUE, &sa,
        PAGE_READWRITE, 0, size + sizeof(share_memory_info), key_name);

    if (NULL == hHandle ||
        GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hHandle)
        {
            CloseHandle(hHandle);
        }
        return 0;
    }

    pTr = MapViewOfFile(hHandle, FILE_MAP_ALL_ACCESS, 0, 0, size + sizeof(share_memory_info));

    if (NULL == pTr)
    {
        CloseHandle(hHandle);
        return 0;
    }

    share_memory_info* info = (share_memory_info*)pTr;

    info->handle = hHandle;
    info->mem = pTr;
    info->size = size + sizeof(share_memory_info);

    return (char*)pTr + sizeof(share_memory_info);
}

void shm_free(void* mem)
{
    HANDLE hd = INVALID_HANDLE_VALUE;
    share_memory_info* info = (share_memory_info*)((char*)mem - sizeof(share_memory_info));

    if (info->mem != info)
    {
        return;
    }

    hd = info->handle;
    UnmapViewOfFile(info->mem);
    CloseHandle(hd);
}