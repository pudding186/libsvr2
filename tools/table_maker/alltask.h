#pragma once
#include "TaskManager.h"
#include "markupstl.h"
#include "../../include/utility.hpp"
#include "../../include/timer.h"
#include "TableMakerDlg.h"
#include <vector>
#include <string>
#include "../../../dmxlsx/include/dmxlsx.h"

extern wchar_t* U8ToUnicode(const char* szU8);
extern char* UnicodeToU8(const wchar_t* wszString);
extern char* UnicodeToLocal(const wchar_t* wszString);
extern wchar_t* LocalToUnicode(const char* szLocal);

class InitTask:
    public ITask
{
public:

    InitTask(unsigned int tick)
    {
        m_elapse = tick;
    }

    virtual void OnProc(TaskProc* proc) override
    {
        CMarkupSTL xml;
        bool aa = xml.Load("./table_gen_cfg.xml");

        xml.ResetMainPos();

        if (xml.FindElem("path"))
        {
            xml.IntoElem();
            xml.ResetMainPos();
            if (xml.FindElem("excel_path"))
            {
                m_excel_path = xml.GetAttrib("name");
            }
            xml.ResetMainPos();
            if (xml.FindElem("code_path"))
            {
                m_code_path = xml.GetAttrib("name");
            }
            xml.ResetMainPos();
            if (xml.FindElem("xml_path"))
            {
                m_xml_path = xml.GetAttrib("name");
            }
            xml.OutOfElem();
        }

        m_excel_file_list.clear();

        const char* ptr = m_excel_path.c_str() + m_excel_path.length() - 1;
        if (*ptr == '\\')
        {
            *(char*)ptr = '/';
        }
        else if (*ptr != '/')
        {
            m_excel_path.append("/");
        }

        ptr = m_code_path.c_str() + m_code_path.length() - 1;

        if (*ptr == '\\')
        {
            *(char*)ptr = '/';
        }
        else if (*ptr != '/')
        {
            m_code_path.append("/");
        }

        ptr = m_xml_path.c_str() + m_xml_path.length() - 1;
        if (*ptr == '\\')
        {
            *(char*)ptr = '/';
        }
        else if (*ptr != '/')
        {
            m_xml_path.append("/");
        }

        wchar_t* w_excel_path = U8ToUnicode(m_excel_path.c_str());
        for_each_wfile(w_excel_path, wopenfile, 0, this);
        delete[] w_excel_path;

    }

    static bool wopenfile(const wchar_t* path, const wchar_t* name, void* user_data)
    {
        InitTask* it = (InitTask*)user_data;

        const wchar_t* ptr = wcsstr(name, L".xlsx");

        size_t file_len = wcslen(name);
        if (ptr)
        {
            if ((ptr + wcslen(L".xlsx")) == (name + file_len))
            {
                it->m_excel_file_list.push_back(name);
            }
        }

        return true;
    }

    virtual void OnResult() override
    {
        wchar_t* w_excel_path = U8ToUnicode(m_excel_path.c_str());
        std::wstring wstr_excel_path = w_excel_path;
        delete[] w_excel_path;

        wchar_t* w_code_path = U8ToUnicode(m_code_path.c_str());
        std::wstring wstr_code_path = w_code_path;
        delete[] w_code_path;

        wchar_t* w_xml_path = U8ToUnicode(m_xml_path.c_str());
        std::wstring wstr_xml_path = w_xml_path;
        delete[] w_xml_path;

        CTableMakerDlg* dlg = dynamic_cast<CTableMakerDlg*>(AfxGetApp()->m_pMainWnd);
        dlg->UpdateExcelList(get_tick()-m_elapse, m_excel_file_list, wstr_excel_path, wstr_code_path, wstr_xml_path);
    }

protected:
    std::vector<std::wstring> m_excel_file_list;
    std::string m_excel_path;
    std::string m_code_path;
    std::string m_xml_path;
    unsigned int m_elapse;
private:
};


class LoadTask:
    public ITask
{
public:
    LoadTask(int idx, const std::wstring& excel_path, const std::wstring& table_name)
    {
        m_idx = idx;
        m_excel_path = excel_path;
        m_table_name = table_name;
        m_err_info = L"";
        m_table_client = 0;
        m_table_server = 0;
    }

    virtual void OnProc(TaskProc* proc) override
    {
        const char* p = 0;

        p = UnicodeToLocal(m_excel_path.c_str());
        std::string excel_path_local = p;
        delete[] p;
        p = UnicodeToLocal(m_table_name.c_str());
        std::string table_name_local = p;
        delete[] p;

        std::string excel_file_path_local = excel_path_local + table_name_local;

        //table_name_local = table_name_local.substr(0, table_name_local.length() - strlen(".xlsx"));

        std::map<std::string, long> col_name_2_idx;
        std::map<std::string, std::string> data_type_to_c;

        data_type_to_c["UINT8"] = "unsigned char";
        data_type_to_c["INT8"] = "char";

        data_type_to_c["UINT16"] = "unsigned short";
        data_type_to_c["INT16"] = "short";

        data_type_to_c["UINT32"] = "unsigned int";
        data_type_to_c["INT32"] = "int";

        data_type_to_c["UINT64"] = "unsigned long long";
        data_type_to_c["INT64"] = "long long";

        m_elapse = get_tick();

        std::string cur_row_col_name = "";

        try
        {
            XLDocument doc;
            doc.OpenDocument(excel_file_path_local);

            auto def_sheet = doc.Workbook().Worksheet("def");

            long row_count = def_sheet.RowCount();
            long col_count = def_sheet.ColumnCount();

            {
                auto first_row = def_sheet.Row(1);
                for (long col = 1; col < col_count; col++)
                {
                    auto cell = first_row.Cell(col);
                    col_name_2_idx[cell.Value().AsString()] = col;
                }
            }

            {
                std::map<std::string, long>::iterator it_col_check;
                it_col_check = col_name_2_idx.find(u8"字段名");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 字段名 列");
                }
                it_col_check = col_name_2_idx.find(u8"中文");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 中文 列");
                }
                it_col_check = col_name_2_idx.find(u8"数据类型");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 数据类型 列");
                }

                it_col_check = col_name_2_idx.find(u8"主键");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 主键 列");
                }

                it_col_check = col_name_2_idx.find(u8"服务端用");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 服务端用 列");
                }

                it_col_check = col_name_2_idx.find(u8"客户端用");
                if (it_col_check == col_name_2_idx.end())
                {
                    throw std::runtime_error(u8"没有找到 客户端用 列");
                }
            }

            column_info info;
            key_info info_k;

            m_table_server = new CTableMaker(table_name_local.substr(0, table_name_local.length()-strlen(".xlsx")));
            m_table_client = new CTableMaker(table_name_local.substr(0, table_name_local.length() - strlen(".xlsx")));

            for (long row = 2; row <= row_count; row++)
            {
                cur_row_col_name = "";

                auto data_row = def_sheet.Row(row);
                info.m_col_name = data_row.Cell(col_name_2_idx[u8"字段名"]).Value().AsString();

                cur_row_col_name = u8"加载字段:" + info.m_col_name;

                info.m_col_note = data_row.Cell(col_name_2_idx[u8"中文"]).Value().AsString();
                info.m_col_type = data_row.Cell(col_name_2_idx[u8"数据类型"]).Value().AsString();

                int is_key = 0;
                if (data_row.Cell(col_name_2_idx[u8"主键"]).Value().ValueType() != XLValueType::Empty)
                {
                    is_key = 1;
                }

                //int gen_type = 0;
                bool gen_client = false;
                bool gen_server = false;
                if (data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().ValueType() != XLValueType::Empty)
                {
                    if (data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().ValueType() == XLValueType::String)
                    {
                        std::string gen_type_str = data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().AsString();

                        if (gen_type_str.length())
                        {
                            if (std::stoi(gen_type_str) == 1)
                            {
                                gen_server = true;
                            }
                            //gen_type = std::stoi(gen_type_str);
                        }
                    }
                    else if (data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().ValueType() == XLValueType::Integer)
                    {
                        if (data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().Get<int>() == 1)
                        {
                            gen_server = true;
                        }
                        //gen_type = data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().Get<int>();
                    }
                }

                if (data_row.Cell(col_name_2_idx[u8"客户端用"]).Value().ValueType() != XLValueType::Empty)
                {
                    if (data_row.Cell(col_name_2_idx[u8"客户端用"]).Value().ValueType() == XLValueType::String)
                    {
                        std::string gen_type_str = data_row.Cell(col_name_2_idx[u8"客户端用"]).Value().AsString();

                        if (gen_type_str.length())
                        {
                            if (std::stoi(gen_type_str) == 1)
                            {
                                gen_client = true;
                            }
                            //gen_type = std::stoi(gen_type_str);
                        }
                    }
                    else if (data_row.Cell(col_name_2_idx[u8"客户端用"]).Value().ValueType() == XLValueType::Integer)
                    {
                        if (data_row.Cell(col_name_2_idx[u8"客户端用"]).Value().Get<int>() == 1)
                        {
                            gen_client = true;
                        }
                        //gen_type = data_row.Cell(col_name_2_idx[u8"服务端用"]).Value().Get<int>();
                    }
                }

                if (info.m_col_name != "")
                {
                    std::map<std::string, std::string>::iterator it_data_check = data_type_to_c.find(info.m_col_type);
                    if (it_data_check == data_type_to_c.end())
                    {
                        if (info.m_col_type.find("char[") == std::string::npos)
                        {
                            char err[512];
                            sprintf_s(err, u8"%s type %s 无效的数据类型", info.m_col_name.c_str(), info.m_col_type.c_str());
                            throw std::runtime_error(err);
                        }
                        else
                        {
                            info.m_col_type = "char*";
                            info.m_data_type = col_string;
                        }
                    }
                    else
                    {
                        info.m_col_type = it_data_check->second;


                        if (info.m_col_type.find("unsigned char") != std::string::npos)
                        {
                            info.m_data_type = col_uint8;
                        }
                        else if (info.m_col_type.find("char") != std::string::npos)
                        {
                            info.m_data_type = col_int8;
                        }
                        else if (info.m_col_type.find("unsigned short") != std::string::npos)
                        {
                            info.m_data_type = col_uint16;
                        }
                        else if (info.m_col_type.find("short") != std::string::npos)
                        {
                            info.m_data_type = col_int16;
                        }
                        else if (info.m_col_type.find("unsigned int") != std::string::npos)
                        {
                            info.m_data_type = col_uint32;
                        }
                        else if (info.m_col_type.find("int") != std::string::npos)
                        {
                            info.m_data_type = col_int32;
                        }
                        else if (info.m_col_type.find("unsigned long") != std::string::npos)
                        {
                            info.m_data_type = col_uint64;
                        }
                        else if (info.m_col_type.find("long") != std::string::npos)
                        {
                            info.m_data_type = col_int64;
                        }
                    }
                }
                else
                {
                    char err[512];
                    sprintf_s(err, u8"读取字段名失败! 行数%d", row);
                    throw std::runtime_error(err);
                }

                if (is_key == 1)
                {
                    info_k.m_key_list.insert(info.m_col_name);
                    info_k.m_key_order.push_back(info.m_col_name);
                }
                else
                {
                    if (info.m_col_name == "KeyName" &&
                        info.m_col_type == "char*")
                    {
                        key_info info_key_name;
                        info_key_name.m_key_list.insert(info.m_col_name);
                        info_key_name.m_key_order.push_back(info.m_col_name);
                        m_table_server->add_key(info_key_name);
                        m_table_client->add_key(info_key_name);
                    }
                }

                if (gen_client)
                {

                    std::string err_info = m_table_client->add_column(info);
                    if (err_info.length())
                    {
                        std::runtime_error(err_info.c_str());
                    }
                }

                if (gen_server)
                {
                    std::string err_info = m_table_server->add_column(info);
                    if (err_info.length())
                    {
                        std::runtime_error(err_info.c_str());
                    }
                }

            }

            if (info_k.m_key_list.size() != info_k.m_key_order.size())
            {
                throw std::runtime_error(u8"key 字段有重复");
            }
            m_table_server->add_key(info_k);
            m_table_client->add_key(info_k);

        }
        catch (std::runtime_error& e)
        {
            if (m_table_client)
            {
                delete m_table_client;
            }

            if (m_table_server)
            {
                delete m_table_server;
            }
            
            m_table_client = 0;
            m_table_server = 0;

            if (cur_row_col_name.length())
            {
                cur_row_col_name += u8"失败! ";
            }
            
            cur_row_col_name += e.what();

            wchar_t* werr = U8ToUnicode(cur_row_col_name.c_str());
            m_err_info = werr;
            delete[] werr;
        }

        m_elapse = get_tick() - m_elapse;
    }

    virtual void OnResult() override
    {
        CTableMakerDlg* dlg = dynamic_cast<CTableMakerDlg*>(AfxGetApp()->m_pMainWnd);

        dlg->AddExcelResult(
            m_idx,
            m_err_info, 
            m_elapse, 
            m_table_name.substr(0, m_table_name.length() - wcslen(L".xlsx")),
            m_table_server, 
            m_table_client);
    }
protected:
    int         m_idx;
    unsigned int m_elapse;
    std::wstring    m_err_info;
    std::wstring    m_excel_path;
    std::wstring    m_table_name;
    CTableMaker*    m_table_server;
    CTableMaker*    m_table_client;

private:
};

class GenCodeTask:
    public ITask
{
public:
    GenCodeTask(int idx, const std::wstring& code_path, const std::wstring& table_name, CTableMaker* table_maker)
    {
        m_idx = idx;
        m_code_path = code_path;
        m_table_name = table_name;
        m_table_maker = table_maker;
    }

    virtual void OnProc(TaskProc* proc) override
    {
        unsigned int check_tick = get_tick();

        char* psz = UnicodeToLocal(m_code_path.c_str());
        std::string code_path_local = psz;
        delete[] psz;
        std::string err;

        if (m_table_maker)
        {
            if (m_table_maker->gen_code(code_path_local, err))
            {
                m_err_msg = L"";
            }
            else
            {
                wchar_t* wsz_err = LocalToUnicode(err.c_str());
                m_err_msg = wsz_err;
                delete[] wsz_err;
            }
        }
        else
        {
            m_err_msg = L"请先确认 ";
            m_err_msg += m_table_name;
            m_err_msg += L".xlsx 加载成功";
        }

        m_real_elapse = (float)(get_tick() - check_tick);
    }

    virtual void OnResult() override
    {
        CTableMakerDlg* dlg = dynamic_cast<CTableMakerDlg*>(AfxGetApp()->m_pMainWnd);
        dlg->AddCodeResult(m_idx, m_err_msg, m_real_elapse, m_table_name);
    }
protected:
    std::wstring    m_err_msg;
    std::wstring    m_code_path;
    std::wstring    m_table_name;
    CTableMaker*    m_table_maker;
    int             m_idx;
    float           m_real_elapse;
private:
};

class GenXmlTask:
    public ITask
{
public:
    //GenXmlTask(int idx, const std::wstring& xml_path, const std::wstring& table_name)
    GenXmlTask(int idx, const std::wstring& excel_path, const std::wstring& table_name, const std::wstring& xml_path, CTableMaker* table_maker)
    {
        m_excel_path = excel_path;
        m_table_name = table_name;
        m_xml_path = xml_path;
        m_table_maker = table_maker;
        m_idx = idx;
    }

    virtual void OnProc(TaskProc* proc) override
    {
        unsigned int check_tick = get_tick();
        FILE* xml_file = 0;
        try
        {
            if (!m_table_maker)
            {
                m_err_info = L"请先确认 ";
                m_err_info += m_table_name;
                m_err_info += L".xlsx 加载成功";
                m_real_elapse = 0.0f;
                return;
            }
            std::map<std::string, long> col_name_2_idx;
            std::map<long, std::string> col_idx_2_name;

            std::wstring excel_file_path = m_excel_path + m_table_name + L".xlsx";

            char* sz_tmp = UnicodeToLocal(excel_file_path.c_str());
            std::string excel_file_path_local = sz_tmp;
            delete[] sz_tmp;

            XLDocument doc;
            doc.OpenDocument(excel_file_path_local);

            auto content_sheet = doc.Workbook().Worksheet("content");

            long row_count = content_sheet.RowCount();
            long col_count = content_sheet.ColumnCount();


            {
                auto first_row = content_sheet.Row(1);
                for (long col = 1; col <= col_count; col++)
                {
                    auto cell = first_row.Cell(col);

                    if (cell.Value().AsString() == u8"")
                    {
                        char err[512];
                        sprintf_s(err, u8"content 第 %d 列 是空值", col);
                        throw std::runtime_error(err);
                    }

                    if (col_name_2_idx.find(cell.Value().AsString()) != col_name_2_idx.end())
                    {
                        char err[512];
                        sprintf_s(err, u8"content 第 %d 列 <%s> 有重复", col, cell.Value().AsString().c_str());
                        throw std::runtime_error(err);
                    }

                    col_name_2_idx[cell.Value().AsString()] = col;
                    col_idx_2_name[col] = cell.Value().AsString();
                }
            }

            if (col_name_2_idx.size() != col_idx_2_name.size())
            {
                throw std::runtime_error(u8"有重复的数据列");
            }

            table_column_info col_info = m_table_maker->get_table_columns();

            for (auto it = col_info.begin();
                it != col_info.end(); ++it)
            {
                column_info& info = it->second;

                if (col_name_2_idx.find(info.m_col_name) == col_name_2_idx.end())
                {
                    std::string err = u8"content 中没有" + info.m_col_name + u8"列数据";
                    throw std::runtime_error(err.c_str());
                }
            }

            std::wstring xml_file_path = m_xml_path + m_table_name + L".xml";

            xml_file = _wfsopen(xml_file_path.c_str(), L"wb", _SH_DENYWR);
            if (!xml_file)
            {
                char* tmp = UnicodeToU8(xml_file_path.c_str());
                std::string path = tmp;
                delete[] tmp;

                std::string err = u8"打开" + path + u8"失败:" + strerror(errno);
                throw std::runtime_error(err.c_str());
            }

            fprintf(xml_file, u8"<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\r\n");
            fprintf(xml_file, u8"<root>\r\n");

            std::vector<std::string> row_content;
            row_content.reserve(col_name_2_idx.size());

            for (long row = 2; row <= row_count; row++)
            {
                row_content.clear();

                for (std::map<long, std::string>::iterator it = col_idx_2_name.begin();
                    it != col_idx_2_name.end(); ++it)
                {
                    auto it_col = col_info.find(it->second);
                    if (it_col == col_info.end())
                    {
                        continue;
                    }

                    std::string value = content_sheet.Row(row).Cell(it->first).Value().AsString();

                    if (m_table_maker->is_key_column(it_col->second))
                    {
                        if (value.empty() || value == "")
                        {
                            char err[512];
                            sprintf_s(err, u8"主键 %s ! 不能为空 行数%d",it->second.c_str(), row);
                            throw std::runtime_error(err);
                        }
                    }

                    std::string content = it->second + u8"=\"" + value + u8"\" ";

                    if (content.find("EXCEL_STRING") != std::string::npos)
                    {
                        row_content.clear();
                        break;
                    }
                    else
                    {
                        row_content.push_back(content);
                    }
                }

                if (row_content.size())
                {
                    fprintf(xml_file, u8"  <content ");
                }

                for (size_t i = 0; i < row_content.size(); i++)
                {
                    fprintf(xml_file, u8"%s", row_content[i].c_str());
                }

                if (row_content.size())
                {
                    fprintf(xml_file, u8"/>\r\n");
                }
            }

            fprintf(xml_file, u8"</root>\r\n");

            fclose(xml_file);
            m_err_info = L"";

        }
        catch (std::runtime_error& e)
        {
            if (xml_file)
            {
                fclose(xml_file);
            }
            wchar_t* werr = U8ToUnicode(e.what());
            m_err_info = werr;
            delete[] werr;
        }

        m_real_elapse = (float)(get_tick() - check_tick);
    }

    virtual void OnResult() override
    {
        CTableMakerDlg* dlg = dynamic_cast<CTableMakerDlg*>(AfxGetApp()->m_pMainWnd);
        dlg->AddXmlResult(m_idx, m_err_info, m_real_elapse, m_table_name);
    }
protected:
    int             m_idx;
    float           m_real_elapse;
    std::wstring    m_err_info;
    std::wstring    m_excel_path;
    std::wstring    m_table_name;
    std::wstring    m_xml_path;
    CTableMaker*    m_table_maker;
private:
};
