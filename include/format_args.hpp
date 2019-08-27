#pragma once

#include <typeinfo>
#include <string.h>
#include "./smemory.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/format-inl.h>
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
    virtual void format_to_string(std::string& format_string) = 0;
};

template <typename First, typename... Rest>
struct SFormatArgs<First, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value() {}
    SFormatArgs(First&& first, Rest&&... rest)
        :value(std::forward<First>(first)),
        SFormatArgs<Rest...>(std::forward<Rest>(rest)...) {}

    size_t size() { return sizeof...(Rest); }

    typename std::enable_if<std::is_arithmetic<First>::value, First>::type value;

};

template <typename... Rest>
struct SFormatArgs<std::string&, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value("") {}
    SFormatArgs(const std::string& str, Rest&&... rest)
        :value(str),
        SFormatArgs<Rest...>(std::forward<Rest>(rest)...) {}

    virtual void format_to_string(std::string& format_string)
    {
        format_impl(format_string, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    std::string value;
};

template <typename... Rest>
struct SFormatArgs<const std::string&, Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() :value("") {}
    SFormatArgs(const std::string& str, Rest&&... rest)
        :value(str),
        SFormatArgs<Rest...>(std::forward<Rest>(rest)...) {}

    virtual void format_to_string(std::string& format_string)
    {
        format_impl(format_string, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    std::string value;
};

template <size_t N, typename... Rest>
struct SFormatArgs<const char (&)[N], Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() {}
    SFormatArgs(char const (&c_str)[N], Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...)
    {
        memcpy(value, c_str, N);
    }

    virtual void format_to_string(std::string& format_string)
    {
        format_impl(format_string, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    char value[N];
};

template <size_t N, typename... Rest>
struct SFormatArgs<char(&)[N], Rest...>
    :public SFormatArgs<Rest...>
{
    SFormatArgs() {}
    SFormatArgs(char const (&c_str)[N], Rest&&... rest)
        :SFormatArgs<Rest...>(std::forward<Rest>(rest)...)
    {
        memcpy(value, c_str, N);
    }

    virtual void format_to_string(std::string& format_string)
    {
        format_impl(format_string, *this, idx_seq_type<sizeof...(Rest) + 1>());
    }

    char value[N];
};

template <size_t N, typename FARGS> struct SFormatElement;

template <typename T, typename... Rest>
struct SFormatElement<0, SFormatArgs<T, Rest...>>
{
    typedef T type;
    typedef SFormatArgs<T, Rest...> SFormatType;
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
void format_impl(std::string& format_string, SFormatArgs<Rest...> &args, idx_seq<I...>)
{
    format_string = fmt::format(get<I>(args)...);
}

//////////////////////////////////////////////////////////////////////////

template <class... Types>
auto new_format_args(Types&&... args)
{
    return S_NEW(SFormatArgs<Types...>, 1, std::forward<Types>(args)...);
}