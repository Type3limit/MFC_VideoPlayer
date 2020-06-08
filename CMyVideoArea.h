#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include <atlimage.h>
#include<afxmt.h>


extern "C"
{
#include "./include/libavcodec/avcodec.h"
#include "./include/libavformat/avformat.h"
#include "./include/libswscale/swscale.h"
#include "./include/libavutil/avutil.h"
#include "./include/libavutil/imgutils.h"
#include "SDL2/SDL.h"
}

#define DELAY_TIMER 1001
#define WMU_STOP  (WM_USER+1)
#define WMU_READY_TO_DRAW (WM_USER+2)
#define WMU_PLAY_OVER (WM_USER+3)




constexpr int EXIT_EXCPTION = -1;
constexpr int EXIT_NORMAL = 0;

struct CoverImage
{

	HBITMAP st_hBitmap;
	int st_nDelay;

	CoverImage(HBITMAP hBitmap=NULL,int nDelay=0)
	{
		st_hBitmap = hBitmap;
		st_nDelay = nDelay;
	}
	~CoverImage()
	{
		  DeleteObject(st_hBitmap);
	}

};

class CMyVideoArea :
	public CStatic
{
	friend UINT ThreadPlay(LPVOID lpParam);
	friend UINT ThreadRender(LPVOID lpParam);
private:
	//工作线程控制变量
	bool m_bThreadExit;
	bool m_bThreadBegin;
	bool m_bThreadPause;
	bool m_bEndNormal;
	int  m_nFrameRate;

	//绘制矩形
	CRect m_ScreenRect;
	CRect m_DrawRect;
	CRect m_ImageRect;
	CRgn m_Rgn;
	//图形相关
	CImage m_cImage;
	//文件路径
	char m_cFilePath[200];
	//画图宽高
	int m_nImageWidth;
	int m_nImageHeight;
	//线程控制
	CWinThread* m_pThreadPlay;
	CWinThread* m_pThreadRender;
	//全屏控制
	bool m_bIsFullScreen;

	//解码相关


private:

	void RestoreInBmp(AVFrame* pFrame, int nWidth, int nHeight, int nBpp,int Delay,int* padding);//将信息存储为bmp文件用以显示，将存入全局队列当中
	void AdjustDrawRect(bool model = true);//调整绘制区域
	int FFmpegplayer();//解码线程工作函数
	void RenderArea();//绘制控制线程
	void CleanUp();//清理资源

	int SaveYUVFrameToFile(AVFrame* frame, int width, int height, int flag);//解码数据检查，仅供Debug使用

protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

public:
	CMyVideoArea();
	~CMyVideoArea();
	
	void SetFile(LPCSTR lParam);//设置文件名
	void PlayPause();//暂停与继续
	void PlayBegin();//开始播放
	void PlayStop();//停止播放
	void SetFullScreen(bool bCurModel);//全屏
	bool IsAllOver();//是否所有线程全部结束掉
};

