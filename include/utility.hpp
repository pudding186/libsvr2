#pragma once
#include <wchar.h>
#include <string.h>
#include "./lib_svr_def.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern void init_lib_svr(void);

extern long long rand_integer(long long min, long long max);
extern unsigned long long BKDRHash(const char* str);
extern unsigned long long BKDRHashW(const wchar_t* str);

extern unsigned char* (sha1_format)(const unsigned char *d, size_t n, unsigned char *md);

extern int (base64_encode)(const char *in, int in_len, char *out, int out_size);

extern int (base64_decode)(const char *in, int in_len, char *out, int out_size);

//extern int (mb_to_wc)(unsigned int code_page, const char* src, int c_len, wchar_t* dst, int w_size);
//extern int (wc_to_mb)(unsigned int code_page, const wchar_t* src, int w_len, char* dst, int c_size);

extern bool is_valid_utf8(const unsigned char *s, size_t length);

extern size_t (lltostr)(long long val, char* buf, size_t size, unsigned int radix);
extern size_t (ulltostr)(unsigned long long val, char* buf, size_t size, unsigned int radix);

extern bool (for_each_wfile)(const wchar_t* dir, pfn_wfile do_file, pfn_wdir do_dir, void* user_data);

typedef struct st_mem_seg
{
    const void* mem;
    size_t      mem_size;
}mem_seg;

extern const void* (memmem_s)(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

#ifdef _MSC_VER

#elif __GNUC__
extern unsigned long long (htonll)(unsigned long long value);
extern unsigned long long (ntohll)(unsigned long long value);
#else
#error "unknown compiler"
#endif
extern size_t (split_mem_to_segments)(const void* mem, size_t mem_size, const void* split, size_t  split_size, mem_seg* segs, size_t max_mem_seg);

#ifdef  __cplusplus
}

#include <algorithm>
//////////////////////////////////////////////////////////////////////////
#define DECLARE_SINGLETON(cls)\
private:\
    static cls* sm_poInstance;\
public:\
    static bool CreateInstance()\
    {\
        if(NULL == sm_poInstance)sm_poInstance = new cls;\
        return sm_poInstance != NULL;\
    }\
    static bool ReplaceInstance(cls* instance)\
    {\
        if (sm_poInstance)\
        {\
            delete sm_poInstance;\
            sm_poInstance = instance;\
        }\
        return sm_poInstance != NULL;\
    }\
    static cls* Instance(){ return sm_poInstance; }\
    static void DestoryInstance()\
    {\
        if(sm_poInstance != NULL)\
        {\
            delete sm_poInstance;\
            sm_poInstance = NULL;\
        }\
    }

#define INSTANCE_SINGLETON(cls) \
    cls* cls::sm_poInstance = NULL;

#define DEFMEMBER(type,member,name)\
    public:\
    inline void set_##name(type name){member=name;}\
    inline type get_##name() const {return member;}\
protected:\
    type member;\
public:

#define DEFREFMEMBER(type,member,name)\
	public:\
	inline void set_##name(const type& name){member=name;}\
	inline const type& get_##name() const {return member;}\
protected:\
	type member;\
public:

//////////////////////////////////////////////////////////////////////////

class CFuncPerformanceInfo
{
    friend class CFuncPerformanceMgr;
protected:
    CFuncPerformanceInfo * next;
public:
    const char* func_name;
    unsigned long long elapse_cycles;
    unsigned long long hit_count;
    CFuncPerformanceInfo(const char* func_name, HFUNCPERFMGR mgr);
    inline CFuncPerformanceInfo* NextInfo(void)
    {
        return next;
    }
};

class CFuncPerformanceCheck
{
public:
    CFuncPerformanceCheck(CFuncPerformanceInfo* info, HFUNCPERFMGR mgr);
    ~CFuncPerformanceCheck(void);
protected:
    //unsigned long long m_cycles;
    CFuncPerformanceInfo* m_parent_func_perf_info;
    CFuncPerformanceInfo* m_func_perf_info;
    HFUNCPERFMGR	m_mgr;
private:
};

extern HFUNCPERFMGR(CreateFuncPerfMgr)(void);
extern void (DestroyFuncPerfMgr)(HFUNCPERFMGR mgr);
extern CFuncPerformanceInfo* (FuncPerfFirst)(HFUNCPERFMGR mgr);
extern int (GetFuncStackTop)(HFUNCPERFMGR mgr);
extern CFuncPerformanceInfo* (GetStackFuncPerfInfo)(HFUNCPERFMGR mgr, int idx);

extern HFUNCPERFMGR(DefFuncPerfMgr)(void);

//extern void (FuncStackToFile)(HFUNCPERFMGR mgr, const char* file_path);
extern size_t (FuncStackToCache)(HFUNCPERFMGR mgr, char* cache, size_t cache_size);

#define FUNC_PERFORMANCE_CHECK \
	static thread_local CFuncPerformanceInfo s_func_perf_info(__FUNCTION__, DefFuncPerfMgr());\
	++ s_func_perf_info.hit_count;\
	CFuncPerformanceCheck func_perf_check(&s_func_perf_info, DefFuncPerfMgr());

//////////////////////////////////////////////////////////////////////////

template <size_t N>
inline void StrSafeCopy(char(&Destination)[N], const char* Source) throw() {
    //static_assert(N > 0, "StrSafeCopy dst size == 0");
    // initialize for check below
    if (NULL == Source) {
        Destination[0] = '\0';
        return;
    }

    size_t nSrcLen = strnlen(Source, N - 1);
    memcpy(Destination, Source, nSrcLen + 1);
    Destination[N - 1] = '\0';
}

template <typename T>
inline void StrSafeCopy(T& Destination, const char* Source, size_t len)
{
    // Use cast to ensure that we only allow character arrays
    (static_cast<char[sizeof(Destination)]>(Destination));
    size_t size = sizeof(Destination);

    size_t l = size - 1;

    if (l < len)
    {
        l = len;
    }
    strncpy(Destination, Source, l);
    Destination[l] = 0;
}

#endif

