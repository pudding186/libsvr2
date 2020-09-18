
// TableMakerDlg.h: 头文件
//

#pragma once


// CTableMakerDlg 对话框
class CTableMakerDlg : public CDialogEx
{
// 构造
public:
	CTableMakerDlg(CWnd* pParent = nullptr);	// 标准构造函数

//// 对话框数据
//#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TABLEMAKER_DIALOG };
//#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

protected:
    CListCtrl   m_excel_list;
    CXListBox   m_result_box;
    CProgressCtrl m_progress;
    UINT_PTR    m_task_timer;
	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()


    afx_msg void OnLvnItemchangedListExcel(NMHDR* pNMHDR, LRESULT* pResult);
public:
    afx_msg void OnBnClickedButtonLoad();
    afx_msg void OnClose();
    afx_msg void OnBnClickedButtonCode();
    afx_msg void OnBnClickedButtonXml();
    afx_msg void OnBnClickedButtonXmlEx();
    afx_msg void OnBnClickedButtonXmlAll();
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnMenuAllCancel();
    afx_msg void OnMenuAllSelect();
    afx_msg void OnMenuAllReverse();
    afx_msg void OnMenuLoad();
    afx_msg void OnMenuClear();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    //afx_msg LRESULT OnKickIdle(WPARAM wParam, LPARAM lParam);

    void UpdateExcelList(
        unsigned int elapse,
        std::vector<std::wstring>& excel_list,
        std::wstring& excel_path,
        std::wstring& code_path,
        std::wstring& xml_path);

    void AddExcelResult(
        int idx,
        const std::wstring& err_msg,
        unsigned int elapse,
        std::wstring table_name,
        CTableMaker* server_table,
        CTableMaker* client_table,
        CTableMaker* all_table);

    void AddCodeResult(
        int idx,
        const std::wstring& err_msg,
        float elapse,
        std::wstring table_name);

    void AddXmlResult(
        int idx,
        const std::wstring& err_msg,
        float elapse,
        std::wstring table_name);

protected:
    std::wstring m_excel_path;
    std::wstring m_code_path;
    std::wstring m_xml_path;
    std::vector<std::wstring> m_excel_file_list;
    std::map<std::wstring, CTableMaker*>m_maker_server_list;
    std::map<std::wstring, CTableMaker*>m_maker_client_list;
    std::map<std::wstring, CTableMaker*>m_maker_all_list;
    std::set<std::wstring> m_selected_table;
    std::map<std::wstring, bool> m_excel_load_list;
    std::map<std::wstring, bool> m_code_gen_list;
    std::map<std::wstring, bool> m_xml_gen_list;
    unsigned int    m_excel_load_tick;
    unsigned int    m_gen_code_tick;
    unsigned int    m_gen_xml_tick;

public:
    afx_msg void OnTimer(UINT_PTR nIDEvent);
};
