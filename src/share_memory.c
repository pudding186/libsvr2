#include <stdio.h>
#include "./lib_svr_common_def.h"
#include "../include/share_memory.h"


#ifdef _MSC_VER
#include <windows.h>
typedef struct st_share_memory_info
{
    HANDLE	handle;
    void*	mem;
    size_t	size;
}share_memory_info;

void* shm_alloc(int key, unsigned int size)
{
    void* pTr = NULL;
    HANDLE hHandle = NULL;
    char key_name[64];
    SECURITY_ATTRIBUTES sa = { 0 };
    SECURITY_DESCRIPTOR sd = { 0 };

    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = &sd;
    sa.bInheritHandle = FALSE;

    //_stprintf_s(key_name, 64, _T("Global\\%s"), shm_name);
    snprintf(key_name, sizeof(key_name), "Global\\libsvr_%d", key);

    hHandle = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa,
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

#elif __GNUC__

#define _XOPEN_SOURCE
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct st_share_memory_info
{
    int	    handle;
    void*	mem;
    size_t	size;
}share_memory_info;

void* shm_alloc(int key, unsigned int size)
{
    void* pTr = 0;
    int mem_id = shmget((key_t)key, size + sizeof(share_memory_info), 0666 | IPC_CREAT | IPC_EXCL);

    if (mem_id == -1)
    {
        return 0;
    }
    
    pTr = shmat(mem_id, 0, SHM_R);

    if ((void*)-1 == pTr)
    {
        shmctl(mem_id, IPC_RMID, 0);
        return 0;
    }

    share_memory_info* info = (share_memory_info*)pTr;

    info->handle = mem_id;
    info->mem = pTr;
    info->size = size + sizeof(share_memory_info);

    return (char*)pTr + sizeof(share_memory_info);
}

void shm_free(void* mem)
{
    share_memory_info* info = (share_memory_info*)((char*)mem - sizeof(share_memory_info));

    if (info->mem != info)
    {
        return;
    }

    shmdt(info->mem);
    shmctl(info->handle, IPC_RMID, 0);
}

#else
#error "unknown compiler"
#endif



