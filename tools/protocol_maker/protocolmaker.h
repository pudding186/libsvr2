﻿#pragma once
#include "markupstl.h"
#include <set>
#include <vector>
#include <map>

#define MT

enum EDataType
{
    eStruct = 1,
    eUnion = 2,
    eProtocol
};

enum EATTRIB
{
    eName = 0,
    eType,
    eCount,
    eRefer,
    eId,
    eSelect,
    eLength,
    eComment,
    eArray,
    eAlias,
    eDefault,
    eIndex,
    eMax
};

typedef std::set<std::string> CDataSet;
typedef std::vector<std::string> CAttrib;
typedef std::map<std::string, CAttrib> CItem;
typedef std::map<std::string, CItem> CData;
typedef std::vector<std::string> CProtocolArry;

class CProtocolMaker
{
public:
    CProtocolMaker(void);
    ~CProtocolMaker(void);

    bool MakeProtocol(const std::string& strXML, const std::string& strOutPutPath, const std::string& strPrefix = "");
    std::string& GetErrorInfo(void);
protected:
    bool __IsKeyType(const std::string& strType);
    bool __IsStruct(const std::string& strType);
    bool __IsUnion(const std::string& strType);
    bool __IsMacro(const std::string& strType);
    bool __IsAllDigit(const std::string& strTest);

    bool __CheckPackge(const std::string& strPkg);
    bool __WritePackge(FILE* pHppFile, CMarkupSTL& rXml, const std::string& strPackgePath);
    bool __WriteMacro(FILE* pHppFile, CMarkupSTL& rXml);
    bool __WriteData(FILE* pHppFile, FILE* pCppFile, CMarkupSTL& rXml);
    //bool __WriteHandler(FILE* pHppFile, CMarkupSTL& rXml);
    //bool __WriteDynamicProtocol(FILE* pHppFile, CMarkupSTL& rXml);
    //bool __WriteDataFunction(FILE* pHppFile, FILE* pCppFile, CMarkupSTL& rXml);
    bool __WriteItem(FILE* pHppFile, CMarkupSTL& rXml, EDataType eDataType, CItem& mapItem);
    bool __WriteStructProtocolEnCodeFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolDeCodeFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolEnCodeExFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolDeCodeExFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolResetFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolOperatorEqual(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolOperatorCopy(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteStructProtocolToJsonFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteMarshalUnmarshalJsonFunc(CMarkupSTL& rXml, FILE* pHppFile);
    bool __WriteHandler(CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile);
    //bool __WriteUnionEnCodeFunc(CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile);
    //bool __WriteUnionDeCodeFunc(CMarkupSTL& rXml, FILE* pHppFile, FILE* pCppFile);
    bool __WriteProtocolClass(const std::string& strProtocolName, FILE* pHppFile);
private:
    CDataSet        m_setKeyType;
    CDataSet        m_setMacro;
    CData           m_mapStruct;
    CData           m_mapUnion;
    CData           m_mapProtocol;
    CProtocolArry   m_vecProtocol;
    std::string     m_strMoudleID;

    std::string m_strErrorInfo;
};

