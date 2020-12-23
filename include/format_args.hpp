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

using my_fmt_memory_buffer = fmt::basic_memory_buffer<char, 1024>;
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
extern HMEMORYMANAGER logger_mem_pool(void);

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

    char* copy_str(const char* str_ptr, size_t str_len)
    {
        char* ptr = 0;

        if (cache_use_size + str_len + 1 > sizeof(str_cache))
        {
            ptr = (char*)memory_manager_alloc(logger_mem_pool(), str_len + 1);
        }
        else
        {
            ptr = str_cache + cache_use_size;
        }

        memcpy(ptr, str_ptr, str_len);
        ptr[str_len] = 0;
        cache_use_size += (str_len + 1);
        return ptr;
    }

    void free_str(const char* str)
    {
        if (str > str_cache + sizeof(str_cache) ||
            str < str_cache)
        {
            memory_manager_free(logger_mem_pool(), (void*)str);
        }
    }

    virtual void format_to_buffer(my_fmt_memory_buffer& buffer) = 0;
    virtual void format_c_to_buffer(my_fmt_memory_buffer& buffer) = 0;
};

template <typename First, typename... Rest>
struct SFormatArgs<First, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(First&& first, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(std::forward<First>(first)){}

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
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(std::forward<First*>(first)){}

    void* value;
};

template <typename... Rest>
struct SFormatArgs<std::string, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(const std::string& str, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(SFormatArgs<>::copy_str(str.c_str(), str.length())){}

    ~SFormatArgs()
    {
        SFormatArgs<>::free_str(value);
    }

    const char* value;
};

template <typename... Rest>
struct SFormatArgs<char*, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}

    SFormatArgs(const char* cstr, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(SFormatArgs<>::copy_str(cstr, strlen(cstr))){}

    ~SFormatArgs()
    {
        SFormatArgs<>::free_str(value);
    }

    const char* value;
};

template <typename... Rest>
struct SFormatArgs<const char*, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(const char* cstr, Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(SFormatArgs<>::copy_str(cstr, strlen(cstr))){}

    ~SFormatArgs()
    {
        SFormatArgs<>::free_str(value);
    }

    const char* value;
};

template <size_t N, typename... Rest>
struct SFormatArgs<char(&)[N], Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(char(&fmt)[N], Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(SFormatArgs<>::copy_str(fmt, strnlen(fmt, N))) {}

    ~SFormatArgs()
    {
        SFormatArgs<>::free_str(value);
    }

    const char* value;
};

template <size_t N, typename... Rest>
struct SFormatArgs<const char(&)[N], Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value(0) {}
    SFormatArgs(const char (&fmt)[N], Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...),
        value(SFormatArgs<>::copy_str(fmt, N-1)) {}

    ~SFormatArgs()
    {
        SFormatArgs<>::free_str(value);
    }

    virtual void format_to_buffer(my_fmt_memory_buffer& buffer)
    {
        try
        {
            format_to_buffer_impl(buffer, *this, idx_seq_type<sizeof...(Rest) + 1>());
        }
        catch (const fmt::format_error& e)
        {
            buffer.clear();
            fmt::format_to(buffer, u8"fmt::error=[{}] data=[{}]", e.what(), value);
        }
        catch (...)
        {
            buffer.clear();
            fmt::format_to(buffer, u8"fmt::error=[unknow] data=[{}]", value);
        }
    }

    virtual void format_c_to_buffer(my_fmt_memory_buffer& buffer)
    {
        try
        {
            format_c_to_buffer_impl(buffer, *this, idx_seq_type<sizeof...(Rest) + 1>());
        }
        catch (const fmt::format_error& e)
        {
            buffer.clear();
            fmt::format_to(buffer, u8"fmt::error=[{}] data=[{}]", e.what(), value);
        }
        catch (...)
        {
            buffer.clear();
            fmt::format_to(buffer, u8"fmt::error=[unknow] data=[{}]", value);
        }
    }

    const char* value;
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

template <size_t N, typename... Rest>
struct SFormatElement<0, SFormatArgs<const char(&)[N], Rest...>>
{
    typedef const char* type;
    typedef SFormatArgs<const char*, Rest...> SFormatType;
};

template <size_t N, typename T, typename... Rest>
struct SFormatElement<N, SFormatArgs<T, Rest...>>
    :public SFormatElement<N - 1, SFormatArgs<Rest...>> {};

template <size_t N, typename... Rest>
typename SFormatElement<N, SFormatArgs<Rest...>>::type& SFormatArgsGet(SFormatArgs<Rest...> &stp) {
    typedef typename SFormatElement<N, SFormatArgs<Rest...>>::SFormatType type;
    return ((type &)stp).value;
}

template<typename... Rest, size_t... I>
void format_to_buffer_impl(my_fmt_memory_buffer& buffer, SFormatArgs<Rest...> &args, idx_seq<I...>)
{
    fmt::format_to(buffer, SFormatArgsGet<I>(args)...);
}

inline void vprintf_to_buffer(my_fmt_memory_buffer& buffer, fmt::string_view format_str, fmt::printf_args args)
{
    fmt::vprintf(buffer, format_str, args);
}

template <typename... Args>
inline void printf_to_buffer(my_fmt_memory_buffer& buffer, fmt::string_view format_str, const Args & ... args)
{
    vprintf_to_buffer(buffer, format_str, fmt::make_printf_args(args...));
}

template<typename... Rest, size_t... I>
void format_c_to_buffer_impl(my_fmt_memory_buffer& buffer, SFormatArgs<Rest...> &args, idx_seq<I...>)
{
    printf_to_buffer(buffer, SFormatArgsGet<I>(args)...);
}

//////////////////////////////////////////////////////////////////////////
