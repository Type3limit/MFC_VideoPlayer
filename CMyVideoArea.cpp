#include "pch.h"
#include "CMyVideoArea.h"




CMyVideoArea::CMyVideoArea() :
	m_bThreadBegin(false), m_bThreadExit(false),
	m_bThreadPause(false), m_bIsFullScreen(false),
	m_nImageHeight(0), m_nImageWidth(0),
	m_pThreadRender(nullptr), m_bEndNormal(false),
	m_pThreadPlay(nullptr)
{
	m_pPaintMutex = new CMutex(FALSE, NULL);

}

CMyVideoArea::~CMyVideoArea()
{
	delete m_pPaintMutex;
	TRACE("All threads over done.\n");
}

BEGIN_MESSAGE_MAP(CMyVideoArea, CStatic)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_MESSAGE(WMU_PLAY_ERROR, &CMyVideoArea::ErrorWarning)
END_MESSAGE_MAP()

void CMyVideoArea::OnPaint()
{
	GetWindowRect(&m_ScreenRect);
	ScreenToClient(&m_ScreenRect);
	CPaintDC pDC(this);
	CDC memDc;
	memDc.CreateCompatibleDC(&pDC);
	if (memDc)
	{
		CBitmap memBitmap;
		memBitmap.CreateCompatibleBitmap(&pDC, m_ScreenRect.Width(), m_ScreenRect.Height());
		auto Cur = memDc.SelectObject(memBitmap);//将CBitmap的一位位图选出，避免高速绘制时的内存泄漏
		
		memDc.FillSolidRect(&m_ScreenRect, RGB(0, 0, 0));
		memDc.SetStretchBltMode(HALFTONE);

		if (!m_cImage.IsNull())
		{
			m_pPaintMutex->Lock();
			m_cImage.Draw(memDc, m_DrawRect, m_ImageRect);
			m_pPaintMutex->Unlock();
		}

		pDC.BitBlt(0, 0, m_ScreenRect.Width(), m_ScreenRect.Height(), &memDc, 0, 0, SRCCOPY);
		memBitmap.DeleteObject();
		DeleteObject(Cur);//释放选出的位图
	}
	memDc.DeleteDC();
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

	AVFormatContext* pFormatCtx;
	int nVideoIndex, nAudioIndex;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameRGB;
	uint8_t* pOutBufferRGB;

	AVPacket* pPacket;
	AVStream* pStream;
	int nRet = 0, nGotPicture = 0;
	struct SwsContext* pImgConvertCtx;

	int nReturnValue = EXIT_NORMAL;

	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	TRACE("Work Thread Beagin!\n");
	if (avformat_open_input(&pFormatCtx, (LPCSTR)m_cFileName, NULL, NULL) != 0) //打开视频文件
	{
		m_cExceptionStr.Format(_T("Couldn't open input stream.\n"));
		nReturnValue = EXIT_EXCPTION;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) //找到视频流
	{
		m_cExceptionStr.Format(_T("Couldn't find stream information.\n"));
		nReturnValue = EXIT_EXCPTION;
	}
	nVideoIndex = -1;
	nAudioIndex = -1;
	for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) //找到类型对应的流
	{
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)//视频流
			nVideoIndex = i;
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)//音频流
			nAudioIndex = i;
	}

	if (nVideoIndex == -1) {

		m_cExceptionStr.Format(_T("Didn't find a video stream.\n"));//未找到视频流
		nReturnValue = EXIT_EXCPTION;
	}
	if (nAudioIndex == -1)
	{
		m_cExceptionStr.Format(_T("Didn't find a audio stream.\n"));//未找到音频流（本程序暂无音频播放能力）
		nReturnValue = EXIT_EXCPTION;
	}
	pCodecCtx = avcodec_alloc_context3(NULL);
	if (pCodecCtx == NULL)
	{
		m_cExceptionStr.Format(_T("codec context alloc failed!"));
		nReturnValue = EXIT_EXCPTION;

	}
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[nVideoIndex]->codecpar);

	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);//根据上下文找到编解码器，软解
	//m_pCodec = avcodec_find_decoder_by_name("h264_mediacodec");//硬解码
	if (pCodec == NULL) {
		m_cExceptionStr.Format(_T("Codec not found.\n"));
		nReturnValue = EXIT_EXCPTION;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) //打开编解码器
	{
		m_cExceptionStr.Format(_T("Could not open codec.\n"));
		nReturnValue = EXIT_EXCPTION;
	}
	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();


	if (pFrame == NULL || pFrameRGB == NULL)
	{
		m_cExceptionStr.Format(_T("Frame alloc failed!"));
		nReturnValue = EXIT_EXCPTION;
	}



	pOutBufferRGB = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize,
		pOutBufferRGB, AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height, 1);

	//调整绘制区
	m_nImageWidth = pCodecCtx->width;
	m_nImageHeight = pCodecCtx->height;

	AdjustDrawRect();

	//设置图形转换的上下文
	pImgConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_BGR24, SWS_FAST_BILINEAR, NULL, NULL, NULL);

	pStream = pFormatCtx->streams[nVideoIndex];
	pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));

	uint64_t uiBasePts = AV_NOPTS_VALUE;
	double fLastPTS = 0.0;
	int64_t CurPts = 0;
	double FrameTime = 0.0;
	int Delay = 0;
	double diff = 0.0;
#pragma endregion

	//每次循环读入一个包
	while (!m_bThreadExit && nReturnValue == EXIT_NORMAL)
	{
		if (av_read_frame(pFormatCtx, pPacket) >= 0)
		{
			if (pPacket->stream_index == nVideoIndex)
			{
				nRet = avcodec_send_packet(pCodecCtx, pPacket);

				if (nRet != 0) {
					m_cExceptionStr.Format(_T("Decode Error%d.\n"), nRet);
					nReturnValue = EXIT_EXCPTION;

				}
				while ((nRet = avcodec_receive_frame(pCodecCtx, pFrame)) == 0)
				{
					//获得延时
					if (pPacket->dts != AV_NOPTS_VALUE)//若dts可用，取当前的帧的dts
					{
						CurPts = pPacket->dts;
					}
					else if (pPacket->dts == AV_NOPTS_VALUE &&
						pFrame->opaque && *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE)//dts不可用，则同步到外部（因无音频，暂不可用）
					{
						CurPts = *((uint64_t*)pFrame->opaque);
					}
					else//否则使用当前帧的pts
						CurPts = pPacket->pts;
					FrameTime = CurPts * av_q2d(pFormatCtx->streams[nVideoIndex]->time_base);
					if (FrameTime > 0.0)
					{
						diff = FrameTime - fLastPTS;
						Delay = (int)(diff * 1000);
						fLastPTS = FrameTime;
					}

					sws_scale(pImgConvertCtx,
						(const uint8_t* const*)pFrame->data,
						pFrame->linesize,
						0, pCodecCtx->height,
						pFrameRGB->data, pFrameRGB->linesize);

					RestoreMetaData(pFrameRGB, pFrame->width, pFrame->height, 24, Delay, pFrame->linesize);//将MetaData直接存为HBITMAP,24位深

					av_frame_unref(pFrame);
				}

			}
			av_packet_unref(pPacket);
		}
		else //正常结束，即视频包已被读完或读取视频包失败
		{
			if (m_ImageQueue.IsEmpty())
			{
				break;
			}
		}
	}
	//清理资源
	sws_freeContext(pImgConvertCtx);
	av_frame_free(&pFrameRGB);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	avformat_free_context(pFormatCtx);
	av_free(pOutBufferRGB);
	LetRenderThreadEnd();
	CleanUp();
	TRACE("work thread ended\n");
	::PostMessage(this->GetParent()->GetSafeHwnd(), WMU_PLAY_OVER, 0, 0);

	return nReturnValue;
}

void CMyVideoArea::RestoreMetaData(AVFrame* pFrame, int nWidth, int nHeight, int nBpp, int Delay, int* padding)
{
	int Delta = ((nWidth * 3) % 4 == 0) ? 0 : 4 - (nWidth * 3) % 4;
	uint8_t* pBuffer = NULL;
	if (Delta == 0)
		goto RejudgeOver;
	pBuffer = new uint8_t[((nWidth * 3) + Delta) * nHeight];
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
			pBuffer[3 * (nWidth * i + j)] = B;
			pBuffer[3 * (nWidth * i + j) + 1] = G;
			pBuffer[3 * (nWidth * i + j) + 2] = R;	//BGR存储像素点信息
		}
	}
RejudgeOver:
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + ((nWidth * 3) + Delta) * nHeight;
	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = nWidth;
	bmpinfo.biHeight = -nHeight;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = 24;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = nWidth * nHeight;
	bmpinfo.biXPelsPerMeter = 0;
	bmpinfo.biYPelsPerMeter = 0;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	CClientDC dc(this);

	//创建设备相关位图，用于快速绘制，因DDB位图需要二次转换
	HBITMAP hTemp = nullptr;

	if (Delta != 0)
	{
		hTemp = CreateDIBitmap(dc.GetSafeHdc(),							//设备上下文的句柄 
			(LPBITMAPINFOHEADER)&bmpinfo,				//位图信息头指针 
			(long)CBM_INIT,								//初始化标志 
			pBuffer,						//初始化数据指针 
			(LPBITMAPINFO)&bmpinfo,						//位图信息指针 
			DIB_RGB_COLORS);
	}
	else
	{
		hTemp = CreateDIBitmap(dc.GetSafeHdc(),							//设备上下文的句柄 
			(LPBITMAPINFOHEADER)&bmpinfo,				//位图信息头指针 
			(long)CBM_INIT,								//初始化标志 
			pFrame->data[0],						//初始化数据指针 
			(LPBITMAPINFO)&bmpinfo,						//位图信息指针 
			DIB_RGB_COLORS);
	}
	if (hTemp != NULL)
	{
		CoverImage* CurBuf = new CoverImage(hTemp, Delay);
		m_ImageQueue.PutItem(CurBuf);

	}
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

void CMyVideoArea::SetFile(CStringA  str)
{
	if (!m_cFileName.IsEmpty())
		m_cFileName.Empty();
	m_cFileName = str;
}

UINT CMyVideoArea::ThreadPlay(LPVOID lpParam)
{
	CMyVideoArea* MyArea = (CMyVideoArea*)lpParam;

	int n = MyArea->FFmpegplayer();
	if (n == EXIT_EXCPTION)
	{
		MyArea->SendMessage(WMU_PLAY_ERROR, 0, 0);
		MyArea->CleanUp();
	}

	return n;
}

UINT CMyVideoArea::ThreadRender(LPVOID lpParam)
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
	//开启线程
	if (m_pThreadPlay != nullptr || m_pThreadRender != nullptr)
	{
		m_pThreadPlay = nullptr;
		m_pThreadRender = nullptr;
	}
	m_pThreadPlay = AfxBeginThread(ThreadPlay, this);
	m_pThreadRender = AfxBeginThread(ThreadRender, this);
}

void CMyVideoArea::PlayStop()
{
	TRACE("force thread ended\n ");
	if(m_bThreadBegin)
	    LetAllThreadEnd();
	m_bThreadBegin = false;
}

void CMyVideoArea::PlayPause()
{
	m_bThreadPause = !m_bThreadPause;
	m_bThreadPause? TRACE(_T("thread pause\n")):TRACE(_T("thread continue\n"));
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
	int  Delay = 0;

	TRACE("Render Thread Begin\n");
	while (!m_bEndNormal)
	{
		if (!m_bThreadPause)
		{

			CoverImage* pCurBuf = m_ImageQueue.TakeItem();
			Delay = pCurBuf->st_nDelay;

			HBITMAP hBuf = nullptr;
			hBuf = pCurBuf->Detach();
			delete pCurBuf;

			m_ImageQueue.RemoveFront();

			m_pPaintMutex->Lock();

			if (!m_cImage.IsNull())
				m_cImage.Destroy();

			if (hBuf != nullptr)
				m_cImage.Attach(hBuf);

			m_pPaintMutex->Unlock();

			Invalidate();

			Sleep(Delay);
		}
		else
			Sleep(16);//mininum time spin
	}
	TRACE("render thread ended\n");
}

void CMyVideoArea::CleanUp()
{
	m_bThreadBegin = false;
	m_bThreadPause = false;
	m_bEndNormal = false;
	m_bThreadExit = false;

	while (!m_ImageQueue.IsEmpty())
	{
		CoverImage* pTmp = m_ImageQueue.TakeItemWithoutBlock();
		DeleteObject(pTmp->Detach());
		if (!pTmp)
			delete pTmp;
		m_ImageQueue.RemoveFront();
	}
	m_ImageQueue.EmptyQueue();
	m_cImage.Destroy();
	Invalidate();//更新为黑矩形
}

void CMyVideoArea::LetAllThreadEnd()
{
	LetRenderThreadEnd();
	m_bThreadExit = true;
	if (m_pThreadPlay != nullptr)//释放解码线程
	{
		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadPlay->m_hThread, 125))//若仍存在，说明处于生产者阻塞态
		{
			TRACE("Blocked ,remove something\n");

			while (!m_ImageQueue.IsEmpty())
			{
				CoverImage* pTmp = m_ImageQueue.TakeItemWithoutBlock();
				DeleteObject(pTmp->Detach());
				delete pTmp;
				m_ImageQueue.RemoveFront();
			}
			WaitForSingleObject(m_pThreadPlay->m_hThread, INFINITE);
		}
	}
	TRACE("all threads should be stopped now \n");
}

void CMyVideoArea::LetRenderThreadEnd()
{
	m_bEndNormal = true;
	if (m_pThreadRender != nullptr)
	{
		if (m_bThreadPause)//若当前线程处于暂停态，则继续以便线程执行正常以释放内存
		{
			m_pThreadRender->ResumeThread();
		}
		if (WAIT_TIMEOUT == WaitForSingleObject(m_pThreadRender->m_hThread, 125))//若仍存在，说明处于消费者阻塞态
		{
			//放一个值使其解除阻塞
			if (!m_ImageQueue.IsFull())
			{
				TRACE("Blocked,put something\n");
				m_ImageQueue.PutItem(new CoverImage(nullptr, 0));
			}
			WaitForSingleObject(m_pThreadRender->m_hThread, INFINITE);
		}
	}
	TRACE("render thread should be stopped now \n");
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
	fopen_s(&fileHandle, filename, "wb+");
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

LRESULT CMyVideoArea::ErrorWarning(WPARAM, LPARAM)
{
	AfxMessageBox(m_cExceptionStr, MB_OK);
	TRACE("error happened\n");
	m_cExceptionStr.Empty();
	return 0UL;
}