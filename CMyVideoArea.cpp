#include "pch.h"
#include "CMyVideoArea.h"
#include "CMyBlockQueue.h"

static CMyBlockQueue<HBITMAP> Global_BitMapQueue;
static CMyBlockQueue <int> Global_DelayQueue;

CMyVideoArea::CMyVideoArea() :
	m_bThreadBegin(false), m_bThreadExit(false),
	m_bThreadPause(false), m_nFrameRate(0),
	m_pThreadRender(nullptr), m_bEndNormal(false),
	m_pThreadPlay(nullptr)
{
	memset(m_cFilePath, 0, sizeof(m_cFilePath));
}

CMyVideoArea::~CMyVideoArea()
{
	if (m_pThreadRender)
	{
		if (m_bThreadPause)//若当前线程处于暂停态，则继续以便线程执行正常以释放内存
		{
			m_pThreadRender->ResumeThread();
		}
		PostThreadMessage(m_pThreadRender->m_nThreadID, WMU_STOP, 0, 0);

		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadRender->m_hThread, 125))//若仍存在，说明处于消费者阻塞态
		{
			//放一个值使其解除阻塞
			if (!Global_DelayQueue.IsFull())
			{
				TRACE("Blocked,put something\n");
				Global_DelayQueue.PutItem(0);
			}
			PostThreadMessage(m_pThreadRender->m_nThreadID, WMU_STOP, 0, 0);
			WaitForSingleObject(m_pThreadRender->m_hThread, INFINITE);
		}
	}
	TRACE("Render Thread Ended\n");

	if (m_pThreadPlay)//释放解码线程
	{
		if (m_bThreadPause)//若当前线程处于暂停态，则继续以便线程执行正常以释放内存
		{
			m_pThreadPlay->ResumeThread();
		}

		PostThreadMessage(m_pThreadPlay->m_nThreadID, WMU_STOP, 0, 0);
		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadPlay->m_hThread, 125))//若仍存在，说明处于生产者阻塞态
		{
			//取消值使其解除阻塞。
			if (!Global_BitMapQueue.IsEmpty())
			{
				TRACE("Blocked,remove bitmap handle\n");
				Global_BitMapQueue.EmptyQueue();
			}

			if (!Global_DelayQueue.IsEmpty())
			{
				TRACE("Blocked,remove Delay \n");
				Global_DelayQueue.EmptyQueue();
			}


			PostThreadMessage(m_pThreadPlay->m_nThreadID, WMU_STOP, 0, 0);
			WaitForSingleObject(m_pThreadPlay->m_hThread, INFINITE);
		}
	}
	TRACE("Work Thread Ended\n");
	CleanUp();
	TRACE("All threads over done.");
}

BEGIN_MESSAGE_MAP(CMyVideoArea, CStatic)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

void CMyVideoArea::OnPaint()
{
	
	static LARGE_INTEGER dwCount = {0};
	static LARGE_INTEGER frequency = {0};
	CPaintDC pDC(this);
	if (m_bThreadBegin && !m_bThreadPause)//已经开始解码并且未处于暂停态
	{
		QueryPerformanceFrequency(&frequency);
		double quadpart = (double)frequency.QuadPart;
		LARGE_INTEGER CurBuf;
		QueryPerformanceCounter(&CurBuf);
		double delay = (CurBuf.QuadPart - dwCount.QuadPart) / quadpart;
		TRACE("Paint Depart with %lf,Bitmap has %d left\n", delay,Global_BitMapQueue.QueueSize());
		QueryPerformanceCounter(&dwCount);

		HBITMAP hBuf = NULL;
		do {
			hBuf = Global_BitMapQueue.TakeItemWithoutBlock();
		} while (!hBuf || Global_BitMapQueue.IsEmpty());//直到取得一个非空buffer或当前已无帧图可用

		if (!hBuf)//若仍为空，则说明无帧图可用，不进行绘制
			return;

		Global_DelayQueue.RemoveFront();

		if (!m_cImage.IsNull())
			m_cImage.Destroy();

		m_cImage.Attach(hBuf);

		CDC memDc;
		memDc.CreateCompatibleDC(&pDC);
		if (memDc)
		{
			CBitmap memBitmap;
			memBitmap.CreateCompatibleBitmap(&pDC, m_ScreenRect.Width(), m_ScreenRect.Height());
			auto Cur = memDc.SelectObject(memBitmap);//将CBitmap的一位位图选出，避免高速绘制时的内存泄漏
			memDc.FillSolidRect(&m_ScreenRect, RGB(0, 0, 0));
			memDc.SetStretchBltMode(HALFTONE);
			m_cImage.Draw(memDc, m_DrawRect, m_ImageRect);
			
			pDC.BitBlt(0, 0, m_ScreenRect.Width(), m_ScreenRect.Height(), &memDc, 0, 0, SRCCOPY);
			memBitmap.DeleteObject();
			DeleteObject(Cur);//释放选出的位图
		}
		memDc.DeleteDC();
		Global_BitMapQueue.RemoveFront();
	}

	else if (m_bThreadBegin && m_bThreadPause)//已开始解码但处于暂停态
	{
		pDC.FillSolidRect(m_ScreenRect, RGB(0, 0, 0));//仅单图无双缓冲必要，直接使用黑背景掩盖
		pDC.SetStretchBltMode(HALFTONE);
		m_cImage.Draw(pDC, m_DrawRect, m_ImageRect);
	}

	else//其他状态时统一使用黑色背景
	{
		GetWindowRect(&m_ScreenRect);
		ScreenToClient(m_ScreenRect);
		pDC.FillSolidRect(m_ScreenRect, RGB(0, 0, 0));
	}
	ReleaseDC(&pDC);
}

void CMyVideoArea::OnSize(UINT nType, int cx, int cy)
{
	if (m_bThreadBegin)
	{
		AdjustDrawRect();//调整绘制矩形
	}
}

int CMyVideoArea::FFmpegplayer()
{

#pragma region InitalizeRegion

	AVFormatContext* m_pFormatCtx;
	int m_nVideoIndex, m_nAudioIndex;
	AVCodecContext* m_pCodecCtx;
	AVCodec* m_pCodec;
	AVFrame* m_pFrame, * m_pFrameRGB;
	uint8_t* m_pOutBufferRGB;

	AVPacket* m_pPacket;
	AVStream* m_pStream;
	int m_nRet = 0, m_nGotPicture = 0;

	struct SwsContext* m_pImgConvertCtx;

	avformat_network_init();
	m_pFormatCtx = avformat_alloc_context();

	TRACE("Work Thread Beagin!\n");
	if (avformat_open_input(&m_pFormatCtx, m_cFilePath, NULL, NULL) != 0) //打开视频文件
	{
		AfxMessageBox(_T("Couldn't open input stream.\n"));
		
	}
	if (avformat_find_stream_info(m_pFormatCtx, NULL) < 0) //找到视频流
	{
		AfxMessageBox(_T("Couldn't find stream information.\n"));
		
	}
	m_nVideoIndex = -1;
	m_nAudioIndex = -1;
	for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; i++) //找到类型对应的流
	{
		if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)//视频流
			m_nVideoIndex = i;
		if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)//音频流
			m_nAudioIndex = i;
	}

	if (m_nVideoIndex == -1) {
		AfxMessageBox(_T("Didn't find a video stream.\n"));//未找到视频流
		
	}
	//if (m_nAudioIndex == -1)
	//{
	//	AfxMessageBox(_T("Didn't find a audio stream.\n"));//未找到音频流（本程序暂无音频播放能力）
	//	return EXIT_EXCPTION;
	//}
	m_pCodecCtx = avcodec_alloc_context3(NULL);
	if (m_pCodecCtx == NULL)
	{
		AfxMessageBox(_T("codec context alloc failed!"));

	}
	avcodec_parameters_to_context(m_pCodecCtx, m_pFormatCtx->streams[m_nVideoIndex]->codecpar);

	m_pCodec = avcodec_find_decoder(m_pCodecCtx->codec_id);//根据上下文找到编解码器，软解
	//m_pCodec = avcodec_find_decoder_by_name("h264_mediacodec");//硬解码
	if (m_pCodec == NULL) {
		AfxMessageBox(_T("Codec not found.\n"));
		
	}
	if (avcodec_open2(m_pCodecCtx, m_pCodec, NULL) < 0) //打开编解码器
	{
		AfxMessageBox(_T("Could not open codec.\n"));
		
	}
	m_pFrame = av_frame_alloc();
	m_pFrameRGB = av_frame_alloc();
	

	if (m_pFrame == NULL || m_pFrameRGB == NULL)
		AfxMessageBox(_T("Frame alloc failed!"));

	
	m_pOutBufferRGB = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_pCodecCtx->width, m_pCodecCtx->height,1));
	av_image_fill_arrays(m_pFrameRGB->data, m_pFrameRGB->linesize,
		m_pOutBufferRGB, AV_PIX_FMT_BGR24, m_pCodecCtx->width, m_pCodecCtx->height, 1);

	//调整绘制区
	m_nImageWidth = m_pCodecCtx->width;
	m_nImageHeight = m_pCodecCtx->height;
	AdjustDrawRect();

	//设置图形转换的上下文
	m_pImgConvertCtx = sws_getContext(m_pCodecCtx->width, m_pCodecCtx->height, m_pCodecCtx->pix_fmt,
			  m_pCodecCtx->width, m_pCodecCtx->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	m_pStream = m_pFormatCtx->streams[m_nVideoIndex];
	m_pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));

	uint64_t uiBasePts = AV_NOPTS_VALUE;
	double fLastPTS = 0.0;
	int64_t CurPts = 0;
	double FrameTime = 0.0;
	int Delay = 0;
	double diff = 0.0;
	
	MSG msg;
#pragma endregion

//每次循环读入一个包
	while (!PeekMessage(&msg, NULL, WMU_STOP, WMU_STOP, PM_NOREMOVE))
	{
		
		if (av_read_frame(m_pFormatCtx, m_pPacket) >= 0)
		{
			if (m_pPacket->stream_index == m_nVideoIndex)
			{
				m_nRet = avcodec_send_packet(m_pCodecCtx, m_pPacket);
					
				if (m_nRet != 0) {
					AfxMessageBox(_T("Decode Error%d.\n"),m_nRet);
					
				}
				while ((m_nRet = avcodec_receive_frame(m_pCodecCtx, m_pFrame)) == 0)
				{
					//获得延时
					if (m_pPacket->dts != AV_NOPTS_VALUE)//若dts可用，取当前的帧的dts
					{
						CurPts = m_pPacket->dts;
					}
					else if (m_pPacket->dts == AV_NOPTS_VALUE &&
						m_pFrame->opaque && *(uint64_t*)m_pFrame->opaque != AV_NOPTS_VALUE)//dts不可用，则同步到外部（因无音频，暂不可用）
					{
						CurPts = *((uint64_t*)m_pFrame->opaque);
					}
					else//否则使用当前帧的pts
						CurPts = m_pPacket->pts;
					FrameTime = CurPts * av_q2d(m_pFormatCtx->streams[m_nVideoIndex]->time_base);
					if (FrameTime > 0.0)
					{
						diff = FrameTime - fLastPTS;
						Delay = (int)(diff * 1000);
						fLastPTS = FrameTime;
					}

					sws_scale(m_pImgConvertCtx,
						(const uint8_t* const*)m_pFrame->data,
						m_pFrame->linesize,
						0, m_pCodecCtx->height,
						m_pFrameRGB->data, m_pFrameRGB->linesize);

					
					
					RestoreInBmp(m_pFrameRGB, m_pFrame->width, m_pFrame->height, 24, Delay,m_pFrame->linesize);//将MetaData直接存为HBITMAP,24位深
					//Sleep(Delay / 2);

					//1.0 保证在降低cpu占用的同时有足够的帧供以绘制线程作时间控制
					//1.1 利用满时阻塞可以更有效地降低CPU占用
					
					//av_free(tmpBuffer);
					av_frame_unref(m_pFrame);
					
					//av_frame_unref(m_pFrameRGB);
				}

			}
			av_packet_unref(m_pPacket);
		}
		else //正常结束，即视频包已被读完或读取视频包失败
		{
			m_bEndNormal = true;
			if (m_pThreadRender)
			{
				if (Global_DelayQueue.IsEmpty())
				{
					TRACE(_T("Vedio Normally End!\n"));
					PostThreadMessage(m_pThreadRender->m_nThreadID, WMU_STOP, 0, 0);
					WaitForSingleObject(m_pThreadRender, INFINITE);
					CleanUp();
					break;
				}
			}
			else
			{
				TRACE(_T("Render thread ended before work thread.\n"));
				CleanUp();
				break;

			}
		}

	}
	//清理资源
	sws_freeContext(m_pImgConvertCtx);
	av_frame_free(&m_pFrameRGB);
	av_frame_free(&m_pFrame);

	avcodec_close(m_pCodecCtx);
	avformat_close_input(&m_pFormatCtx);
	avformat_free_context(m_pFormatCtx);

	av_free(m_pOutBufferRGB);
	//av_free(m_pOutBufferYUV);
	TRACE("work thread normal ended\n");
	::PostMessage(this->GetParent()->m_hWnd, WMU_PLAY_OVER, 0, 0);
	if (!m_bEndNormal)
		CleanUp();
	return EXIT_NORMAL;
}

void CMyVideoArea::RestoreInBmp(AVFrame* pFrame, int nWidth, int nHeight, int nBpp, int delay, int* padding)
{
	
	int Delta = ((nWidth * 3) % 4 == 0) ? 0 : 4 - (nWidth * 3) % 4;
	uint8_t* pBuffer = NULL;
	if (Delta == 0)
		goto RejudgeOver;
	pBuffer = new uint8_t[(((nWidth * 3) + Delta) * nHeight)];
	memset(pBuffer, 255, sizeof(pBuffer));
	int nCurWidth = nWidth + Delta;
	
	uint8_t  R, G, B;
	int i, j;
	for (i = 0; i < nHeight; i++)
	{
		for (j = 0; j < nWidth; j++)
		{
				B = pFrame->data[0][3 * (nWidth * i + j) + 0 - i * Delta];			
				G = pFrame->data[0][3 * (nWidth * i + j) + 1 - i * Delta];
				R = pFrame->data[0][3 * (nWidth * i + j) + 2 - i * Delta];
				pBuffer[3*(nWidth * i + j)] = B;
			    pBuffer[3*(nWidth * i + j)+1] = G;
			    pBuffer[3*(nWidth * i + j)+2] = R;	//BGR存储像素点信息

		}
	}

RejudgeOver:
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (((nWidth * 3) + Delta)* nHeight);

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = nWidth;
	bmpinfo.biHeight = -nHeight;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = nBpp;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = nWidth * nHeight;
	bmpinfo.biXPelsPerMeter =0;
	bmpinfo.biYPelsPerMeter =0;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	CClientDC dc(this);


	//char filename[30] = { 0 };
	//sprintf_s(filename,"./RGB/RGBData%d.rgb", number++);
	//FILE* pFileBGR;
	//fopen_s(&pFileBGR, filename, "wb+");
	//fwrite(pFrame->data[0], nWidth*nHeight*3, 1, pFileBGR);
	//fclose(pFileBGR);

	//创建设备相关位图，用于快速绘制，因DDB位图需要二次转换
	HBITMAP hTemp = NULL;
	if(Delta!=0)
	 hTemp = CreateDIBitmap(dc.GetSafeHdc(),							//设备上下文的句柄 
		(LPBITMAPINFOHEADER)&bmpinfo,				//位图信息头指针 
		(long)CBM_INIT,								//初始化标志 
		pBuffer,						//初始化数据指针 
		(LPBITMAPINFO)&bmpinfo,						//位图信息指针 
		DIB_RGB_COLORS);
	else
	 hTemp = CreateDIBitmap(dc.GetSafeHdc(),							//设备上下文的句柄 
			(LPBITMAPINFOHEADER)&bmpinfo,				//位图信息头指针 
			(long)CBM_INIT,								//初始化标志 
			pFrame->data[0],						//初始化数据指针 
			(LPBITMAPINFO)&bmpinfo,						//位图信息指针 
			DIB_RGB_COLORS);
	if (hTemp != NULL)//避免空位图入队
	{
		Global_BitMapQueue.PutItem(hTemp);
		Global_DelayQueue.PutItem(delay);
	}
	ReleaseDC(&dc);
	delete[]pBuffer;
}

void CMyVideoArea::AdjustDrawRect(bool model)
{
	GetWindowRect(&m_ScreenRect);
	ScreenToClient(&m_ScreenRect);
	if (model)
	{
		m_ImageRect = CRect(0, 0, m_nImageWidth, m_nImageHeight);
	}

	int nImgWidth = m_ImageRect.Width();
	int nImgHeight = m_ImageRect.Height();
	int nActualWidth = m_ScreenRect.Width();
	int nActualHeight = m_ScreenRect.Height();

	float fscaleimgHW = static_cast<float>(nImgHeight) / static_cast<float>(nImgWidth);
	float fscaleimgWH = static_cast<float>(nImgWidth) / static_cast<float>(nImgHeight);


	int cur_W = 0, cur_H = 0;
	float fscale = 1.0;
	if (nImgWidth > nActualWidth && nImgHeight <= nActualHeight)//图像宽度大于客户区且图像高小于等于客户区
	{
		cur_W = nActualWidth;
		cur_H = static_cast<int>(cur_W * fscaleimgHW);
		m_DrawRect = CRect((nActualWidth - cur_W) / 2,
			(nActualHeight - cur_H) / 2,
			(nActualWidth + cur_W) / 2,
			(nActualHeight + cur_H) / 2);
	}
	else if (nImgWidth <= nActualWidth && nImgHeight > nActualHeight)//图像宽度小于等于客户区且图像高于客户区
	{
		cur_H = nActualHeight;
		cur_W = static_cast<int>(cur_H * fscaleimgWH);
		m_DrawRect = CRect((nActualWidth - cur_W) / 2,
			(nActualHeight - cur_H) / 2,
			(nActualWidth + cur_W) / 2,
			(nActualHeight + cur_H) / 2);
	}
	else if (nImgWidth <= nActualWidth && nImgHeight < nActualHeight)//图像宽度小于等于客户区且图像高小于客户区
	{
		m_DrawRect = CRect((nActualWidth - nImgWidth) / 2,
			(nActualHeight - nImgHeight) / 2,
			(nActualWidth + nImgWidth) / 2,
			(nActualHeight + nImgHeight) / 2);
	}
	else//图像宽大于客户区且图像高大于客户区
	{

		float fscaleCWH = static_cast<float>(nActualWidth) / static_cast<float>(nActualHeight);
		float fscaleIWH = static_cast<float>(nImgWidth) / static_cast<float>(nImgHeight);
		float fscaleCHW = static_cast<float>(nActualHeight) / static_cast<float>(nActualWidth);
		float fscaleIHW = static_cast<float>(nImgHeight) / static_cast<float>(nImgWidth);


		float fscaleInuseClient = (fscaleCWH - fscaleCHW) > 0 ? fscaleCWH : fscaleCHW;
		bool bClientSide = (fscaleCWH - fscaleCHW) > 0 ? true : false;//判断屏幕哪一边更宽
		float fscaleInuseImage = (fscaleIWH - fscaleIHW) > 0 ? fscaleIWH : fscaleIHW;
		bool bImageSide = (fscaleIWH - fscaleIHW) > 0 ? true : false;//判断图像那一边更宽
		bool bChoose = (fscaleInuseClient - fscaleInuseImage) > 0 ? true : false;//判端屏幕的比例是否比图像比例更极端


		if (bChoose)
		{
			fscaleimgHW - fscaleimgWH < 0 ? cur_H = nActualHeight : cur_W = nActualWidth;
			fscaleimgHW - fscaleimgWH < 0 ? cur_W = static_cast<int>(cur_H * fscaleimgWH) : cur_H = static_cast<int>(cur_W * fscaleimgHW);
		}
		else
		{
			fscaleimgHW - fscaleimgWH > 0 ? cur_W = nActualWidth : cur_H = nActualHeight;
			fscaleimgHW - fscaleimgWH > 0 ? cur_H = static_cast<int>(cur_W * fscaleimgHW) : cur_W = static_cast<int>(cur_H * fscaleimgWH);
		}
		//二次矫正，避免图像过大的问题
		if (cur_W > nActualWidth)
		{
			int tempWidth = cur_W;
			cur_W = nActualWidth;
			fscale = static_cast<float>(cur_W) / static_cast<float>(tempWidth);
			cur_H = static_cast<int>(cur_H * fscale);

		}
		else if (cur_H > nActualHeight)
		{
			int tempHeight = cur_H;
			cur_H = nActualHeight;
			fscale = static_cast<float>(cur_H) / static_cast<float>(tempHeight);
			cur_W = static_cast<int>(cur_W * fscale);
		}
		m_DrawRect = CRect((nActualWidth - cur_W) / 2,
			(nActualHeight - cur_H) / 2,
			(nActualWidth + cur_W) / 2,
			(nActualHeight + cur_H) / 2);


	}

}

void CMyVideoArea::SetFile(LPCSTR lpParam)
{
	if (lpParam[0] == '\0')//简单校验
	{
		AfxMessageBox(L"文件名有误！");
		return;
	}
	memset(m_cFilePath, 0, sizeof(m_cFilePath));
	strcpy_s(m_cFilePath, lpParam);
}

UINT ThreadPlay(LPVOID lpParam)
{
	CMyVideoArea* MyArea = (CMyVideoArea*)lpParam;

	MyArea->FFmpegplayer();

	return 0;
}

UINT ThreadRender(LPVOID lpParam)
{
	CMyVideoArea* dlg = (CMyVideoArea*)lpParam;
	dlg->RenderArea();
	return 0;
}

void CMyVideoArea::PlayBegin()
{

	m_bEndNormal = false;
	m_bThreadPause = false;
	m_bThreadBegin = true;
	Global_BitMapQueue.EmptyQueue();
	Global_DelayQueue.EmptyQueue();
	if (!m_pThreadPlay)
		m_pThreadPlay = AfxBeginThread(ThreadPlay, this);//开启线程
	if (!m_pThreadRender)
		m_pThreadRender = AfxBeginThread(ThreadRender, this);
}

void CMyVideoArea::PlayStop()
{
	if (m_pThreadRender)
	{
		if (m_bThreadPause)//若当前线程处于暂停态，则继续以便线程执行正常以释放内存
		{
			m_pThreadRender->ResumeThread();
		}
		PostThreadMessage(m_pThreadRender->m_nThreadID, WMU_STOP, 0, 0);

		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadRender->m_hThread, 125))//若仍存在，说明处于消费者阻塞态
		{
			//放一个值使其解除阻塞
			if (!Global_DelayQueue.IsFull())
			{
				TRACE("Blocked,put something\n");
				Global_DelayQueue.PutItem(0);
			}
			PostThreadMessage(m_pThreadRender->m_nThreadID, WMU_STOP, 0, 0);
			WaitForSingleObject(m_pThreadRender->m_hThread, INFINITE);
		}
	}
	TRACE("Force Render Thread Ended\n");

	if (m_pThreadPlay)//释放解码线程
	{
		if (m_bThreadPause)//若当前线程处于暂停态，则继续以便线程执行正常以释放内存
		{
			m_pThreadPlay->ResumeThread();
		}
		PostThreadMessage(m_pThreadPlay->m_nThreadID, WMU_STOP, 0, 0);
		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadPlay->m_hThread, 125))
		{
			//取消值使其解除阻塞。
			if (!Global_BitMapQueue.IsEmpty())
			{
				TRACE("Blocked,remove bitmap handle\n");
				Global_BitMapQueue.EmptyQueue();
			}
			if (!Global_DelayQueue.IsEmpty())
			{
				TRACE("Blocked,remove Delay \n");
				Global_DelayQueue.EmptyQueue();
			}
			PostThreadMessage(m_pThreadPlay->m_nThreadID, WMU_STOP, 0, 0);
			WaitForSingleObject(m_pThreadPlay->m_hThread, INFINITE);
		}
	}
	CleanUp();
	TRACE("Force Work Thread Ended\n");
	Invalidate();//更新为黑矩形
}

void CMyVideoArea::PlayPause()
{
	m_bThreadPause = !m_bThreadPause;
	if (m_bThreadPause)
	{
		TRACE(_T("thread pause\n"));
		m_pThreadPlay->SuspendThread();
		m_pThreadRender->SuspendThread();

	}
	else
	{
		//从暂停态恢复，则释放资源
		TRACE(_T("thread continue\n"));
		if (!m_cImage.IsNull())
			m_cImage.Destroy();
		m_pThreadPlay->ResumeThread();
		m_pThreadRender->ResumeThread();

	}


}

void CMyVideoArea::SetFullScreen(bool bCurModel)
{
	if (bCurModel)
	{
		m_bIsFullScreen = true;
		CRect WindowRect;
		::GetWindowRect(::GetDesktopWindow(), &WindowRect);
		m_ScreenRect = WindowRect;
	}
	else
	{
		m_bIsFullScreen = true;
	}
}

void CMyVideoArea::RenderArea()
{
	int Delay = 0;
	MSG msg;
	TRACE("Render Thread Begin\n");
	while (!PeekMessage(&msg, NULL, WMU_STOP, WMU_STOP, PM_NOREMOVE))
	{
		if (!Global_DelayQueue.IsEmpty())
		{
			Delay = Global_DelayQueue.TakeItem();
			
		}
		if(Delay>18)
		Sleep(Delay);
		Invalidate();
	}
	DeleteObject(msg.hwnd);
	TRACE("render thread ended\n");
}

void CMyVideoArea::CleanUp()
{

	m_bThreadBegin = false;
	m_bThreadPause = false;
	m_bEndNormal = false;

	if (m_pThreadPlay)
	{
		WaitForSingleObject(m_pThreadPlay, INFINITE);
	}
	if (m_pThreadRender)
	{
		WaitForSingleObject(m_pThreadRender, INFINITE);
	}
	TRACE("In Clean Empty Queue\n");//清空队列
	Global_DelayQueue.EmptyQueue();
	Global_BitMapQueue.EmptyQueue();

	m_pThreadPlay = NULL;
	m_pThreadRender = NULL;

}

int CMyVideoArea::SaveYUVFrameToFile(AVFrame* frame, int width, int height, int flag)//Debug use
{
	
		FILE* fileHandle;
		int y, writeError;
		char filename[32];
		static int frameNumber = 0;
		if (flag == 0)
		{//src
			sprintf_s(filename, "./YUVS/frameS%d.yuv", frameNumber);
		}
		else if (flag == 1)
		{//dst
			sprintf_s(filename, "./YUVD/frameD%d.yuv", frameNumber);
		}
		fopen_s(&fileHandle,filename, "wb+");
		if (fileHandle == NULL)
		{
			TRACE("Unable to open % s…\n", filename);
			return 0;
		}
		/*Writing Y plane data to file.*/
		for (y = 0; y < height; y++)
		{
			writeError = fwrite(frame->data[0] + y * frame->linesize[0], 1, width, fileHandle);
			if (writeError != width)
			{
				TRACE("Unable to write Y plane data!\n");
				return 0;
			}
		}

		/*Dividing by 2.*/
		height >>= 1;
		width >>= 1;

		/*Writing U plane data to file.*/
		for (y = 0; y < height; y++)
		{
			writeError = fwrite(frame->data[1] + y * frame->linesize[1], 1, width, fileHandle);
			if (writeError != width)
			{
				TRACE("Unable to write U plane data!\n");
				return 0;
			}
		}

		/*Writing V plane data to file.*/
		for (y = 0; y < height; y++)
		{
			writeError = fwrite(frame->data[2] + y * frame->linesize[2], 1, width, fileHandle);
			if (writeError != width)
			{
				TRACE("Unable to write V plane data!\n");
				return 0;
			}
		}

		fclose(fileHandle);
		frameNumber++;
		return 1;
}

bool CMyVideoArea::IsAllOver()
{
	if (m_pThreadPlay == NULL && m_pThreadRender == NULL)
		return true;
	return false;
}