// protocol_maker.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "protocolmaker.h"

CProtocolMaker gProtocolMaker;

CProtocolMaker gProtocolMaker2;

CProtocolMaker gProtocolMaker3;

CProtocolMaker gProtocolMaker4;

void debug_protocol()
{
    if (!gProtocolMaker.MakeProtocol("D:\\mt_2020\\trunk\\common\\protocol\\Global.xml", "D:\\mt_2020\\trunk\\common\\protocol\\"))
    {
        printf("make %s fail! errinfo:\n", "D:\\mt_2020\\trunk\\common\\protocol\\Global.xml");
        printf(gProtocolMaker.GetErrorInfo().c_str());
        printf("\n");
        return;
    }
    
    if (!gProtocolMaker2.MakeProtocol("D:\\mt_2020\\trunk\\common\\protocol\\StructDef.xml", "D:\\mt_2020\\trunk\\common\\protocol\\"))
    {
        printf("make %s fail! errinfo:\n", "D:\\mt_2020\\trunk\\common\\protocol\\StructDef.xml");
        printf(gProtocolMaker.GetErrorInfo().c_str());
        printf("\n");
        return;
    }

    if (!gProtocolMaker3.MakeProtocol("D:\\mt_2020\\trunk\\common\\protocol_st\\STDataInfo.xml", "D:\\mt_2020\\trunk\\common\\protocol_st\\"))
    {
        printf("make %s fail! errinfo:\n", "D:\\mt_2020\\trunk\\common\\protocol_st\\STDataInfo.xml");
        printf(gProtocolMaker.GetErrorInfo().c_str());
        printf("\n");
        return;
    }

    if (!gProtocolMaker4.MakeProtocol("D:\\mt_2020\\trunk\\common\\protocol_st\\STGlobal.xml", "D:\\mt_2020\\trunk\\common\\protocol_st\\"))
    {
        printf("make %s fail! errinfo:\n", "D:\\mt_2020\\trunk\\common\\protocol_st\\STGlobal.xml");
        printf(gProtocolMaker.GetErrorInfo().c_str());
        printf("\n");
        return;
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
    debug_protocol();

    return 1;
#endif

    //char* pszXml = NULL;
    //char* pszPath = NULL;
    //char* pszPrefix = NULL;
    //if (3 == argc)
    //{
    //    pszXml = argv[1];
    //    pszPath = argv[2];
    //}

    //if (2 == argc)
    //{
    //    pszXml = argv[0];
    //    pszPath = argv[1];
    //}
    std::string xml = "";
    std::string path = "";
    std::string prefix = "";

    if (4 == argc)
    {
        xml = argv[1];
        path = argv[2];
        prefix = argv[3];
    }
    else if (3 == argc)
    {
        xml = argv[1];
        path = argv[2];
    }
    else
    {
        printf("make %s fail! errinfo: wrong argv[%d]\n", xml.c_str(), argc);
        
        for (int i = 0; i < argc; i++)
        {
            printf("argv[%d] = %s\n", i, argv[i]);
        }

        printf("\n");
        return -1;
    }


    if (!gProtocolMaker.MakeProtocol( xml, path, prefix))
    {
        printf("make %s fail! errinfo:\n", xml.c_str());
        printf(gProtocolMaker.GetErrorInfo().c_str());
		printf("\n");
        return -1;
    }
	else
	{
		printf("make %s success!\n", xml.c_str());
	}

	return 0;
}

