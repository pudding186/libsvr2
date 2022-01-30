#include "./lib_svr_common_def.h"

#ifdef _MSC_VER
#include <intrin.h>
#include <windows.h>

#undef max
#undef min

#elif __GNUC__
#include <limits.h>
#include <endian.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <x86intrin.h>
#else
#error "unknown compiler"
#endif


#include <time.h>
#include <stdio.h>

#include "../include/utility.hpp"
#include "../include/memory_pool.h"
#include "../include/smemory.hpp"
#include "../include/timer.h"

class RandomNumGenarator
{
public:
    RandomNumGenarator(long long lSeed1 = 0, long long lSeed2 = 0)
    {
        if (!lSeed1)
        {
            m_lSeed1 = __rdtsc();
        }
        else
            m_lSeed1 = lSeed1;

        if (!lSeed2)
        {
            m_lSeed2 = __rdtsc();
        }
        else
            m_lSeed2 = lSeed2;
    }

    int GetOneRandNum(long _tModNum)
    {
        int _ResultNum = 0;

        long long lTempNum = 0;
        lTempNum = (a1 * m_lSeed1 + c1) % m1 - (a2 * m_lSeed2 + c2) % m2;
        if (lTempNum < 0) {
            //lTempNum += m - 1;
        }

        _ResultNum = static_cast<int>(lTempNum);
        m_lSeed1 = (a1 * m_lSeed1 + c1) % m1;
        m_lSeed2 = (a2 * m_lSeed2 + c2) % m2;

        if (0 == _tModNum) {
            return _ResultNum;
        }
        else
        {
            return _ResultNum % _tModNum;
        }
    }

    long long GetOneRandNum64(long long _tModNum)
    {
        long long _ResultNum = 0;

        long long lTempNum = 0;
        lTempNum = (a1 * m_lSeed1 + c1) % m1 - (a2 * m_lSeed2 + c2) % m2;
        if (lTempNum < 0) {
            //lTempNum += m - 1;
        }

        _ResultNum = lTempNum;
        m_lSeed1 = (a1 * m_lSeed1 + c1) % m1;
        m_lSeed2 = (a2 * m_lSeed2 + c2) % m2;

        if (0 == _tModNum) {
            return _ResultNum;
        }
        else
        {
            return _ResultNum % _tModNum;
        }
    }
protected:
private:
    long long m_lSeed1;
    long long m_lSeed2;

    static const long long m1 = 2147483563;
    static const long long m2 = 2147483399;
    static const long long m = m2;
    static const long long a1 = 40014;
    static const long long a2 = 40692;
    static const long long c1 = 12211;
    static const long long c2 = 3791;
};

//void init_lib_svr(void)
//{
//    SMemory::Delete(SMemory::New<char>(1));
//}

long long rand_integer(long long min, long long max)
{
    static RandomNumGenarator rnd_gen64(get_time(), get_tick());

    long long range = max - min;
    if (range <= 0)
        return min;

    long long rnd = rnd_gen64.GetOneRandNum64(range + 1);
    rnd = llabs(rnd);

    return min + rnd;
}

//////////////////////////////////////////////////////////////////////////
unsigned long long BKDRHash(const char* str)
{
    unsigned long long hash = 0;
    while (*str)
    {
        hash = hash * 131 + (*str++);
    }

    return hash;
}

unsigned long long BKDRHashW(const wchar_t* str)
{
    unsigned long long hash = 0;

    while (*str)
    {
        hash = hash * 131 + (*str++);
    }

    return hash;
}

//////////////////////////////////////////////////////////////////////////
#define MAX_FILE_FULL_PATH  512
typedef struct st_dir_file_node
{
    struct st_dir_file_node* next;
    wchar_t dir_file_full_path[MAX_FILE_FULL_PATH];
}dir_file_node;

void pop_front(dir_file_node** head, dir_file_node** tail, HMEMORYUNIT dir_file_node_unit)
{
    if (*head)
    {
        if (*head == *tail)
        {
            memory_unit_free(dir_file_node_unit, *head);
            *head = 0;
            *tail = 0;
        }
        else
        {
            dir_file_node* tmp = *head;
            *head = (*head)->next;
            memory_unit_free(dir_file_node_unit, tmp);
        }
    }
}

void push_back(dir_file_node** head, dir_file_node** tail, dir_file_node* node)
{
    if (*tail)
    {
        (*tail)->next = node;
        *tail = node;
        node->next = 0;
    }
    else
    {
        *head = node;
        *tail = node;
        node->next = 0;
    }
}

//int mb_to_wc(unsigned int code_page, const char* src, int c_len, wchar_t* dst, int w_size)
//{
//    int translate_len = MultiByteToWideChar(code_page, 0, src, c_len, dst, w_size);
//
//    if (!translate_len)
//    {
//        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
//        {
//            return MultiByteToWideChar(code_page, 0, src, c_len, 0, 0);
//        }
//
//        return 0;
//    }
//
//    return translate_len;
//}
//
//int wc_to_mb(unsigned int code_page, const wchar_t* src, int w_len, char* dst, int c_size)
//{
//    int translate_len = WideCharToMultiByte(code_page, 0, src, w_len, dst, c_size, 0, 0);
//
//    if (!translate_len)
//    {
//        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
//        {
//            return WideCharToMultiByte(code_page, 0, src, w_len, 0, 0, 0, 0);
//        }
//
//        return 0;
//    }
//
//    return translate_len;
//}

// Based on utf8_check.c by Markus Kuhn, 2005
// https://www.cl.cam.ac.uk/~mgk25/ucs/utf8_check.c
// Optimized for predominantly 7-bit content by Alex Hultman, 2016
// Licensed as Zlib, like the rest of this project
bool is_valid_utf8(const unsigned char *s, size_t length)
{
    for (const unsigned char *e = s + length; s != e; ) {
        if (s + 4 <= e && ((*(uint32_t *)s) & 0x80808080) == 0) {
            s += 4;
        }
        else {
            while (!(*s & 0x80)) {
                if (++s == e) {
                    return true;
                }
            }

            if ((s[0] & 0x60) == 0x40) {
                if (s + 1 >= e || (s[1] & 0xc0) != 0x80 || (s[0] & 0xfe) == 0xc0) {
                    return false;
                }
                s += 2;
            }
            else if ((s[0] & 0xf0) == 0xe0) {
                if (s + 2 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 ||
                    (s[0] == 0xe0 && (s[1] & 0xe0) == 0x80) || (s[0] == 0xed && (s[1] & 0xe0) == 0xa0)) {
                    return false;
                }
                s += 3;
            }
            else if ((s[0] & 0xf8) == 0xf0) {
                if (s + 3 >= e || (s[1] & 0xc0) != 0x80 || (s[2] & 0xc0) != 0x80 || (s[3] & 0xc0) != 0x80 ||
                    (s[0] == 0xf0 && (s[1] & 0xf0) == 0x80) || (s[0] == 0xf4 && s[1] > 0x8f) || s[0] > 0xf4) {
                    return false;
                }
                s += 4;
            }
            else {
                return false;
            }
        }
    }
    return true;
}

size_t ulltostr(unsigned long long val, char* buf, size_t size, unsigned int radix)
{
    char* p;
    char* firstdig;
    char temp;
    unsigned digval;
    size_t length;

    length = 0;

    p = buf;

    firstdig = p;

    do
    {
        digval = (unsigned)(val%radix);
        val /= radix;

        if (digval > 9)
            *p++ = (char)(digval - 10 + 'a');
        else
            *p++ = (char)(digval + '0');

        length++;

    } while (val > 0 && length < size);

    if (length >= size)
    {
        buf[0] = '\0';
        return 0;
    }

    *p-- = '\0';

    do
    {
        temp = *p;
        *p = *firstdig;
        *firstdig = temp;
        --p;
        ++firstdig;
    } while (firstdig < p);

    return length;
}

size_t lltostr(long long val, char* buf, size_t size, unsigned int radix)
{
    if (val < 0)
    {
        if (size < 2)
        {
            return false;
        }

        *buf = '-';
        buf++;
        size--;
        val = -val;
    }

    return ulltostr(val, buf, size, radix);
}

const void *memmem_s(const void *haystack, size_t haystacklen,
	const void *needle, size_t needlelen)
{
	if (needlelen > 1)
	{
		const char* ptr_find = (const char*)haystack;
		const char* ptr_end = (const char*)haystack + haystacklen;

		for (;;)
		{
			ptr_find = (const char*)memchr(ptr_find, *(const char*)needle, ptr_end - ptr_find);

			if (!ptr_find)
			{
				return 0;
			}

			if ((size_t)(ptr_end - ptr_find) < needlelen)
			{
				return 0;
			}

			if (memcmp(ptr_find, needle, needlelen))
			{
				ptr_find += needlelen;
			}
			else
				return ptr_find;

		}
	}
	else
		return memchr(haystack, *(char*)needle, haystacklen);
}

unsigned long long get_cycle(void)
{
    return __rdtsc();
}

char bin2str_table[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

bool bin2str(unsigned char* bin_data, size_t bin_len, char* str, size_t str_len)
{
    if ((bin_len * 2 + 1) > str_len)
    {
        return false;
    }

    size_t i;
    for (i = 0; i < bin_len; ++i) {
        unsigned char b = bin_data[i];
        str[i * 2] = bin2str_table[b >> 4 & 0xF];
        str[i * 2 + 1] = bin2str_table[b & 0xF];
    }

    str[i * 2] = 0;
    return str;
}


char str2bin_table[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,
    0,  0,  0,  0,  0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 10, 11, 12,
   13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

bool str2bin(const char* str, unsigned char* bin, size_t bin_len)
{
    size_t i = 0;
    while (str[i * 2]) {
        auto c0 = (unsigned char)str[i * 2];
        auto c1 = (unsigned char)str[i * 2 + 1];

        bin[i] = str2bin_table[c0] * 0x10 + str2bin_table[c1];
        i++;

        if (i > bin_len)
        {
            return false;
        }
    }

    return true;
}

#ifdef _MSC_VER
bool (for_each_wfile)(const wchar_t* dir, pfn_wfile do_file, pfn_wdir do_dir, void* user_data)
{
    HMEMORYUNIT dir_file_node_unit = create_memory_unit(sizeof(dir_file_node));
    WIN32_FIND_DATAW FindFileDataW;
    HANDLE hFind = INVALID_HANDLE_VALUE;

    dir_file_node* node = (dir_file_node*)memory_unit_alloc(dir_file_node_unit);

    dir_file_node* dir_list_head = 0;
    dir_file_node* dir_list_tail = 0;

    wcscpy_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, dir);

    switch (node->dir_file_full_path[wcslen(node->dir_file_full_path) - 1])
    {
    case L'/':
    case L'\\':
    {
        wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"*");
    }
    break;
    case L'*':
    {
    }
    break;
    default:
    {
        wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"/*");
    }
    }

    dir_list_head = node;
    dir_list_tail = node;
    node->next = 0;

    do
    {
        hFind = FindFirstFileW(dir_list_head->dir_file_full_path, &FindFileDataW);

        dir_list_head->dir_file_full_path[wcslen(dir_list_head->dir_file_full_path) - 1] = L'\0';

        if (INVALID_HANDLE_VALUE == hFind)
        {
            destroy_memory_unit(dir_file_node_unit);
            return false;
        }
        else
        {
            if ((FILE_ATTRIBUTE_DIRECTORY & FindFileDataW.dwFileAttributes) &&
                wcscmp(FindFileDataW.cFileName, L".") &&
                wcscmp(FindFileDataW.cFileName, L".."))
            {
                node = (dir_file_node*)memory_unit_alloc(dir_file_node_unit);

                wcscpy_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, dir_list_head->dir_file_full_path);
                wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, FindFileDataW.cFileName);
                wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"/");

                if (do_dir)
                {
                    if (!do_dir(node->dir_file_full_path, user_data))
                    {
                        destroy_memory_unit(dir_file_node_unit);
                        return false;
                    }
                }

                wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"*");
                push_back(&dir_list_head, &dir_list_tail, node);
            }
            else
            {
                if (wcscmp(FindFileDataW.cFileName, L".") &&
                    wcscmp(FindFileDataW.cFileName, L".."))
                {
                    if (do_file)
                    {
                        if (!do_file(dir_list_head->dir_file_full_path, FindFileDataW.cFileName, user_data))
                        {
                            destroy_memory_unit(dir_file_node_unit);
                            return false;
                        }
                    }
                }
            }


            while (FindNextFileW(hFind, &FindFileDataW))
            {
                if ((FILE_ATTRIBUTE_DIRECTORY & FindFileDataW.dwFileAttributes) &&
                    wcscmp(FindFileDataW.cFileName, L".") &&
                    wcscmp(FindFileDataW.cFileName, L".."))
                {
                    node = (dir_file_node*)memory_unit_alloc(dir_file_node_unit);

                    wcscpy_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, dir_list_head->dir_file_full_path);
                    wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, FindFileDataW.cFileName);
                    wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"/");

                    if (do_dir)
                    {
                        if (!do_dir(node->dir_file_full_path, user_data))
                        {
                            destroy_memory_unit(dir_file_node_unit);
                            return false;
                        }
                    }

                    wcscat_s(node->dir_file_full_path, MAX_FILE_FULL_PATH, L"*");
                    push_back(&dir_list_head, &dir_list_tail, node);
                }
                else
                {
                    if (wcscmp(FindFileDataW.cFileName, L".") &&
                        wcscmp(FindFileDataW.cFileName, L".."))
                    {
                        if (do_file)
                        {
                            if (!do_file(dir_list_head->dir_file_full_path, FindFileDataW.cFileName, user_data))
                            {
                                destroy_memory_unit(dir_file_node_unit);
                                return false;
                            }
                        }
                    }
                }
            }
        }

        pop_front(&dir_list_head, &dir_list_tail, dir_file_node_unit);
        FindClose(hFind);

    } while (dir_list_head);

    destroy_memory_unit(dir_file_node_unit);
    return true;
}


//////////////////////////////////////////////////////////////////////////
#elif __GNUC__
unsigned long long (htonll)(unsigned long long value)
{
	return htobe64(value);
}

unsigned long long (ntohll)(unsigned long long value)
{
	return be64toh(value);
}

#endif

size_t split_mem_to_segments(const void* mem, size_t mem_size, const void* split, size_t  split_size, mem_seg* segs, size_t max_mem_seg)
{
    const char* ptr_begin = (const char*)mem;
    const char* ptr_end = (const char*)mem + mem_size;
    const char* ptr_pos;
    size_t seg_count = 0;

    ptr_pos = (const char*)memmem_s(ptr_begin, ptr_end - ptr_begin, split, split_size);

    while (ptr_pos)
    {
        if (seg_count < max_mem_seg)
        {
            if (ptr_pos > ptr_begin)
            {
                segs[seg_count].mem = ptr_begin;
            }
            else if (ptr_pos == ptr_begin)
            {
                segs[seg_count].mem = 0;
            }

            segs[seg_count].mem_size = ptr_pos - ptr_begin;
        }

        ++seg_count;

        ptr_begin = ptr_pos + split_size;
        ptr_pos = (const char*)memmem_s(ptr_begin, ptr_end - ptr_begin, split, split_size);
    }

    if (ptr_begin < ptr_end)
    {
        if (seg_count < max_mem_seg)
        {
            segs[seg_count].mem = ptr_begin;
            segs[seg_count].mem_size = ptr_end - ptr_begin;
        }

        ++seg_count;
    }

    return seg_count;
}

//////////////////////////////////////////////////////////////////////////

#define MAX_STACK_CAPACITY	320

class CFuncPerformance
{
    friend class CFuncPerformanceCheck;
public:
    CFuncPerformance(void)
    {
        m_func_list = 0;
        //m_func_stack = 0;
        //m_shm_key = 0;
        m_cur_func_perf_info = 0;
        m_top = 0;
    }
    ~CFuncPerformance(void)
    {

    }

    inline void RecordFunc(CFuncPerformanceInfo* func_perf_info)
    {
        func_perf_info->next = m_func_list;
        m_func_list = func_perf_info;
    }

    CFuncPerformanceInfo* FuncPerfFirst(void)
    {
        return m_func_list;
    }

    CFuncPerformanceInfo* StackFuncPerfInfo(int idx)
    {
        if (idx >= 0 && idx < std::min(m_top, MAX_STACK_CAPACITY - 1))
        {
            return m_stack[idx];
        }

        return 0;
    }

    inline int StackTop(void)
    {
        return m_top;
    }

    inline CFuncPerformanceInfo** Stack(void)
    {
        return m_stack;
    }

    inline void Push(CFuncPerformanceInfo* func_perf_info)
    {
        if (m_top < MAX_STACK_CAPACITY - 20)
        {
            m_stack[m_top] = func_perf_info;
        }
        else
        {
            m_top -= 20;
            throw "stack overflow";
        }
        ++m_top;
    }

    inline void Pop(void)
    {
        if (m_top > 0)
        {
            --m_top;
        }
        else
        {
            throw "stack illegal";
        }
    }
protected:
private:
    CFuncPerformanceInfo*   m_func_list;
    CFuncPerformanceInfo*   m_cur_func_perf_info;
    CFuncPerformanceInfo*   m_stack[MAX_STACK_CAPACITY];
    int			            m_top;
};

CFuncPerformanceInfo::CFuncPerformanceInfo(const char* func_name, CFuncPerformance& fpf)
    :func_name(func_name)
    , elapse_cycles(0)
    , hit_count(0)
{
    fpf.RecordFunc(this);
}

CFuncPerformanceCheck::CFuncPerformanceCheck(CFuncPerformanceInfo* info, CFuncPerformance& fpf)
{
    m_fpf = &fpf;
    m_func_perf_info = info;
    m_parent_func_perf_info = fpf.m_cur_func_perf_info;
    fpf.m_cur_func_perf_info = info;
    fpf.Push(info);
    m_cycles = __rdtsc();
}

CFuncPerformanceCheck::~CFuncPerformanceCheck(void)
{
    m_cycles = __rdtsc() - m_cycles;
    m_fpf->Pop();
    m_func_perf_info->elapse_cycles += m_cycles;
    m_func_perf_info->once_cycles = m_cycles;
    if (m_parent_func_perf_info)
    {
        m_parent_func_perf_info->elapse_cycles -= m_cycles;
    }
    m_fpf->m_cur_func_perf_info = m_parent_func_perf_info;
}

CFuncPerformanceInfo* FuncPerfFirst(CFuncPerformance& fpf)
{
    return fpf.FuncPerfFirst();
}

int GetFuncStackTop(CFuncPerformance& fpf)
{
    return fpf.StackTop();
}

CFuncPerformanceInfo** GetFuncStack(CFuncPerformance& fpf)
{
    return fpf.Stack();
}

CFuncPerformanceInfo* GetStackFuncPerfInfo(CFuncPerformance& fpf, int idx)
{
    return fpf.StackFuncPerfInfo(idx);
}

CFuncPerformance& DefFuncPerf(void)
{
    static thread_local CFuncPerformance def_func_perf_mgr;
    return def_func_perf_mgr;
}

size_t FuncStackToCache(CFuncPerformance& fpf, char* cache, size_t cache_size)
{
    size_t total_len = 0;
    cache_size -= 1;
    cache[cache_size] = 0;
    int format_len = snprintf(cache, cache_size, "****** call stack ******\r\n");
    int stack_idx = fpf.StackTop();

    if (format_len < 0)
    {
        return 0;
    }
    else
    {
        total_len += format_len;
    }

    while (stack_idx > 0)
    {
        CFuncPerformanceInfo* info = fpf.StackFuncPerfInfo(stack_idx - 1);
        if (info)
        {
            format_len = snprintf(cache + total_len, cache_size - total_len,
                "[stack:%2d] call %s()\r\n", stack_idx, info->func_name);
        }
        else
        {
            format_len = snprintf(cache + total_len, cache_size - total_len,
                "[stack:%2d] call ?()\r\n", stack_idx);
        }

        if (format_len < 0)
        {
            return 0;
        }
        else
        {
            total_len += format_len;
        }

        if (cache_size <= total_len)
        {
            return cache_size;
        }

        --stack_idx;
    }

    return total_len;
}

