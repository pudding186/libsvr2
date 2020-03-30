#include <string.h>
#include "../include/utility.hpp"
#include "./lib_svr_common_def.h"

bkdr_str* alloc_bkdr_str(const char* str, bool is_copy)
{
    if (is_copy)
    {
        size_t str_len = strlen(str);
        void* ptr = libsvr_memory_manager_alloc(sizeof(bkdr_str) + str_len + 1);
        ((bkdr_str*)ptr)->hash_code = BKDRHash(str);
        ((bkdr_str*)ptr)->str = (char*)ptr + sizeof(bkdr_str);
        char* p_str = (char*)ptr + sizeof(bkdr_str);
        memcpy(p_str, str, str_len);
        p_str[str_len] = 0;



        return (bkdr_str*)ptr;
    }
    else
    {
        void* ptr = libsvr_memory_manager_alloc(sizeof(bkdr_str));
        ((bkdr_str*)ptr)->hash_code = BKDRHash(str);
        ((bkdr_str*)ptr)->str = str;

        return (bkdr_str*)ptr;
    }
}

void free_bkdr_str(bkdr_str* str)
{
    libsvr_memory_manager_free(str);
}

bkdr_wstr* alloc_bkdr_wstr(const wchar_t* str, bool is_copy)
{
    if (is_copy)
    {
        size_t str_len = wcslen(str);
        void* ptr = libsvr_memory_manager_alloc(sizeof(bkdr_wstr) + (str_len + 1)*sizeof(wchar_t));
        ((bkdr_wstr*)ptr)->hash_code = BKDRHashW(str);
        ((bkdr_wstr*)ptr)->str = (wchar_t*)((char*)ptr + sizeof(bkdr_wstr));
        wchar_t* p_wstr = (wchar_t*)((char*)ptr + sizeof(bkdr_wstr));
        memcpy(p_wstr, str, str_len * sizeof(wchar_t));
        p_wstr[str_len] = 0;

        return (bkdr_wstr*)ptr;
    }
    else
    {
        void* ptr = libsvr_memory_manager_alloc(sizeof(bkdr_wstr));
        ((bkdr_wstr*)ptr)->hash_code = BKDRHashW(str);
        ((bkdr_wstr*)ptr)->str = str;

        return (bkdr_wstr*)ptr;
    }
}

void free_bkdr_wstr(bkdr_wstr* str)
{
    libsvr_memory_manager_free(str);
}

ptrdiff_t bkdr_str_cmp(const void* str1, const void* str2)
{
    if (((bkdr_str*)str1)->hash_code < ((bkdr_str*)str2)->hash_code)
    {
        return -1;
    }
    else if (((bkdr_str*)str1)->hash_code >((bkdr_str*)str2)->hash_code)
    {
        return 1;
    }

    return strcmp(((bkdr_str*)str1)->str, ((bkdr_str*)str2)->str);
}

ptrdiff_t bkdr_wstr_cmp(const void* str1, const void* str2)
{
    if (((bkdr_wstr*)str1)->hash_code < ((bkdr_wstr*)str2)->hash_code)
    {
        return -1;
    }
    else if (((bkdr_wstr*)str1)->hash_code >((bkdr_wstr*)str2)->hash_code)
    {
        return 1;
    }

    return wcscmp(((bkdr_wstr*)str1)->str, ((bkdr_wstr*)str2)->str);
}

ptrdiff_t integer_key_group_cmp(void* key_group1, void* key_group2)
{
    if (((integer_key_group*)key_group1)->key_end < ((integer_key_group*)key_group2)->key_begin)
    {
        return -1;
    }
    else if (((integer_key_group*)key_group1)->key_begin > ((integer_key_group*)key_group2)->key_end)
    {
        return 1;
    }
    else
        return 0;
}

