
// MFC_VideoPlayerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "MFC_VideoPlayer.h"
#include "MFC_VideoPlayerDlg.h"

#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CMFCVideoPlayerDlg 对话框



CMFCVideoPlayerDlg::CMFCVideoPlayerDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_MFC_VIDEOPLAYER_DIALOG, pParent), m_bIsFullScreen(false),
	m_bIsPause(false), m_bHasBegin(false)
{

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCVideoPlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_url);
	DDX_Control(pDX, IDC_STARTBUTTON, m_StartButton);
	DDX_Control(pDX, IDC_PAUSEBUTTON, m_PauseButton);
	DDX_Control(pDX, IDC_FULLSCREENBUTTON, m_FullScreenButton);
	DDX_Control(pDX, IDC_FILEBUTTON, m_FileButton);
	DDX_Control(pDX, IDC_SCREEN, m_ScreenArea);
}

BEGIN_MESSAGE_MAP(CMFCVideoPlayerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CLOSE()

	ON_BN_CLICKED(IDC_FILEBUTTON, &CMFCVideoPlayerDlg::OnBnClickedFilebutton)
	ON_BN_CLICKED(IDC_STARTBUTTON, &CMFCVideoPlayerDlg::OnBnClickedStartbutton)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_PAUSEBUTTON, &CMFCVideoPlayerDlg::OnBnClickedPausebutton)

	ON_BN_CLICKED(IDC_FULLSCREENBUTTON, &CMFCVideoPlayerDlg::OnBnClickedFullscreenbutton)

	ON_MESSAGE(WMU_PLAY_OVER, &CMFCVideoPlayerDlg::OnVideoPlayOver)

END_MESSAGE_MAP()


// CMFCVideoPlayerDlg 消息处理程序

BOOL CMFCVideoPlayerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}


void CMFCVideoPlayerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{

	CDialog::OnSysCommand(nID, lParam);

}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CMFCVideoPlayerDlg::OnPaint()
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
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CMFCVideoPlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CMFCVideoPlayerDlg::OnBnClickedFilebutton()
{
	CString FilePathName;
	CString Filter = CString("视频文件|*.mp4;*.mpeg;*.avi;*.mkv;*.rmvb;*.wmv;*.flv||");
	CFileDialog dlg(TRUE, L".", L"", OFN_HIDEREADONLY | OFN_READONLY, Filter,NULL);
	if (dlg.DoModal() == IDOK) 
	{
		FilePathName = dlg.GetPathName();
		m_url.SetWindowText(FilePathName);
		char FilePath[200] = { 0 };
		GetWindowTextA(m_url.m_hWnd, FilePath, 200);
		m_ScreenArea.SetFile(FilePath);
		m_StartButton.EnableWindow(TRUE);

	}
	delete dlg;
}

void CMFCVideoPlayerDlg::OnBnClickedStartbutton()
{
	// TODO: 在此添加控件通知处理程序代码
	static DWORD dwCount = 0;
	if (GetTickCount() - dwCount < 100)//If click within 0.1s depart, do nothing, leave time for release resources;
	{
		return;
	}
	dwCount = GetTickCount();

	if (!m_bHasBegin)
	{
		if (m_url.GetWindowTextLengthW() < 1)
		{
			AfxMessageBox(L"请输入文件名！");
			return;
		}
		m_bHasBegin = true;
		m_ScreenArea.PlayBegin();
		m_PauseButton.ShowWindow(SW_SHOW);
		m_FullScreenButton.ShowWindow(SW_SHOW);
		m_bIsPause = false;
		m_PauseButton.SetWindowTextW(L"暂停");
		m_StartButton.SetWindowTextW(L"停止播放");
		m_FileButton.EnableWindow(FALSE);
		m_url.EnableWindow(FALSE);
	}
	else
	{
		m_ScreenArea.PlayStop();//通过消息来更新界面，让资源完整释放后再响应用户操作
	}

}


void CMFCVideoPlayerDlg::OnSize(UINT nType, int cx, int cy)
{
	Invalidate();
	CDialog::OnSize(nType, cx, cy);
	// TODO: 在此处添加消息处理程序代码
}


void CMFCVideoPlayerDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	lpMMI->ptMinTrackSize.x = 725;
	lpMMI->ptMinTrackSize.y = 505;
	CDialog::OnGetMinMaxInfo(lpMMI);
}


void CMFCVideoPlayerDlg::OnBnClickedPausebutton()
{
	// TODO: 在此添加控件通知处理程序代码
	static DWORD dwCount = 0;
	if (GetTickCount() - dwCount < 100)//If click within 0.1s depart, do nothing, leave time for release resources;
	{
		return;
	}
	dwCount = GetTickCount();

	if (!m_bIsPause)
	{
		m_PauseButton.SetWindowTextW(L"继续");
	}
	else
	{
		m_PauseButton.SetWindowTextW(L"暂停");
	}
	m_bIsPause = !m_bIsPause;
	m_ScreenArea.PlayPause();

}

void CMFCVideoPlayerDlg::OnBnClickedFullscreenbutton()
{
	m_bIsFullScreen = true;
	int cxScreen, cyScreen;
	cxScreen = GetSystemMetrics(SM_CXSCREEN);
	cyScreen = GetSystemMetrics(SM_CYSCREEN);

	WINDOWPLACEMENT wpNew;
	CRect WindowRect;
	::GetWindowRect(::GetDesktopWindow(), &WindowRect);
	GetWindowPlacement(&m_stWpOld);
	CRect ClientRect;
	RepositionBars(0, 0xffff, AFX_IDW_PANE_FIRST, reposQuery, &ClientRect);
	ClientToScreen(&ClientRect);
	wpNew.length = sizeof(WINDOWPLACEMENT);
	wpNew.showCmd = SW_MAXIMIZE;
	wpNew.rcNormalPosition = WindowRect;
	SetWindowPos(&wndTopMost, 0, 0, cxScreen, cyScreen, SWP_SHOWWINDOW);

	LONG IStyle = ::GetWindowLong(this->m_hWnd, GWL_STYLE);

	::SetWindowLong(this->m_hWnd, GWL_STYLE, IStyle & ~WS_CAPTION);
	::SetWindowPos(this->m_hWnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	SetWindowPlacement(&wpNew);

	m_ScreenArea.MoveWindow(0, 0, WindowRect.Width(), WindowRect.Height());

	m_FileButton.ShowWindow(SW_HIDE);
	m_FullScreenButton.ShowWindow(SW_HIDE);
	m_url.ShowWindow(SW_HIDE);
	m_PauseButton.ShowWindow(SW_HIDE);
	m_StartButton.ShowWindow(SW_HIDE);
}

BOOL CMFCVideoPlayerDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
	{
		if (m_bIsFullScreen)
		{
			m_bIsFullScreen = false;
			m_ScreenArea.SetFullScreen(m_bIsFullScreen);

			LONG IStyle = ::GetWindowLong(this->m_hWnd, GWL_STYLE);
			::SetWindowLong(this->m_hWnd, GWL_STYLE, IStyle | WS_CAPTION);
			::SetWindowPos(this->m_hWnd, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER
				| SWP_NOACTIVATE | SWP_FRAMECHANGED);
			SetWindowPlacement(&m_stWpOld);
			if (m_bHasBegin)
			{
			m_FullScreenButton.ShowWindow(SW_SHOW);
			m_PauseButton.ShowWindow(SW_SHOW);
			}
			m_FileButton.ShowWindow(SW_SHOW);
			m_url.ShowWindow(SW_SHOW);
			m_StartButton.ShowWindow(SW_SHOW);

			return TRUE;
		}
		else
		{
			CMFCVideoPlayerDlg::OnCancel();
			return TRUE;
		}
	}
	return CDialog::PreTranslateMessage(pMsg);
}


void CMFCVideoPlayerDlg::OnCancel()
{
	if (IDOK == AfxMessageBox(L"Sure To Quit?", MB_OKCANCEL))
	{
		
		m_ScreenArea.PlayStop();
		//DestroyWindow();
		EndDialog(IDCANCEL);
	}
}

void CMFCVideoPlayerDlg::OnOK()
{
	DestroyWindow();
	EndDialog(IDOK);
	return;
}

void CMFCVideoPlayerDlg::OnClose()
{
	OnCancel();
}

LRESULT CMFCVideoPlayerDlg::OnVideoPlayOver(WPARAM wParam,LPARAM lParam)
{
	
	m_bHasBegin = false;
	m_PauseButton.ShowWindow(SW_HIDE);
	m_FullScreenButton.ShowWindow(SW_HIDE);
	m_StartButton.SetWindowTextW(L"开始播放");
	m_FileButton.EnableWindow(TRUE);
	m_url.EnableWindow(TRUE);
	return 0UL;
}