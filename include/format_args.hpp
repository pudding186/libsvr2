#pragma once

#include <typeinfo>
#include <string.h>
#include "./smemory.hpp"
#ifdef _MSC_VER
#undef max
#undef min
#endif

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
// the type holding sequences
template <size_t... N>
struct idx_seq {};

// First define the template signature
template <size_t... N>
struct idx_seq_gen;

// forward extend recursively
template <size_t I, size_t... N>
struct idx_seq_gen<I, N...>
{
    // Take front most number of sequence,
    // decrement it, and prepend it twice.
    // First I - 1 goes into the counter,
    // Second I - 1 goes into the sequence.
    using type = typename idx_seq_gen<I - 1, I - 1, N...>::type;
};

// Recursion abort
template <size_t... N>
struct idx_seq_gen<0, N...>
{
    using type = idx_seq<N...>;
};

template <size_t N>
using idx_seq_type = typename idx_seq_gen<N>::type;

//////////////////////////////////////////////////////////////////////////
template <class T>
struct unwrap_refwrapper_type
{
    using type = T;
};

template <class T>
struct unwrap_refwrapper_type<std::reference_wrapper<T>>
{
    using type = T & ;
};

template <class T>
using special_decay_type = typename unwrap_refwrapper_type<typename std::decay<T>::type>::type;

//////////////////////////////////////////////////////////////////////////

template <typename... Args>
struct SFormatArgs{};

template <>
struct SFormatArgs<>
{
    char    str_cache[256];
    size_t  cache_use_size;
    SFormatArgs()
    {
        cache_use_size = 0;
    }

    ~SFormatArgs()
    {
    }

    virtual void format_to_buffer(fmt::memory_buffer& buffer) = 0;
    virtual void format_c_to_buffer(fmt::memory_buffer& buffer) = 0;
};

template <typename First, typename... Rest>
struct SFormatArgs<First, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(First&& first, Rest&&... rest)
        :value(std::forward<First>(first)),
        SFormatArgs<Rest...>(std::forward<Rest>(rest)...) {}

    size_t size() { return sizeof...(Rest); }

    typename std::enable_if <
        std::is_integral<First>::value ||
        std::is_floating_point<First>::value ||
        std::is_enum<First>::value, First>::type value;
};

template <typename First, typename... Rest>
struct SFormatArgs<First*, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(First*&& first, Rest&&... rest)
        :value(std::forward<First*>(first)),
        SFormatArgs<Rest...>(std::forward<Rest>(rest)...) {}

    size_t size() { return sizeof...(Rest); }

    void* value;
};

extern HMEMORYMANAGER logger_mem_pool(void);

template <typename... Rest>
struct SFormatArgs<std::string, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(const std::string& str, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...)
    {
        size_t str_len = str.length();

        char* ptr = SFormatArgs<>::str_cache + SFormatArgs<>::cache_use_size;

        need_free = false;
        if (SFormatArgs<>::cache_use_size + str_len + 1 >sizeof(SFormatArgs<>::str_cache))
        {
            ptr = (char*)memory_manager_alloc(logger_mem_pool(), str_len + 1);
            need_free = true;
        }

        memcpy(ptr, str.c_str(), str_len);
        ptr[str_len] = 0;
        SFormatArgs<>::cache_use_size += (str_len + 1);
        value = ptr;
    }

    ~SFormatArgs()
    {
        if (need_free)
        {
            memory_manager_free(logger_mem_pool(), (void*)value);
        }
    }



    const char* value;
    bool need_free;
};

template <typename... Rest>
struct SFormatArgs<char*, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}

    SFormatArgs(const char* cstr, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...)
    {
        size_t str_len = strlen(cstr);

        char* ptr = SFormatArgs<>::str_cache + SFormatArgs<>::cache_use_size;

        need_free = false;
        if (SFormatArgs<>::cache_use_size + str_len + 1 > sizeof(SFormatArgs<>::str_cache))
        {
            ptr = (char*)memory_manager_alloc(logger_mem_pool(), str_len + 1);
            need_free = true;
        }

        memcpy(ptr, cstr, str_len);
        ptr[str_len] = 0;
        SFormatArgs<>::cache_use_size += (str_len + 1);
        value = ptr;
    }

    ~SFormatArgs()
    {
        if (need_free)
        {
            memory_manager_free(logger_mem_pool(), (void*)value);
        }
    }

    const char* value;
    bool need_free;
};

template <typename... Rest>
struct SFormatArgs<const char*, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(const char* cstr, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...)
    {
        size_t str_len = strlen(cstr);

        char* ptr = SFormatArgs<>::str_cache + SFormatArgs<>::cache_use_size;

        need_free = false;
        if (SFormatArgs<>::cache_use_size + str_len + 1 > sizeof(SFormatArgs<>::str_cache))
        {
            ptr = (char*)memory_manager_alloc(logger_mem_pool(), str_len + 1);
            need_free = true;
        }

        memcpy(ptr, cstr, str_len);
        ptr[str_len] = 0;
        SFormatArgs<>::cache_use_size += (str_len + 1);
        value = ptr;
    }

    ~SFormatArgs()
    {
        if (need_free)
        {
            memory_manager_free(logger_mem_pool(), (void*)value);
        }
    }

    virtual void format_to_buffer(fmt::memory_buffer& buffer)
    {
        format_to_buffer_impl(buffer, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    virtual void format_c_to_buffer(fmt::memory_buffer& buffer)
    {
        format_c_to_buffer_impl(buffer, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    const char* value;
    bool need_free;
};

template <size_t N, typename FARGS> struct SFormatElement;

template <typename T, typename... Rest>
struct SFormatElement<0, SFormatArgs<T, Rest...>>
{
    typedef T type;
    typedef SFormatArgs<T, Rest...> SFormatType;
};

template <typename T, typename... Rest>
struct SFormatElement<0, SFormatArgs<T*, Rest...>>
{
    typedef void* type;
    typedef SFormatArgs<void*, Rest...> SFormatType;
};

template <typename... Rest>
struct SFormatElement<0, SFormatArgs<std::string, Rest...>>
{
    typedef const char* type;
    typedef SFormatArgs<const char*, Rest...> SFormatType;
};

template <typename... Rest>
struct SFormatElement<0, SFormatArgs<char*, Rest...>>
{
    typedef const char* type;
    typedef SFormatArgs<const char*, Rest...> SFormatType;
};

template <typename... Rest>
struct SFormatElement<0, SFormatArgs<const char*, Rest...>>
{
    typedef const char* type;
    typedef SFormatArgs<const char*, Rest...> SFormatType;
};

template <size_t N, typename T, typename... Rest>
struct SFormatElement<N, SFormatArgs<T, Rest...>>
    :public SFormatElement<N - 1, SFormatArgs<Rest...>> {};

template <size_t N, typename... Rest>
typename SFormatElement<N, SFormatArgs<Rest...>>::type& get(SFormatArgs<Rest...> &stp) {
    typedef typename SFormatElement<N, SFormatArgs<Rest...>>::SFormatType type;
    return ((type &)stp).value;
}

template<typename... Rest, size_t... I>
void format_to_buffer_impl(fmt::memory_buffer& buffer, SFormatArgs<Rest...> &args, idx_seq<I...>)
{
    fmt::format_to(buffer, get<I>(args)...);
}

inline void vprintf_to_buffer(fmt::memory_buffer& buffer, fmt::string_view format_str, fmt::printf_args args)
{
    fmt::printf(buffer, format_str, args);
}

template <typename... Args>
inline void printf_to_buffer(fmt::memory_buffer& buffer, fmt::string_view format_str, const Args & ... args)
{
    vprintf_to_buffer(buffer, format_str, fmt::make_printf_args(args...));
}

template<typename... Rest, size_t... I>
void format_c_to_buffer_impl(fmt::memory_buffer& buffer, SFormatArgs<Rest...> &args, idx_seq<I...>)
{
    printf_to_buffer(buffer, get<I>(args)...);
}

//////////////////////////////////////////////////////////////////////////
