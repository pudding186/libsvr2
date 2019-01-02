#pragma once
#include <wchar.h>
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

extern int (mb_to_wc)(unsigned int code_page, const char* src, int c_len, wchar_t* dst, int w_size);
extern int (wc_to_mb)(unsigned int code_page, const wchar_t* src, int w_len, char* dst, int c_size);

extern bool (for_each_wfile)(const wchar_t* dir, pfn_wfile do_file, pfn_wdir do_dir, void* user_data);

typedef struct st_mem_seg
{
    const void* mem;
    size_t      mem_size;
}mem_seg;

extern const void* (memmem)(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
extern size_t (split_mem_to_segments)(const void* mem, size_t mem_size, const void* split, size_t  split_size, mem_seg* segs, size_t max_mem_seg);

#ifdef  __cplusplus
}

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
    unsigned long long m_cycles;
    CFuncPerformanceInfo* m_parent_func_perf_info;
    CFuncPerformanceInfo* m_func_perf_info;
    HFUNCPERFMGR	m_mgr;
private:
};

extern HFUNCPERFMGR(CreateFuncPerfMgr)(int shm_key = 0);
extern void (DestroyFuncPerfMgr)(HFUNCPERFMGR mgr);
extern CFuncPerformanceInfo* (FuncPerfFirst)(HFUNCPERFMGR mgr);
extern int (GetFuncStackTop)(HFUNCPERFMGR mgr);
extern CFuncPerformanceInfo* (GetStackFuncPerfInfo)(HFUNCPERFMGR mgr, int idx);

extern HFUNCPERFMGR(DefFuncPerfMgr)(void);

//extern void (FuncStackToFile)(HFUNCPERFMGR mgr, const char* file_path);
extern size_t (FuncStackToCache)(HFUNCPERFMGR mgr, char* cache, size_t cache_size);

#define FUNC_PERFORMANCE_CHECK \
	__declspec(thread) static CFuncPerformanceInfo s_func_perf_info(__FUNCTION__, DefFuncPerfMgr());\
	++ s_func_perf_info.hit_count;\
	CFuncPerformanceCheck func_perf_check(&s_func_perf_info, DefFuncPerfMgr());

#endif

