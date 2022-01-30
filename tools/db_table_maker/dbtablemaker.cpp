#include <vector>
#include <map>
#include <set>

//#include "utility.hpp"
#include "markupstl.h"
#include "dbtablemaker.hpp"

#define FIND_ELEM(elem)             oXml.ResetMainPos();\
                                    if (!oXml.FindElem(elem))\
                                    {\
                                        m_strErrorInfo = "Can't Find Element <";\
                                        m_strErrorInfo += elem;\
                                        m_strErrorInfo += "> Error= ";\
                                        m_strErrorInfo += oXml.GetError();\
                                        return m_strErrorInfo;\
                                    }

#define GET_ATTRIB(attrib)          strVal = oXml.GetAttrib(attrib);\
                                    if (strVal.empty())\
                                    {\
                                        m_strErrorInfo = "Can't Find Attrib=<";\
                                        m_strErrorInfo += attrib;\
                                        m_strErrorInfo += "> Error= "; \
                                        m_strErrorInfo += oXml.GetError();\
                                        return m_strErrorInfo;\
                                    }

#define INTO_ELEM()                 if (!oXml.IntoElem())\
                                    {\
                                        m_strErrorInfo=u8"into element fail";\
                                        return m_strErrorInfo;\
                                    }
                                        

#define OUTOF_ELEM()                if (!oXml.OutOfElem())\
                                    {\
                                        m_strErrorInfo=u8"outof element fail";\
                                        return m_strErrorInfo;\
                                    }

typedef struct st_mem_seg
{
    const void* mem;
    size_t      mem_size;
}mem_seg;

const void* memmem_s(const void* haystack, size_t haystacklen,
    const void* needle, size_t needlelen)
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

DBTableMaker::DBTableMaker(void)
{

}

DBTableMaker::~DBTableMaker(void)
{

}

typedef struct st_column_info
{
    std::string m_name;
    std::string m_type;
    std::string m_length;
    std::string m_extra;
    std::string m_comment;
}column_info;

typedef struct st_key_info
{
    std::string m_name;
    std::string m_type;
    std::vector<std::string> m_field_list;
}key_info;

typedef std::map<std::string, column_info> column_map;
typedef std::vector<column_info> column_vector;

typedef std::map<std::string, key_info> key_map;
typedef std::vector<key_info> key_vector;

typedef struct st_table_info
{
    std::string m_name;
    std::string m_extra;
    std::string m_engine;
    std::string m_charset;
    std::string m_row_format;
    column_vector m_all_fields;

    key_vector m_key_primary;
    key_vector m_key_unique;
    key_vector m_key_index;

}table_info;

//std::string _WriteColumnClass(FILE* pHppFile, std::vector<column_info>& columns)
std::string _WriteColumnClass(FILE* pHppFile, table_info& table)
{
    std::vector<column_info>& columns = table.m_all_fields;

    for (size_t i = 0; i < columns.size(); i++)
    {
        column_info& info = columns[i];

        fprintf(pHppFile, u8"    class %s\r\n", info.m_name.c_str());
        if (info.m_type == u8"int8")
        {
            fprintf(pHppFile, u8"        :public FieldINT8\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(char data = 0) :FieldINT8(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"uint8")
        {
            fprintf(pHppFile, u8"        :public FieldUINT8\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(unsigned char data = 0) :FieldUINT8(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"int16")
        {
            fprintf(pHppFile, u8"        :public FieldINT16\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(short data = 0) :FieldINT16(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"uint16")
        {
            fprintf(pHppFile, u8"        :public FieldUINT16\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(unsigned short data = 0) :FieldUINT16(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"int32")
        {
            fprintf(pHppFile, u8"        :public FieldINT32\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(int data = 0) :FieldINT32(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"uint32")
        {
            fprintf(pHppFile, u8"        :public FieldUINT32\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(unsigned int data = 0) :FieldUINT32(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"int64")
        {
            fprintf(pHppFile, u8"        :public FieldINT64\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(long long data = 0) :FieldINT64(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"uint64")
        {
            fprintf(pHppFile, u8"        :public FieldUINT64\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(unsigned long long data = 0) :FieldUINT64(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"varchar")
        {

            long long test = _atoi64(info.m_length.c_str());

            if (test <= 0)
            {
                std::string err = u8"field=" + info.m_name + u8"length=" + info.m_length + u8"illegal!";
                return err;
            }

            fprintf(pHppFile, u8"        :public FieldVarChar<%s>\r\n", info.m_length.c_str());
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(const std::string& data = \"\") :FieldVarChar(data) {}\r\n", info.m_name.c_str());
            fprintf(pHppFile, u8"        template<size_t N>\r\n");
            fprintf(pHppFile, u8"        %s(const char(&data)[N]) : FieldVarChar(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"text")
        {
            fprintf(pHppFile, u8"        :public FieldText\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(const std::string& data = \"\") :FieldText(data) {}\r\n", info.m_name.c_str());
            fprintf(pHppFile, u8"        template<size_t N>\r\n");
            fprintf(pHppFile, u8"        %s(const char(&data)[N]) : FieldText(data) {}\r\n", info.m_name.c_str());
        }
        else if (info.m_type == u8"longtext")
        {
            fprintf(pHppFile, u8"        :public FieldLongText\r\n");
            fprintf(pHppFile, u8"    {\r\n");
            fprintf(pHppFile, u8"    public:\r\n");
            fprintf(pHppFile, u8"        %s(const std::string& data = \"\") :FieldLongText(data) {}\r\n", info.m_name.c_str());
            fprintf(pHppFile, u8"        template<size_t N>\r\n");
            fprintf(pHppFile, u8"        %s(const char(&data)[N]) : FieldLongText(data) {}\r\n", info.m_name.c_str());
        }
        else
        {
            std::string err = u8"field=" + info.m_name + u8"type=" + info.m_type + u8"unknow!";
            return err;
        }

        fprintf(pHppFile, u8"        static const std::string& ColumnName(void) { static std::string column_name = u8\"%s\"; return column_name; }\r\n", info.m_name.c_str());
        
        if (info.m_type == u8"int8")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldINT8::ColumnType(); }\r\n");

            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldINT8::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"uint8")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldUINT8::ColumnType(); }\r\n");
            
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldUINT8::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"int16")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldINT16::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldINT16::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"uint16")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldUINT16::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldUINT16::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"int32")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldINT32::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldINT32::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"uint32")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldUINT32::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldUINT32::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"int64")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldINT64::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldINT64::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"uint64")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldUINT64::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldUINT64::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"varchar")
        {

            long long test = _atoi64(info.m_length.c_str());

            if (test <= 0)
            {
                std::string err = u8"field=" + info.m_name + u8"length=" + info.m_length + u8"illegal!";
                return err;
            }

            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldVarChar<%s>::ColumnType(); }\r\n", info.m_length.c_str());

            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldVarChar<%s>::ColumnExtra(); }\r\n", info.m_length.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"text")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldText::ColumnType(); }\r\n");
            

            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldText::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else if (info.m_type == u8"longtext")
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnType(void) { return FieldLongText::ColumnType(); }\r\n");
            if (info.m_extra.empty())
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return FieldLongText::ColumnExtra(); }\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
            }
        }
        else
        {
            std::string err = u8"field=" + info.m_name + u8"type=" + info.m_type + u8"unknow!";
            return err;
        }

        //if (info.m_extra.empty())
        //{
        //    fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { return IField::ColumnNull(); }\r\n");
        //}
        //else
        //{
        //    fprintf(pHppFile, u8"        static const std::string& ColumnExtra(void) { static std::string column_extra = u8\"%s\"; return column_extra; }\r\n", info.m_extra.c_str());
        //}

        if (info.m_comment.empty())
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnComment(void) { return IField::ColumnNull(); }\r\n");
        }
        else
        {
            fprintf(pHppFile, u8"        static const std::string& ColumnComment(void) { static std::string column_comment = u8\"%s\"; return column_comment; }\r\n", info.m_comment.c_str());
        }

        fprintf(pHppFile, u8"        const std::string& GetColumnName(void) const override { return %s::ColumnName(); }\r\n", info.m_name.c_str());
        fprintf(pHppFile, u8"        const std::string& GetColumnType(void) const override { return %s::ColumnType(); }\r\n", info.m_name.c_str());
        fprintf(pHppFile, u8"        const std::string& GetColumnExtra(void) const override { return %s::ColumnExtra(); }\r\n", info.m_name.c_str());
        fprintf(pHppFile, u8"        const std::string& GetColumnComment(void) const override { return %s::ColumnComment(); }\r\n", info.m_name.c_str());


        fprintf(pHppFile, u8"        IField* Clone(void) const override { return S_NEW(%s, 1, *this); }\r\n", info.m_name.c_str());
        fprintf(pHppFile, u8"    };\r\n");
    }

    fprintf(pHppFile, u8"    using Row = SFieldList<\r\n");
    for (size_t i = 0; i < columns.size(); i++)
    {
        column_info& info = columns[i];

        if (i == columns.size()-1)
        {
            fprintf(pHppFile, u8"        %s::%s\r\n", table.m_name.c_str(), info.m_name.c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        %s::%s,\r\n", table.m_name.c_str(), info.m_name.c_str());
        }
    }
    fprintf(pHppFile, u8"    >;\r\n");

    fprintf(pHppFile, u8"    using PriKey = SFieldList<\r\n");
    key_info& primary_key = table.m_key_primary[0];
    for (size_t i = 0; i < primary_key.m_field_list.size(); i++)
    {
        if (i == primary_key.m_field_list.size() - 1)
        {
            fprintf(pHppFile, u8"        %s::%s\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        %s::%s,\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
    }
    fprintf(pHppFile, u8"    >;\r\n");
    
    return "";
}


std::string _WriteTableClass(FILE* pHppFile, table_info& table)
{
    std::string err = "";

    fprintf(pHppFile, u8"class %s\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"    :public ITable\r\n");
    fprintf(pHppFile, u8"{\r\n");
    fprintf(pHppFile, u8"public:\r\n");

    err = _WriteColumnClass(pHppFile, table);
    if (err != "")
    {
        return err;
    }

    fprintf(pHppFile, u8"    %s(HMYSQLCONNECTION connection)\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"        :ITable(connection){}\r\n\r\n");

    fprintf(pHppFile, u8"    static const std::string& TableName(void)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        static std::string table_name = u8\"%s\";\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"        return table_name;\r\n");
    fprintf(pHppFile, u8"    }\r\n\r\n");
    fprintf(pHppFile, u8"    static const std::string& TableEngine(void)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    if (table.m_engine.empty())
    {
        fprintf(pHppFile, u8"        return ITable::DefTableEngine();\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        static std::string table_engine = u8\"%s\";\r\n", table.m_engine.c_str());
        fprintf(pHppFile, u8"        return table_engine;\r\n");
    }
    fprintf(pHppFile, u8"    }\r\n\r\n");

    fprintf(pHppFile, u8"    static const std::string& TableCharset(void)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    if (table.m_charset.empty())
    {
        fprintf(pHppFile, u8"        return ITable::DefTableCharset();\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        static std::string table_charset = u8\"%s\";\r\n", table.m_charset.c_str());
        fprintf(pHppFile, u8"        return table_charset;\r\n");
    }
    fprintf(pHppFile, u8"    }\r\n\r\n");

    fprintf(pHppFile, u8"    static const std::string& TableRowFormat(void)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    if (table.m_row_format.empty())
    {
        fprintf(pHppFile, u8"        return ITable::DefTableRowFormat();\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        static std::string table_row_format = u8\"%s\";\r\n", table.m_row_format.c_str());
        fprintf(pHppFile, u8"        return table_row_format;\r\n");
    }
    fprintf(pHppFile, u8"    }\r\n\r\n");

    fprintf(pHppFile, u8"    static const std::string& TableExtra(void)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    if (table.m_extra.empty())
    {
        fprintf(pHppFile, u8"        return ITable::TableNull();\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        static std::string table_extra = u8\"%s\";\r\n", table.m_extra.c_str());
        fprintf(pHppFile, u8"        return table_extra;\r\n");
    }
    fprintf(pHppFile, u8"    }\r\n\r\n");

    fprintf(pHppFile, u8"    const std::string& GetTableName(void) const override { return %s::TableName(); }\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"    const std::string& GetTableExtra(void) const override { return %s::TableExtra(); }\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"    const std::string& GetTableEngine(void) const override { return %s::TableEngine(); }\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"    const std::string& GetTableCharset(void) const override { return %s::TableCharset(); }\r\n", table.m_name.c_str());
    fprintf(pHppFile, u8"    const std::string& GetTableRowFormat(void) const override { return %s::TableRowFormat(); }\r\n", table.m_name.c_str());

    fprintf(pHppFile, u8"    SFieldList<>* FieldList(void) override\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        static Row field_list;\r\n");
    fprintf(pHppFile, u8"        return &field_list;\r\n");
    fprintf(pHppFile, u8"    }\r\n");

    fprintf(pHppFile, u8"    SFieldList<>* PrimaryKey(void) override\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        static SFieldList<\r\n");
    key_info& primary_key = table.m_key_primary[0];
    for (size_t i = 0; i < primary_key.m_field_list.size(); i++)
    {
        if (i == primary_key.m_field_list.size() - 1)
        {
            fprintf(pHppFile, u8"        %s::%s\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        %s::%s,\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
    }
    fprintf(pHppFile, u8"        > primary_%s;\r\n", primary_key.m_name.c_str());
    fprintf(pHppFile, u8"        return &primary_%s;\r\n", primary_key.m_name.c_str());
    fprintf(pHppFile, u8"    }\r\n");

    fprintf(pHppFile, u8"    static PriKey PriKeyFromRow(const Row& row)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        return PriKey(\r\n");
    for (size_t i = 0; i < primary_key.m_field_list.size(); i++)
    {
        if (i == primary_key.m_field_list.size() - 1)
        {
            fprintf(pHppFile, u8"        row.Field<%s::%s>().GetData()\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        row.Field<%s::%s>().GetData(),\r\n", table.m_name.c_str(), primary_key.m_field_list[i].c_str());
        }
    }
    fprintf(pHppFile, u8"        );\r\n", primary_key.m_name.c_str());
    fprintf(pHppFile, u8"    }\r\n");

    fprintf(pHppFile, u8"    std::map<std::string, SFieldList<>*> UniqueKey(void) override\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        std::map<std::string, SFieldList<>*> unique_map;\r\n");

    for (size_t i = 0; i < table.m_key_unique.size(); i++)
    {
        fprintf(pHppFile, u8"        static SFieldList<\r\n");
        key_info& info = table.m_key_unique[i];
        for (size_t f = 0; f < info.m_field_list.size(); f++)
        {
            if (f == info.m_field_list.size()-1)
            {
                fprintf(pHppFile, u8"        %s::%s\r\n", table.m_name.c_str(), info.m_field_list[f].c_str());
            }
            else
            {
                fprintf(pHppFile, u8"        %s::%s,\r\n", table.m_name.c_str(), info.m_field_list[f].c_str());
            }
        }
        fprintf(pHppFile, u8"        > unique_%s;\r\n", table.m_key_unique[i].m_name.c_str());

        fprintf(pHppFile, u8"        unique_map[u8\"%s\"] = &unique_%s;\r\n", info.m_name.c_str(), info.m_name.c_str());
    }

    fprintf(pHppFile, u8"        return unique_map;\r\n");

    fprintf(pHppFile, u8"    }\r\n");



    fprintf(pHppFile, u8"    std::map<std::string, SFieldList<>*> IndexKey(void) override\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        std::map<std::string, SFieldList<>*> index_map;\r\n");

    for (size_t i = 0; i < table.m_key_index.size(); i++)
    {
        fprintf(pHppFile, u8"        static SFieldList<\r\n");
        key_info& info = table.m_key_index[i];
        for (size_t f = 0; f < info.m_field_list.size(); f++)
        {
            if (f == info.m_field_list.size() - 1)
            {
                fprintf(pHppFile, u8"        %s::%s\r\n", table.m_name.c_str(), info.m_field_list[f].c_str());
            }
            else
            {
                fprintf(pHppFile, u8"        %s::%s,\r\n", table.m_name.c_str(), info.m_field_list[f].c_str());
            }
        }
        fprintf(pHppFile, u8"        > index_%s;\r\n", table.m_key_index[i].m_name.c_str());

        fprintf(pHppFile, u8"        index_map[u8\"%s\"] = &index_%s;\r\n", info.m_name.c_str(), info.m_name.c_str());
    }

    fprintf(pHppFile, u8"        return index_map;\r\n");
    fprintf(pHppFile, u8"    }\r\n");

    fprintf(pHppFile, u8"    \r\n");
    fprintf(pHppFile, u8"};\r\n");
    return "";
}

bool _CheckColumnType(const std::string& type)
{
    if (
        type == "int8" ||
        type == "uint8" ||
        type == "int16" ||
        type == "uint16" ||
        type == "int32" ||
        type == "uint32" ||
        type == "int64" ||
        type == "uint64" ||
        type == "varchar" ||
        type == "text" ||
        type == "longtext"
        )
    {
        return true;
    }

    return false;
}

bool _CheckKeyType(const std::string& type)
{
    if (
        type == "primary" ||
        type == "unique" ||
        type == "index"
        )
    {
        return true;
    }

    return false;
}

std::string& StringTrim(std::string& str)
{
    if (str.empty()) {
        return str;
    }
    str.erase(0, str.find_first_not_of(" "));
    str.erase(str.find_last_not_of(" ") + 1);
    return str;
}

std::string DBTableMaker::MakeDBTable(const std::string& xml, const std::string& out_put_path)
{
    CMarkupSTL oXml;
    std::string strVal;
    std::string strHppFile;
    std::string strCppFile;
    std::string strMouleName;
    FILE* pHppFile = NULL;
    FILE* pCppFile = NULL;

    if (!oXml.Load(xml.c_str()))
    {
        m_strErrorInfo = u8"加载数据库表定义文件" + xml + u8"失败! ";
        m_strErrorInfo += oXml.GetError();
        return m_strErrorInfo;
    }

    table_info tbl;

    column_map field_map;
    key_map unique_map;
    key_map index_map;
    //column_vector field_list;

    FIND_ELEM(u8"table");

    GET_ATTRIB(u8"name");
    tbl.m_name = strVal;

    //GET_ATTRIB(u8"engine");
    tbl.m_engine = oXml.GetAttrib(u8"engine");

    //GET_ATTRIB(u8"charset");
    tbl.m_charset = oXml.GetAttrib(u8"charset");

    //GET_ATTRIB(u8"row_format");
    tbl.m_row_format = oXml.GetAttrib(u8"row_format");

    //GET_ATTRIB(u8"extra");
    //tbl.m_extra = strVal;
    tbl.m_extra = oXml.GetAttrib(u8"extra");

    INTO_ELEM();
    oXml.ResetMainPos();
    while (oXml.FindElem(u8"field"))
    {
        column_info info;

        GET_ATTRIB("name");
        info.m_name = strVal;

        GET_ATTRIB("type");
        info.m_type = strVal;

        info.m_length = oXml.GetAttrib(u8"length");

        info.m_extra = oXml.GetAttrib(u8"extra");

        info.m_comment = oXml.GetAttrib(u8"comment");

        if (!_CheckColumnType(info.m_type))
        {
            m_strErrorInfo = u8"field name=" + info.m_name + u8"type=" + info.m_type + u8" unknow!";
            return m_strErrorInfo;
        }

        auto it_column = field_map.find(info.m_name);

        if (it_column != field_map.end())
        {
            m_strErrorInfo = u8"field name=" + info.m_name + u8" already exist!";
            return m_strErrorInfo;
        }

        field_map[info.m_name] = info;
        //field_list.push_back(info);
        tbl.m_all_fields.push_back(info);
    }

    oXml.ResetMainPos();
    while (oXml.FindElem(u8"key"))
    {
        key_info info;

        GET_ATTRIB("type");
        info.m_type = strVal;

        if (!_CheckKeyType(info.m_type))
        {
            m_strErrorInfo = u8"key type=" + info.m_type + u8" unknow!";
            return m_strErrorInfo;
        }

        std::string key_fields = oXml.GetAttrib("fields");

        if (key_fields.empty())
        {
            m_strErrorInfo = u8"key type=" + info.m_type + u8" fields is null!";
            return m_strErrorInfo;
        }
        
        std::set<std::string> key_check;
        mem_seg field_seg[128];

        size_t field_count = split_mem_to_segments(key_fields.c_str(), key_fields.length(), ",", 1, field_seg, 128);

        for (size_t i = 0; i < field_count; i++)
        {
            std::string field((const char*)field_seg[i].mem, field_seg[i].mem_size);

            StringTrim(field);

            auto it = key_check.find(field);
            if (it != key_check.end())
            {
                m_strErrorInfo = u8"key field" + field + u8" repeat!";
                return m_strErrorInfo;
            }
            else
            {
                key_check.insert(field);

                if (field_map.find(field) == field_map.end())
                {
                    m_strErrorInfo = u8"key field" + field + u8" not found!";
                    return m_strErrorInfo;
                }

                info.m_field_list.push_back(field);
            }
        }


        for (size_t i = 0; i < info.m_field_list.size(); i++)
        {
            if (i == 0)
            {
                info.m_name = info.m_field_list[0];
            }
            else
            {
                info.m_name += u8"_" + info.m_field_list[i];
            }
        }

        if (info.m_type == "primary")
        {
            if (tbl.m_key_primary.size())
            {
                m_strErrorInfo = u8"primary key could be only one !";
                return m_strErrorInfo;
            }

            tbl.m_key_primary.push_back(info);
        }
        else if (info.m_type == "unique")
        {
            if (unique_map.find(info.m_name) != unique_map.end())
            {
                m_strErrorInfo = u8"unique key " + info.m_name + u8"already exist !";
                return m_strErrorInfo;
            }

            tbl.m_key_unique.push_back(info);
            unique_map[info.m_name] = info;
        }
        else if (info.m_type == "index")
        {
            if (index_map.find(info.m_name) != index_map.end())
            {
                m_strErrorInfo = u8"index key " + info.m_name + u8"already exist !";
                return m_strErrorInfo;
            }

            tbl.m_key_index.push_back(info);
            index_map[info.m_name] = info;
        }
        else
        {
            m_strErrorInfo = u8"key type=" + info.m_type + u8" unknow!";
            return m_strErrorInfo;
        }

        //tbl.m_all_keys.push_back(info);
    }
    OUTOF_ELEM();

    strHppFile = out_put_path + tbl.m_name + ".hpp";
    strCppFile = out_put_path + tbl.m_name + ".cpp";
    pHppFile = fopen(strHppFile.c_str(), "wb");
    pCppFile = fopen(strCppFile.c_str(), "wb");
    if (NULL == pHppFile || NULL == pCppFile)
        goto ERROR_DEAL;

    {
        //utf8-bom
        char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, pHppFile);
        fwrite(bom, 1, 3, pCppFile);
    }

    fprintf(pHppFile, u8"#pragma once\r\n\r\n");
    fprintf(pHppFile, u8"#include \"db_fields.hpp\"\r\n");
    fprintf(pHppFile, u8"#include \"db_mysql.hpp\"\r\n");
    fprintf(pCppFile, u8"#include \"%s.hpp\"\r\n\r\n", tbl.m_name.c_str());

    m_strErrorInfo = _WriteTableClass(pHppFile, tbl);

    fclose(pHppFile);
    fclose(pCppFile);
    return "";

ERROR_DEAL:
    return m_strErrorInfo;
}
