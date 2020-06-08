
// MFC_VideoPlayerDlg.h: 头文件
//

#pragma once
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL2/SDL.h"
}

#include  "CMyVideoArea.h"

// CMFCVideoPlayerDlg 对话框
class CMFCVideoPlayerDlg : public CDialog
{
// 构造
public:
	CMFCVideoPlayerDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFC_VIDEOPLAYER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

private:
	bool m_bHasBegin;
	bool m_bIsPause;

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:

	afx_msg void OnBnClickedFilebutton();
	// 文件路径
	CEdit m_url;
	CMyVideoArea m_ScreenArea;
	WINDOWPLACEMENT m_stWpOld;
	CEvent m_DrawEvent;
	bool m_bIsFullScreen;
	BOOL CMFCVideoPlayerDlg::PreTranslateMessage(MSG* pMsg); 
public:
	
	afx_msg void OnOK();
	afx_msg void OnCancel();
	afx_msg void OnClose();
	afx_msg void OnBnClickedStartbutton();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	
	afx_msg void OnBnClickedPausebutton();

	afx_msg void OnBnClickedFullscreenbutton();

	afx_msg LRESULT OnVideoPlayOver(WPARAM wParam,LPARAM lParam);

	CButton m_StartButton;
	CButton m_PauseButton;
	CButton m_FullScreenButton;
	CButton m_FileButton;
	
};
