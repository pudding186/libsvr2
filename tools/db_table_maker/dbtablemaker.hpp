#pragma once
#include <string>

class DBTableMaker
{
public:
    DBTableMaker(void);
    ~DBTableMaker(void);

    std::string MakeDBTable(const std::string& xml, const std::string& out_put_path);

private:
    std::string m_strErrorInfo;
};

