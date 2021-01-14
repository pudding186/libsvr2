#pragma once

#include <string>
#include <vector>
#include <map>
#include <assert.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>
#include "./rb_tree.h"

extern "C"
{
#include "client_mysql.h"
#include "lib_svr_def.h"
}

#include "smemory.hpp"

#define MAX_ESCAPE_CACHE_SIZE   1024*128
//////////////////////////////////////////////////////////////////////////
using DataState = enum EDataState
{
    data_state_init = 0,
    data_state_load,
    data_state_update,
};


class IField
{
public:
    DataState m_state;
    IField() :m_state(data_state_init) {}
    virtual const std::string& GetColumnName(void) const = 0;
    virtual const std::string& GetColumnType(void) const = 0;
    virtual const std::string& GetColumnExtra(void) const = 0;
    virtual const std::string& GetColumnComment(void) const = 0;
    virtual void FromSQL(CLIENTMYSQLVALUE client_mysql_value) = 0;
    virtual std::string ToSQL(HCLIENTMYSQL client_mysql) = 0;
    virtual IField* Clone() const = 0;
    static const std::string& ColumnNull(void) { static std::string column_null = u8""; return column_null; }
protected:
private:
    
};

template<typename T>
class FieldINT
    :public IField
{
public:
    FieldINT() : m_data(0) {}
    FieldINT(T data) :m_data(data) {}

    FieldINT(const FieldINT<T>& rhs) :m_data(rhs.m_data) {}
    FieldINT(FieldINT<T>&& rhs) :m_data(std::move(rhs.m_data)) {}

    static const std::string& ColumnExtra(void)
    {
        static std::string column_attr = u8"NOT NULL DEFAULT '0'";
        return column_attr;
    }

    inline T GetData(void) { return m_data; }
    void SetData(T data, bool is_from_db = false)
    {
        switch (m_state)
        {
        case data_state_init:
        {
            if (is_from_db)
            {
                m_state = data_state_load;
            }
            else
            {
                m_state = data_state_update;
            }

            m_data = data;
        }
        break;
        case data_state_load:
        {
            if (m_data != data)
            {
                m_data = data;

                if (!is_from_db)
                {
                    m_state = data_state_update;
                }
            }
        }
        break;
        case data_state_update:
        {
            if (m_data != data)
            {
                if (is_from_db)
                {
                    assert(false);
                }
                else
                {
                    m_data = data;
                }
            }
        }
        break;
        }
    }

    bool operator < (const FieldINT<T>& other) const
    {
        return m_data < other.m_data;
    }

    bool operator > (const FieldINT<T>& other) const
    {
        return m_data > other.m_data;
    }

    FieldINT<T>& operator=(const FieldINT<T>& rhs)
    {
        if (this != &rhs)
        {
            m_data = rhs.m_data;
        }

        return *this;
    }

    FieldINT<T>& operator=(FieldINT<T>&& rhs)
    {
        if (this != &rhs)
        {
            m_data = rhs.m_data;
        }

        return *this;
    }

    std::string ToSQL(HCLIENTMYSQL client_mysql) override
    {
        client_mysql = client_mysql;
        return fmt::format("{:d}", m_data);
    }
protected:
private:
    typename std::enable_if<std::is_integral<T>::value, T>::type m_data;
};

class FieldTXT
    :public IField
{
public:
    FieldTXT() :m_data("") {}
    FieldTXT(const std::string& data) :m_data(data) {}
    FieldTXT(const char* data) :m_data(std::string(data, strlen(data))) {}
    template<size_t N>
    FieldTXT(const char(&data)[N]) :m_data(std::string(data, strnlen(data, N))) {}

    FieldTXT(const FieldTXT& rhs) :m_data(rhs.m_data) {}
    FieldTXT(FieldTXT&& rhs) :m_data(std::move(rhs.m_data)) {}

    static const std::string& ColumnExtra(void)
    {
        static std::string column_attr = u8"NOT NULL";
        return column_attr;
    }

    inline std::string GetData(void) { return m_data; }
    void SetData(const std::string& data, bool is_from_db = false)
    {
        switch (m_state)
        {
        case data_state_init:
        {
            if (is_from_db)
            {
                m_state = data_state_load;
            }
            else
            {
                m_state = data_state_update;
            }

            m_data = data;
        }
        break;
        case data_state_load:
        {
            if (m_data != data)
            {
                m_data = data;

                if (!is_from_db)
                {
                    m_state = data_state_update;
                }
            }
        }
        break;
        case data_state_update:
        {
            if (m_data != data)
            {
                if (is_from_db)
                {
                    assert(false);
                }
                else
                {
                    m_data = data;
                }
            }
        }
        break;
        }
    }

    bool operator < (const FieldTXT& other) const
    {
        return m_data < other.m_data;
    }

    bool operator > (const FieldTXT& other) const
    {
        return m_data > other.m_data;
    }

    FieldTXT& operator=(const FieldTXT& rhs)
    {
        if (this != &rhs)
        {
            m_data = rhs.m_data;
        }

        return *this;
    }

    FieldTXT& operator=(FieldTXT&& rhs)
    {
        if (this != &rhs)
        {
            m_data = std::move(rhs.m_data);
        }

        return *this;
    }

    std::string ToSQL(HCLIENTMYSQL client_mysql) override
    {
        char buf[MAX_ESCAPE_CACHE_SIZE];
        char* cache = buf;
        unsigned long cache_size = MAX_ESCAPE_CACHE_SIZE;

        if ((m_data.length() * 2 + 1) > MAX_ESCAPE_CACHE_SIZE)
        {
            cache_size = static_cast<unsigned long>(m_data.length() * 2 + 2);
            cache = S_NEW(char, cache_size);
        }

        unsigned long escape_length = client_mysql_escape_string
        (
            client_mysql,
            m_data.c_str(),
            static_cast<unsigned long>(m_data.length()),
            cache,
            cache_size
            );

        std::string escape_str = "'" + std::string(cache, escape_length) + "'";

        if (cache != buf)
        {
            S_DELETE(cache);
            cache = nullptr;
        }

        return escape_str;
    }

protected:
private:
    std::string m_data;
};

class FieldINT8
    :public FieldINT<char>
{
public:
    FieldINT8() :FieldINT<char>(0) {}
    FieldINT8(char data) :FieldINT<char>(data) {}
    static const std::string& ColumnType(void)
    { 
        static std::string column_type = u8"tinyint(4)"; 
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<char>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_int8(client_mysql_value));
    }
};

class FieldUINT8
    :public FieldINT<unsigned char>
{
public:
    FieldUINT8() :FieldINT<unsigned char>(0) {}
    FieldUINT8(unsigned char data) :FieldINT<unsigned char>(data) {}

    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"tinyint(4) unsigned";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<unsigned char>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_uint8(client_mysql_value));
    }
};

class FieldINT16
    :public FieldINT<short>
{
public:
    FieldINT16() :FieldINT<short>(0) {}
    FieldINT16(short data) :FieldINT<short>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"smallint(6)";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<short>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_int16(client_mysql_value));
    }
};

class FieldUINT16
    :public FieldINT<unsigned short>
{
public:
    FieldUINT16() :FieldINT<unsigned short>(0) {}
    FieldUINT16(unsigned short data) :FieldINT<unsigned short>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"smallint(6) unsigned";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<unsigned short>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_uint16(client_mysql_value));
    }
};

class FieldINT32
    :public FieldINT<int>
{
public:
    FieldINT32() :FieldINT<int>(0) {}
    FieldINT32(int data) :FieldINT<int>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"int(11)";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<int>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_int32(client_mysql_value));
    }
};

class FieldUINT32
    :public FieldINT<unsigned int>
{
public:
    FieldUINT32() :FieldINT<unsigned int>(0) {}
    FieldUINT32(unsigned int data) :FieldINT<unsigned int>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"int(11) unsigned";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<unsigned int>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_uint32(client_mysql_value));
    }
};

class FieldINT64
    :public FieldINT<long long>
{
public:
    FieldINT64() :FieldINT<long long>(0) {}
    FieldINT64(long long data) :FieldINT<long long>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"bigint(20)";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<long long>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_int64(client_mysql_value));
    }
};

class FieldUINT64
    :public FieldINT<unsigned long long>
{
public:
    FieldUINT64() :FieldINT<unsigned long long>(0) {}
    FieldUINT64(unsigned long long data) :FieldINT<unsigned long long>(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"bigint(20) unsigned";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldINT<unsigned long long>::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(client_mysql_value_uint64(client_mysql_value));
    }
};

class FieldText
    :public FieldTXT
{
public:
    FieldText() :FieldTXT("") {}
    FieldText(const std::string& data) :FieldTXT(data) {}
    FieldText(const char* data) :FieldTXT(data) {}
    template<size_t N>
    FieldText(const char(&data)[N]) : FieldTXT(data) {};
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"text";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldTXT::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(std::string(client_mysql_value.value, client_mysql_value.size));
    }
};

class FieldLongText
    :public FieldTXT
{
public:
    FieldLongText() :FieldTXT(""){}
    FieldLongText(const std::string& data) :FieldTXT(data) {}
    FieldLongText(const char* data) :FieldTXT(data) {}
    template<size_t N>
    FieldLongText(const char(&data)[N]) : FieldTXT(data) {};
    static const std::string& ColumnType(void)
    {
        static std::string column_type = u8"longtext";
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        return FieldTXT::ColumnExtra();
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(std::string(client_mysql_value.value, client_mysql_value.size));
    }
};


template <size_t N>
class FieldVarChar
    :public FieldTXT
{
public:
    FieldVarChar() :FieldTXT(""){}
    FieldVarChar(const std::string& data) :FieldTXT(data) {}
    FieldVarChar(const char* data) :FieldTXT(data) {}
    template<size_t M>
    FieldVarChar(const char(&data)[M]) : FieldTXT(data) {}
    static const std::string& ColumnType(void)
    {
        static std::string column_type =
            fmt::format(u8"varchar({})", N);
        return column_type;
    }

    static const std::string& ColumnExtra(void)
    {
        static std::string column_attr = u8"COLLATE utf8_bin NOT NULL";
        return column_attr;
    }

    void FromSQL(CLIENTMYSQLVALUE client_mysql_value) override
    {
        SetData(std::string(client_mysql_value.value, client_mysql_value.size));
    }
};

//////////////////////////////////////////////////////////////////////////

template <typename... Args>
struct SFieldList {};

template<>
struct SFieldList<>
{
    SFieldList(){}
    ~SFieldList(){}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4100 )
#elif __GNUC__

#else
#error "unknown compiler"
#endif

    ptrdiff_t Compare(const SFieldList<>& other) const
    {
        return 0;
    }

    virtual std::string ListSQL(size_t idx = 0)
    {
        return "";
    }

    virtual std::string ListNameSQL(size_t idx = 0)
    {
        return "";
    }

    virtual std::string SetSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0)
    {
        return "";
    }

    virtual std::string AndSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0)
    {
        return "";
    }

    virtual std::string OrSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0)
    {
        return "";
    }

    virtual std::string ValueSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0)
    {
        return "";
    }

    virtual std::string CreateSQL(void)
    {
        return "";
    }

    virtual void GetDesc(std::vector<std::string>& name_type_list)
    {

    }

    virtual size_t size() const
    {
        return 0;
    }

    virtual void LoadData(const CLIENTMYSQLROW& row, unsigned long idx = 0)
    {

    }

#ifdef _MSC_VER
#pragma warning( pop )
#elif __GNUC__

#else
#error "unknown compiler"
#endif
};

template <typename First, typename... Rest>
struct SFieldList<First, Rest...>
    :public SFieldList<Rest...>
{
    SFieldList(){}
    SFieldList(First&& first, Rest&&... rest)
        :SFieldList<Rest...>(std::forward<Rest>(rest)...),
        value(std::forward<First>(first)) {}

    ptrdiff_t Compare(const SFieldList<First, Rest...>& other) const
    {
        if (value < other.value)
        {
            return -1;
        }
        else if (value > other.value)
        {
            return 1;
        }
        else
        {
            return SFieldList<Rest...>::Compare(static_cast<const SFieldList<Rest...>&>(other));
        }
    }

    std::string ListSQL(size_t idx = 0) override
    {
        std::string sql;

        if (idx == 0)
        {
            sql += u8" ";
        }
        else
        {
            sql += u8", ";
        }

        sql += u8"`" + First::ColumnName() + u8"`";

        return sql += SFieldList<Rest...>::ListSQL(idx + 1);
    }

    std::string ListNameSQL(size_t idx = 0) override
    {
        std::string sql;

        if (idx == 0)
        {
            sql += u8" ";
        }
        else
        {
            sql += u8", ";
        }

        sql += u8"`" + First::ColumnName() + u8"`";

        return sql += SFieldList<Rest...>::ListNameSQL(idx + 1);
    }

    std::string SetSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        if (idx)
        {
            sql += u8", ";
        }
        
        sql += u8"`" + First::ColumnName() + u8"`";
        sql += u8" = ";
        sql += value.ToSQL(client_mysql);

        return sql += SFieldList<Rest...>::SetSQL(client_mysql, idx + 1);
    }

    std::string AndSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        if (idx)
        {
            sql += u8" AND ";
        }

        sql += u8"`" + First::ColumnName() + u8"`";
        sql += " = ";
        sql += value.ToSQL(client_mysql);

        return sql += SFieldList<Rest...>::AndSQL(client_mysql, idx + 1);
    }

    std::string OrSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        if (idx)
        {
            sql += u8" OR ";
        }

        sql += u8"`" + First::ColumnName() + u8"`";
        sql += u8" = ";
        sql += value.ToSQL(client_mysql);

        return sql += SFieldList<Rest...>::OrSQL(client_mysql, idx + 1);
    }

    std::string ValueSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        if (idx)
        {
            sql += u8", ";
        }

        sql += value.ToSQL(client_mysql);

        return sql += SFieldList<Rest...>::ValueSQL(client_mysql, idx + 1);
    }

    std::string CreateSQL(void) override
    {
        std::string sql;

        sql +=
            u8"`" +
            First::ColumnName() +
            u8"` " +
            First::ColumnType() +
            u8" " +
            First::ColumnExtra() +
            u8" ";
            if (First::ColumnComment().length())
            {
                sql += u8"COMMENT \"";
                sql += First::ColumnComment();
                sql += u8"\",";
            }
            else
            {
                sql += u8",";
            }
            
            

        return sql += SFieldList<Rest...>::CreateSQL();
    }

    void GetDesc(std::vector<std::string>& name_type_list)
    {
        name_type_list.push_back(First::ColumnName());
        name_type_list.push_back(First::ColumnType());
        name_type_list.push_back(First::ColumnExtra());

        SFieldList<Rest...>::GetDesc(name_type_list);
    }

    virtual size_t size() const override
    {
        return sizeof...(Rest) + 1;
    }

    void LoadData(const CLIENTMYSQLROW& row, unsigned long idx = 0) override
    {
        value.FromSQL(client_mysql_value(row, idx));
        SFieldList<Rest...>::LoadData(row, idx + 1);
    }

    template<typename F>
    F& GetField(void)
    {
        return GetFieldByType<F>(*this);
    }

    typename std::enable_if<
        std::is_base_of<FieldUINT8, First>::value
        || std::is_base_of<FieldINT8, First>::value
        || std::is_base_of<FieldUINT16, First>::value
        || std::is_base_of<FieldINT16, First>::value
        || std::is_base_of<FieldUINT32, First>::value
        || std::is_base_of<FieldINT32, First>::value
        || std::is_base_of<FieldUINT64, First>::value
        || std::is_base_of<FieldINT64, First>::value
        || std::is_base_of<FieldTXT, First>::value
        , First>::type value;
};

struct SDynaFieldList
    :public SFieldList<>
{
    SDynaFieldList()
    {
        m_dyna_fields = create_rb_tree(0);
    }

    SDynaFieldList(const SDynaFieldList& rhs)
    {
        m_dyna_fields = create_rb_tree(0);

        HRBNODE rhs_node = rb_first(rhs.m_dyna_fields);

        while (rhs_node)
        {
            IField* field = (IField*)rb_node_value_user(rhs_node);
            rb_tree_insert_user(m_dyna_fields, field->GetColumnName().c_str(), field->Clone());
            rhs_node = rb_next(rhs_node);
        }
    }

    ~SDynaFieldList()
    {
        if (m_dyna_fields)
        {
            HRBNODE node = rb_first(m_dyna_fields);
            while (node)
            {
                S_DELETE(rb_node_value_user(node));
                node = rb_next(node);
            }

            destroy_rb_tree(m_dyna_fields);
            m_dyna_fields = nullptr;
        }
    }

    template<typename T>
    typename std::enable_if<std::is_base_of<FieldUINT8, T>::value
        || std::is_base_of<FieldINT8, T>::value
        || std::is_base_of<FieldUINT16, T>::value
        || std::is_base_of<FieldINT16, T>::value
        || std::is_base_of<FieldUINT32, T>::value
        || std::is_base_of<FieldINT32, T>::value
        || std::is_base_of<FieldUINT64, T>::value
        || std::is_base_of<FieldINT64, T>::value
        || std::is_base_of<FieldTXT, T>::value, bool>::type AddField(const T& field)
    {
        HRBNODE node = nullptr;
        IField* new_field = S_NEW(T, 1, field);

        if (rb_tree_try_insert_user(m_dyna_fields, new_field->GetColumnName().c_str(), new_field, &node))
        {
            return true;
        }

        S_DELETE(new_field);

        return false;
    }

    template<typename T>
    typename std::enable_if<std::is_base_of<FieldUINT8, T>::value
        || std::is_base_of<FieldINT8, T>::value
        || std::is_base_of<FieldUINT16, T>::value
        || std::is_base_of<FieldINT16, T>::value
        || std::is_base_of<FieldUINT32, T>::value
        || std::is_base_of<FieldINT32, T>::value
        || std::is_base_of<FieldUINT64, T>::value
        || std::is_base_of<FieldINT64, T>::value
        || std::is_base_of<FieldTXT, T>::value>::type DelField()
    {
        T del_field;

        HRBNODE node = rb_tree_find_user(m_dyna_fields, del_field.GetColumnName().c_str());

        if (node)
        {
            S_DELETE(rb_node_value_user(node));
            rb_tree_erase(m_dyna_fields, node);
        }
    }

    void Clear(void)
    {
        HRBNODE node = rb_first(m_dyna_fields);
        while (node)
        {
            S_DELETE(rb_node_value_user(node));
            node = rb_next(node);
        }

        rb_tree_clear(m_dyna_fields);
    }

    std::string ListSQL(size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            if (idx == 0)
            {
                sql += u8" ";
                idx++;
            }
            else
            {
                sql += u8", ";
            }
            IField* field = (IField*)rb_node_value_user(node);

            sql += u8"`" + field->GetColumnName() + u8"`";

            node = rb_next(node);
        }

        return sql;
    }

    std::string ListNameSQL(size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            if (idx == 0)
            {
                sql += u8" ";
                idx++;
            }
            else
            {
                sql += u8", ";
            }
            IField* field = (IField*)rb_node_value_user(node);

            sql += u8"`" + field->GetColumnName() + u8"`";

            node = rb_next(node);
        }

        return sql;
    }

    std::string SetSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            if (idx)
            {
                sql += u8", ";
            }

            sql += u8"`" + field->GetColumnName() + u8"`";
            sql += u8" = ";
            sql += field->ToSQL(client_mysql);

            node = rb_next(node);
            ++idx;
        }

        return sql;
    }

    std::string AndSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            if (idx)
            {
                sql += u8" AND ";
            }

            sql += u8"`" + field->GetColumnName() + u8"`";
            sql += " = ";
            sql += field->ToSQL(client_mysql);

            node = rb_next(node);
            ++idx;
        }

        return sql;
    }

    std::string OrSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            if (idx)
            {
                sql += u8" OR ";
            }

            sql += u8"`" + field->GetColumnName() + u8"`";
            sql += " = ";
            sql += field->ToSQL(client_mysql);

            node = rb_next(node);
            ++idx;
        }

        return sql;
    }

    std::string ValueSQL(HCLIENTMYSQL& client_mysql, size_t idx = 0) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            if (idx)
            {
                sql += u8", ";
            }

            sql += field->ToSQL(client_mysql);

            node = rb_next(node);
            ++idx;
        }

        return sql;
    }

    std::string CreateSQL(void) override
    {
        std::string sql;

        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            sql +=
                u8"`" +
                field->GetColumnName() +
                u8"` " +
                field->GetColumnType() +
                u8" " +
                field->GetColumnExtra() +
                u8" ";
            if (field->GetColumnComment().length())
            {
                sql += u8"COMMENT \"";
                sql += field->GetColumnComment();
                sql += u8"\",";
            }
            else
            {
                sql += u8",";
            }

            node = rb_next(node);
        }

        return sql;
    }

    void GetDesc(std::vector<std::string>& name_type_list)
    {
        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);

            name_type_list.push_back(field->GetColumnName());
            name_type_list.push_back(field->GetColumnType());
            name_type_list.push_back(field->GetColumnExtra());

            node = rb_next(node);
        }
    }

    virtual size_t size() const override
    {
        return rb_tree_size(m_dyna_fields);
    }

    void LoadData(const CLIENTMYSQLROW& row, unsigned long idx = 0) override
    {
        HRBNODE node = rb_first(m_dyna_fields);

        while (node)
        {
            IField* field = (IField*)rb_node_value_user(node);
            field->FromSQL(client_mysql_value(row, idx));
            node = rb_next(node);
            idx++;
        }
    }


    HRBTREE m_dyna_fields;


};


//////////////////////////////////////////////////////////////////////////
template <size_t N, typename FARGS> struct SFieldListElement;

template <typename T, typename... Rest>
struct SFieldListElement<0, SFieldList<T, Rest...>>
{
    typedef T type;
    typedef SFieldList<T, Rest...> SFieldListType;
};

template <size_t N, typename T, typename... Rest>
struct SFieldListElement<N, SFieldList<T, Rest...>>
    :public SFieldListElement<N - 1, SFieldList<Rest...>> {};

template <size_t N, typename... Rest>
typename SFieldListElement<N, SFieldList<Rest...>>::type& GetFieldByIndex(const SFieldList<Rest...>& stp) {
    typedef typename SFieldListElement<N, SFieldList<Rest...>>::SFieldListType type;
    return ((type&)stp).value;
}

template <typename T, std::size_t N, typename... Args>
struct indexOf;

template <typename T, std::size_t N, typename... Args>
struct indexOf<T, N, T, Args...>
{
    enum { value = N };
};

template <typename T, std::size_t N, typename U, typename... Args>
struct indexOf<T, N, U, Args...>
{
    enum { value = indexOf<T, N + 1, Args...>::value };
};

template <typename T, std::size_t N>
struct indexOf<T, N>
{
    enum { value = -1 };
    static_assert(value != -1, "the type is not exist");
};

template <typename T, typename... Args>
T& GetFieldByType(const SFieldList<Args...>& t)
{
    return GetFieldByIndex<indexOf<T, 0, Args...>::value>(t);
}

template <typename T>
ptrdiff_t FieldListCompare(const void* key1, const void* key2)
{
    const T* d1 = static_cast<const T*>(key1);
    const T* d2 = static_cast<const T*>(key2);

    return d1->Compare(*d2);
}


//////////////////////////////////////////////////////////////////////////
class IResult
{
public:
    IResult():m_err_msg(""),m_wrn_msg(""){}
    virtual void OnCall(void) = 0;
    virtual void OnResult(CLIENTMYSQLRES& res) = 0;
    inline void SetError(const std::string& err_msg) { m_err_msg = err_msg; }
    inline const std::string& GetError(void) { return m_err_msg; }
    inline void SetWarn(const std::string& wrn_msg) { m_wrn_msg = wrn_msg; }
    inline const std::string& GetWarn(void) { return m_wrn_msg; }
protected:
    std::string m_err_msg;
    std::string m_wrn_msg;
};
template <typename... Fields>
class RecordResult
    :public IResult
{
public:
    RecordResult(const std::function<void(bool, const std::vector<SFieldList<Fields...>>&)>& func)
        :m_func(func)
    {
        m_datas.clear();
    }

    void OnCall(void) override
    {
        if (m_func)
        {
            m_func(m_err_msg.empty(), m_datas);
        }
    }

    void OnResult(CLIENTMYSQLRES& res) override
    {
        m_err_msg = "";

        m_datas.resize(client_mysql_rows_num(&res));

        for (auto& field : m_datas)
        {
            field.LoadData(client_mysql_fetch_row(&res));
        }
    }

    std::vector<SFieldList<Fields...>> MoveData(void)
    {
        return std::move(m_datas);
    }

protected:
private:
    std::vector<SFieldList<Fields...>> m_datas;
    std::function<void(bool, std::vector<SFieldList<Fields...>>&)> m_func;
};

class AffectResult
    :public IResult
{
public:
    AffectResult(const std::function<void(bool, unsigned long long affect_row)>& func)
        :m_affect_row(0), m_func(func){}

    void OnCall(void) override
    {
        if (m_func)
        {
            m_func(m_err_msg.empty(), m_affect_row);
        }
    }

    void OnResult(CLIENTMYSQLRES& res) override
    {
        m_affect_row = client_mysql_result_affected(&res);
    }

    unsigned long long GetAffectRow(void)
    {
        return m_affect_row;
    }

protected:
private:
    unsigned long long m_affect_row;
    std::function<void(bool, unsigned long long)> m_func;
};

//////////////////////////////////////////////////////////////////////////
class ITable
{
public:
    ITable(HMYSQLCONNECTION connection)
        :m_connection(connection) {}
public:
    inline HMYSQLCONNECTION Connection(void) { return m_connection; }
    static const std::string& TableNull(void) { static std::string table_null = u8""; return table_null; }
    static const std::string& TableExtra(void) { static std::string table_extra = u8""; return table_extra; }
    static const std::string& DefTableEngine(void) { static std::string table_engine = u8"InnoDB"; return table_engine; }
    static const std::string& DefTableCharset(void) { static std::string table_charset = u8"utf8"; return table_charset; }
    static const std::string& DefTableRowFormat(void) { static std::string table_row_format = u8"compact"; return table_row_format; }
    virtual const std::string& GetTableName(void) const = 0;
    virtual const std::string& GetTableExtra(void) const = 0;
    virtual const std::string& GetTableEngine(void) const = 0;
    virtual const std::string& GetTableCharset(void) const = 0;
    virtual const std::string& GetTableRowFormat(void) const = 0;
    virtual SFieldList<>* FieldList(void) = 0;
    virtual SFieldList<>* PrimaryKey(void) = 0;
    virtual std::map<std::string, SFieldList<>*> UniqueKey(void) = 0;
    virtual std::map<std::string, SFieldList<>*> IndexKey(void) = 0;
protected:
private:
    HMYSQLCONNECTION   m_connection;
};