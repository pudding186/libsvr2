#include "tablemaker.h"

void SplitString(const std::string& strContent, char cSpliter, std::vector<std::string>& vecParams, size_t nMaxSplit/* = 0*/)
{
    std::string::size_type iStart = 0, iPos;
    iPos = strContent.find(cSpliter, iStart);
    while(std::string::npos != iPos)
    {
        if(iPos > iStart)
        {
            std::string strParam = strContent.substr(iStart, iPos - iStart);
            vecParams.push_back(strParam);

            if (nMaxSplit && vecParams.size() == nMaxSplit - 1)
            {
                iStart = iPos + 1;
                break;
            }
        }
        iStart = iPos + 1;
        iPos = strContent.find(cSpliter, iStart);
    }

    if(iStart < strContent.size())
    {
        std::string strParam = strContent.substr(iStart, strContent.size() - iStart);
        vecParams.push_back(strParam);
    }
}

std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); } // correct
    );
    return s;
}

std::string str_toupper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); } // correct
    );
    return s;
}

CTableMaker::CTableMaker(const std::string& table_name)
{
    m_table_name = table_name;
    std::transform(m_table_name.begin(), m_table_name.end(), m_table_name.begin(), ::tolower);
    

    m_table_column.clear();
    m_table_key.clear();

    std::vector<std::string> name_list;

    SplitString(m_table_name, '_', name_list, 0);

    //m_struct_name = m_table_name;
    //std::transform(m_struct_name.begin(), m_struct_name.begin()+1, m_struct_name.begin(), ::toupper);

    for (size_t i = 0; i < name_list.size(); i++)
    {
        std::transform(name_list[i].begin(), name_list[i].begin()+1, name_list[i].begin(), ::toupper);
    }

    for (size_t i = 0; i < name_list.size(); i++)
    {
        m_struct_name += name_list[i];
    }

    m_class_name = m_struct_name + "Config";
}

CTableMaker::~CTableMaker(void)
{
}

std::string CTableMaker::add_column( column_info& info )
{
    if (m_table_column.find(info.m_col_name) != m_table_column.end())
    {
        std::string err = "表中有重复的字段名 " + info.m_col_name;

        return err;
    }

    m_table_column[info.m_col_name] = info;

    m_table_column_arry.push_back(info.m_col_name);

    return "";
}

std::string CTableMaker::add_key( key_info& info )
{
    if (m_table_key.find(info.m_key_list) != m_table_key.end())
    {
        std::string err = "表中有重复的索引, 索引组成: ";

        for (std::set<std::string>::iterator it = info.m_key_list.begin();
            it != info.m_key_list.end(); ++it)
        {
            err += (*it) + " ";
        }

        return err;
    }

    m_table_key[info.m_key_list] = info;

    return "";
}

bool CTableMaker::is_key_column(const column_info& info)
{
    for (auto it = m_table_key.begin(); it != m_table_key.end(); ++it)
    {
        if (it->first.find(info.m_col_name) != it->first.end())
        {
            return true;
        }
    }

    return false;
}

bool CTableMaker::gen_code( const std::string& path, std::string& err )
{
    std::string hpp_file_path = path + m_table_name + "_table.hpp";
    std::string cpp_file_path = path + m_table_name + "_table.cpp";

    FILE* hpp_file = fopen(hpp_file_path.c_str(), "wb");

    if (!hpp_file)
    {
        err = strerror(errno);
        goto ERROR_DEAL;
    }
    else
    {
        //utf8-bom
        char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, hpp_file);
    }

    fprintf(hpp_file, u8"/// Generate by table_maker tools, PLEASE DO NOT EDIT IT!\r\n");
    fprintf(hpp_file, u8"#pragma once\r\n");
    fprintf(hpp_file, u8"#include \"tree_help.h\"\r\n");
    fprintf(hpp_file, u8"#include \"utility.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"smemory.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"pugixml.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"pugiconfig.hpp\"\r\n");
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"#include \"lua_table.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"lua_table_context.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"lua_table_sugar.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"lua_readonly.hpp\"\r\n");
    fprintf(hpp_file, u8"#include \"lua_system.hpp\"\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
    fprintf(hpp_file, u8"\r\n");
    fprintf(hpp_file, u8"namespace DATA\r\n");
    fprintf(hpp_file, u8"{\r\n");
    _print_data_struct(hpp_file);
    _print_class(hpp_file);
    fprintf(hpp_file, u8"}\r\n");

    fclose(hpp_file);

    FILE* cpp_file = fopen(cpp_file_path.c_str(), "wb");
    if (!cpp_file)
    {
        goto ERROR_DEAL;
    }
    else
    {
        //utf8-bom
        char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, cpp_file);
    }

    fprintf(cpp_file, u8"#include \"pch.h\"\r\n");
    fprintf(cpp_file, u8"#include \"%s_table.hpp\"\r\n", m_table_name.c_str());
    fprintf(cpp_file, u8"namespace DATA\r\n");
    fprintf(cpp_file, u8"{\r\n");
    fprintf(cpp_file, u8"    INSTANCE_SINGLETON(%s)\r\n", m_class_name.c_str());
    fprintf(cpp_file, u8"}\r\n");

    fclose(cpp_file);
    return true;

ERROR_DEAL:
    return false;
}

void FirstCaps(std::string& src)
{
    std::transform(src.begin(), src.end(), src.begin(), ::tolower);
    std::transform(src.begin(), src.begin()+1, src.begin(), ::toupper);
}

void CTableMaker::_print_data_struct( FILE* hpp_file )
{
    fprintf(hpp_file, u8"typedef struct st_%s\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"{\r\n");

    std::vector<column_info*> type_64;
    std::vector<column_info*> type_32;
    std::vector<column_info*> type_16;
    std::vector<column_info*> type_8;

    type_64.reserve(64);
    type_32.reserve(64);
    type_16.reserve(64);
    type_8.reserve(64);

    size_t max_name_length = 0;

    for (table_column_info::iterator it = m_table_column.begin();
        it != m_table_column.end(); ++it)
    {
        column_info& info = it->second;

        switch (info.m_data_type)
        {
        case col_string:
            {
                type_64.push_back(&info);
            }
        	break;
        case col_uint8:
        case col_int8:
            {
                type_8.push_back(&info);
            }
            break;
        case col_uint16:
        case col_int16:
            {
                type_16.push_back(&info);
            }
            break;
        case col_uint32:
        case col_int32:
            {
                type_32.push_back(&info);
            }
            break;
        case col_uint64:
        case col_int64:
            {
                type_64.push_back(&info);
            }
            break;
        }

        if (info.m_col_name.length() > max_name_length)
        {
            max_name_length = info.m_col_name.length();

            max_name_length = 8 - max_name_length%8 + max_name_length;
        }
    }

    for (size_t i = 0; i < type_64.size(); i++)
    {
        column_info* info = type_64[i];

        fprintf(hpp_file, u8"    %-*s %s;%-*s/// %s\r\n", 19, 
            info->m_col_type.c_str(), info->m_col_name.c_str(), (int)(max_name_length - info->m_col_name.length()), " ", info->m_col_note.c_str());
    }

    for (size_t i = 0; i < type_32.size(); i++)
    {
        column_info* info = type_32[i];

        fprintf(hpp_file, u8"    %-*s %s;%-*s/// %s\r\n", 19, 
            info->m_col_type.c_str(), info->m_col_name.c_str(), (int)(max_name_length - info->m_col_name.length()), " ", info->m_col_note.c_str());
    }

    for (size_t i = 0; i < type_16.size(); i++)
    {
        column_info* info = type_16[i];

        fprintf(hpp_file, u8"    %-*s %s;%-*s/// %s\r\n", 19, 
            info->m_col_type.c_str(), info->m_col_name.c_str(), (int)(max_name_length - info->m_col_name.length()), " ", info->m_col_note.c_str());
    }

    for (size_t i = 0; i < type_8.size(); i++)
    {
        column_info* info = type_8[i];

        fprintf(hpp_file, u8"    %-*s %s;%-*s/// %s\r\n", 19, 
            info->m_col_type.c_str(), info->m_col_name.c_str(), (int)(max_name_length - info->m_col_name.length()), " ", info->m_col_note.c_str());
    }

    fprintf(hpp_file, u8"}%s;\r\n\r\n", m_struct_name.c_str());
}

void CTableMaker::_print_class( FILE* hpp_file )
{
    if (m_table_column.empty())
    {
        return;
    }

    fprintf(hpp_file, u8"class %s\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"{\r\n");
    fprintf(hpp_file, u8"    DECLARE_SINGLETON(%s)\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"private:\r\n");

    size_t max_member_length = m_struct_name.length()+2;
    max_member_length = 8 - max_member_length%8 + max_member_length;
    std::string tmp = m_struct_name + "**";

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        //key_info& info = it->second;
        fprintf(hpp_file, u8"    %-*s m_data_map%zu;\r\n", (int)(max_member_length), "HRBTREE", key_count);
    }

    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"    %-*s m_lua_data_map;\r\n", (int)(max_member_length), "Lua::Table");
    fprintf(hpp_file, u8"#endif\r\n");
    
    fprintf(hpp_file, u8"    std::vector<%s*> m_data_array;\r\n", m_struct_name.c_str());

    fprintf(hpp_file, u8"public:\r\n");
    _print_func_At(hpp_file);
    _print_func_GetFieldCount(hpp_file);
    _print_func_GetSize(hpp_file);
    _print_func_OrderAttribute(hpp_file);
    _print_func_FillData(hpp_file);
    _print_func_FillMapping(hpp_file);
    _print_func_FillLuaData(hpp_file);
    _print_func_FillLuaMapping(hpp_file);
    _print_func_AllocRow(hpp_file);
    _print_func_FreeRow(hpp_file);
    _print_func_QuickMapping(hpp_file);
    _print_func_ReleaseMapping(hpp_file);
    _print_func_Release(hpp_file);
    _print_func_Get(hpp_file);
    _print_func_Lua_Get(hpp_file);
    _print_func_Lua_TableData(hpp_file);
    _print_func_Load(hpp_file);
    _print_func_ReLoad(hpp_file);
    _print_func_ReLoadEx(hpp_file);
    _print_func_ReLoadLua(hpp_file);
    _print_func_RegLuaDelegate(hpp_file);
    _print_func_Construct_Destruct(hpp_file);
    _print_func_InitColInfo_UnInitColInfo_GetColVar(hpp_file);
    fprintf(hpp_file, u8"};\r\n");
    fprintf(hpp_file, u8"#define s%s (*DATA::%s::Instance())\r\n", m_class_name.c_str(), m_class_name.c_str());

}

void CTableMaker::_print_func_GetSize( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    size_t GetSize(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        return m_data_array.size();\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_GetFieldCount( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    size_t GetFieldCount(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        return %zu;\r\n", m_table_column.size());
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_At( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    %s* At(size_t index)\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        if(index >= m_data_array.size())\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            return nullptr;\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        return m_data_array[index];\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_FillData(FILE* hpp_file)
{
    fprintf(hpp_file, u8"    bool FillData(%s* row, pugi::xml_node& element, char* err, size_t err_len)\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        pugi::xml_attribute attr;\r\n");
    fprintf(hpp_file, u8"        pugi::xml_attribute cur_attr = element.first_attribute();\r\n");
    fprintf(hpp_file, u8"\r\n");

    for (size_t i = 0; i < m_table_column_arry.size(); i++)
    {
        column_info& info = m_table_column.find(m_table_column_arry[i])->second;

        fprintf(hpp_file, u8"        attr = order_attribute(\"%s\", element, &cur_attr);\r\n", info.m_col_name.c_str());

        switch (info.m_data_type)
        {
        case col_string:
        {
            fprintf(hpp_file, u8"        if (!attr)\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            row->%s = nullptr;\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"attribute %s not found\");\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            return false;\r\n");
            fprintf(hpp_file, u8"        }\r\n");
            fprintf(hpp_file, u8"        else\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            const char* attr_str = attr.as_string();\r\n");
            fprintf(hpp_file, u8"            size_t attr_str_len = strlen(attr_str);\r\n\r\n");
            fprintf(hpp_file, u8"            if (row->%s)\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"            {\r\n");
            fprintf(hpp_file, u8"                if (strcmp(attr_str, row->%s))\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                {\r\n");
            fprintf(hpp_file, u8"                    if (S_CAPACITY_MEM(row->%s) > attr_str_len)\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                    {\r\n");
            fprintf(hpp_file, u8"                        memcpy(row->%s, attr_str, attr_str_len);\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                        row->%s[attr_str_len] = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                    }\r\n");
            fprintf(hpp_file, u8"                    else\r\n");
            fprintf(hpp_file, u8"                    {\r\n");
            fprintf(hpp_file, u8"                        row->%s = (char*)S_MALLOC_EX(attr_str_len + 1, \"%s.%s\");\r\n", info.m_col_name.c_str(), m_struct_name.c_str(), info.m_col_name.c_str());
            fprintf(hpp_file, u8"                        memcpy(row->%s, attr_str, attr_str_len);\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                        row->%s[attr_str_len] = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                    }\r\n");
            fprintf(hpp_file, u8"                }\r\n");
            fprintf(hpp_file, u8"            }\r\n");
            fprintf(hpp_file, u8"            else\r\n");
            fprintf(hpp_file, u8"            {\r\n");
            fprintf(hpp_file, u8"                row->%s = (char*)S_MALLOC_EX(attr_str_len + 1, \"%s.%s\");\r\n", info.m_col_name.c_str(), m_struct_name.c_str(), info.m_col_name.c_str());
            fprintf(hpp_file, u8"                memcpy(row->%s, attr_str, attr_str_len);\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"                row->%s[attr_str_len] = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"            }\r\n");
            //fprintf(hpp_file, u8"            const char* tmp = attr.as_string();\r\n");
            //fprintf(hpp_file, u8"            size_t str_len = strlen(tmp);\r\n");
            //fprintf(hpp_file, u8"            if (row->%s)\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            {\r\n");
            //fprintf(hpp_file, u8"                S_DELETE(row->%s);\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            }\r\n");
            //fprintf(hpp_file, u8"            row->%s = S_NEW(char, str_len + 1);\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            memcpy(row->%s, tmp, str_len);\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            row->%s[str_len] = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"        }\r\n");
        }
        break;
        case col_uint8:
        case col_int8:
        case col_uint16:
        case col_int16:
        case col_int32:
        case col_uint32:
        {
            fprintf(hpp_file, u8"        if (!attr)\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            row->%s = 0;\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"attribute %s not found\");\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            return false;\r\n");
            fprintf(hpp_file, u8"        }\r\n");
            fprintf(hpp_file, u8"        else\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            if (!attr.empty())\r\n");
            fprintf(hpp_file, u8"            {\r\n");

            if (info.m_data_type == col_int32 ||
                info.m_data_type == col_uint32)
            {
            fprintf(hpp_file, u8"                long long value = attr.as_llong();\r\n");
            }
            else
            {
            fprintf(hpp_file, u8"                int value = attr.as_int();\r\n");
            }
            
            fprintf(hpp_file, u8"                if (value > (std::numeric_limits<%s>::max)())\r\n", info.m_col_type.c_str());
            fprintf(hpp_file, u8"                {\r\n");
            fprintf(hpp_file, u8"                    snprintf(err, err_len, u8\"attribute %s value > %s max\");\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
            fprintf(hpp_file, u8"                    return false;\r\n");
            fprintf(hpp_file, u8"                }\r\n");
            fprintf(hpp_file, u8"                else if (value < (std::numeric_limits<%s>::min)())\r\n", info.m_col_type.c_str());
            fprintf(hpp_file, u8"                {\r\n");
            fprintf(hpp_file, u8"                    snprintf(err, err_len, u8\"attribute %s value < %s min\");\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
            fprintf(hpp_file, u8"                    return false;\r\n");
            fprintf(hpp_file, u8"                }\r\n");
            fprintf(hpp_file, u8"                else\r\n");
            fprintf(hpp_file, u8"                {\r\n");
            fprintf(hpp_file, u8"                    row->%s = (%s)value;\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
            fprintf(hpp_file, u8"                }\r\n");
            fprintf(hpp_file, u8"            }\r\n");
            fprintf(hpp_file, u8"            else\r\n");
            fprintf(hpp_file, u8"            {\r\n");
            fprintf(hpp_file, u8"                row->%s = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"            }\r\n");
            fprintf(hpp_file, u8"        }\r\n");
        }
        break;
        case col_uint64:
        case col_int64:
        {
            fprintf(hpp_file, u8"        if (!attr)\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            row->%s = 0;\r\n", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"attribute %s not found\");", info.m_col_name.c_str());
            //fprintf(hpp_file, u8"            return false;");
            fprintf(hpp_file, u8"        }\r\n");
            fprintf(hpp_file, u8"        else\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            if (!attr.empty())\r\n");
            fprintf(hpp_file, u8"            {\r\n");

            if (info.m_data_type == col_int64)
            {
                fprintf(hpp_file, u8"                long long value = attr.as_llong();\r\n");
            }
            else if (info.m_data_type == col_uint64)
            {
                fprintf(hpp_file, u8"                unsigned long long value = attr.as_ullong();\r\n");
            }

            fprintf(hpp_file, u8"                row->%s = value;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"            }\r\n");
            fprintf(hpp_file, u8"            else\r\n");
            fprintf(hpp_file, u8"            {\r\n");
            fprintf(hpp_file, u8"                row->%s = 0;\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"            }\r\n");
            fprintf(hpp_file, u8"        }\r\n");
        }
        break;
        }
    }
    fprintf(hpp_file, u8"        return true;\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_FillLuaData(FILE* hpp_file)
{
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"    void FillLuaData(Lua::Table& table, const %s* row)\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    for (size_t i = 0; i < m_table_column_arry.size(); i++)
    {
        column_info& info = m_table_column.find(m_table_column_arry[i])->second;

        fprintf(hpp_file, u8"        table[\"%s\"] = row->%s;\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
    }

    fprintf(hpp_file, u8"    }\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
}

void CTableMaker::_print_func_Get( FILE* hpp_file )
{
    if (m_table_key.empty())
    {
        return;
    }

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;

        fprintf(hpp_file, u8"    %s* GetBy", m_struct_name.c_str());
        size_t param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;
            fprintf(hpp_file, u8"%s", info.m_col_name.c_str());

            if (param_idx == k_info.m_key_order.size())
            {
                fprintf(hpp_file, u8"(");
            }
        }
        param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (param_idx == k_info.m_key_order.size())
            {
                if (info.m_col_type == "char*")
                    fprintf(hpp_file, u8"const char* %s)\r\n", info.m_col_name.c_str());
                else
                    fprintf(hpp_file, u8"%s %s)\r\n", info.m_col_type.c_str(), info.m_col_name.c_str());
            }
            else
            {
                if (info.m_col_type == "char*")
                    fprintf(hpp_file, u8"const char* %s, ", info.m_col_name.c_str());
                else
                    fprintf(hpp_file, u8"%s %s, ", info.m_col_type.c_str(), info.m_col_name.c_str());
            }
        }

        fprintf(hpp_file, u8"    {\r\n");

        std::string space_str = "        ";
        size_t key_idx = 1;
        std::list<column_info*> key_list;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (key_idx < k_info.m_key_order.size())
            {
                fprintf(hpp_file, space_str.c_str());
                switch (info.m_data_type)
                {
                case col_string:
                {
                    if (key_idx == 1)
                    {
                        fprintf(hpp_file, u8"HRBTREE tree%zu = (HRBTREE)str_bkdr_tree_find(m_data_map%zu, %s);\r\n", key_idx, key_count, info.m_col_name.c_str());
                    }
                    else
                    {
                        fprintf(hpp_file, u8"HRBTREE tree%zu = (HRBTREE)str_bkdr_tree_find(tree%zu, %s);\r\n", key_idx, key_idx - 1, info.m_col_name.c_str());
                    }
                }
                break;
                case col_int8:
                case col_uint8:
                case col_int16:
                case col_uint16:
                case col_int32:
                case col_uint32:
                case col_uint64:
                case col_int64:
                    {
                        if (key_idx == 1)
                        {
                            fprintf(hpp_file, u8"HRBTREE tree%zu = (HRBTREE)int_seg_tree_find(m_data_map%zu, %s);\r\n",key_idx, key_count, info.m_col_name.c_str());
                        }
                        else
                        {
                            fprintf(hpp_file, u8"HRBTREE tree%zu = (HRBTREE)int_seg_tree_find(tree%zu, %s);\r\n",key_idx, key_idx-1, info.m_col_name.c_str());
                        }
                    }
                    break;
                default:
                    {
                    }
                }

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"if (tree%zu)\r\n", key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file,"{\r\n");
                space_str.append("    ");

                key_list.push_front(&info);
            }
            else
            {
                fprintf(hpp_file, space_str.c_str());
                switch (info.m_data_type)
                {
                case col_string:
                {
                    if (key_idx == 1)
                    {
                        fprintf(hpp_file, u8"return (%s*)str_bkdr_tree_find(m_data_map%zu, %s);\r\n",
                            m_struct_name.c_str(), key_count, info.m_col_name.c_str());
                    }
                    else
                    {
                        fprintf(hpp_file, u8"return (%s*)str_bkdr_tree_find(tree%zu, %s);\r\n",
                            m_struct_name.c_str(), key_idx - 1, info.m_col_name.c_str());
                    }
                }
                break;
                case col_int8:
                case col_uint8:
                case col_int16:
                case col_uint16:
                case col_int32:
                case col_uint32:
                case col_uint64:
                case col_int64:
                    {
                        if (key_idx == 1)
                        {
                            fprintf(hpp_file, u8"return (%s*)int_seg_tree_find(m_data_map%zu, %s);\r\n", 
                                m_struct_name.c_str(), key_count, info.m_col_name.c_str());
                        }
                        else
                        {
                            fprintf(hpp_file, u8"return (%s*)int_seg_tree_find(tree%zu, %s);\r\n", 
                                m_struct_name.c_str(), key_idx-1, info.m_col_name.c_str());
                        }
                    }
                    break;
                default:
                    {
                    }
                }
            }
        }

        while (!key_list.empty())
        {
            space_str = space_str.substr(0, space_str.length()-4);
            fprintf(hpp_file, space_str.c_str());
            fprintf(hpp_file, u8"}\r\n");
            key_list.pop_front();
        }

        if (k_info.m_key_order.size() > 1)
        {
            fprintf(hpp_file, u8"        return 0;\r\n");
        }
        
        fprintf(hpp_file, u8"    }\r\n");
    }
}

void CTableMaker::_print_func_Lua_Get(FILE* hpp_file)
{
    if (m_table_key.empty())
    {
        return;
    }

    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;

        fprintf(hpp_file, u8"    static int GetBy");
        size_t param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;
            fprintf(hpp_file, u8"%s", info.m_col_name.c_str());

            if (param_idx == k_info.m_key_order.size())
            {
                fprintf(hpp_file, u8"(lua_State* state)\r\n");
                fprintf(hpp_file, u8"    {\r\n");
            }
        }

        fprintf(hpp_file, u8"        lua_checkstack(state, 2);\r\n");
        fprintf(hpp_file, u8"        lua_getglobal(state, \"%s\");\r\n", m_class_name.c_str());
        param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            switch (info.m_data_type)
            {
            case col_string:
            {
                fprintf(hpp_file, u8"        lua_pushstring(state, luaL_checkstring(state, %zu));\r\n", param_idx);
            }
            break;
            case col_int8:
            case col_uint8:
            case col_int16:
            case col_uint16:
            case col_int32:
            case col_uint32:
            case col_int64:
            case col_uint64:
            {
                fprintf(hpp_file, u8"        lua_pushinteger(state, luaL_checkinteger(state, %zu));\r\n", param_idx);
            }
            break;
            default:
            {
            }
            }

            fprintf(hpp_file, u8"        lua_gettable(state, -2);\r\n");

            fprintf(hpp_file, u8"        if (lua_isnil(state, -1))\r\n");
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            lua_remove(state, -2);\r\n");
            fprintf(hpp_file, u8"            return 1;\r\n");
            fprintf(hpp_file, u8"        }\r\n");

            fprintf(hpp_file, u8"        lua_remove(state, -2);\r\n");
        }

        fprintf(hpp_file, u8"        return 1;\r\n");
        fprintf(hpp_file, u8"    }\r\n");
    }

    fprintf(hpp_file, u8"#endif\r\n");
}

void CTableMaker::_print_func_ReleaseMapping( FILE* hpp_file )
{
    if (m_table_key.empty())
    {
        return;
    }

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;

        fprintf(hpp_file, u8"    void ReleaseMapping%zu(void)\r\n", key_count);
        fprintf(hpp_file, u8"    {\r\n");
        fprintf(hpp_file, u8"        if (!m_data_map%zu)\r\n", key_count);
        fprintf(hpp_file, u8"        {\r\n");
        fprintf(hpp_file, u8"            return;\r\n");
        fprintf(hpp_file, u8"        }\r\n");

        std::string space_str = u8"        ";
        size_t key_idx = 1;
        std::list<column_info*> key_list;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (key_idx < k_info.m_key_order.size())
            {
                fprintf(hpp_file, space_str.c_str());

                if (key_idx == 1)
                {
                    fprintf(hpp_file, u8"HRBNODE node_%zu = rb_first(m_data_map%zu);\r\n", key_idx, key_count);
                }
                else
                {
                    fprintf(hpp_file, u8"HRBNODE node_%zu = rb_first((HRBTREE)value_arry%zu[i%zu]);\r\n", key_idx, key_idx-1, key_idx-1);
                }

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"while (node_%zu)\r\n", key_idx);

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                space_str.append(u8"    ");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"void** value_arry%zu;\r\n", key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"size_t value_arry_size%zu;\r\n", key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"void* tmp%zu[1];\r\n", key_idx);

                fprintf(hpp_file, space_str.c_str());

                if (key_idx == 1)
                {
                    fprintf(hpp_file, u8"if (tree_is_int_seg(m_data_map%zu))\r\n", key_count);
                }
                else
                {
                    fprintf(hpp_file, u8"if (tree_is_int_seg((HRBTREE)value_arry%zu[i%zu]))\r\n", key_idx-1, key_idx-1);
                }
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    value_arry_size%zu = int_seg_tree_node_value(node_%zu, &value_arry%zu);\r\n", key_idx, key_idx, key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"else\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    value_arry_size%zu = 1;\r\n", key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    tmp%zu[0] = rb_node_value_user(node_%zu);\r\n", key_idx, key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    value_arry%zu = tmp%zu;\r\n", key_idx, key_idx);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"for (size_t i%zu = 0; i%zu < value_arry_size%zu; i%zu++)\r\n", key_idx, key_idx, key_idx, key_idx);

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                space_str.append(u8"    ");

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"if (value_arry%zu[i%zu])\r\n", key_idx, key_idx);

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                space_str.append(u8"    ");
            }

            key_list.push_front(&info);
        }

        while (!key_list.empty())
        {
            //column_info* col_info = key_list.front();

            fprintf(hpp_file, space_str.c_str());

            if (key_list.size() == 1)
            {
                fprintf(hpp_file, u8"if (tree_is_int_seg(m_data_map%zu))\r\n", key_count);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_int_seg_tree(m_data_map%zu);\r\n", key_count);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"else if (tree_is_str_bkdr(m_data_map%zu))\r\n", key_count);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_str_bkdr_tree(m_data_map%zu);\r\n", key_count);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"else\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_rb_tree(m_data_map%zu);\r\n", key_count);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, u8"        m_data_map%zu = 0;\r\n", key_count);
            }
            else
            {
                fprintf(hpp_file, u8"if (tree_is_int_seg((HRBTREE)value_arry%zu[i%zu]))\r\n", key_list.size()-1, key_list.size()-1);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_int_seg_tree((HRBTREE)value_arry%zu[i%zu]);\r\n", key_list.size()-1, key_list.size()-1);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"else if (tree_is_str_bkdr((HRBTREE)value_arry%zu[i%zu]))\r\n", key_list.size() - 1, key_list.size() - 1);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_str_bkdr_tree((HRBTREE)value_arry%zu[i%zu]);\r\n", key_list.size() - 1, key_list.size() - 1);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"else\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"{\r\n");
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"    destroy_rb_tree((HRBTREE)value_arry%zu[i%zu]);\r\n", key_list.size()-1, key_list.size()-1);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");

                space_str = space_str.substr(0, space_str.length()-4);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");

                space_str = space_str.substr(0, space_str.length()-4);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");

                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"node_%zu = rb_next(node_%zu);\r\n", key_list.size()-1, key_list.size()-1);

                space_str = space_str.substr(0, space_str.length()-4);
                fprintf(hpp_file, space_str.c_str());
                fprintf(hpp_file, u8"}\r\n");
            }

            key_list.pop_front();
        }

        fprintf(hpp_file, u8"    }\r\n");
    }
}

void CTableMaker::_print_func_QuickMapping( FILE* hpp_file )
{
    if (m_table_key.empty())
    {
        return;
    }

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;

        fprintf(hpp_file, u8"    void QuickMapping%zu(HRBTREE& tree)\r\n", key_count);
        fprintf(hpp_file, u8"    {\r\n");
        fprintf(hpp_file, u8"        if (tree_is_int_seg(tree))\r\n");
        fprintf(hpp_file, u8"        {\r\n");
        fprintf(hpp_file, u8"            return;\r\n");
        fprintf(hpp_file, u8"        }\r\n");

        size_t key_idx = 1;
        std::list<column_info*> key_list;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (key_idx < k_info.m_key_order.size())
            {
                for (size_t i = 0; i < key_idx; i++)
                {
                    fprintf(hpp_file, u8"    ");
                }
                if (key_idx == 1)
                {
                    fprintf(hpp_file, u8"    HRBNODE node_%zu = rb_first(tree);\r\n", key_idx);
                }
                else
                {
                    fprintf(hpp_file, u8"    HRBNODE node_%zu = rb_first((HRBTREE)rb_node_value_user(node_%zu));\r\n", key_idx, key_idx - 1);
                }

                for (size_t i = 0; i < key_idx; i++)
                {
                    fprintf(hpp_file, u8"    ");
                }
                fprintf(hpp_file, u8"    while(node_%zu)\r\n", key_idx);
                for (size_t i = 0; i < key_idx; i++)
                {
                    fprintf(hpp_file, u8"    ");
                }
                fprintf(hpp_file, u8"    {\r\n");
            }

            key_list.push_front(&info);
        }

        while (!key_list.empty())
        {
            column_info* col_info = key_list.front();

            if (key_list.size() == 1)
            {
                switch (col_info->m_data_type)
                {
                case col_int8:
                case col_uint8:
                case col_int16:
                case col_uint16:
                case col_int32:
                case col_uint32:
                case col_int64:
                case col_uint64:
                    {
                        fprintf(hpp_file, u8"        tree = create_int_seg_tree(tree, 2048);\r\n");
                    }
                	break;
                }
            }
            else
            {
                switch (col_info->m_data_type)
                {
                case col_int8:
                case col_uint8:
                case col_int16:
                case col_uint16:
                case col_int32:
                case col_uint32:
                case col_int64:
                case col_uint64:
                    {
                        for (size_t i = 0; i < key_list.size(); i++)
                        {
                            fprintf(hpp_file, u8"    ");
                        }

                        fprintf(hpp_file, u8"    HRBTREE new_tree_%zu = create_int_seg_tree((HRBTREE)rb_node_value_user(node_%zu), 2048);\r\n", key_list.size(), key_list.size()-1);

                        for (size_t i = 0; i < key_list.size(); i++)
                        {
                            fprintf(hpp_file, u8"    ");
                        }

                        fprintf(hpp_file, u8"    rb_node_set_value_user(node_%zu, new_tree_%zu);\r\n", key_list.size()-1, key_list.size());
                    }
                    break;
                }

                for (size_t i = 0; i < key_list.size(); i++)
                {
                    fprintf(hpp_file, u8"    ");
                }

                fprintf(hpp_file, u8"    node_%zu = rb_next(node_%zu);\r\n", key_list.size()-1, key_list.size()-1);

                for (size_t i = 0; i < key_list.size()-1; i++)
                {
                    fprintf(hpp_file, u8"    ");
                }

                fprintf(hpp_file, u8"    }\r\n");
            }

            key_list.pop_front();
        }

        fprintf(hpp_file, u8"    }\r\n");
    }
}

void CTableMaker::_print_func_FillMapping( FILE* hpp_file )
{
    if (m_table_key.empty())
    {
        return;
    }

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;
        

        fprintf(hpp_file, u8"    bool FillMapping%zu(%s* row, HRBTREE tree, char* err, size_t err_len)\r\n", key_count, m_struct_name.c_str());
        fprintf(hpp_file, u8"    {\r\n");
        fprintf(hpp_file, u8"        HRBNODE exist_node = 0;\r\n");
        fprintf(hpp_file, u8"        HRBTREE key_1_map = tree;\r\n");

        size_t key_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            switch (info.m_data_type)
            {
            case col_string:
                {
                    fprintf(hpp_file, u8"        exist_node = 0;\r\n");
                    if (it_key_next == k_info.m_key_order.end())
                    {
                        fprintf(hpp_file, u8"        str_bkdr_tree_try_insert(key_%zu_map, row->%s, true, row, &exist_node);\r\n", key_idx, info.m_col_name.c_str());
                    }
                    else
                    {
                        fprintf(hpp_file, u8"        str_bkdr_tree_try_insert(key_%zu_map, row->%s, true, 0, &exist_node);\r\n", key_idx, info.m_col_name.c_str());
                    }
                    
                }
                break;
            case col_uint8:
            case col_int8:
            case col_uint16:
            case col_int16:
            case col_uint32:
            case col_int32:
            case col_uint64:
            case col_int64:
                {
                    fprintf(hpp_file, u8"        exist_node = 0;\r\n");
                    if (it_key_next == k_info.m_key_order.end())
                    {
                        fprintf(hpp_file, u8"        rb_tree_try_insert_integer(key_%zu_map, row->%s, row, &exist_node);\r\n", key_idx, info.m_col_name.c_str());
                    }
                    else
                    {
                        fprintf(hpp_file, u8"        rb_tree_try_insert_integer(key_%zu_map, row->%s, 0, &exist_node);\r\n", key_idx, info.m_col_name.c_str());
                    }
                }
                break;
            }

            if (it_key_next == k_info.m_key_order.end())
            {
                fprintf(hpp_file, u8"        %s* exist_row = (%s*)rb_node_value_user(exist_node);\r\n", m_struct_name.c_str(), m_struct_name.c_str());
                fprintf(hpp_file, u8"        if (exist_row != row)\r\n");
                fprintf(hpp_file, u8"        {\r\n");
                
                switch (info.m_data_type)
                {
                case col_string:
                    {
                        fprintf(hpp_file, u8"            snprintf(err, err_len, \"key %s=%%s repeat\", row->%s);\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
                    }
                    break;
                case col_uint8:
                case col_int8:
                case col_uint16:
                case col_int16:
                case col_int32:
                    {
                        fprintf(hpp_file, u8"            snprintf(err, err_len, \"key %s=%%d repeat\", row->%s);\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
                    }
                    break;
                case col_uint32:
                    {
                        fprintf(hpp_file, u8"            snprintf(err, err_len, \"key %s=%%u repeat\", row->%s);\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
                    }
                    break;
                case col_uint64:
                    {
                        fprintf(hpp_file, u8"            snprintf(err, err_len, \"key %s=%%llu repeat\", row->%s);\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
                    }
                    break;
                case col_int64:
                    {
                        fprintf(hpp_file, u8"            snprintf(err, err_len, \"key %s=%%lld repeat\", row->%s);\r\n", info.m_col_name.c_str(), info.m_col_name.c_str());
                    }
                    break;
                }

                fprintf(hpp_file, u8"            return false;\r\n");
                fprintf(hpp_file, u8"        }\r\n");
            }
            else
            {
                fprintf(hpp_file, u8"        HRBTREE key_%zu_map = (HRBTREE)rb_node_value_user(exist_node);\r\n", key_idx+1);
                fprintf(hpp_file, u8"        if (!key_%zu_map)\r\n", key_idx+1);
                fprintf(hpp_file, u8"        {\r\n");

                if (m_table_column.find(*it_key_next)->second.m_data_type == col_string)
                {
                    fprintf(hpp_file, u8"            key_%zu_map = create_str_bkdr_tree();\r\n", key_idx + 1);
                }
                else
                {
                    fprintf(hpp_file, u8"            key_%zu_map = create_rb_tree(0);\r\n", key_idx + 1);
                }
                
                fprintf(hpp_file, u8"            rb_node_set_value_user(exist_node, key_%zu_map);\r\n", key_idx+1);
                fprintf(hpp_file, u8"        }\r\n");
            }
        }

        fprintf(hpp_file, u8"        return true;\r\n");
        fprintf(hpp_file, u8"    }\r\n");
    }
}

void CTableMaker::_print_func_FillLuaMapping(FILE* hpp_file)
{
    if (m_table_key.empty())
    {
        return;
    }

    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        key_info& k_info = it->second;


        fprintf(hpp_file, u8"    void FillLuaMapping%zu(Lua::Table& lua_map0, Lua::Table& lua_row, %s* row)\r\n", key_count, m_struct_name.c_str());
        fprintf(hpp_file, u8"    {\r\n");

        size_t key_idx = 0;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (it_key_next == k_info.m_key_order.end())
            {
                fprintf(hpp_file, u8"        lua_map%zu[row->%s] = lua_row;\r\n", key_idx, info.m_col_name.c_str());
            }
            else
            {
                fprintf(hpp_file, u8"        if (!lua_map%zu[row->%s].Check<Lua::Table>())\r\n", key_idx, info.m_col_name.c_str());
                fprintf(hpp_file, u8"        {\r\n");
                fprintf(hpp_file, u8"            lua_map%zu[row->%s] = Lua::Table::NewTable(LuaState);\r\n", key_idx, info.m_col_name.c_str());
                fprintf(hpp_file, u8"        }\r\n");
                fprintf(hpp_file, u8"        Lua::Table lua_map%zu = lua_map%zu[row->%s].Get<Lua::Table>();\r\n", key_idx+1, key_idx, info.m_col_name.c_str());
            }
        }

        fprintf(hpp_file, u8"    }\r\n");
    }

    fprintf(hpp_file, u8"#endif\r\n");
}

void CTableMaker::_print_func_AllocRow( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    %s* AllocRow(void)\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        %s* row = S_NEW(%s, 1);\r\n", m_struct_name.c_str(), m_struct_name.c_str());

    for (table_column_info::iterator it = m_table_column.begin();
        it != m_table_column.end(); ++it)
    {
        column_info& info = it->second;

        if (info.m_data_type == col_string)
        {
            fprintf(hpp_file, u8"        row->%s = 0;\r\n", info.m_col_name.c_str());
        }
    }
    fprintf(hpp_file, u8"        return row;\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_FreeRow( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    void FreeRow(%s* row)\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");

    for (table_column_info::iterator it = m_table_column.begin();
        it != m_table_column.end(); ++it)
    {
        column_info& info = it->second;

        if (info.m_data_type == col_string)
        {
            fprintf(hpp_file, u8"        if (row->%s)\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"        {\r\n");
            fprintf(hpp_file, u8"            S_FREE(row->%s);\r\n", info.m_col_name.c_str());
            fprintf(hpp_file, u8"        }\r\n");
        }
    }
    fprintf(hpp_file, u8"        S_DELETE(row);\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_Release( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    void Release(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"        ReleaseMapping%zu();\r\n", key_count);
    }

    fprintf(hpp_file, u8"        for (auto& data:m_data_array)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            FreeRow(data);\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        m_data_array.clear();\r\n");
    //fprintf(hpp_file, u8"        for (size_t i = 0; i < m_data_arry_size; i++)\r\n");
    //fprintf(hpp_file, u8"        {\r\n");
    //fprintf(hpp_file, u8"            FreeRow(m_data_arry[i]);\r\n");
    //fprintf(hpp_file, u8"        }\r\n");
    //fprintf(hpp_file, u8"        if (m_data_arry)\r\n");
    //fprintf(hpp_file, u8"        {\r\n");
    //fprintf(hpp_file, u8"            S_DELETE(m_data_arry);\r\n");  
    //fprintf(hpp_file, u8"            m_data_arry = 0;\r\n");
    //fprintf(hpp_file, u8"            m_data_arry_size = 0;\r\n");
    //fprintf(hpp_file, u8"        }\r\n");

    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_Load( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    bool Load(const char* path, char* err, size_t err_len)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        Release();\r\n");
    fprintf(hpp_file, u8"        pugi::xml_document doc;\r\n");
    fprintf(hpp_file, u8"        pugi::xml_parse_result rs = doc.load_file(path);\r\n");
    fprintf(hpp_file, u8"        if (rs.status != pugi::xml_parse_status::status_ok)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"%%s : %%s\", path, rs.description());\r\n");
    fprintf(hpp_file, u8"            return false;\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        pugi::xml_node root = doc.child(u8\"root\");\r\n");
    fprintf(hpp_file, u8"        if (!root)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"root not found!\");\r\n");
    fprintf(hpp_file, u8"            return false;\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        m_data_array.clear();\r\n");
    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        table_column_info::iterator it_col = m_table_column.find(*(it->second.m_key_order.begin()));
        if (it_col != m_table_column.end() &&
            it_col->second.m_data_type == col_string)
        {
            fprintf(hpp_file, u8"        m_data_map%zu = create_str_bkdr_tree();\r\n", key_count);
        }
        else
        {
            fprintf(hpp_file, u8"        m_data_map%zu = create_rb_tree(0);\r\n", key_count);
        }
    }
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"        m_lua_data_map = Lua::Table::NewTable(LuaState);\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
    fprintf(hpp_file, u8"        for (pugi::xml_node content = root.child(u8\"content\");\r\n");
    fprintf(hpp_file, u8"            content; content = content.next_sibling())\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            %s* row = AllocRow();\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"            if (FillData(row, content, err, err_len))\r\n");
    fprintf(hpp_file, u8"            {\r\n");

    fprintf(hpp_file, u8"                m_data_array.push_back(row);\r\n");
    fprintf(hpp_file, u8"            }\r\n");
    fprintf(hpp_file, u8"            else\r\n");
    fprintf(hpp_file, u8"            {\r\n");
    fprintf(hpp_file, u8"                std::string strError;\r\n");
    fprintf(hpp_file, u8"                strError.append(err);\r\n");
    fprintf(hpp_file, u8"                snprintf(err, err_len, u8\"load row: %%zu fail: %%s\", m_data_array.size(), strError.c_str());\r\n");
    fprintf(hpp_file, u8"                return false;\r\n");
    fprintf(hpp_file, u8"            }\r\n");

    key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"            if (!FillMapping%zu(row, m_data_map%zu, err, err_len))\r\n", key_count, key_count);
        fprintf(hpp_file, u8"            {\r\n");
        fprintf(hpp_file, u8"                return false;\r\n");
        fprintf(hpp_file, u8"            }\r\n");
    }
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"            Lua::Table lua_row = Lua::Table::NewTable(LuaState);\r\n");
    fprintf(hpp_file, u8"            FillLuaData(lua_row, row);\r\n");
    
    key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"            FillLuaMapping%zu(m_lua_data_map, lua_row, row);\r\n", key_count);
    }
    fprintf(hpp_file, u8"#endif\r\n");

    fprintf(hpp_file, u8"        }\r\n");
    key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"        QuickMapping%zu(m_data_map%zu);\r\n", key_count, key_count);
    }
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"        RegLuaDelegate();\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
    fprintf(hpp_file, u8"        return true;\r\n");

    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_ReLoadEx(FILE* hpp_file)
{
    fprintf(hpp_file, u8"    bool ReLoadEx(pugi::xml_document& doc, char* err, size_t err_len)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        pugi::xml_node root = doc.child(u8\"root\");\r\n");
    fprintf(hpp_file, u8"        if (!root)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"root not found\");\r\n");
    fprintf(hpp_file, u8"            return false;\r\n");
    fprintf(hpp_file, u8"        }\r\n");

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        table_column_info::iterator it_col = m_table_column.find(*(it->second.m_key_order.begin()));
        if (it_col != m_table_column.end() &&
            it_col->second.m_data_type == col_string)
        {
            fprintf(hpp_file, u8"        HRBTREE tmp_data_map%zu = create_str_bkdr_tree();\r\n", key_count);
        }
        else
        {
            fprintf(hpp_file, u8"        HRBTREE tmp_data_map%zu = create_rb_tree(0);\r\n", key_count);
        }
    }


    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"        m_lua_data_map = Lua::Table::NewTable(LuaState);\r\n");
    fprintf(hpp_file, u8"#endif\r\n");

    fprintf(hpp_file, u8"        for (pugi::xml_node content = root.child(u8\"content\");\r\n");
    fprintf(hpp_file, u8"            content; content = content.next_sibling())\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            pugi::xml_attribute attr;\r\n");
    fprintf(hpp_file, u8"            pugi::xml_attribute cur_attr = content.first_attribute();\r\n");

    key_count = 1;
    {
        std::vector<std::string> del_key_string;

        key_info& k_info = m_table_key.begin()->second;
        size_t param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;


            fprintf(hpp_file, u8"            %s key_%s;\r\n", info.m_col_type.c_str(), info.m_col_name.c_str());
            fprintf(hpp_file, u8"            attr = order_attribute(\"%s\", content, &cur_attr);\r\n", info.m_col_name.c_str());

            switch (info.m_data_type)
            {
            case col_string:
            {
                fprintf(hpp_file, u8"            if (!attr)\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");
                fprintf(hpp_file, u8"            else\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                const char* tmp = attr.as_string();\r\n");
                fprintf(hpp_file, u8"                size_t str_len = strlen(tmp);\r\n");
                fprintf(hpp_file, u8"                key_%s = S_NEW(char, str_len + 1);\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"                memcpy(key_%s, tmp, str_len);\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"                key_%s[str_len] = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");

                std::string del_string = u8"S_DELETE(key_";
                del_string += info.m_col_name;
                del_string += u8");\r\n";

                del_key_string.push_back(del_string);
            }
            break;
            case col_uint8:
            case col_int8:
            case col_uint16:
            case col_int16:
            case col_int32:
            {
                fprintf(hpp_file, u8"            if (!attr)\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");
                fprintf(hpp_file, u8"            else\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = (%s)attr.as_int();\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
                fprintf(hpp_file, u8"            }\r\n");
            }
            break;
            case col_uint32:
            {
                fprintf(hpp_file, u8"            if (!attr)\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");
                fprintf(hpp_file, u8"            else\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = (%s)attr.as_uint();\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
                fprintf(hpp_file, u8"            }\r\n");
            }
            break;
            case col_uint64:
            {
                fprintf(hpp_file, u8"            if (!attr)\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");
                fprintf(hpp_file, u8"            else\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = (%s)attr.as_ullong();\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
                fprintf(hpp_file, u8"            }\r\n");
            }
            break;
            case col_int64:
            {
                fprintf(hpp_file, u8"            if (!attr)\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = 0;\r\n", info.m_col_name.c_str());
                fprintf(hpp_file, u8"            }\r\n");
                fprintf(hpp_file, u8"            else\r\n");
                fprintf(hpp_file, u8"            {\r\n");
                fprintf(hpp_file, u8"                key_%s = (%s)attr.as_llong();\r\n", info.m_col_name.c_str(), info.m_col_type.c_str());
                fprintf(hpp_file, u8"            }\r\n");
            }
            break;
            }
        }

        fprintf(hpp_file, u8"            %s* row = 0;\r\n", m_struct_name.c_str());
        fprintf(hpp_file, u8"            bool is_new_row = false;\r\n");
        fprintf(hpp_file, u8"            if (m_data_map1)\r\n");
        fprintf(hpp_file, u8"            {\r\n");
        fprintf(hpp_file, u8"                row = GetBy");
        param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            fprintf(hpp_file, u8"%s", info.m_col_name.c_str());

            if (param_idx == k_info.m_key_order.size())
            {
                fprintf(hpp_file, u8"(");
            }
        }
        param_idx = 1;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++param_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            if (param_idx == k_info.m_key_order.size())
            {
                fprintf(hpp_file, u8"key_%s);\r\n", info.m_col_name.c_str());
            }
            else
            {
                fprintf(hpp_file, u8"key_%s, ", info.m_col_name.c_str());
            }
        }
        fprintf(hpp_file, u8"            }\r\n");
        fprintf(hpp_file, u8"            if (!row)\r\n");
        fprintf(hpp_file, u8"            {\r\n");
        fprintf(hpp_file, u8"                row = AllocRow();\r\n");
        fprintf(hpp_file, u8"                is_new_row = true;\r\n");
        fprintf(hpp_file, u8"            }\r\n");
        fprintf(hpp_file, u8"            if (FillData(row, content, err, err_len))\r\n");
        fprintf(hpp_file, u8"            {\r\n");
        //fprintf(hpp_file, u8"                m_data_arry[m_data_arry_size] = row;\r\n");
        //fprintf(hpp_file, u8"                ++m_data_arry_size;\r\n");
        fprintf(hpp_file, u8"                if (is_new_row)\r\n");
        fprintf(hpp_file, u8"                {\r\n");
        fprintf(hpp_file, u8"                    m_data_array.push_back(row);\r\n");
        fprintf(hpp_file, u8"                }\r\n");
        fprintf(hpp_file, u8"            }\r\n");
        fprintf(hpp_file, u8"            else\r\n");
        fprintf(hpp_file, u8"            {\r\n");
        fprintf(hpp_file, u8"                std::string strError;\r\n");
        fprintf(hpp_file, u8"                strError.append(err);\r\n");
        fprintf(hpp_file, u8"                snprintf(err, err_len, u8\"load row: %%zu fail: %%s\", m_data_array.size(), strError.c_str());\r\n");
        fprintf(hpp_file, u8"                return false;\r\n");
        fprintf(hpp_file, u8"            }\r\n");
        
        key_count = 1;
        for (table_key_info::iterator it = m_table_key.begin();
            it != m_table_key.end(); ++it, ++key_count)
        {
            fprintf(hpp_file, u8"            if (!FillMapping%zu(row, tmp_data_map%zu, err, err_len))\r\n", key_count, key_count);
            fprintf(hpp_file, u8"            {\r\n");
            fprintf(hpp_file, u8"                return false;\r\n");
            fprintf(hpp_file, u8"            }\r\n");
        }
        fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
            str_toupper(m_table_name).c_str());
        fprintf(hpp_file, u8"            Lua::Table lua_row = Lua::Table::NewTable(LuaState);\r\n");
        fprintf(hpp_file, u8"            FillLuaData(lua_row, row);\r\n");

        key_count = 1;
        for (table_key_info::iterator it = m_table_key.begin();
            it != m_table_key.end(); ++it, ++key_count)
        {
            fprintf(hpp_file, u8"            FillLuaMapping%zu(m_lua_data_map, lua_row, row);\r\n", key_count);
        }

        fprintf(hpp_file, u8"#endif\r\n");

        for (size_t i = 0; i < del_key_string.size(); i++)
        {
            fprintf(hpp_file, u8"            %s", del_key_string[i].c_str());
        }
    }

    fprintf(hpp_file, u8"        }\r\n");

    key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"        QuickMapping%zu(tmp_data_map%zu);\r\n", key_count, key_count);
        fprintf(hpp_file, u8"        ReleaseMapping%zu();\r\n", key_count);
        fprintf(hpp_file, u8"        m_data_map%zu = tmp_data_map%zu;\r\n", key_count, key_count);
    }
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"        RegLuaDelegate();\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
    fprintf(hpp_file, u8"        return true;\r\n");

    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_OrderAttribute(FILE* hpp_file)
{
    fprintf(hpp_file, u8"    pugi::xml_attribute order_attribute(const char* attr_name, pugi::xml_node & element, pugi::xml_attribute * cur_attr)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        pugi::xml_attribute xml_attr = *cur_attr;\r\n");
    fprintf(hpp_file, u8"        if (!xml_attr)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            *cur_attr = element.first_attribute();\r\n");
    fprintf(hpp_file, u8"            xml_attr = *cur_attr;\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        do\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            if (!strcmp(xml_attr.name(), attr_name))\r\n");
    fprintf(hpp_file, u8"            {\r\n");
    fprintf(hpp_file, u8"                *cur_attr = xml_attr.next_attribute();\r\n");
    fprintf(hpp_file, u8"                return xml_attr;\r\n");
    fprintf(hpp_file, u8"            }\r\n");
    fprintf(hpp_file, u8"            xml_attr = xml_attr.next_attribute();\r\n");
    fprintf(hpp_file, u8"            if (!xml_attr)\r\n");
    fprintf(hpp_file, u8"            {\r\n");
    fprintf(hpp_file, u8"                xml_attr = element.first_attribute();\r\n");
    fprintf(hpp_file, u8"            }\r\n");
    fprintf(hpp_file, u8"        } while (xml_attr != (*cur_attr));\r\n");
    fprintf(hpp_file, u8"        return pugi::xml_attribute();\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_ReLoad( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    bool ReLoad(const char* path, char* err, size_t err_len)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        pugi::xml_document doc;\r\n");
    fprintf(hpp_file, u8"        pugi::xml_parse_result rs = doc.load_file(path);\r\n");
    fprintf(hpp_file, u8"        if (rs.status != pugi::xml_parse_status::status_ok)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            snprintf(err, err_len, u8\"%%s : %%s\", path, rs.description());\r\n");
    fprintf(hpp_file, u8"            return false;\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        return ReLoadEx(doc, err, err_len);\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_Construct_Destruct( FILE* hpp_file )
{
    fprintf(hpp_file, u8"    %s(void)\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"        m_data_map%zu = 0;\r\n", key_count);
    }
    
    //fprintf(hpp_file, u8"        m_data_arry = 0;\r\n");
    //fprintf(hpp_file, u8"        m_data_arry_size = 0;\r\n");
    //fprintf(hpp_file, u8"        InitColInfo();\r\n");
    fprintf(hpp_file, u8"        m_data_array.clear();\r\n");
    fprintf(hpp_file, u8"    }\r\n");

    fprintf(hpp_file, u8"    ~%s(void)\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"    {\r\n");
    //fprintf(hpp_file, u8"        UnInitColInfo();\r\n");
    fprintf(hpp_file, u8"        Release();\r\n");
    fprintf(hpp_file, u8"    }\r\n");
}

void CTableMaker::_print_func_InitColInfo_UnInitColInfo_GetColVar( FILE* hpp_file )
{
    //fprintf(hpp_file, u8"    void InitColInfo(void)\r\n");
    //fprintf(hpp_file, u8"    {\r\n");
    //fprintf(hpp_file, u8"        m_col_info_map = create_rb_tree(0);\r\n");

    //for (table_column_info::iterator it = m_table_column.begin();
    //    it != m_table_column.end(); ++it)
    //{
    //    column_info& info = it->second;

    //    std::string type;

    //    switch (info.m_data_type)
    //    {
    //    case col_string:
    //        type = "col_string";
    //        break;
    //    case col_uint8:
    //        type = "col_uint8";
    //        break;
    //    case col_int8:
    //        type = "col_int8";
    //        break;
    //    case col_uint16:
    //        type = "col_uint16";
    //        break;
    //    case col_int16:
    //        type = "col_int16";
    //        break;
    //    case col_uint32:
    //        type = "col_uint32";
    //        break;
    //    case col_int32:
    //        type = "col_int32";
    //        break;
    //    case col_uint64:
    //        type = "col_uint64";
    //        break;
    //    case col_int64:
    //        type = "col_int64";
    //        break;
    //    }

    //    fprintf(hpp_file, "        add_col_info(m_col_info_map, \"%s\", offsetof(%s, %s::%s), %s);\r\n",
    //        info.m_col_name.c_str(), m_struct_name.c_str(), m_struct_name.c_str(), info.m_col_name.c_str(), type.c_str());
    //}



    //fprintf(hpp_file, u8"    }\r\n");



    //fprintf(hpp_file, u8"    void UnInitColInfo(void)\r\n");
    //fprintf(hpp_file, u8"    {\r\n");

    //for (table_column_info::iterator it = m_table_column.begin();
    //    it != m_table_column.end(); ++it)
    //{
    //    column_info& info = it->second;

    //    fprintf(hpp_file, "        del_col_info(m_col_info_map, \"%s\");\r\n", info.m_col_name.c_str());
    //}


    //fprintf(hpp_file, u8"        destroy_rb_tree(m_col_info_map);\r\n");
    //fprintf(hpp_file, u8"    }\r\n");


    //fprintf(hpp_file, "    col_var GetColVar(%s* row, const char* col_name)\r\n", m_struct_name.c_str());
    //fprintf(hpp_file, "    {\r\n");
    //fprintf(hpp_file, "        col_var var;\r\n");
    //fprintf(hpp_file, "        col_var_info* info = get_col_info(m_col_info_map, col_name);\r\n");
    //fprintf(hpp_file, "        if (info)\r\n");
    //fprintf(hpp_file, "        {\r\n");
    //fprintf(hpp_file, "            var.col_var_ptr = (char*)(row)+info->col_var_offset;\r\n");
    //fprintf(hpp_file, "            var.col_var_type = info->col_var_type;\r\n");
    //fprintf(hpp_file, "        }\r\n");
    //fprintf(hpp_file, "        else\r\n");
    //fprintf(hpp_file, "        {\r\n");
    //fprintf(hpp_file, "            var.col_var_ptr = 0;\r\n");
    //fprintf(hpp_file, "            var.col_var_type = col_null;\r\n");
    //fprintf(hpp_file, "        }\r\n");
    //fprintf(hpp_file, "        return var;\r\n");
    //fprintf(hpp_file, "    }\r\n");
}

void CTableMaker::_print_func_RegLuaDelegate(FILE* hpp_file)
{
    if (m_table_key.empty())
    {
        return;
    }
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"    void RegLuaDelegate(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        m_lua_data_map.Set();\r\n");

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        std::string get_func_name = u8"GetBy";

        key_info& k_info = it->second;

        size_t key_idx = 0;
        for (std::list<std::string>::iterator it_key = k_info.m_key_order.begin();
            it_key != k_info.m_key_order.end(); ++it_key, ++key_idx)
        {
            table_column_info::iterator it_col = m_table_column.find(*it_key);
            std::list<std::string>::iterator it_key_next = it_key;
            ++it_key_next;
            column_info& info = it_col->second;

            get_func_name += info.m_col_name;
        }

        fprintf(hpp_file, u8"        lua_pushcfunction(LuaState, %s);\r\n", get_func_name.c_str());
        fprintf(hpp_file, u8"        lua_setfield(LuaState, -2, \"%s\");\r\n", get_func_name.c_str());
    }

    fprintf(hpp_file, u8"        lua_pushcfunction(LuaState, GetTableData);\r\n");
    fprintf(hpp_file, u8"        lua_setfield(LuaState, -2, \"GetTableData\");\r\n");

    fprintf(hpp_file, u8"        lua_pop(LuaState, 1);\r\n");

    fprintf(hpp_file, u8"        Lua::ReadOnly::SetReadOnly(m_lua_data_map);\r\n");

    fprintf(hpp_file, u8"        m_lua_data_map.Set();\r\n");

    fprintf(hpp_file, u8"        lua_setglobal(LuaState, \"%s\");\r\n", m_class_name.c_str());

    fprintf(hpp_file, u8"    }\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
}

void CTableMaker::_print_func_ReLoadLua(FILE* hpp_file)
{
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"    void ReLoadLua(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        m_lua_data_map.Disable();\r\n");
    fprintf(hpp_file, u8"        m_lua_data_map = Lua::Table::NewTable(LuaState);\r\n");
    //fprintf(hpp_file, u8"        for (size_t i = 0; i < m_data_arry_size; i++)\r\n");
    fprintf(hpp_file, u8"        for (auto& row : m_data_array)\r\n");
    fprintf(hpp_file, u8"        {\r\n");
    //fprintf(hpp_file, u8"            %s* row = m_data_arry[i];\r\n", m_struct_name.c_str());
    fprintf(hpp_file, u8"            Lua::Table lua_row = Lua::Table::NewTable(LuaState);\r\n");
    fprintf(hpp_file, u8"            FillLuaData(lua_row, row);\r\n");

    size_t key_count = 1;
    for (table_key_info::iterator it = m_table_key.begin();
        it != m_table_key.end(); ++it, ++key_count)
    {
        fprintf(hpp_file, u8"            FillLuaMapping%zu(m_lua_data_map, lua_row, row);\r\n", key_count);
    }
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        RegLuaDelegate();\r\n");
    fprintf(hpp_file, u8"    }\r\n");

    fprintf(hpp_file, u8"    void ResetLua(void)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        m_lua_data_map.Reset();\r\n");
    fprintf(hpp_file, u8"    }\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
}

void CTableMaker::_print_func_Lua_TableData(FILE* hpp_file)
{
    fprintf(hpp_file, u8"#if defined(%s_TABLE_TO_LUA) || defined(TABLE_TO_LUA)\r\n",
        str_toupper(m_table_name).c_str());
    fprintf(hpp_file, u8"    static int GetTableData(lua_State* state)\r\n");
    fprintf(hpp_file, u8"    {\r\n");
    fprintf(hpp_file, u8"        Lua::Table tbl = Lua::Table::NewTable(state);\r\n");
    fprintf(hpp_file, u8"        if (%s::Instance())\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"        {\r\n");
    fprintf(hpp_file, u8"            for (size_t i = 0; i < %s::Instance()->GetSize(); i++)\r\n", m_class_name.c_str());
    fprintf(hpp_file, u8"            {\r\n");
    fprintf(hpp_file, u8"                Lua::Table row = Lua::Table::NewTable(state);\r\n");
    fprintf(hpp_file, u8"                %s::Instance()->FillLuaData(row, %s::Instance()->At(i));\r\n", m_class_name.c_str(), m_class_name.c_str());
    fprintf(hpp_file, u8"                tbl[i+1] = row;\r\n");
    fprintf(hpp_file, u8"            }\r\n");
    fprintf(hpp_file, u8"        }\r\n");
    fprintf(hpp_file, u8"        tbl.Set();\r\n");
    fprintf(hpp_file, u8"        return 1;\r\n");
    fprintf(hpp_file, u8"    }\r\n");
    fprintf(hpp_file, u8"#endif\r\n");
}





