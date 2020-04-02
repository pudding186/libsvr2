
// TableMakerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "table_maker.h"
#include "TableMakerDlg.h"
#include "afxdialogex.h"
#include "alltask.h"
#include "TaskManager.h"

#ifdef _DEBUG
#pragma comment(lib, "../dmxlsx/bin/Debug/dmxlsx.lib")
#pragma comment(lib, "../dmxlsx/bin/Debug/dmxlsximpl.lib")
#pragma comment(lib, "../dmxlsx/bin/Debug/pugixml.lib")
#define new DEBUG_NEW
#else
#pragma comment(lib, "../dmxlsx/bin/Release/dmxlsx.lib")
#pragma comment(lib, "../dmxlsx/bin/Release/dmxlsximpl.lib")
#pragma comment(lib, "../dmxlsx/bin/Release/pugixml.lib")
#endif

TaskManager g_task_mgr;

void DoEvents()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

// CTableMakerDlg 对话框


CTableMakerDlg::CTableMakerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TABLEMAKER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTableMakerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST_EXCEL, m_excel_list);
    DDX_Control(pDX, IDC_LIST_RESULT, m_result_box);
}

BEGIN_MESSAGE_MAP(CTableMakerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDCANCEL, &CTableMakerDlg::OnClose)
    ON_BN_CLICKED(IDC_BUTTON_LOAD, &CTableMakerDlg::OnBnClickedButtonLoad)
    ON_WM_CLOSE()
    ON_BN_CLICKED(IDC_BUTTON_CODE, &CTableMakerDlg::OnBnClickedButtonCode)
    ON_BN_CLICKED(IDC_BUTTON_XML, &CTableMakerDlg::OnBnClickedButtonXml)
    ON_WM_CONTEXTMENU()
    ON_COMMAND(ID_MENU_ALL_CANCEL, &CTableMakerDlg::OnMenuAllCancel)
    ON_COMMAND(ID_MENU_ALL_SELECT, &CTableMakerDlg::OnMenuAllSelect)
    ON_COMMAND(ID_MENU_ALL_REVERSE, &CTableMakerDlg::OnMenuAllReverse)
    ON_COMMAND(ID_MENU_LOAD, &CTableMakerDlg::OnMenuLoad)
    ON_COMMAND(ID_RESULT_CLEAR, &CTableMakerDlg::OnMenuClear)
    ON_WM_CTLCOLOR()
    //ON_MESSAGE(WM_KICKIDLE, OnKickIdle)
    ON_WM_TIMER()
END_MESSAGE_MAP()


//bool open_file(const char* path, const char* file, void* user_data)
//{
//    CTableMakerDlg* dlg = (CTableMakerDlg*)user_data;
//
//    return dlg->open_excel(path, file);
//}
// CTableMakerDlg 消息处理程序

wchar_t* U8ToUnicode(const char* szU8)
{
    //UTF8 to Unicode
    //预转换，得到所需空间的大小
    int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, szU8, static_cast<int>(strlen(szU8)), NULL, 0);
    //分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
    wchar_t* wszString = new wchar_t[wcsLen + 1];
    //转换
    ::MultiByteToWideChar(CP_UTF8, NULL, szU8, static_cast<int>(strlen(szU8)), wszString, wcsLen);
    //最后加上'\0'
    wszString[wcsLen] = '\0';

    return wszString;
}

char* UnicodeToU8(const wchar_t* wszString)
{
    // unicode to UTF8
    //预转换，得到所需空间的大小，这次用的函数和上面名字相反
    int u8Len = ::WideCharToMultiByte(CP_UTF8, NULL, wszString, static_cast<int>(wcslen(wszString)), NULL, 0, NULL, NULL);
    //同上，分配空间要给'\0'留个空间
    //UTF8虽然是Unicode的压缩形式，但也是多字节字符串，所以可以以char的形式保存
    char* szU8 = new char[u8Len + 1];
    //转换
    //unicode版对应的strlen是wcslen
    ::WideCharToMultiByte(CP_UTF8, NULL, wszString, static_cast<int>(wcslen(wszString)), szU8, u8Len, NULL, NULL);
    //最后加上'\0'
    szU8[u8Len] = '\0';
    return szU8;
}

char* UnicodeToLocal(const wchar_t* wszString)
{
    // unicode to UTF8
    //预转换，得到所需空间的大小，这次用的函数和上面名字相反
    int u8Len = ::WideCharToMultiByte(CP_ACP, NULL, wszString, static_cast<int>(wcslen(wszString)), NULL, 0, NULL, NULL);
    //同上，分配空间要给'\0'留个空间
    //UTF8虽然是Unicode的压缩形式，但也是多字节字符串，所以可以以char的形式保存
    char* szU8 = new char[u8Len + 1];
    //转换
    //unicode版对应的strlen是wcslen
    ::WideCharToMultiByte(CP_ACP, NULL, wszString, static_cast<int>(wcslen(wszString)), szU8, u8Len, NULL, NULL);
    //最后加上'\0'
    szU8[u8Len] = '\0';
    return szU8;
}

wchar_t* LocalToUnicode(const char* szLocal)
{
    //UTF8 to Unicode
    //预转换，得到所需空间的大小
    int wcsLen = ::MultiByteToWideChar(CP_ACP, NULL, szLocal, static_cast<int>(strlen(szLocal)), NULL, 0);
    //分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
    wchar_t* wszString = new wchar_t[wcsLen + 1];
    //转换
    ::MultiByteToWideChar(CP_ACP, NULL, szLocal, static_cast<int>(strlen(szLocal)), wszString, wcsLen);
    //最后加上'\0'
    wszString[wcsLen] = '\0';

    return wszString;
}


BOOL CTableMakerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	//ShowWindow(SW_MAXIMIZE);

	ShowWindow(SW_SHOW);

	// TODO: 在此添加额外的初始化代码
    ListView_SetExtendedListViewStyle(m_excel_list.GetSafeHwnd(), m_excel_list.GetExStyle() | LVS_EX_CHECKBOXES);
    m_excel_list.InsertColumn(0, L"Excel", LVCFMT_LEFT, 150);
    m_result_box.AddLine(CXListBox::Blue, CXListBox::White, L"遍历所有excel文件!");

    if (!g_task_mgr.Init(1024))
    {
        return FALSE;
    }

    if (!g_task_mgr.AddTask(g_task_mgr.CreateTask<InitTask>(get_tick())))
    {
        return FALSE;
    }

    m_task_timer = SetTimer(1, 30, 0);

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CTableMakerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CTableMakerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTableMakerDlg::OnLvnItemchangedList1(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    // TODO: Add your control notification handler code here
    /*
    typedef struct tagNMLISTVIEW
    {
    NMHDR   hdr;
    int     iItem;
    int     iSubItem;
    UINT    uNewState;
    UINT    uOldState;
    UINT    uChanged;
    POINT   ptAction;
    LPARAM  lParam;
    } NMLISTVIEW, *LPNMLISTVIEW;
    */
    CString csTrace;

    if ((pNMLV->uOldState & INDEXTOSTATEIMAGEMASK(1)) /* old state : unchecked */
        && (pNMLV->uNewState & INDEXTOSTATEIMAGEMASK(2)) /* new state : checked */
        )
    {
        csTrace.Format(L"Item %d is checked\n", pNMLV->iItem);
        TRACE(csTrace);
        //SetDlgItemText(IDC_TRACE, csTrace);
    }
    else if ((pNMLV->uOldState & INDEXTOSTATEIMAGEMASK(2)) /* old state : checked */
        && (pNMLV->uNewState & INDEXTOSTATEIMAGEMASK(1)) /* new state : unchecked */
        )
    {
        csTrace.Format(L"Item %d is unchecked\n", pNMLV->iItem);
        TRACE(csTrace);
        //SetDlgItemText(IDC_TRACE, csTrace);
    }
    else // if you don't click on the check-box
    {
        TRACE(L"Item %d does't change the check-status\n", pNMLV->iItem);
    }


    *pResult = 0;
}

void CTableMakerDlg::OnBnClickedButtonLoad()
{
    for (auto it = m_maker_client_list.begin();
        it != m_maker_client_list.end(); ++it)
    {
        delete it->second;
    }

    m_maker_client_list.clear();

    for (auto it = m_maker_server_list.begin();
        it != m_maker_server_list.end(); ++it)
    {
        delete it->second;
    }

    m_maker_server_list.clear();
    m_excel_load_list.clear();
    m_selected_table.clear();
    m_excel_load_tick = get_tick();

    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        if (m_excel_list.GetCheck(i))
        {
            std::wstring tmp = m_excel_file_list[i];
            m_selected_table.insert(tmp.substr(0, tmp.length() - wcslen(L".xlsx")));
            std::wstring msg = L"加载";
            msg += m_excel_file_list[i];
            msg += L"  ";
            int idx = m_result_box.AddLine(CXListBox::Blue, CXListBox::White, msg.c_str());
            g_task_mgr.AddTask(g_task_mgr.CreateTask<LoadTask>(idx, m_excel_path, m_excel_file_list[i]));
        }
    }

}

void CTableMakerDlg::OnClose()
{
    KillTimer(m_task_timer);
    g_task_mgr.Uninit();
	CDialog::OnCancel();
}

void CTableMakerDlg::OnBnClickedButtonCode()
{
    m_selected_table.clear();
    m_code_gen_list.clear();
    m_gen_code_tick = get_tick();

    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        if (m_excel_list.GetCheck(i))
        {
            std::wstring tmp = m_excel_file_list[i];
            tmp = tmp.substr(0, tmp.length() - wcslen(L".xlsx"));
            m_selected_table.insert(tmp);
            std::wstring msg = L"生成";
            msg += m_excel_file_list[i];
            msg += L"  对应代码";

            CTableMaker* table_maker = 0;

            m_selected_table.insert(tmp);

            auto it_maker = m_maker_server_list.find(tmp);

            if (it_maker != m_maker_server_list.end())
            {
                table_maker = it_maker->second;
            }
            int idx = m_result_box.AddLine(CXListBox::Blue, CXListBox::White, msg.c_str());
            g_task_mgr.AddTask(g_task_mgr.CreateTask<GenCodeTask>(idx, m_code_path, tmp, table_maker));
        }
    }
}

void CTableMakerDlg::OnBnClickedButtonXml()
{
    m_selected_table.clear();
    m_xml_gen_list.clear();
    m_gen_xml_tick = get_tick();

    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        if (m_excel_list.GetCheck(i))
        {
            std::wstring tmp = m_excel_file_list[i];
            tmp = tmp.substr(0, tmp.length() - wcslen(L".xlsx"));
            m_selected_table.insert(tmp);
            std::wstring msg = L"生成";
            msg += m_excel_file_list[i];
            msg += L"  对应XML";

            CTableMaker* table_maker = 0;
            m_selected_table.insert(tmp);

            auto it_maker = m_maker_server_list.find(tmp);

            if (it_maker != m_maker_server_list.end())
            {
                table_maker = it_maker->second;
            }

            int idx = m_result_box.AddLine(CXListBox::Blue, CXListBox::White, msg.c_str());
            g_task_mgr.AddTask(g_task_mgr.CreateTask<GenXmlTask>(idx, m_excel_path, tmp, m_xml_path, table_maker));
        }
    }

}

void CTableMakerDlg::OnContextMenu(CWnd* pWnd, CPoint point)
{
    CRect rect;
    GetDlgItem(IDC_LIST_EXCEL)->GetWindowRect(&rect);

    if (rect.PtInRect(point))
    {
        CMenu menu;
        VERIFY(menu.LoadMenu(IDR_MENU_EXCEL));

        CMenu* popup = menu.GetSubMenu(0);

        popup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);

        return;
    }

    GetDlgItem(IDC_LIST_RESULT)->GetWindowRect(&rect);

    if (rect.PtInRect(point))
    {
        CMenu menu;
        VERIFY(menu.LoadMenu(IDR_MENU_RESULT));

        CMenu* popup = menu.GetSubMenu(0);

        popup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);

        return;
    }
}

void CTableMakerDlg::OnMenuAllCancel()
{
    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        m_excel_list.SetCheck(i, FALSE);
    }
}

void CTableMakerDlg::OnMenuAllSelect()
{
    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        m_excel_list.SetCheck(i, TRUE);
    }
}

void CTableMakerDlg::OnMenuAllReverse()
{
    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        if (m_excel_list.GetCheck(i))
        {
            m_excel_list.SetCheck(i, FALSE);
        }
        else
        {
            m_excel_list.SetCheck(i, TRUE);
        }
    }
}

void CTableMakerDlg::OnMenuLoad()
{

}

void CTableMakerDlg::OnMenuClear()
{
    m_result_box.ResetContent();
}

HBRUSH CTableMakerDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

    // TODO:  Change any attributes of the DC here
    switch (pWnd->GetDlgCtrlID())
    {
    case IDC_LIST_RESULT:
    {
        //OutputDebugString(L"lalal");
    }
    break;
    }

    // TODO:  Return a different brush if the default is not desired
    return hbr;
}

//LRESULT CTableMakerDlg::OnKickIdle(WPARAM wParam, LPARAM lParam)
//{
//    //if (g_task_mgr.Run(0))
//    //{
//    //    return 1;
//    //}
//
//    //return 0;
//    g_task_mgr.Run(0);
//
//    return 1;
//}


void CTableMakerDlg::UpdateExcelList(unsigned int elapse, std::vector<std::wstring>& excel_list, std::wstring& excel_path, std::wstring& code_path, std::wstring& xml_path)
{
    m_excel_path = excel_path;
    m_code_path = code_path;
    m_xml_path = xml_path;

    m_excel_file_list = excel_list;

    for (size_t i = 0; i < m_excel_file_list.size(); i++)
    {
        std::wstring name = m_excel_file_list[i];

        name = name.substr(0, name.length() - wcslen(L".xlsx"));
        m_excel_list.InsertItem(i, name.c_str());
    }
    wchar_t tmp_str[256];
    _snwprintf(tmp_str, 256, L"遍历总耗时%.2f", (float)(elapse) / 1000);
    m_result_box.AddLine(CXListBox::Blue, CXListBox::White, tmp_str);
}

void CTableMakerDlg::AddExcelResult(
    int idx,
    const std::wstring& err_msg, 
    unsigned int elapse, 
    std::wstring table_name, 
    CTableMaker* server_table, 
    CTableMaker* client_table)
{
    float real_elapse = (float)(get_tick() - m_excel_load_tick);

    std::wstring msg = L"加载";
    msg += table_name;
    msg += L".xlsx  ";

    float f_elapse = elapse;

    CXListBox::Color tc, bc;
    bc = CXListBox::White;

    if (err_msg.length())
    {
        tc = CXListBox::Red;

        msg.append(L"×    ");
        msg.append(err_msg);

        m_excel_load_list[table_name] = false;
    }
    else
    {
        tc = CXListBox::Green;

        msg.append(L"√    ");
        msg.append(L"成功 ");

        m_maker_server_list[table_name] = server_table;
        m_maker_client_list[table_name] = client_table;
        m_excel_load_list[table_name] = true;
    }

    msg.append(L"耗时");
    wchar_t wsz_tmp[256];
    _snwprintf(wsz_tmp, 256, L"%.2f秒", f_elapse / 1000);
    msg.append(wsz_tmp);
    m_result_box.DeleteString(idx);
    m_result_box.InsertString(idx, msg.c_str(), tc, bc);

    if (m_excel_load_list.size() == m_selected_table.size())
    {
        bool all_load_success = true;

        for (auto it = m_excel_load_list.begin(); it != m_excel_load_list.end(); ++it)
        {
            if (!it->second)
            {
                all_load_success = false;
                break;
            }
        }

        if (all_load_success)
        {
            tc = CXListBox::Green;
            _snwprintf(wsz_tmp, 256, L"所有文件加载成功,耗时%.2f秒", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
        else
        {
            tc = CXListBox::Red;
            _snwprintf(wsz_tmp, 256, L"所有文件加载完毕,耗时%.2f秒 有文件出错请检查！", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
    }
}

void CTableMakerDlg::AddCodeResult(
    int idx, 
    const std::wstring & err_msg, 
    float elapse, 
    std::wstring table_name)
{
    std::wstring msg = L"生成 ";
    msg += table_name;
    msg += L"C++代码  ";

    float f_elapse = elapse;

    CXListBox::Color tc, bc;
    bc = CXListBox::White;

    if (err_msg.length())
    {
        tc = CXListBox::Red;

        msg.append(L"×    ");
        msg.append(err_msg);

        m_code_gen_list[table_name] = false;
    }
    else
    {
        tc = CXListBox::Green;

        msg.append(L"√    ");
        msg.append(L"成功 ");

        m_code_gen_list[table_name] = true;
    }

    msg.append(L"耗时");
    wchar_t wsz_tmp[256];
    _snwprintf(wsz_tmp, 256, L"%.2f秒", elapse/ 1000);
    msg.append(wsz_tmp);
    m_result_box.DeleteString(idx);
    m_result_box.InsertString(idx, msg.c_str(), tc, bc);

    if (m_code_gen_list.size() == m_selected_table.size())
    {
        bool all_load_success = true;
        float real_elapse = (float)(get_tick() - m_gen_code_tick);

        for (auto it = m_code_gen_list.begin(); it != m_code_gen_list.end(); ++it)
        {
            if (!it->second)
            {
                all_load_success = false;
                break;
            }
        }

        if (all_load_success)
        {
            tc = CXListBox::Green;
            _snwprintf(wsz_tmp, 256, L"所有文件生成代码成功,耗时%.2f秒", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
        else
        {
            tc = CXListBox::Red;
            _snwprintf(wsz_tmp, 256, L"所有文件生成代码完毕,耗时%.2f秒 有文件出错请检查！", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
    }
}



void CTableMakerDlg::AddXmlResult(int idx, const std::wstring& err_msg, float elapse, std::wstring table_name)
{
    std::wstring msg = L"生成";
    msg += table_name;
    msg += L".xml  ";

    float f_elapse = elapse;

    CXListBox::Color tc, bc;
    bc = CXListBox::White;

    if (err_msg.length())
    {
        tc = CXListBox::Red;

        msg.append(L"×    ");
        msg.append(err_msg);

        m_xml_gen_list[table_name] = false;
    }
    else
    {
        tc = CXListBox::Green;

        msg.append(L"√    ");
        msg.append(L"成功 ");

        m_xml_gen_list[table_name] = true;
    }

    msg.append(L"耗时");
    wchar_t wsz_tmp[256];
    _snwprintf(wsz_tmp, 256, L"%.2f秒", elapse / 1000);
    msg.append(wsz_tmp);
    m_result_box.DeleteString(idx);
    m_result_box.InsertString(idx, msg.c_str(), tc, bc);

    if (m_xml_gen_list.size() == m_selected_table.size())
    {
        bool all_load_success = true;
        float real_elapse = (float)(get_tick() - m_gen_xml_tick);

        for (auto it = m_xml_gen_list.begin(); it != m_xml_gen_list.end(); ++it)
        {
            if (!it->second)
            {
                all_load_success = false;
                break;
            }
        }

        if (all_load_success)
        {
            tc = CXListBox::Green;
            _snwprintf(wsz_tmp, 256, L"所有文件生成xml成功,耗时%.2f秒", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
        else
        {
            tc = CXListBox::Red;
            _snwprintf(wsz_tmp, 256, L"所有文件生成xml完毕,耗时%.2f秒 有文件出错请检查！", real_elapse / 1000);
            m_result_box.AddLine(tc, bc, wsz_tmp);
        }
    }
}

void CTableMakerDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值

    CDialogEx::OnTimer(nIDEvent);

    g_task_mgr.Run(0);
}
