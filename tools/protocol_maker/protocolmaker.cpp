#include "StdAfx.h"
#include "protocolmaker.h"
//#include <string>
//
#define FIND_ELEM(elem)             oXml.ResetMainPos();\
                                    if (!oXml.FindElem(elem))\
                                    {\
                                        m_strErrorInfo = "Can't Find Element <elem>";\
                                        goto ERROR_DEAL;\
                                    }

#define GET_ATTRIB(attrib)          strVal = oXml.GetAttrib(attrib);\
                                    if (strVal.empty())\
                                    {\
                                        m_strErrorInfo = "Can't Find Attrib attrib";\
                                        goto ERROR_DEAL;\
                                    }

#define INTO_ELEM()                 if (!oXml.IntoElem())\
                                        goto ERROR_DEAL;

#define OUTOF_ELEM()                if (!oXml.OutOfElem())\
                                        goto ERROR_DEAL;

#define ERRORINFO_DATA(strType,strName,strComment)\
                                    m_strErrorInfo = "<data ";\
                                    if (!strType.empty())\
                                    {\
                                        m_strErrorInfo +="type=\""+strType+"\" ";\
                                    }\
                                    if (!strName.empty())\
                                    {\
                                        m_strErrorInfo +="name=\""+strName+"\" ";\
                                    }\
                                    if (!strComment.empty())\
                                    {\
                                        m_strErrorInfo +="comment=\""+strComment+"\" ";\
                                    }\
                                    m_strErrorInfo += ">\r\n";

#define ERRORINFO_ITEM(strName, strType, strCount, strRefer, strId, strSelect, strLength, strComment, strArray)\
                                    m_strErrorInfo +="\t<item ";\
                                    if (!strName.empty())\
                                    {\
                                        m_strErrorInfo += "name=\""+strName+"\" ";\
                                    }\
                                    if (!strType.empty())\
                                    {\
                                        m_strErrorInfo += "type=\""+strType+"\" ";\
                                    }\
                                    if (!strCount.empty())\
                                    {\
                                        m_strErrorInfo += "count=\""+strCount+"\" ";\
                                    }\
                                    if (!strRefer.empty())\
                                    {\
                                        m_strErrorInfo += "refer=\""+strRefer+"\" ";\
                                    }\
                                    if (!strId.empty())\
                                    {\
                                        m_strErrorInfo += "id=\""+strId+"\" ";\
                                    }\
                                    if (!strSelect.empty())\
                                    {\
                                        m_strErrorInfo += "select=\""+strSelect+"\" ";\
                                    }\
                                    if (!strLength.empty())\
                                    {\
                                    m_strErrorInfo += "length=\""+strLength+"\" ";\
                                    }\
                                    if (!strComment.empty())\
                                    {\
                                        m_strErrorInfo += "comment=\""+strComment+"\" ";\
                                    }\
                                    if (!strArray.empty())\
                                    {\
                                        m_strErrorInfo += "comment=\""+strArray+"\" ";\
                                    }\
                                    m_strErrorInfo +="/>\r\n";


std::string __integral_to_c_type(std::string & str_integral)
{
    if (str_integral == "uint8")
    {
        return "unsigned char";
    }
    else if (str_integral == "int8")
    {
        return "char";
    }
    else if (str_integral == "uint16")
    {
        return "unsigned short";
    }
    else if (str_integral == "int16")
    {
        return "short";
    }
    else if (str_integral == "uint32")
    {
        return "unsigned int";
    }
    else if (str_integral == "int32")
    {
        return "int";
    }
    else if (str_integral == "uint64")
    {
        return "unsigned long long";
    }
    else if (str_integral == "int64")
    {
        return "long long";
    }

    return "";
}

bool __is_integral(std::string & strType)
{
    if (strType == "uint8" ||
        strType == "int8" ||
        strType == "uint16" ||
        strType == "int16" ||
        strType == "uint32" ||
        strType == "int32" ||
        strType == "uint64" ||
        strType == "int64")
    {
        return true;
    }

    return false;
}

CProtocolMaker::CProtocolMaker(void)
{
    
}

CProtocolMaker::~CProtocolMaker(void)
{
}

bool CProtocolMaker::MakeProtocol(const std::string& strXML, const std::string& strOutPutPath, const std::string& strPrefix)
{
    CMarkupSTL oXml;
    std::string strVal;
    std::string strHppFile;
    std::string strCppFile;
    std::string strMouleName;
    FILE* pHppFile = NULL;
    FILE* pCppFile = NULL;


    if (!oXml.Load(strXML.c_str()))
    {
        m_strErrorInfo = "加载协议文件"+strXML+"失败! ";
        m_strErrorInfo += oXml.GetError();
        goto ERROR_DEAL;
    }

    //获取协议模块名作为文件名
    FIND_ELEM("protocol_define");
    GET_ATTRIB("name");

    strHppFile = strOutPutPath + strPrefix + strVal + ".hpp";
    strCppFile = strOutPutPath + strPrefix + strVal + ".cpp";
    pHppFile = fopen(strHppFile.c_str(), "wb");
    pCppFile = fopen(strCppFile.c_str(), "wb");
    if (NULL == pHppFile || NULL == pCppFile)
        goto ERROR_DEAL;



    {
        //utf8-bom
        char bom[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
        fwrite(bom, 1, 3, pHppFile);
        fwrite(bom, 1, 3, pCppFile);
    }

    fprintf(pHppFile, u8"#pragma once\r\n\r\n");
    fprintf(pHppFile, u8"#include \"net_data.hpp\"\r\n\r\n");
    fprintf(pCppFile, u8"#include <new>\r\n\r\n");
    fprintf(pCppFile, u8"#include \"%s.hpp\"\r\n\r\n", (strPrefix + strVal).c_str());

    strMouleName = strVal;

    GET_ATTRIB("moudleid");
    m_strMoudleID = strVal;
    if (!__IsAllDigit(m_strMoudleID))
    {
        m_strErrorInfo="moulleid is not digital!";
        goto ERROR_DEAL;
    }

    INTO_ELEM();

    if (!__WritePackge(pHppFile, oXml, strOutPutPath))
        goto ERROR_DEAL;

    if (!strPrefix.empty())
    {
        fprintf(pHppFile, u8"namespace %s {\r\n\r\n", strPrefix.c_str());
    }

    if (!__WriteMacro(pHppFile, oXml))
        goto ERROR_DEAL;

    if (!__WriteData(pHppFile, pCppFile, oXml))
        goto ERROR_DEAL;

    //if (!__WriteDynamicProtocol(pHppFile, oXml))
    //    goto ERROR_DEAL;
    //if (!__WriteDataFunction(pHppFile, pCppFile, oXml))
    //    goto ERROR_DEAL;
    OUTOF_ELEM();

    if (!__WriteProtocolClass(strMouleName, pHppFile))
    {
        goto ERROR_DEAL;
    }

    if (!strPrefix.empty())
    {
        fprintf(pHppFile, u8"}\r\n");
    }

    return true;

ERROR_DEAL:
    return false;
}


bool CProtocolMaker::__IsKeyType( const std::string& strType )
{
    //if (strType == "char"
    //    || strType == "byte"
    //    || strType == "short"
    //    || strType == "word"
    //    || strType == "int"
    //    || strType == "int64"
    //    || strType == "dword"
    //    || strType == "qword"
    //    || strType == "string")
    if (strType == "int8"
        || strType == "uint8"
        || strType == "int16"
        || strType == "uint16"
        || strType == "int32"
        || strType == "int64"
        || strType == "uint32"
        || strType == "uint64"
        || strType == "string")
    {
        return true;
    }

    return false;
}

bool CProtocolMaker::__IsStruct( const std::string& strType )
{
    CData::iterator it = m_mapStruct.find(strType);
    if (it == m_mapStruct.end())
    {
        return false;
    }

    return true;
}

bool CProtocolMaker::__IsUnion( const std::string& strType )
{
    CData::iterator it = m_mapUnion.find(strType);
    if (it == m_mapUnion.end())
    {
        return false;
    }

    return true;
}

bool CProtocolMaker::__IsMacro( const std::string& strType )
{
    CDataSet::iterator it = m_setMacro.find(strType);
    if (it == m_setMacro.end())
    {
        return false;
    }

    return true;
}

bool CProtocolMaker::__IsAllDigit( const std::string& strTest )
{
    if (strTest.empty())
    {
        return false;
    }

    const char* pTr = strTest.c_str();

    for (size_t i = 0; i < strTest.length(); i++)
    {
        if (!isdigit(pTr[i]))
        {
            return false;
        }
    }

    return true;
}

bool CProtocolMaker::__CheckPackge( const std::string& strPkg )
{
    CMarkupSTL oXml;
    std::string strVal;
    //std::string strHppFile;
    //std::string strCppFile;
    //std::string strMouleName;
    //FILE* pHppFile = NULL;
    //FILE* pCppFile = NULL;


    if (!oXml.Load(strPkg.c_str()))
    {
        m_strErrorInfo = "加载协议文件"+strPkg+"失败!";
        goto ERROR_DEAL;
    }

    CMarkupSTL& rXml = oXml;

    FIND_ELEM("protocol_define");
    INTO_ELEM();

    //查找宏定义
    rXml.ResetMainPos();
    while (rXml.FindElem("macro"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strValue = rXml.GetAttrib("value");
        std::string strComment = rXml.GetAttrib("comment");
        if (strName.empty() ||strValue.empty())
        {
            m_strErrorInfo = "宏定义错误 name 或 value 属性为空";
            return false;
        }
        m_setMacro.insert(strName);
    }
    rXml.ResetMainPos();
    while (rXml.FindElem("data"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strComment = rXml.GetAttrib("comment");
        EDataType eDataType = eStruct;

        ERRORINFO_DATA(strType, strName, strComment);

        if (strType == "struct")
        {
            if (m_mapStruct.find(strName) != m_mapStruct.end())
            {
                m_strErrorInfo += "重定义";
                return false;
            }
            eDataType = eStruct;
        }
        else if (strType == "protocol")
        {
            continue;
        }
        else
        {
            m_strErrorInfo += "type属性的类型未知，只能定义为struct";
            return false;
        }

        if (!rXml.IntoElem())
        {
            m_strErrorInfo += "进入子级失败";
            return false;
        }

        CItem mapItem;

        while (rXml.FindElem("item"))
        {
            CAttrib vecAttrib(8);
            vecAttrib[eName] = rXml.GetAttrib("name");
            vecAttrib[eType] = rXml.GetAttrib("type");
            vecAttrib[eCount] = rXml.GetAttrib("count");
            vecAttrib[eRefer] = rXml.GetAttrib("refer");
            vecAttrib[eId] = rXml.GetAttrib("id");
            vecAttrib[eSelect] = rXml.GetAttrib("select");
            vecAttrib[eComment] = rXml.GetAttrib("comment");
            vecAttrib[eLength] = rXml.GetAttrib("length");

            CItem::iterator it  = mapItem.find(vecAttrib[eName]);
            if (it != mapItem.end())
            {
                m_strErrorInfo += "name属性值重复";
                return false;
            }
            else
                mapItem[vecAttrib[eName]] = vecAttrib;
        }

        if (!rXml.OutOfElem())
        {
            m_strErrorInfo += "从子集返回失败";
            return false;
        }

        if (strType == "struct")
        {
            m_mapStruct[strName] = mapItem;
        }
        else
        {
            return false;
        }
    }
    OUTOF_ELEM();

    

    return true;
ERROR_DEAL:
    return false;
}

bool CProtocolMaker::__WritePackge( FILE* pHppFile, CMarkupSTL& rXml, const std::string& strPackgePath )
{
    //查找所有数据定义
    fprintf(pHppFile, u8"//===============包含的其他协议文件===============\r\n");
    rXml.ResetMainPos();
    while (rXml.FindElem("package"))
    {
        std::string strPackage = rXml.GetAttrib("name");

        if (!__CheckPackge(strPackgePath+strPackage+".xml"))
        {
            return false;
        }

        fprintf(pHppFile, u8"#include \"%s.hpp\"\r\n ", strPackage.c_str());
    }

    return true;
}

bool CProtocolMaker::__WriteMacro( FILE* pHppFile, CMarkupSTL& rXml )
{
    //查找所有的宏定义
    fprintf(pHppFile, u8"//===============宏定义开始===============\r\n");
    rXml.ResetMainPos();
    while (rXml.FindElem("macro"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strValue = rXml.GetAttrib("value");
        std::string strComment = rXml.GetAttrib("comment");
        if (strName.empty() ||strValue.empty())
        {
            m_strErrorInfo = "宏定义错误 name 或 value 属性为空";
            return false;
        }

        if (strComment.empty())
            fprintf(pHppFile, u8"#define %-*s %s\r\n", 30, strName.c_str(), strValue.c_str());
        else
            fprintf(pHppFile, u8"#define %-*s %s //%s\r\n", 30, strName.c_str(), strValue.c_str(), strComment.c_str());
        m_setMacro.insert(strName);
    }
    fprintf(pHppFile, u8"//===============宏定义结束===============\r\n\r\n");
    return true;
}

bool CProtocolMaker::__WriteHandler(CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile)
{
    std::string strName = rXml.GetAttrib("name");
    std::string strHandlerName = strName + u8"::Handler";

    //std::string strType = rXml.GetAttrib("type");
    //std::string strComment = rXml.GetAttrib("comment");

    if (!rXml.IntoElem())
    {
        m_strErrorInfo += "进入子级失败";
        return false;
    }

    std::vector<CAttrib> m_item_list;

    std::vector<CAttrib> m_number_list;
    std::vector<CAttrib> m_string_list;
    std::vector<CAttrib> m_array_list;
    std::vector<CAttrib> m_array_old_list;
    std::vector<CAttrib> m_struct_list;
    //std::vector<CAttrib> m_str_def_list;
    //std::vector<CAttrib> m_int_def_list;
    std::map<std::string, CAttrib> m_def_list;

    int idx = 0;
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        CAttrib vecAttrib(eMax);
        vecAttrib[eName] = rXml.GetAttrib("name");
        vecAttrib[eType] = rXml.GetAttrib("type");
        vecAttrib[eCount] = rXml.GetAttrib("count");
        vecAttrib[eRefer] = rXml.GetAttrib("refer");
        vecAttrib[eId] = rXml.GetAttrib("id");
        vecAttrib[eSelect] = rXml.GetAttrib("select");
        vecAttrib[eComment] = rXml.GetAttrib("comment");
        vecAttrib[eLength] = rXml.GetAttrib("length");
        vecAttrib[eArray] = rXml.GetAttrib("array");
        vecAttrib[eAlias] = rXml.GetAttrib("alias");
        vecAttrib[eDefault] = rXml.GetAttrib("default");

        char idx_buff[32];
        snprintf(idx_buff, sizeof(idx_buff), "%d", idx);
        vecAttrib[eIndex] = idx_buff;

        std::string strName = vecAttrib[eName];
        std::string strType = vecAttrib[eType];
        std::string strCount = vecAttrib[eCount];
        std::string strArray = vecAttrib[eArray];
        std::string strDefault = vecAttrib[eDefault];

        if (strDefault == "true")
        {
            m_def_list[strName] = vecAttrib;
        }

        if (strType == "string")
        {
            m_string_list.push_back(vecAttrib);
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                m_number_list.push_back(vecAttrib);
            }
            else
            {
                m_struct_list.push_back(vecAttrib);
            }
        }
        else
        {
            if (!strArray.empty())
            {
                if (strType == "uint8" || strType == "int8")
                {
                    m_string_list.push_back(vecAttrib);
                }
                else
                    m_array_list.push_back(vecAttrib);
            }

            if (!strCount.empty())
            {
                m_array_old_list.push_back(vecAttrib);
            }
        }

        m_item_list.push_back(vecAttrib);

        ++idx;
    }
    
    fprintf(pHppFile, u8"\r\n    //===============Json===============\r\n\r\n");

    fprintf(pHppFile, u8"    class Handler :\r\n");
    fprintf(pHppFile, u8"        public JsonHandler\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"    public:\r\n");
    fprintf(pHppFile, u8"        Handler(%s& data, JsonHandler* parent = nullptr)\r\n", strName.c_str());
    fprintf(pHppFile, u8"            :\r\n");
    //fprintf(pHppFile, u8"            m_handler(nullptr),\r\n");
    fprintf(pHppFile, u8"            JsonHandler(parent),\r\n");
    fprintf(pHppFile, u8"            m_data(data),\r\n");

    for (size_t i = 0; i < m_struct_list.size(); i++)
    {
        CAttrib& attr = m_struct_list[i];

        fprintf(pHppFile, u8"            m_%s_handler(m_data.%s, this),\r\n", attr[eName].c_str(), attr[eName].c_str());
    }

    for (size_t i = 0; i < m_array_list.size(); i++)
    {
        CAttrib& attr = m_array_list[i];

        fprintf(pHppFile, u8"            m_%s_handler(m_data.%s, this),\r\n", attr[eName].c_str(), attr[eName].c_str());
    }

    for (size_t i = 0; i < m_array_old_list.size(); i++)
    {
        CAttrib& attr = m_array_old_list[i];

        fprintf(pHppFile, u8"            m_%s_handler(m_data.%s, this, %s),\r\n", 
            attr[eName].c_str(), attr[eName].c_str(), attr[eCount].c_str());
    }

    fprintf(pHppFile, u8"            m_default_handler(this),\r\n");
    fprintf(pHppFile, u8"            m_state(0) { (void)m_data; }\r\n\r\n");
    fprintf(pHppFile, u8"        ~Handler(){}\r\n\r\n");

    fprintf(pHppFile, u8"        void ResetState(void)\r\n");
    fprintf(pHppFile, u8"        {\r\n");
    fprintf(pHppFile, u8"            m_state = 0;\r\n");
    if (m_item_list.size())
    {
        fprintf(pHppFile, u8"            m_assigned_bitset.reset();\r\n");
    }
    
    for (size_t i = 0; i < m_struct_list.size(); i++)
    {
        CAttrib& attr = m_struct_list[i];

        fprintf(pHppFile, u8"            m_%s_handler.ResetState();\r\n", attr[eName].c_str());
    }

    for (size_t i = 0; i < m_array_list.size(); i++)
    {
        CAttrib& attr = m_array_list[i];

        if (!__is_integral(attr[eType]))
        {
            fprintf(pHppFile, u8"            m_%s_handler.ResetState();\r\n", attr[eName].c_str());
        }
    }

    for (size_t i = 0; i < m_array_old_list.size(); i++)
    {
        CAttrib& attr = m_array_old_list[i];

        if (!__is_integral(attr[eType]))
        {
            fprintf(pHppFile, u8"            m_%s_handler.ResetState();\r\n", attr[eName].c_str());
        }
    }

    fprintf(pHppFile, u8"        }\r\n\r\n");

    if (m_item_list.size())
    {
        fprintf(pHppFile, u8"        bool Key(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            if (m_state >= static_cast<unsigned int>(sizeof(m_key_list) / sizeof(m_key_list[0])))\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state = 0;\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            if (strncmp(str, m_alias_list[m_state], length) && \r\n");
        fprintf(pHppFile, u8"                strncmp(str, m_key_list[m_state], length))\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                for (size_t i = 0; i < (sizeof(m_key_list)/sizeof(m_key_list[0])); i++)\r\n");
        fprintf(pHppFile, u8"                {\r\n");
        fprintf(pHppFile, u8"                    if (!strncmp(str, m_alias_list[i], length) ||\r\n");
        fprintf(pHppFile, u8"                        !strncmp(str, m_key_list[i], length))\r\n");
        fprintf(pHppFile, u8"                    {\r\n");
        fprintf(pHppFile, u8"                        m_state = static_cast<int>(i);\r\n");
        fprintf(pHppFile, u8"                        return true;\r\n");
        fprintf(pHppFile, u8"                    }\r\n");
        fprintf(pHppFile, u8"                }\r\n\r\n");
        fprintf(pHppFile, u8"                m_state = static_cast<unsigned int>(sizeof(m_key_list) / sizeof(m_key_list[0]));\r\n");
        fprintf(pHppFile, u8"                return true;\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        bool Key(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)str;\r\n");
        fprintf(pHppFile, u8"            (void)length;\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }

    if (m_number_list.size())
    {
        fprintf(pHppFile, u8"        bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            switch (m_state)\r\n");
        fprintf(pHppFile, u8"            {\r\n");

        for (size_t i = 0; i < m_number_list.size(); i++)
        {
            CAttrib& attr = m_number_list[i];

            fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
            fprintf(pHppFile, u8"            {\r\n");
            fprintf(pHppFile, u8"                m_assigned_bitset.set(m_state);\r\n");
            fprintf(pHppFile, u8"                JsonDelIntegral(m_data.%s, str, length);\r\n", attr[eName].c_str());
            fprintf(pHppFile, u8"            }\r\n");
            fprintf(pHppFile, u8"            break;\r\n");
        }

        fprintf(pHppFile, u8"            default:\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                return true;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            m_state++;\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        bool RawNumber(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)str;\r\n");
        fprintf(pHppFile, u8"            (void)length;\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }

    if (m_string_list.size())
    {
        fprintf(pHppFile, u8"        bool String(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            switch (m_state)\r\n");
        fprintf(pHppFile, u8"            {\r\n");

        for (size_t i = 0; i < m_string_list.size(); i++)
        {
            CAttrib& attr = m_string_list[i];

            if ((attr[eType] == "uint8" || attr[eType] == "int8") && attr[eArray].length())
            {
                fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
                fprintf(pHppFile, u8"            {\r\n");
                fprintf(pHppFile, u8"                m_assigned_bitset.set(m_state);\r\n");
                fprintf(pHppFile, u8"                DataArrayHandler<%s, %s> handler(m_data.%s, this);\r\n", 
                    __integral_to_c_type(attr[eType]).c_str(), 
                    __integral_to_c_type(attr[eArray]).c_str(),
                    attr[eName].c_str());

                fprintf(pHppFile, u8"                handler.String(str, length, copy);\r\n");
                fprintf(pHppFile, u8"            }\r\n");
                fprintf(pHppFile, u8"            break;\r\n");
            }
            else
            {
                fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
                fprintf(pHppFile, u8"            {\r\n");
                fprintf(pHppFile, u8"                m_assigned_bitset.set(m_state);\r\n");
                fprintf(pHppFile, u8"                JsonDelString(m_data.%s, str, length);\r\n", attr[eName].c_str());
                fprintf(pHppFile, u8"            }\r\n");
                fprintf(pHppFile, u8"            break;\r\n");
            }
        }

        fprintf(pHppFile, u8"            default:\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state = 0;\r\n");
        fprintf(pHppFile, u8"                return true;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            m_state++;\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        bool String(const char* str, rapidjson::SizeType length, bool copy) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)str;\r\n");
        fprintf(pHppFile, u8"            (void)length;\r\n");
        fprintf(pHppFile, u8"            (void)copy;\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");
    }

    if (m_struct_list.size())
    {
        fprintf(pHppFile, u8"        JsonHandler* StartObject() override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            switch (m_state)\r\n");
        fprintf(pHppFile, u8"            {\r\n");

        for (size_t i = 0; i < m_struct_list.size(); i++)
        {
            CAttrib& attr = m_struct_list[i];

            fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
            fprintf(pHppFile, u8"            {\r\n");
            fprintf(pHppFile, u8"                m_assigned_bitset.set(m_state);\r\n");
            fprintf(pHppFile, u8"                return &m_%s_handler;\r\n", attr[eName].c_str());
            fprintf(pHppFile, u8"            }\r\n");
            fprintf(pHppFile, u8"            break;\r\n");

        }

        fprintf(pHppFile, u8"            default:\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_default_handler.Reset(this);\r\n");
        fprintf(pHppFile, u8"                return &m_default_handler;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");



        fprintf(pHppFile, u8"        bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)memberCount;\r\n\r\n");
        fprintf(pHppFile, u8"            if (childen != &m_default_handler)\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state++;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            else\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state = 0;\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        JsonHandler* StartObject() override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            m_default_handler.Reset(this);\r\n");
        fprintf(pHppFile, u8"            return &m_default_handler;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");

        fprintf(pHppFile, u8"        bool EndObject(JsonHandler* childen, rapidjson::SizeType memberCount) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)childen;\r\n");
        fprintf(pHppFile, u8"            (void)memberCount;\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n");
    }

    if (m_array_list.size() || m_array_old_list.size())
    {
        fprintf(pHppFile, u8"        JsonHandler* StartArray() override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            switch (m_state)\r\n");
        fprintf(pHppFile, u8"            {\r\n");

        for (size_t i = 0; i < m_array_list.size(); i++)
        {
            CAttrib& attr = m_array_list[i];

            fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
            fprintf(pHppFile, u8"            {\r\n");
            fprintf(pHppFile, u8"                m_assigned_bitset.set(m_state);\r\n");
            fprintf(pHppFile, u8"                return &m_%s_handler;\r\n", attr[eName].c_str());
            fprintf(pHppFile, u8"            }\r\n");
            fprintf(pHppFile, u8"            break;\r\n");
        }

        for (size_t i = 0; i < m_array_old_list.size(); i++)
        {
            CAttrib& attr = m_array_old_list[i];

            fprintf(pHppFile, u8"            case %s:\r\n", attr[eIndex].c_str());
            fprintf(pHppFile, u8"            {\r\n");

            fprintf(pHppFile, u8"                return &m_%s_handler;\r\n", attr[eName].c_str());
            fprintf(pHppFile, u8"            }\r\n");
            fprintf(pHppFile, u8"            break;\r\n");
        }

        fprintf(pHppFile, u8"            default:\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_default_handler.Reset(this);\r\n");
        fprintf(pHppFile, u8"                return &m_default_handler;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");



        fprintf(pHppFile, u8"        bool EndArray(JsonHandler* childen, rapidjson::SizeType elementCount) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)elementCount;\r\n\r\n");
        fprintf(pHppFile, u8"            if (childen != &m_default_handler)\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state++;\r\n");
        fprintf(pHppFile, u8"            }\r\n");
        fprintf(pHppFile, u8"            else\r\n");
        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                m_state = 0;\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n");
    }
    else
    {
        fprintf(pHppFile, u8"        JsonHandler* StartArray() override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            m_default_handler.Reset(this);\r\n");
        fprintf(pHppFile, u8"            return &m_default_handler;\r\n");
        fprintf(pHppFile, u8"        }\r\n\r\n");

        fprintf(pHppFile, u8"        bool EndArray(JsonHandler* childen, rapidjson::SizeType elementCount) override\r\n");
        fprintf(pHppFile, u8"        {\r\n");
        fprintf(pHppFile, u8"            (void)childen;\r\n");
        fprintf(pHppFile, u8"            (void)elementCount;\r\n");
        fprintf(pHppFile, u8"            return true;\r\n");
        fprintf(pHppFile, u8"        }\r\n");
    }

    fprintf(pHppFile, u8"        bool IsAllMemberSet(void)\r\n");
    fprintf(pHppFile, u8"        {\r\n");

    if (m_item_list.size())
    {
        if (m_def_list.empty())
        {
            fprintf(pHppFile, u8"            if ((m_assigned_bitset ^ template_assigned_bitset).any())\r\n");
        }
        else
        {
            fprintf(pHppFile, u8"            if (((template_assigned_bitset & m_assigned_bitset) ^ template_assigned_bitset).any())\r\n");
        }

        fprintf(pHppFile, u8"            {\r\n");
        fprintf(pHppFile, u8"                return false;\r\n");
        fprintf(pHppFile, u8"            }\r\n\r\n");

        for (size_t i = 0; i < m_item_list.size(); i++)
        {
            CAttrib& attr = m_item_list[i];
            bool need_all_set = false;

            if (attr[eType] == "string")
            {
                //m_string_list.push_back(vecAttrib);
            }
            else if (attr[eCount].empty() && attr[eArray].empty())
            {
                if (__is_integral(attr[eType]))
                {
                    //m_number_list.push_back(vecAttrib);
                }
                else
                {
                    //m_struct_list.push_back(vecAttrib);
                    need_all_set = true;
                }
            }
            else
            {
                if (!attr[eArray].empty())
                {
                    if (attr[eType] == "uint8" || attr[eType] == "int8")
                    {
                        //m_string_list.push_back(vecAttrib);
                    }
                    else
                    {
                        //m_array_list.push_back(vecAttrib);
                        need_all_set = true;
                    }
                }

                if (!attr[eCount].empty())
                {
                    //m_array_old_list.push_back(vecAttrib);
                    need_all_set = true;
                }
            }

            if (need_all_set)
            {
                if (m_def_list.find(attr[eName]) != m_def_list.end())
                {
                    fprintf(pHppFile, u8"            if (m_assigned_bitset.test(%s))\r\n", attr[eIndex].c_str());
                    fprintf(pHppFile, u8"            {\r\n");
                    fprintf(pHppFile, u8"                if (!m_%s_handler.IsAllMemberSet())\r\n", attr[eName].c_str());
                    fprintf(pHppFile, u8"                {\r\n");
                    fprintf(pHppFile, u8"                    return false;\r\n");
                    fprintf(pHppFile, u8"                }\r\n\r\n");
                    fprintf(pHppFile, u8"            }\r\n");
                }
                else
                {
                    fprintf(pHppFile, u8"            if (!m_%s_handler.IsAllMemberSet())\r\n", attr[eName].c_str());
                    fprintf(pHppFile, u8"            {\r\n");
                    fprintf(pHppFile, u8"                return false;\r\n");
                    fprintf(pHppFile, u8"            }\r\n\r\n");
                }
            }
        }

        //for (size_t i = 0; i < m_array_list.size(); i++)
        //{
        //    CAttrib& attr = m_array_list[i];

        //    fprintf(pHppFile, u8"            if (m_assigned_bitset.test(%s))\r\n", attr[eIndex].c_str());
        //    fprintf(pHppFile, u8"            {\r\n");
        //    fprintf(pHppFile, u8"                if (!m_%s_handler.IsAllMemberSet())\r\n", attr[eName].c_str());
        //    fprintf(pHppFile, u8"                {\r\n");
        //    fprintf(pHppFile, u8"                    return false;\r\n");
        //    fprintf(pHppFile, u8"                }\r\n\r\n");
        //    fprintf(pHppFile, u8"            }\r\n");
        //}

        //for (size_t i = 0; i < m_array_old_list.size(); i++)
        //{
        //    CAttrib& attr = m_array_old_list[i];

        //    fprintf(pHppFile, u8"            if (m_assigned_bitset.test(%s))\r\n", attr[eIndex].c_str());
        //    fprintf(pHppFile, u8"            {\r\n");
        //    fprintf(pHppFile, u8"                if (!m_%s_handler.IsAllMemberSet())\r\n", attr[eName].c_str());
        //    fprintf(pHppFile, u8"                {\r\n");
        //    fprintf(pHppFile, u8"                    return false;\r\n");
        //    fprintf(pHppFile, u8"                }\r\n\r\n");
        //    fprintf(pHppFile, u8"            }\r\n");
        //}
    }

    fprintf(pHppFile, u8"            return true;\r\n");
    fprintf(pHppFile, u8"        }\r\n");

    fprintf(pHppFile, u8"    private:\r\n");
    fprintf(pHppFile, u8"        %s& m_data;\r\n", strName.c_str());
    for (size_t i = 0; i < m_struct_list.size(); i++)
    {
        CAttrib& attr = m_struct_list[i];

        fprintf(pHppFile, u8"        %s::Handler m_%s_handler;\r\n", attr[eType].c_str(), attr[eName].c_str());
    }

    for (size_t i = 0; i < m_array_list.size(); i++)
    {
        CAttrib& attr = m_array_list[i];

        if (__is_integral(attr[eType]))
        {
            fprintf(pHppFile, u8"        DataArrayHandler<%s, %s> m_%s_handler;\r\n",
                __integral_to_c_type(attr[eType]).c_str(), __integral_to_c_type(attr[eArray]).c_str(), attr[eName].c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        DataArrayHandler<%s, %s, %s::Handler> m_%s_handler;\r\n",
                attr[eType].c_str(), __integral_to_c_type(attr[eArray]).c_str(), attr[eType].c_str(), attr[eName].c_str());
        }

        
    }

    for (size_t i = 0; i < m_array_old_list.size(); i++)
    {
        CAttrib& attr = m_array_old_list[i];

        if (__is_integral(attr[eType]))
        {
            fprintf(pHppFile, u8"        DataArrayOldHandler<%s, %s> m_%s_handler;\r\n", 
                __integral_to_c_type(attr[eType]).c_str(), attr[eCount].c_str(), attr[eName].c_str());
        }
        else
        {
            fprintf(pHppFile, u8"        DataArrayOldHandler<%s, %s, %s::Handler> m_%s_handler;\r\n",
                attr[eType].c_str(), attr[eCount].c_str(), attr[eType].c_str(), attr[eName].c_str());
        }
    }
    fprintf(pHppFile, u8"        HoleJsonHandler m_default_handler;\r\n");
    fprintf(pHppFile, u8"        unsigned int m_state;\r\n");

    if (m_item_list.size())
    {
        fprintf(pHppFile, u8"        std::bitset<%zu> m_assigned_bitset;\r\n\r\n", m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());
        
        fprintf(pHppFile, u8"        static const std::bitset<%zu> template_assigned_bitset;\r\n", m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());
        fprintf(pHppFile, u8"        static const char* m_key_list[%zu];\r\n", m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());
        fprintf(pHppFile, u8"        static const char* m_alias_list[%zu];\r\n", m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());
        
        fprintf(pCppFile, u8"const char* %s::m_key_list[%zu] = {\r\n", strHandlerName.c_str(), m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());

        for (size_t i = 0; i < m_item_list.size(); i++)
        {
            CAttrib& attr = m_item_list[i];
            if ((i + 1) == m_item_list.size())
            {
                fprintf(pCppFile, u8"    u8\"%s\"\r\n", attr[eName].c_str());
            }
            else
            {
                fprintf(pCppFile, u8"    u8\"%s\",\r\n", attr[eName].c_str());
            }
        }

        fprintf(pCppFile, u8"};\r\n");

        fprintf(pCppFile, u8"const char* %s::m_alias_list[%zu] = {\r\n", strHandlerName.c_str(), m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size());

        for (size_t i = 0; i < m_item_list.size(); i++)
        {
            CAttrib& attr = m_item_list[i];

            std::string name = attr[eAlias];

            if (name.empty())
            {
                name = attr[eName];
            }

            if ((i + 1) == m_item_list.size())
            {
                fprintf(pCppFile, u8"    u8\"%s\"\r\n", name.c_str());
            }
            else
            {
                fprintf(pCppFile, u8"    u8\"%s\",\r\n", name.c_str());
            }
        }

        fprintf(pCppFile, u8"};\r\n");

        fprintf(pCppFile, u8"const std::bitset<%zu> %s::template_assigned_bitset { ", m_number_list.size() + m_string_list.size() + m_array_list.size() + m_array_old_list.size() + m_struct_list.size(), strHandlerName.c_str());
        fprintf(pCppFile, u8"\"");
        //for (size_t i = 0; i < m_item_list.size(); i++)
        for (size_t i = m_item_list.size(); i>0; i--)
        {
            CAttrib& attr = m_item_list[i - 1];

            if (m_def_list.find(attr[eName]) == m_def_list.end())
            {
                fprintf(pCppFile, u8"1");
            }
            else
            {
                fprintf(pCppFile, u8"0");
            }
        }
        fprintf(pCppFile, u8"\" };\r\n\r\n");
    }

    fprintf(pHppFile, u8"    };\r\n\r\n");

    //rXml.ResetMainPos();
    //while (rXml.FindElem("item"))
    //{

    //}

    if (!rXml.OutOfElem())
    {
        m_strErrorInfo += "从子集返回失败";
        return false;
    }

    return true;
}

bool CProtocolMaker::__WriteData( FILE* pHppFile, FILE* pCppFile, CMarkupSTL& rXml )
{
    //查找所有数据定义
    fprintf(pHppFile, u8"//===============数据定义开始===============\r\n");
    //fprintf(pHppFile, "#pragma warning( push )\r\n#pragma warning( disable : 4512)\r\n");
    rXml.ResetMainPos();
    while (rXml.FindElem("data"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strComment = rXml.GetAttrib("comment");
        EDataType eDataType = eStruct;

        ERRORINFO_DATA(strType, strName, strComment);

        if (strType == "struct")
        {
            if (m_mapStruct.find(strName) != m_mapStruct.end())
            {
                m_strErrorInfo += "重定义";
                return false;
            }

            //fprintf(pHppFile, "typedef struct struct_%s{\r\n", strName.c_str());
            fprintf(pHppFile, u8"struct %s{\r\n", strName.c_str());
            eDataType = eStruct;
        }
        else if (strType == "union")
        {
            fprintf(pHppFile, u8"union %s{\r\n", strName.c_str());
            eDataType = eUnion;
        }
        else if (strType == "protocol")
        {
            if (m_mapProtocol.find(strName) != m_mapProtocol.end())
            {
                m_strErrorInfo += " 重定义";
                return false;
            }
            //fprintf(pHppFile, u8"struct %s:protocol_base{\r\n", strName.c_str());
            fprintf(pHppFile, u8"struct %s:TProtocol<%s>\r\n{\r\n", strName.c_str(), strName.c_str());
            //fprintf(pHppFile,"\tconst %-*s moudleid;\r\n", 19, "unsigned short");
            //fprintf(pHppFile,"\tconst %-*s protocolid;\r\n", 19, "unsigned short");
            eDataType = eProtocol;
            m_vecProtocol.push_back(strName);

#ifdef MT
            fprintf(pCppFile, u8"template<>\r\n");
            fprintf(pCppFile, u8"const unsigned short TProtocol<%s>::module_id = %s;\r\n", strName.c_str(), m_strMoudleID.c_str());
            fprintf(pCppFile, u8"template<>\r\n");
            fprintf(pCppFile, u8"const unsigned short TProtocol<%s>::protocol_id = %zu;\r\n\r\n", strName.c_str(), m_vecProtocol.size());
#else
            fprintf(pCppFile, u8"template<>\r\n");
            fprintf(pCppFile, u8"const unsigned short TProtocol<%s>::module_id = %s;\r\n", strName.c_str(), m_strMoudleID.c_str());
            fprintf(pCppFile, u8"template<>\r\n");
            fprintf(pCppFile, u8"const unsigned short TProtocol<%s>::protocol_id = %zu;\r\n\r\n", strName.c_str(), m_vecProtocol.size() - 1);
#endif // 

        }
        else
        {
            m_strErrorInfo += "type属性的类型未知，只能定义为struct,union,protocol";
            return false;
        }

        if (!rXml.IntoElem())
        {
            m_strErrorInfo += "进入子级失败";
            return false;
        }

        CItem mapItem;

        while (rXml.FindElem("item"))
        {
            CAttrib vecAttrib(9);
            vecAttrib[eName] = rXml.GetAttrib("name");
            vecAttrib[eType] = rXml.GetAttrib("type");
            vecAttrib[eCount] = rXml.GetAttrib("count");
            vecAttrib[eRefer] = rXml.GetAttrib("refer");
            vecAttrib[eId] = rXml.GetAttrib("id");
            vecAttrib[eSelect] = rXml.GetAttrib("select");
            vecAttrib[eComment] = rXml.GetAttrib("comment");
            vecAttrib[eLength] = rXml.GetAttrib("length");
            vecAttrib[eArray] = rXml.GetAttrib("array");

            CItem::iterator it  = mapItem.find(vecAttrib[eName]);
            if (it != mapItem.end())
            {
                m_strErrorInfo += "name属性值重复";
                return false;
            }
            else
                mapItem[vecAttrib[eName]] = vecAttrib;
        }

        rXml.ResetMainPos();
        while (rXml.FindElem("item"))
        {
            if (!__WriteItem(pHppFile, rXml, eDataType, mapItem))
            {
                return false;
            }
        }

        if (!rXml.OutOfElem())
        {
            m_strErrorInfo += "从子集返回失败";
            return false;
        }

        if (strType == "struct")
        {
			//fprintf(pHppFile, u8"\tbool EnCode(NetEnCode& net_data);\r\n\tbool DeCode(NetDeCode& net_data);\r\n");

            m_mapStruct[strName] = mapItem;

            if (!__WriteStructProtocolEnCodeFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolDeCodeFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!mapItem.empty())
            {
                //fprintf(pHppFile, u8"\tvoid Reset(void);\r\n");
                //fprintf(pHppFile, u8"\t%s& operator = (const %s& src);\r\n", strName.c_str(), strName.c_str());
                //fprintf(pHppFile, u8"\tbool operator == (const %s& src) const;\r\n", strName.c_str());
                //fprintf(pHppFile, u8"\tbool operator != (const %s& src) const;\r\n", strName.c_str());
                if (!__WriteStructProtocolResetFunc(rXml, pHppFile))
                {
                    return false;
                }

                if (!__WriteStructProtocolOperatorCopy(rXml, pHppFile))
                {
                }
                
                if (!__WriteStructProtocolOperatorEqual(rXml, pHppFile))
                {
                    return false;
                }
            }

            if (!__WriteHandler(rXml, pHppFile, pCppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolToJsonFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteMarshalUnmarshalJsonFunc(rXml, pHppFile))
            {
                return false;
            }
        }
        else if (strType == "union")
        {
            m_mapUnion[strName] = mapItem;
        }
        else if (strType == "protocol")
        {
            m_mapStruct[strName] = mapItem;
            m_mapProtocol[strName] = mapItem;

            if (!__WriteStructProtocolEnCodeFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolEnCodeExFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolDeCodeFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolDeCodeExFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!mapItem.empty())
            {
                if (!__WriteStructProtocolResetFunc(rXml, pHppFile))
                {
                    return false;
                }

                if (!__WriteStructProtocolOperatorCopy(rXml, pHppFile))
                {
                    return false;
                }

                if (!__WriteStructProtocolOperatorEqual(rXml, pHppFile))
                {
                    return false;
                }
                //fprintf(pHppFile, u8"\tvoid Reset(void);\r\n");
                //fprintf(pHppFile, u8"\t%s& operator = (const %s& src);\r\n", strName.c_str(), strName.c_str());
                //fprintf(pHppFile, u8"\tbool operator == (const %s& src) const;\r\n", strName.c_str());
                //fprintf(pHppFile, u8"\tbool operator != (const %s& src) const;\r\n", strName.c_str());
            }

            if (!__WriteHandler(rXml, pHppFile, pCppFile))
            {
                return false;
            }

            if (!__WriteStructProtocolToJsonFunc(rXml, pHppFile))
            {
                return false;
            }

            if (!__WriteMarshalUnmarshalJsonFunc(rXml, pHppFile))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        //fprintf(pHppFile, "}%s;\r\n\r\n", strName.c_str());
        fprintf(pHppFile, u8"};\r\n\r\n");
    }
    //fprintf(pHppFile, "#pragma warning( pop ) \r\n");
    fprintf(pHppFile, u8"//===============数据定义结束===============\r\n");
    return true;
}

//bool CProtocolMaker::__WriteDynamicProtocol(FILE* pHppFile, CMarkupSTL& rXml)
//{
//    fprintf(pHppFile, u8"namespace DP\r\n");
//    fprintf(pHppFile, u8"{\r\n");
//    for (size_t i = 0; i < m_vecProtocol.size(); i++)
//    {
//        fprintf(pHppFile, u8"\tstruct %s: DynamicProtocol, ::%s\r\n", m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
//        fprintf(pHppFile, u8"\t{\r\n");
//        fprintf(pHppFile, u8"\t\tbool EnCode(NetEnCode& net_data) override\r\n");
//        fprintf(pHppFile, u8"\t\t{\r\n");
//        fprintf(pHppFile, u8"\t\t\treturn ::%s::EnCode(net_data);\r\n", m_vecProtocol[i].c_str());
//        fprintf(pHppFile, u8"\t\t}\r\n\r\n");
//        fprintf(pHppFile, u8"\t\tbool DeCode(NetDeCode& net_data) override\r\n");
//        fprintf(pHppFile, u8"\t\t{\r\n");
//        fprintf(pHppFile, u8"\t\t\treturn ::%s::DeCode(net_data);\r\n", m_vecProtocol[i].c_str());
//        fprintf(pHppFile, u8"\t\t}\r\n\r\n");
//        fprintf(pHppFile, u8"\t\tunsigned short ModuleId() override\r\n");
//        fprintf(pHppFile, u8"\t\t{\r\n");
//        fprintf(pHppFile, u8"\t\t\treturn ::%s::module_id;\r\n", m_vecProtocol[i].c_str());
//        fprintf(pHppFile, u8"\t\t}\r\n\r\n");
//        fprintf(pHppFile, u8"\t\tunsigned short ProtocolId() override\r\n");
//        fprintf(pHppFile, u8"\t\t{\r\n");
//        fprintf(pHppFile, u8"\t\t\treturn ::%s::protocol_id;\r\n", m_vecProtocol[i].c_str());
//        fprintf(pHppFile, u8"\t\t}\r\n");
//        fprintf(pHppFile, u8"\t};\r\n");
//    }
//    fprintf(pHppFile, u8"}\r\n");
//
//    return true;
//}

//bool CProtocolMaker::__WriteDataFunction( FILE* pHppFile, FILE* pCppFile, CMarkupSTL& rXml )
//{
//    fprintf(pHppFile, u8"//===============打包解包函数定义开始===============\r\n");
//    rXml.ResetMainPos();
//    while (rXml.FindElem("data"))
//    {
//        std::string strType = rXml.GetAttrib("type");
//        if (strType == "struct")
//        {
//            if (!__WriteStructProtocolEnCodeFunc(rXml, pHppFile, pCppFile, false))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolDeCodeFunc(rXml, pHppFile, pCppFile, false))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolResetFunc(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolOperatorCopy(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolOperatorEqual(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//        }
//        else if (strType == "protocol")
//        {
//            if (!__WriteStructProtocolEnCodeFunc(rXml, pHppFile, pCppFile, true))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolDeCodeFunc(rXml, pHppFile, pCppFile, true))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolResetFunc(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolOperatorCopy(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteStructProtocolOperatorEqual(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//        }
//        else if (strType == "union")
//        {
//            if (!__WriteUnionEnCodeFunc(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//            rXml.ResetChildPos();
//            if (!__WriteUnionDeCodeFunc(rXml, pHppFile, pCppFile))
//            {
//                return false;
//            }
//        }
//        else
//        {
//            return false;
//        }
//
//    }
//    fprintf(pHppFile, u8"//===============打包解包函数定义结束===============\r\n");
//    return true;
//}

bool CProtocolMaker::__WriteItem( FILE* pHppFile, CMarkupSTL& rXml, EDataType eDataType, CItem& mapItem)
{
    std::string strName = rXml.GetAttrib("name");
    std::string strType = rXml.GetAttrib("type");
    std::string strCount = rXml.GetAttrib("count");
    std::string strRefer = rXml.GetAttrib("refer");
    std::string strId = rXml.GetAttrib("id");
    std::string strSelect = rXml.GetAttrib("select");
    std::string strComment = rXml.GetAttrib("comment");
    std::string strLength = rXml.GetAttrib("length");
    std::string strArray = rXml.GetAttrib("array");

    ERRORINFO_ITEM(strName, strType, strCount, strRefer, strId, strSelect, strLength, strComment, strArray);

    if (strName.empty() || strType.empty())
    {
        m_strErrorInfo += "中 name属性和type属性不能为空";
        return false;
    }

    if (eUnion == eDataType)
    {
        if (!__IsStruct(strType))
        {
            m_strErrorInfo += "union中的成员必须是struct";
            return false;
        }

        if (strId.empty())
        {
            m_strErrorInfo += "union中id属性不能为空";
            return false;
        }

        if (!__IsAllDigit(strId))
        {
            if (!__IsMacro(strId))
            {
                m_strErrorInfo += "union中id必须为数字或者是定义的宏";
                return false;
            }
        }

        if (!strCount.empty())
        {
            m_strErrorInfo += "union中不能有数组";
            return false;
        }

        if (!strArray.empty())
        {
            m_strErrorInfo += "union中不能有数组";
            return false;
        }

        if (!strRefer.empty())
        {
            m_strErrorInfo += "union中不能有refer属性";
            return false;
        }

        if (!strSelect.empty())
        {
            m_strErrorInfo += "union中不能有select属性";
            return false;
        }

        if (!strLength.empty())
        {
            m_strErrorInfo += "union中不能有length属性";
            return false;
        }
    }
    

    if (eStruct == eDataType || eProtocol == eDataType)
    {
        if (__IsKeyType(strType) || __IsStruct(strType))
        {
            if (!strArray.empty())
            {
                if (!strCount.empty())
                {
                    m_strErrorInfo += "有array属性就不能有count属性";
                    return false;
                }

                if (!strRefer.empty())
                {
                    m_strErrorInfo += "有array属性就不能有refer属性";
                    return false;
                }

                if (strArray != "uint8" &&
                    strArray != "uint16" &&
                    strArray != "uint32" &&
                    strArray != "uint64")
                {
                    m_strErrorInfo += "array属性值必须为 uint8, uint16, uint32, uint64 中的一种";
                    return false;
                }
            }

            if (!strCount.empty())
            {
                if (!__IsAllDigit(strCount))
                {
                    if (!__IsMacro(strCount))
                    {
                        m_strErrorInfo += "item中count必须为数字或者是定义的宏";
                        return false;
                    }
                }
            }

            if (!strRefer.empty())
            {
                if (strCount.empty())
                {
                    m_strErrorInfo += "item中有refer属性前必须先有count属性";
                    return false;
                }

                CItem::iterator it = mapItem.find(strRefer);
                if (it == mapItem.end())
                {
                    m_strErrorInfo += "item中refer的属性值未定义";
                    return false;
                }
                else
                {
                    if (!__IsKeyType((it->second)[eType]))
                    {
                        m_strErrorInfo += "item中refer的属性值必须是基础数据类型的变量";
                        return false;
                    }

                    if (!(it->second)[eCount].empty())
                    {
                        m_strErrorInfo += "item中refer的属性值不能是数组";
                        return false;
                    }
                }
            }
            if (!strId.empty())
            {
                m_strErrorInfo += "只有data是union才能有此属性";
                return false;
            }
            if (!strSelect.empty())
            {
                m_strErrorInfo += "只有type=union才能有此属性";
                return false;
            }

            if (!strLength.empty())
            {
                if (strType == "string")
                {
                    if (!__IsAllDigit(strLength))
                    {
                        if (!__IsMacro(strLength))
                        {
                            m_strErrorInfo += "item中length必须为数字或者是定义的宏";
                            return false;
                        }
                    }
                    else
                    {
                        if (atoi(strLength.c_str()) > 65535)
                        {
                            m_strErrorInfo += "string类型的length属性最大长度不能超过65535";
                            return false;
                        }
                    }
                }
                else
                {
                    m_strErrorInfo += "只有type=string才能有此属性";
                    return false;
                }
            }
        }
        else if (__IsUnion(strType))
        {
            if (strSelect.empty())
            {
                m_strErrorInfo += "union中必须有select属性";
                return false;
            }
            else
            {
                CItem::iterator it = mapItem.find(strSelect);
                if (it == mapItem.end())
                {
                    m_strErrorInfo += "item中select的属性值未定义";
                    return false;
                }
                else
                {
                    if (!__IsKeyType((it->second)[eType]))
                    {
                        m_strErrorInfo += "item中select的属性值必须是基础数据类型的变量";
                        return false;
                    }

                    if (!(it->second)[eCount].empty())
                    {
                        m_strErrorInfo += "item中select的属性值不能为数组";
                        return false;
                    }
                }
            }
            if (!strCount.empty())
            {
                m_strErrorInfo += "type=union不能有count属性";
                return false;
            }
            if (!strRefer.empty())
            {
                m_strErrorInfo += "type=union不能有refer属性";
                return false;
            }
            if (!strArray.empty())
            {
                m_strErrorInfo += "type=union不能有array属性";
                return false;
            }
            if (!strId.empty())
            {
                m_strErrorInfo += "type=union不能有id属性";
                return false;
            }
            if (!strLength.empty())
            {
                m_strErrorInfo += "type=union不能有length属性";
                return false;
            }
        }
        else
        {
            m_strErrorInfo += "Unknow Type "+strType;
            return false;
        }
    }

    std::string strCStyle;
    if (strType == "uint8")
    {
        strCStyle = "unsigned char";
    }
    else if (strType == "int8")
    {
        strCStyle = "char";
    }
    else if (strType == "uint16")
    {
        strCStyle = "unsigned short";
    }
    else if (strType == "int16")
    {
        strCStyle = "short";
    }
    else if (strType == "uint32")
    {
        strCStyle = "unsigned int";
    }
    else if (strType == "int32")
    {
        strCStyle = "int";
    }
    else if (strType == "uint64")
    {
        strCStyle = "unsigned long long";
    }
    else if (strType == "int64")
    {
        strCStyle = "signed long long";
    }
    else if (strType == "string")
    {
        strCStyle = "char";
    }
    else
    {
        strCStyle = strType;
    }

    if (strCount.empty() && strArray.empty())
    {
        if (strType == "string")
        {
            if (!strLength.empty())
                fprintf(pHppFile, u8"\t%-*s %s[%s];", 25, strCStyle.c_str(), strName.c_str(), strLength.c_str());
            else
                fprintf(pHppFile, u8"\t%-*s %s[256];", 25, strCStyle.c_str(), strName.c_str());
        }
        else
        {
            fprintf(pHppFile, u8"\t%-*s %s;", 25, strCStyle.c_str(), strName.c_str());
        }
    }
    else
    {
        if (!strCount.empty())
        {
            if (strType == "string")
            {
                m_strErrorInfo += "type=string不能有count属性";
                return false;
            }
            fprintf(pHppFile, u8"\t%-*s %s[%s];", 25, strCStyle.c_str(), strName.c_str(), strCount.c_str());
        }

        if (!strArray.empty())
        {
            std::string strArrayCtype;
            if (strArray == "uint8")
            {
                strArrayCtype = "unsigned char";
            }
            else if (strArray == "uint16")
            {
                strArrayCtype = "unsigned short";
            }
            else if (strArray == "uint32")
            {
                strArrayCtype = "unsigned int";
            }
            else if (strArray == "uint64")
            {
                strArrayCtype = "unsigned long long";
            }


            if (strType == "string")
            {
                m_strErrorInfo += "type=string不能有array属性";
                return false;
            }
            std::string array_name = "DataArray<" + strCStyle + ", " + strArrayCtype + ">";
            fprintf(pHppFile, u8"\t%-*s %s;", 25, array_name.c_str(), strName.c_str());
        }
    }

    if (!strComment.empty())
    {
        fprintf(pHppFile, u8" //%s", strComment.c_str());
    }
    fprintf(pHppFile, u8"\r\n");


    return true;
}

bool CProtocolMaker::__WriteStructProtocolResetFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");

    CData::iterator it = m_mapStruct.find(strName);
    if (it == m_mapStruct.end() ||
        it->second.empty())
    {
        return true;
    }

    fprintf(pHppFile, u8"\tvoid Reset(void)\r\n\t{\r\n");

    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");
        std::string strEnFuncName = "";

        if (strType == "string")
        {
            fprintf(pHppFile, u8"\t\t%s[0] = 0;\r\n\r\n", strName.c_str());
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                fprintf(pHppFile, u8"\t\t%s = 0;\r\n\r\n", strName.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"\t\t%s.Reset();\r\n\r\n", strName.c_str());
            }
        }
        else
        {
            if (!strArray.empty())
            {
                fprintf(pHppFile, u8"\t\t%s.clear();\r\n\r\n", strName.c_str());
            }

            if (!strCount.empty())
            {
                if (__is_integral(strType))
                {
                    if (strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n", strCount.c_str());
                    }
                    else
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\tmemset(this->%s, 0, iCount*sizeof(%s));\r\n\t}\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
                }
                else
                {
                    fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t{\r\n", strCount.c_str());
                    if (!strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\tbreak;\r\n", strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\t%s[i].Reset();\r\n\t}\r\n\r\n", strName.c_str());
                }
            }
        }
    }
    rXml.OutOfElem();

    fprintf(pHppFile, u8"\t}\r\n");
    return true;
}

bool CProtocolMaker::__WriteStructProtocolOperatorEqual(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");

    CData::iterator it = m_mapStruct.find(strName);
    if (it == m_mapStruct.end() ||
        it->second.empty())
    {
        return true;
    }

    fprintf(pHppFile, u8"\tbool operator==(const %s& src) const\r\n\t{\r\n", strName.c_str());

    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");
        std::string strEnFuncName = "";

        if (strType == "string")
        {
            //if (strncmp(%s, src.%s, sizeof(%s)-1))
            //{
            //    return false;
            //}
            //fprintf(pCppFile, u8"\t%s[0] = 0;\r\n\r\n", strName.c_str());
            fprintf(pHppFile, u8"\t\tif (strncmp(%s, src.%s, sizeof(%s)-1))\r\n", strName.c_str(), strName.c_str(), strName.c_str());
            fprintf(pHppFile, u8"\t\t{\r\n");
            fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
            fprintf(pHppFile, u8"\t\t}\r\n\r\n");
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                //fprintf(pCppFile, u8"\t%s = 0;\r\n\r\n", strName.c_str());
                //if (%s != src.%s)
                //{
                //    return false;
                //}
                fprintf(pHppFile, u8"\t\tif (%s != src.%s)\r\n", strName.c_str(), strName.c_str());
                fprintf(pHppFile, u8"\t\t{\r\n");
                fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
                fprintf(pHppFile, u8"\t\t}\r\n\r\n");
            }
            else
            {
                //fprintf(pCppFile, u8"\t%s.Reset();\r\n\r\n", strName.c_str());
                fprintf(pHppFile, u8"\t\tif (%s != src.%s)\r\n", strName.c_str(), strName.c_str());
                fprintf(pHppFile, u8"\t\t{\r\n");
                fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
                fprintf(pHppFile, u8"\t\t}\r\n\r\n");
            }
        }
        else
        {
            if (!strArray.empty())
            {
                //fprintf(pCppFile, u8"\t%s.clear();\r\n\r\n", strName.c_str());
                fprintf(pHppFile, u8"\t\tif (%s != src.%s)\r\n", strName.c_str(), strName.c_str());
                fprintf(pHppFile, u8"\t\t{\r\n");
                fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
                fprintf(pHppFile, u8"\t\t}\r\n\r\n");
            }

            if (!strCount.empty())
            {
                if (__is_integral(strType))
                {
                    if (strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n", strCount.c_str());
                    }
                    else
                    {
                        //if (this->%s != src.%s)
                        //{
                        //    return false;
                        //}
                        fprintf(pHppFile, u8"\t\tif (this->%s != src.%s)\r\n", strRefer.c_str(), strRefer.c_str());
                        fprintf(pHppFile, u8"\t\t{\r\n");
                        fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
                        fprintf(pHppFile, u8"\t\t}\r\n\r\n");
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = (((%s) < ((int)src.%s)) ? (%s) : ((int)src.%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                    }
                    //if (memcmp(this->%s, src.%s, iCount * sizeof(%s)))

                    //fprintf(pCppFile, u8"\t\tmemset(this->%s, 0, iCount*sizeof(%s));\r\n\t}\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
                    fprintf(pHppFile, u8"\t\t\tif (memcmp(this->%s, src.%s, iCount * sizeof(%s)))", strName.c_str(), strName.c_str(), __integral_to_c_type(strType).c_str());
                    fprintf(pHppFile, u8"\t\t\t{\r\n");
                    fprintf(pHppFile, u8"\t\t\t\treturn false;\r\n");
                    fprintf(pHppFile, u8"\t\t\t}\r\n");
                    fprintf(pHppFile, u8"\t\t}\r\n\r\n");
                }
                else
                {
                    //if (!strRefer.empty())
                    //{
                    //    fprintf(pCppFile, u8"\tif (this->%s != src.%s)\r\n", strRefer.c_str(), strRefer.c_str());
                    //    fprintf(pCppFile, u8"\t{\r\n");
                    //    fprintf(pCppFile, u8"\t\treturn false;\r\n");
                    //    fprintf(pCppFile, u8"\t}\r\n\r\n");
                    //}

                    fprintf(pHppFile, u8"\tfor(int i = 0; i < %s; i++)\r\n\t{\r\n", strCount.c_str());
                    if (!strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\tbreak;\r\n", strRefer.c_str());
                    }

                    //fprintf(pCppFile, u8"\t\t%s[i].Reset();\r\n\t}\r\n\r\n", strName.c_str());
                    fprintf(pHppFile, u8"\t\t\tif (%s[i] != src.%s[i])\r\n", strName.c_str(), strName.c_str());
                    fprintf(pHppFile, u8"\t\t\t{\r\n");
                    fprintf(pHppFile, u8"\t\t\t\treturn false;\r\n");
                    fprintf(pHppFile, u8"\t\t\t}\r\n");
                    fprintf(pHppFile, u8"\t\t}\r\n\r\n");
                }
            }
        }
    }
    rXml.OutOfElem();
    fprintf(pHppFile, u8"\t\treturn true;\r\n");
    fprintf(pHppFile, u8"\t}\r\n");

    fprintf(pHppFile, u8"\tbool operator != (const %s& src) const\r\n\t{\r\n", strName.c_str());
    fprintf(pHppFile, u8"\t\treturn !(*this == src);\r\n");
    fprintf(pHppFile, u8"\t}\r\n");
    return true;
}

bool CProtocolMaker::__WriteStructProtocolOperatorCopy(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");

    CData::iterator it = m_mapStruct.find(strName);
    if (it == m_mapStruct.end() ||
        it->second.empty())
    {
        return true;
    }

    fprintf(pHppFile, u8"\t%s& operator= (const %s& src)\r\n\t{\r\n", strName.c_str(), strName.c_str());

    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");
        std::string strEnFuncName = "";

        if (strType == "string")
        {
            fprintf(pHppFile, u8"\t\t{\r\n");
            fprintf(pHppFile, u8"\t\t\tsize_t str_len = strnlen(src.%s, sizeof(%s) - 1);\r\n", strName.c_str(), strName.c_str());
            fprintf(pHppFile, u8"\t\t\tmemcpy(%s, src.%s, str_len + 1);\r\n", strName.c_str(), strName.c_str());
            fprintf(pHppFile, u8"\t\t\t%s[sizeof(%s) - 1] = 0;\r\n", strName.c_str(), strName.c_str());
            fprintf(pHppFile, u8"\t\t}\r\n\r\n");
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                fprintf(pHppFile, u8"\t\t%s = src.%s;\r\n\r\n", strName.c_str(), strName.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"\t\t%s = src.%s;\r\n\r\n", strName.c_str(), strName.c_str());
            }
        }
        else
        {
            if (!strArray.empty())
            {
                fprintf(pHppFile, u8"\t\t%s = src.%s;\r\n\r\n", strName.c_str(), strName.c_str());
            }

            if (!strCount.empty())
            {
                if (__is_integral(strType))
                {
                    if (strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n", strCount.c_str());
                    }
                    else
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = (((%s) < ((int)src.%s)) ? (%s) : ((int)src.%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\tmemcpy(this->%s, src.%s, iCount*sizeof(%s));\r\n\t}\r\n", strName.c_str(), strName.c_str(), __integral_to_c_type(strType).c_str());
                }
                else
                {
                    fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t{\r\n", strCount.c_str());
                    if (!strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t\tif(i >= (int)src.%s)\r\n\t\t\tbreak;\r\n", strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\t%s[i] = src.%s[i];\r\n\t}\r\n\r\n", strName.c_str(), strName.c_str());
                }
            }
        }
    }
    rXml.OutOfElem();

    fprintf(pHppFile, u8"\t\treturn *this;\r\n");
    fprintf(pHppFile, u8"\t}\r\n");
    return true;
}

bool CProtocolMaker::__WriteStructProtocolToJsonFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");
    fprintf(pHppFile, u8"\tbool ToJson(JsonEnCode& json_data) const\r\n\t{\r\n");
    fprintf(pHppFile, u8"\t\tjson_data.Writer().StartObject();\r\n\r\n");
    bool is_empty = true;
    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        is_empty = false;
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");
        std::string strAlias = rXml.GetAttrib("alias");
        std::string strEnFuncName = "";

        std::string key = strAlias;
        if (key.empty())
        {
            key = strName;
        }

        if (strType == "string")
        {
            fprintf(pHppFile, u8"\t\tjson_data.AddString(u8\"%s\", %s, sizeof(%s));\r\n\r\n", key.c_str(), strName.c_str(), strName.c_str());
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                fprintf(pHppFile, u8"\t\tjson_data.AddIntegral(u8\"%s\", %s);\r\n\r\n", key.c_str(), strName.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"\t\tjson_data.Writer().Key(u8\"%s\", sizeof(u8\"%s\") - 1);\r\n", key.c_str(), key.c_str());
                fprintf(pHppFile, u8"\t\t%s.ToJson(json_data);\r\n\r\n", strName.c_str());
            }
        }
        else
        {
            if (!strArray.empty())
            {
                fprintf(pHppFile, u8"\t\tjson_data.Writer().Key(u8\"%s\", sizeof(u8\"%s\") - 1);\r\n", key.c_str(), key.c_str());
                fprintf(pHppFile, u8"\t\tjson_data.AddArray(%s);\r\n\r\n", strName.c_str());
            }

            if (!strCount.empty())
            {
                if (strRefer.empty())
                {
                    fprintf(pHppFile, u8"\t\t{\r\n\t\t\tint iCount = %s;\r\n", strCount.c_str());
                }
                else
                {
                    fprintf(pHppFile, u8"\t\t{\r\n\t\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                }

                fprintf(pHppFile, u8"\t\t\tjson_data.Writer().Key(u8\"%s\", sizeof(u8\"%s\") - 1);\r\n", key.c_str(), key.c_str());
                fprintf(pHppFile, u8"\t\t\tjson_data.AddArrayOld(%s, iCount);\r\n\t\t}\r\n", strName.c_str());
            }
        }
    }
    rXml.OutOfElem();

    //if (is_empty)
    //{
    //    fprintf(pHppFile, u8"\t\t(void)(net_data);\r\n");
    //}
    fprintf(pHppFile, u8"\t\tjson_data.Writer().EndObject();\r\n\r\n");
    fprintf(pHppFile, u8"\t\treturn json_data.Writer().IsComplete();\r\n");
    fprintf(pHppFile, u8"\t}\r\n\r\n");
    return true;
}

bool CProtocolMaker::__WriteMarshalUnmarshalJsonFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");

    fprintf(pHppFile, u8"    bool MarshalJson(std::string& json) const\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        JsonEnCode json_encode(4096);\r\n\r\n");
    fprintf(pHppFile, u8"        bool json_ret = this->ToJson(json_encode);\r\n\r\n");
    fprintf(pHppFile, u8"        json = json_encode.ToString();\r\n\r\n");
    fprintf(pHppFile, u8"        return json_ret;\r\n");
    fprintf(pHppFile, u8"    }\r\n\r\n");

    //fprintf(pHppFile, u8"    bool UnmarshalJson(const std::string& json)\r\n");
    //fprintf(pHppFile, u8"    {\r\n");
    //fprintf(pHppFile, u8"        if (json.empty())\r\n");
    //fprintf(pHppFile, u8"        {\r\n");
    //fprintf(pHppFile, u8"            return true;\r\n");
    //fprintf(pHppFile, u8"        }\r\n");
    //fprintf(pHppFile, u8"        \r\n");
    //fprintf(pHppFile, u8"        %s::Handler h(*this, nullptr);\r\n", strName.c_str());
    //fprintf(pHppFile, u8"        JsonDeCode jd(&h);\r\n\r\n");
    //fprintf(pHppFile, u8"        JsonAllocator json_allocator;\r\n");
    //fprintf(pHppFile, u8"        rapidjson::GenericReader<rapidjson::UTF8<>, rapidjson::UTF8<>, JsonAllocator> rd(&json_allocator, 1024);\r\n");
    //fprintf(pHppFile, u8"        rapidjson::StringStream ss(json.c_str());\r\n");
    //fprintf(pHppFile, u8"        return rd.Parse<rapidjson::kParseNumbersAsStringsFlag>(ss, jd);\r\n");
    //fprintf(pHppFile, u8"    }\r\n");

    fprintf(pHppFile, u8"    bool UnmarshalJson(const std::string& json)\r\n");
    fprintf(pHppFile, u8"    {\r\n");
    fprintf(pHppFile, u8"        if (json.empty())\r\n");
    fprintf(pHppFile, u8"        {\r\n");
    fprintf(pHppFile, u8"            return true;\r\n");
    fprintf(pHppFile, u8"        }\r\n");
    fprintf(pHppFile, u8"        \r\n");
    fprintf(pHppFile, u8"        %s::Handler h(*this, nullptr);\r\n", strName.c_str());
    fprintf(pHppFile, u8"        JsonDeCode jd(&h);\r\n\r\n");
    fprintf(pHppFile, u8"        JsonAllocator json_allocator;\r\n");
    fprintf(pHppFile, u8"        rapidjson::GenericReader<rapidjson::UTF8<>, rapidjson::UTF8<>, JsonAllocator> rd(&json_allocator, 1024);\r\n");
    fprintf(pHppFile, u8"        rapidjson::StringStream ss(json.c_str());\r\n\r\n");
    fprintf(pHppFile, u8"        if (rd.Parse<rapidjson::kParseNumbersAsStringsFlag>(ss, jd))\r\n");
    fprintf(pHppFile, u8"        {\r\n");
    fprintf(pHppFile, u8"            return h.IsAllMemberSet();\r\n");
    fprintf(pHppFile, u8"        }\r\n\r\n");
    fprintf(pHppFile, u8"        return false;\r\n");
    fprintf(pHppFile, u8"    }\r\n");

    return true;
}

bool CProtocolMaker::__WriteStructProtocolEnCodeFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
	std::string strName = rXml.GetAttrib("name"); 
	fprintf(pHppFile, u8"\tbool EnCode(NetEnCode& net_data)\r\n\t{\r\n");
	
	bool is_empty = true;
	rXml.IntoElem();
    rXml.ResetMainPos();
	while (rXml.FindElem("item"))
	{
		is_empty = false;
		std::string strName = rXml.GetAttrib("name");
		std::string strType = rXml.GetAttrib("type");
		std::string strCount = rXml.GetAttrib("count");
		std::string strRefer = rXml.GetAttrib("refer");
		std::string strSelect = rXml.GetAttrib("select");
		std::string strArray = rXml.GetAttrib("array");
		std::string strEnFuncName = "";

		//if (is_integral(strType))
		//{
		//	strEnFuncName = "AddIntegral"
		//}
		
		if (strType == "string")
		{
			fprintf(pHppFile, u8"\t\tnet_data.AddString(%s, sizeof(%s));\r\n\r\n", strName.c_str(), strName.c_str());
		}
		else if (strCount.empty() && strArray.empty())
		{
			if (__is_integral(strType))
			{
				fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(%s);\r\n\r\n", strName.c_str());
			}
			else
			{
				fprintf(pHppFile, u8"\t\t%s.EnCode(net_data);\r\n\r\n", strName.c_str());
			}
		}
		else
		{
			if (!strArray.empty())
			{
				fprintf(pHppFile, u8"\t\tnet_data.AddArray(%s);\r\n\r\n", strName.c_str());
			}

			if (!strCount.empty())
			{
				if (__is_integral(strType))
				{
					if (strRefer.empty())
					{
						fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n", strCount.c_str());
					}
					else
					{
						fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
					}

					fprintf(pHppFile, u8"\t\t\tnet_data.AddBlob((char*)this->%s, iCount*sizeof(%s));\r\n\t\t}\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
				}
				else
				{
					fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t\t{\r\n", strCount.c_str());
					if (!strRefer.empty())
					{
						fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\t\tbreak;\r\n", strRefer.c_str());
					}

					fprintf(pHppFile, u8"\t\t\t%s[i].EnCode(net_data);\r\n\t\t}\r\n\r\n", strName.c_str());
				}
			}
		}
	}
	rXml.OutOfElem();

	if (is_empty)
	{
		fprintf(pHppFile, u8"\t\t(void)(net_data);\r\n");
	}

	fprintf(pHppFile, u8"\t\treturn true;\r\n");
	fprintf(pHppFile, u8"\t}\r\n");
	return true;
}

bool CProtocolMaker::__WriteStructProtocolDeCodeFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
	std::string strName = rXml.GetAttrib("name");
	fprintf(pHppFile, u8"\tbool DeCode(NetDeCode& net_data)\r\n\t{\r\n");
	bool is_empty = true;
	rXml.IntoElem();
    rXml.ResetMainPos();
	while (rXml.FindElem("item"))
	{
		is_empty = false;
		std::string strName = rXml.GetAttrib("name");
		std::string strType = rXml.GetAttrib("type");
		std::string strCount = rXml.GetAttrib("count");
		std::string strRefer = rXml.GetAttrib("refer");
		std::string strSelect = rXml.GetAttrib("select");
		std::string strArray = rXml.GetAttrib("array");

		if (strType == "string")
		{
			fprintf(pHppFile, u8"\t\tif (!net_data.DelString(%s, sizeof(%s)))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str(), strName.c_str());
		}
		else if (strCount.empty() && strArray.empty())
		{
			if (__is_integral(strType))
			{
				fprintf(pHppFile, u8"\t\tif (!net_data.DelIntegral(%s))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
			}
			else
			{
				fprintf(pHppFile, u8"\t\tif (!%s.DeCode(net_data))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
			}
		}
		else
		{
			if (!strArray.empty())
			{
				fprintf(pHppFile, u8"\t\tif(!net_data.DelArray(%s))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
			}

			if (!strCount.empty())
			{
				if (__is_integral(strType))
				{
					if (strRefer.empty())
					{
						fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n\t\tif(iCount < 0)\r\n\t\t\t\treturn false;\r\n", strCount.c_str());
					}
					else
					{
						fprintf(pHppFile, u8"\t\t{\r\n\t\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n\t\t\tif(iCount < 0)\r\n\t\t\t\treturn false;\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
					}

					fprintf(pHppFile, u8"\t\t\tif(!net_data.DelBlob((char*)this->%s, iCount*sizeof(%s)))\r\n\t\t\t\treturn false;\r\n\t}\r\n\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
				}
				else
				{
					fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t\t{\r\n", strCount.c_str());
					if (!strRefer.empty())
					{
						fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\t\tbreak;\r\n", strRefer.c_str());
					}

					fprintf(pHppFile, u8"\t\t\tif(!%s[i].DeCode(net_data))\r\n\t\t\t\treturn false;\r\n\t\t}\r\n\r\n", strName.c_str());
				}
			}
		}
	}
	rXml.OutOfElem();

	if (is_empty)
	{
		fprintf(pHppFile, u8"\t\t(void)(net_data);\r\n");
	}

	fprintf(pHppFile, u8"\t\treturn true;\r\n");
	fprintf(pHppFile, u8"\t}\r\n");

	return true;
}

bool CProtocolMaker::__WriteStructProtocolEnCodeExFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");
    fprintf(pHppFile, u8"\tbool EnCodeEx(NetEnCode& net_data) override\r\n\t{\r\n");

    bool is_empty = true;
    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        is_empty = false;
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");
        std::string strEnFuncName = "";

        //if (is_integral(strType))
        //{
        //	strEnFuncName = "AddIntegral"
        //}

        if (strType == "string")
        {
            fprintf(pHppFile, u8"\t\tnet_data.AddString(%s, sizeof(%s));\r\n\r\n", strName.c_str(), strName.c_str());
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(%s);\r\n\r\n", strName.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"\t\t%s.EnCode(net_data);\r\n\r\n", strName.c_str());
            }
        }
        else
        {
            if (!strArray.empty())
            {
                fprintf(pHppFile, u8"\t\tnet_data.AddArray(%s);\r\n\r\n", strName.c_str());
            }

            if (!strCount.empty())
            {
                if (__is_integral(strType))
                {
                    if (strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n", strCount.c_str());
                    }
                    else
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\tnet_data.AddBlob((char*)this->%s, iCount*sizeof(%s));\r\n\t\t}\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
                }
                else
                {
                    fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t\t{\r\n", strCount.c_str());
                    if (!strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\t\tbreak;\r\n", strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\t%s[i].EnCode(net_data);\r\n\t\t}\r\n\r\n", strName.c_str());
                }
            }
        }
    }
    rXml.OutOfElem();

    if (is_empty)
    {
        fprintf(pHppFile, u8"\t\t(void)(net_data);\r\n");
    }

    fprintf(pHppFile, u8"\t\treturn true;\r\n");
    fprintf(pHppFile, u8"\t}\r\n");
    return true;
}

bool CProtocolMaker::__WriteStructProtocolDeCodeExFunc(CMarkupSTL& rXml, FILE* pHppFile)
{
    std::string strName = rXml.GetAttrib("name");
    fprintf(pHppFile, u8"\tbool DeCodeEx(NetDeCode& net_data) override\r\n\t{\r\n");
    bool is_empty = true;
    rXml.IntoElem();
    rXml.ResetMainPos();
    while (rXml.FindElem("item"))
    {
        is_empty = false;
        std::string strName = rXml.GetAttrib("name");
        std::string strType = rXml.GetAttrib("type");
        std::string strCount = rXml.GetAttrib("count");
        std::string strRefer = rXml.GetAttrib("refer");
        std::string strSelect = rXml.GetAttrib("select");
        std::string strArray = rXml.GetAttrib("array");

        if (strType == "string")
        {
            fprintf(pHppFile, u8"\t\tif (!net_data.DelString(%s, sizeof(%s)))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str(), strName.c_str());
        }
        else if (strCount.empty() && strArray.empty())
        {
            if (__is_integral(strType))
            {
                fprintf(pHppFile, u8"\t\tif (!net_data.DelIntegral(%s))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
            }
            else
            {
                fprintf(pHppFile, u8"\t\tif (!%s.DeCode(net_data))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
            }
        }
        else
        {
            if (!strArray.empty())
            {
                fprintf(pHppFile, u8"\t\tif(!net_data.DelArray(%s))\r\n\t\t\treturn false;\r\n\r\n", strName.c_str());
            }

            if (!strCount.empty())
            {
                if (__is_integral(strType))
                {
                    if (strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\tint iCount = %s;\r\n\t\tif(iCount < 0)\r\n\t\t\t\treturn false;\r\n", strCount.c_str());
                    }
                    else
                    {
                        fprintf(pHppFile, u8"\t\t{\r\n\t\t\tint iCount = (((%s) < ((int)this->%s)) ? (%s) : ((int)this->%s));\r\n\t\t\tif(iCount < 0)\r\n\t\t\t\treturn false;\r\n", strCount.c_str(), strRefer.c_str(), strCount.c_str(), strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\tif(!net_data.DelBlob((char*)this->%s, iCount*sizeof(%s)))\r\n\t\t\t\treturn false;\r\n\t}\r\n\r\n", strName.c_str(), __integral_to_c_type(strType).c_str());
                }
                else
                {
                    fprintf(pHppFile, u8"\t\tfor(int i = 0; i < %s; i++)\r\n\t\t{\r\n", strCount.c_str());
                    if (!strRefer.empty())
                    {
                        fprintf(pHppFile, u8"\t\t\tif(i >= (int)this->%s)\r\n\t\t\t\tbreak;\r\n", strRefer.c_str());
                    }

                    fprintf(pHppFile, u8"\t\t\tif(!%s[i].DeCode(net_data))\r\n\t\t\t\treturn false;\r\n\t\t}\r\n\r\n", strName.c_str());
                }
            }
        }
    }
    rXml.OutOfElem();

    if (is_empty)
    {
        fprintf(pHppFile, u8"\t\t(void)(net_data);\r\n");
    }

    fprintf(pHppFile, u8"\t\treturn true;\r\n");
    fprintf(pHppFile, u8"\t}\r\n");

    return true;
}

//bool CProtocolMaker::__WriteUnionEnCodeFunc( CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile )
//{
//    std::string strName = rXml.GetAttrib("name");
//    fprintf(pHppFile, u8"int EnCode%s(void* pHost, int iSelect, CNetData* poNetData);\r\n", strName.c_str());
//    fprintf(pCppFile, u8"int EnCode%s(void* pHost, int iSelect, CNetData* poNetData)\r\n{\r\n\tswitch(iSelect){\r\n", strName.c_str());
//    rXml.IntoElem();
//    while (rXml.FindElem("item"))
//    {
//        std::string strId = rXml.GetAttrib("id");
//        std::string strType = rXml.GetAttrib("type");
//        if (strId.empty() || (!__IsStruct(strType)))
//        {
//            return false;
//        }
//
//        fprintf(pCppFile, u8"\tcase %s: return EnCode%s(pHost, poNetData);\r\n", strId.c_str(), strType.c_str());
//    }
//    rXml.OutOfElem();
//
//    fprintf(pCppFile, u8"\tdefault: return -1;\r\n\t}\r\n}\r\n");
//    return true;
//}
//
//bool CProtocolMaker::__WriteUnionDeCodeFunc( CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile )
//{
//    std::string strName = rXml.GetAttrib("name");
//    fprintf(pHppFile, u8"int DeCode%s(void* pHost, int iSelect, CNetData* poNetData);\r\n", strName.c_str());
//    fprintf(pCppFile, u8"int DeCode%s(void* pHost, int iSelect, CNetData* poNetData)\r\n{\r\n\tswitch(iSelect){\r\n", strName.c_str());
//    rXml.IntoElem();
//    while (rXml.FindElem("item"))
//    {
//        std::string strId = rXml.GetAttrib("id");
//        std::string strType = rXml.GetAttrib("type");
//        if (strId.empty() || (!__IsStruct(strType)))
//        {
//            return false;
//        }
//
//        fprintf(pCppFile, u8"\tcase %s: return DeCode%s(pHost, poNetData);\r\n", strId.c_str(), strType.c_str());
//    }
//    rXml.OutOfElem();
//    fprintf(pCppFile, u8"\tdefault: return -1;\r\n\t}\r\n}\r\n");
//    return true;
//}

bool CProtocolMaker::__WriteProtocolClass( const std::string& strProtocolName, FILE* pHppFile )
{
    if (m_vecProtocol.empty())
    {
        return true;
    }

    //fprintf(pHppFile, "typedef int (*EnCodeFunc%s)(void *pHost, CNetData* poNetData);\r\ntypedef int (*DeCodeFunc%s)(void *pHost, CNetData* poNetData);\r\n\r\n", strProtocolName.c_str(), strProtocolName.c_str());
    //fprintf(pHppFile, u8"class C%s\r\n{\r\npublic:\r\n\tC%s();\r\n\tvirtual ~C%s();\r\n", strProtocolName.c_str(), strProtocolName.c_str(), strProtocolName.c_str());
    fprintf(pHppFile, u8"template<typename D>\r\n");
    fprintf(pHppFile, u8"class C%s\r\n{\r\npublic:\r\n", strProtocolName.c_str());
    //添加成员函数
        //构造函数
    fprintf(pHppFile, u8"\tC%s()\r\n\t{\r\n\t\tsize_t max_protocol_size = 0;\r\n", strProtocolName.c_str());
    //for (int i = 0; i < (int)m_vecProtocol.size(); i++)
    //{
    //    fprintf(pHppFile, "\tm_EnCodeFuncArray[%d] = &EnCode%s;\r\n", i, m_vecProtocol[i].c_str());
    //    fprintf(pHppFile, "\tm_DeCodeFuncArray[%d] = &DeCode%s;\r\n", i, m_vecProtocol[i].c_str());
    //}
    for (size_t i = 0; i < m_vecProtocol.size(); i++)
    {
        fprintf(pHppFile, u8"\t\tif (sizeof(%s) > max_protocol_size)\r\n\t\t\tmax_protocol_size = sizeof(%s);\r\n\r\n", m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
    }

    fprintf(pHppFile, u8"\t\tm_protocol_buffer = S_MALLOC(max_protocol_size);\r\n");

    fprintf(pHppFile, u8"\t}\r\n\r\n");
    //析构函数
    fprintf(pHppFile, u8"\tvirtual ~C%s()\r\n\t{\r\n", strProtocolName.c_str());
    //for (int i = 0; i < (int)m_vecProtocol.size(); i++)
    //{
    //    fprintf(pHppFile, "\tm_EnCodeFuncArray[%d] = &EnCode%s;\r\n", i, m_vecProtocol[i].c_str());
    //    fprintf(pHppFile, "\tm_DeCodeFuncArray[%d] = &DeCode%s;\r\n", i, m_vecProtocol[i].c_str());
    //}
    fprintf(pHppFile, u8"\t\tif (m_protocol_buffer)\r\n");
    fprintf(pHppFile, u8"\t\t{\r\n");
    fprintf(pHppFile, u8"\t\t\tS_FREE(m_protocol_buffer);\r\n");
    fprintf(pHppFile, u8"\t\t\tm_protocol_buffer = 0;\r\n");
    fprintf(pHppFile, u8"\t\t}\r\n");

    fprintf(pHppFile, u8"\t}\r\n\r\n");
    //fprintf(pHppFile, "C%s::~C%s()\r\n{\r\n\tif (m_EnCodeBuffer)\r\n\t{\r\n\t\tS_FREE(m_EnCodeBuffer);\r\n\t}\r\n\r\n\tif (m_DeCodeBuffer)\r\n\t{\r\n\t\tS_FREE(m_DeCodeBuffer);\r\n\t}\r\n}\r\n\r\n", strProtocolName.c_str(), strProtocolName.c_str());

    //构建协议函数
    //fprintf(pHppFile, "int C%s::BuildProtocol(void* pHost, char *pNet, int iNetSize)\r\n{\r\n", strProtocolName.c_str());
    fprintf(pHppFile, u8"\ttemplate<typename T>\r\n");
    fprintf(pHppFile, u8"\tbool BuildProtocol(TProtocol<T>& proto, NetEnCode& net_data)\r\n\t{\r\n");
    fprintf(pHppFile, u8"\t\tif (proto.module_id != %s)\r\n", m_strMoudleID.c_str());
    fprintf(pHppFile, u8"\t\t\treturn false;\r\n\r\n");
    fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(proto.module_id);\r\n");
    fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(proto.protocol_id);\r\n\r\n");

    fprintf(pHppFile, u8"\t\treturn static_cast<T&>(proto).EnCode(net_data);\r\n");
    fprintf(pHppFile, u8"\t}\r\n\r\n");


    fprintf(pHppFile, u8"\tbool BuildProtocol(Protocol* proto, NetEnCode& net_data)\r\n\t{\r\n");
    fprintf(pHppFile, u8"\t\tif (proto->ModuleIdEx() != %s)\r\n", m_strMoudleID.c_str());
    fprintf(pHppFile, u8"\t\t\treturn false;\r\n\r\n");
    fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(proto->ModuleIdEx());\r\n");
    fprintf(pHppFile, u8"\t\tnet_data.AddIntegral(proto->ProtocolIdEx());\r\n\r\n");

    fprintf(pHppFile, u8"\t\treturn proto->EnCodeEx(net_data);\r\n");
    fprintf(pHppFile, u8"\t}\r\n\r\n");

    //协议名获取函数
    fprintf(pHppFile, u8"\tconst char* ProtocolName(unsigned short protocol_id) const\r\n\t{\r\n");
    fprintf(pHppFile, u8"\t\tstatic char unknow_protocol[32];\r\n\r\n");
    fprintf(pHppFile, u8"\t\tswitch (protocol_id)\r\n\t\t{\r\n");

    for (size_t i = 0; i < m_vecProtocol.size(); i++)
    {
#ifdef MT
        fprintf(pHppFile, u8"\t\tcase %zu:\r\n", i + 1);
#else
        fprintf(pHppFile, u8"\t\tcase %zu:\r\n", i);
#endif
        fprintf(pHppFile, u8"\t\t{\r\n");
        fprintf(pHppFile, u8"\t\t\treturn %s::Name();\r\n", m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t}\r\n\t\tbreak;\r\n");
    }

    fprintf(pHppFile, u8"\t\tdefault:\r\n");
    fprintf(pHppFile, u8"\t\t{\r\n");
    fprintf(pHppFile, u8"\t\t\tsnprintf(unknow_protocol, sizeof(unknow_protocol), u8\"Unknow Protocol Id = %%d\", protocol_id);\r\n");
    fprintf(pHppFile, u8"\t\t\treturn unknow_protocol;\r\n");
    fprintf(pHppFile, u8"\t\t}\r\n");
    fprintf(pHppFile, u8"\t\t}\r\n");
    fprintf(pHppFile, u8"\t}\r\n\r\n");

    //处理协议函数
    fprintf(pHppFile, u8"\tbool HandleProtocol(NetDeCode& net_data)\r\n\t{\r\n");
    fprintf(pHppFile, u8"\t\tunsigned short m_id = 0;\r\n");
    fprintf(pHppFile, u8"\t\tunsigned short p_id = 0;\r\n");
    fprintf(pHppFile, u8"\t\tsize_t net_data_pos = net_data.CurPos();\r\n\r\n");
    fprintf(pHppFile, u8"\t\tif (!net_data.DelIntegral(m_id) || !net_data.DelIntegral(p_id))\r\n");
    fprintf(pHppFile, u8"\t\t{\r\n");
    fprintf(pHppFile, u8"\t\t\tnet_data.Reset(net_data_pos);\r\n");
    fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
    fprintf(pHppFile, u8"\t\t}\r\n\r\n");
    fprintf(pHppFile, u8"\t\tif (m_id != %s)\r\n", m_strMoudleID.c_str());
    fprintf(pHppFile, u8"\t\t{\r\n");
    fprintf(pHppFile, u8"\t\t\tnet_data.Reset(net_data_pos);\r\n");
    fprintf(pHppFile, u8"\t\t\treturn false;\r\n");
    fprintf(pHppFile, u8"\t\t}\r\n\r\n");
    fprintf(pHppFile, u8"\t\tswitch(p_id)\r\n\t\t{\r\n");

    for (size_t i = 0; i < m_vecProtocol.size(); i++)
    {
#ifdef MT
        fprintf(pHppFile, u8"\t\tcase %zu:\r\n", i + 1);
#else
        fprintf(pHppFile, u8"\t\tcase %zu:\r\n", i);
#endif

        
        fprintf(pHppFile, u8"\t\t{\r\n");
        //fprintf(pHppFile, "\t\t\t%s* proto = (%s*)m_protocol_buffer;\r\n", m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
        //fprintf(pHppFile, "\t\t\tnew(proto)%s();\r\n", m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t\t%s* proto = new(m_protocol_buffer) %s();\r\n", m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t\tif (proto->DeCode(net_data))\r\n");
        fprintf(pHppFile, u8"\t\t\t{\r\n");
        fprintf(pHppFile, u8"\t\t\t\tstatic_cast<D*>(this)->OnRecv_%s(*proto);\r\n", m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t\t\tproto->~%s();\r\n", m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t\t\treturn true;\r\n");
        fprintf(pHppFile, u8"\t\t\t}\r\n\t\t\telse\r\n\t\t\t{\r\n");
        fprintf(pHppFile, u8"\t\t\t\tproto->~%s();\r\n", m_vecProtocol[i].c_str());
        fprintf(pHppFile, u8"\t\t\t\tnet_data.Reset(net_data_pos);\r\n");
        fprintf(pHppFile, u8"\t\t\t\treturn false;\r\n");
        fprintf(pHppFile, u8"\t\t\t}\r\n");
        fprintf(pHppFile, u8"\t\t}\r\n\t\tbreak;\r\n");
    }

    fprintf(pHppFile, u8"\t\tdefault:\r\n\t\t\treturn false;\r\n\t\t}\r\n\r\n");
    fprintf(pHppFile, u8"\t}\r\n\r\n");
    //fprintf(pHppFile, "\tint BuildProtocol(void* pHost, char *pNet, int iNetSize);\r\n\r\n");
    //fprintf(pHppFile, "\tbool HandleProtocol(char *pNet, int iNetSize, void* pHost);\r\n\r\n");
	//fprintf(pHppFile, u8"\tbool BuildProtocol(protocol_base* proto, NetEnCode& net_data);\r\n\r\n");
	//fprintf(pHppFile, u8"\tbool HandleProtocol(NetDeCode& net_data);\r\n\r\n");
    fprintf(pHppFile, u8"\tstatic inline unsigned short GetModuleID(void){ return %s; }\r\n\r\n", m_strMoudleID.c_str());
    fprintf(pHppFile, u8"\tstatic inline unsigned short GetProtocolNum(void){ return %d; }\r\n\r\n", m_vecProtocol.size());
    fprintf(pHppFile, u8"\tstatic const unsigned short module_id = %s;\r\n\r\n", m_strMoudleID.c_str());
    fprintf(pHppFile, u8"\tstatic const unsigned short protocol_num = %d;\r\n\r\n", m_vecProtocol.size());
    //添加各协议的回调纯虚函数
    fprintf(pHppFile, u8"//===============以下协议回调函数需要使用者来实现===============\r\n");
    for (int i = 0; i < (int)m_vecProtocol.size(); i++)
    {
        fprintf(pHppFile, u8"\tvoid OnRecv_%s(%s& rstProtocol){ (void)(rstProtocol); };\r\n", m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
    }
    //fprintf(pHppFile, "private:\r\n\tEnCodeFunc%s m_EnCodeFuncArray[%d];\r\n\tEnCodeFunc%s m_DeCodeFuncArray[%d];\r\n\tchar* m_EnCodeBuffer;\r\n\tchar* m_DeCodeBuffer;\r\n\tsize_t m_EnCodeBufferSize;\r\n\tsize_t m_DeCodeBufferSize;\r\n};\r\n", strProtocolName.c_str(), m_mapProtocol.size(), strProtocolName.c_str(), m_mapProtocol.size());
	fprintf(pHppFile, u8"private:\r\n\t void* m_protocol_buffer;\r\n");

    //fprintf(pHppFile, "\tCNetData m_oData;\r\n");
    //fprintf(pHppFile, "\tm_oData.Prepare(pNet, iNetSize);\r\n\r\n\tunsigned short moudleid = 0;\r\n\tunsigned short protocolid = 0;\r\n\tif (m_oData.DelWord(moudleid) < 0)\r\n\t{\r\n\t\treturn false;\r\n\t}\r\n\tif (moudleid != %s)\r\n\t{\r\n\t\treturn false;\r\n\t}\r\n\tif (m_oData.DelWord(protocolid) < 0)\r\n\t{\r\n\t\treturn false;\r\n\t}\r\n\tif (protocolid >= sizeof(m_DeCodeFuncArray)/sizeof(DeCodeFunc%s))\r\n\t{\r\n\t\treturn false;\r\n\t}\r\n\r\n\tm_oData.Prepare(pNet, iNetSize);\r\n\r\n\tif (m_DeCodeFuncArray[protocolid](pHost, &m_oData) < 0)\r\n\t{\r\n\t\treturn false;\r\n\t}\r\n\r\n",m_strMoudleID.c_str(), strProtocolName.c_str());

    //fprintf(pHppFile, "\tswitch(protocolid)\r\n\t{\r\n");
    //for (int i = 0; i < (int)m_vecProtocol.size(); i++)
    //{
    //    fprintf(pHppFile, "\tcase %d:\r\n\t\tOnRecv_%s(*(%s*)pHost);\r\n\t\tbreak;\r\n", i, m_vecProtocol[i].c_str(), m_vecProtocol[i].c_str());
    //}
    //fprintf(pHppFile, "\tdefault:\r\n\t\treturn false;\r\n\t}\r\n\r\n\treturn true;\r\n}\r\n\r\n");
    //协议回调函数
    fprintf(pHppFile, u8"\r\n};\r\n");

    return true;
}

std::string& CProtocolMaker::GetErrorInfo( void )
{
    return m_strErrorInfo;
}


