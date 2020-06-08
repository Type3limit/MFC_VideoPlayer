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
	//�����߳̿��Ʊ���
	bool m_bThreadExit;
	bool m_bThreadBegin;
	bool m_bThreadPause;
	bool m_bEndNormal;
	int  m_nFrameRate;

	//���ƾ���
	CRect m_ScreenRect;
	CRect m_DrawRect;
	CRect m_ImageRect;
	CRgn m_Rgn;
	//ͼ�����
	CImage m_cImage;
	//�ļ�·��
	char m_cFilePath[200];
	//��ͼ���
	int m_nImageWidth;
	int m_nImageHeight;
	//�߳̿���
	CWinThread* m_pThreadPlay;
	CWinThread* m_pThreadRender;
	//ȫ������
	bool m_bIsFullScreen;

	//�������


private:

	void RestoreInBmp(AVFrame* pFrame, int nWidth, int nHeight, int nBpp,int Delay,int* padding);//����Ϣ�洢Ϊbmp�ļ�������ʾ��������ȫ�ֶ��е���
	void AdjustDrawRect(bool model = true);//������������
	int FFmpegplayer();//�����̹߳�������
	void RenderArea();//���ƿ����߳�
	void CleanUp();//������Դ

	int SaveYUVFrameToFile(AVFrame* frame, int width, int height, int flag);//�������ݼ�飬����Debugʹ��

protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	DECLARE_MESSAGE_MAP()

public:
	CMyVideoArea();
	~CMyVideoArea();
	
	void SetFile(LPCSTR lParam);//�����ļ���
	void PlayPause();//��ͣ�����
	void PlayBegin();//��ʼ����
	void PlayStop();//ֹͣ����
	void SetFullScreen(bool bCurModel);//ȫ��
	bool IsAllOver();//�Ƿ������߳�ȫ��������
};

