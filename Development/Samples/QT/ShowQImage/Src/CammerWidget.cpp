#include "CammerWidget.h"
#include "ui_cammerwidget.h"

#define DEFAULT_SHOW_RATE (30) // 默认显示帧率 | defult display frequency
#define DEFAULT_ERROR_STRING ("N/A") 
#define MAX_FRAME_STAT_NUM (50) 
#define MIN_LEFT_LIST_NUM (2) 
#define MAX_STATISTIC_INTERVAL (5000000000) // List的更新时与最新一帧的时间最大间隔 |  The maximum time interval between the update of list and the latest frame

// 取流回调函数
// get frame callback function 
static void FrameCallback(IMV_Frame* pFrame, void* pUser)
{
	CammerWidget* pCammerWidget = (CammerWidget*)pUser;
	if (!pCammerWidget)
	{
		printf("pCammerWidget is NULL!\n");
		return;
	}

	CFrameInfo frameInfo;
	frameInfo.m_nWidth = (int)pFrame->frameInfo.width;
	frameInfo.m_nHeight = (int)pFrame->frameInfo.height;
	frameInfo.m_nBufferSize = (int)pFrame->frameInfo.size;
	frameInfo.m_nPaddingX = (int)pFrame->frameInfo.paddingX;
	frameInfo.m_nPaddingY = (int)pFrame->frameInfo.paddingY;
	frameInfo.m_ePixelType = pFrame->frameInfo.pixelFormat;
	frameInfo.m_pImageBuf = (unsigned char *)malloc(sizeof(unsigned char) * frameInfo.m_nBufferSize);
	frameInfo.m_nTimeStamp = pFrame->frameInfo.timeStamp;

	// 内存申请失败，直接返回
	// memory application failed, return directly
	if (frameInfo.m_pImageBuf != NULL)
	{
		memcpy(frameInfo.m_pImageBuf, pFrame->pData, frameInfo.m_nBufferSize);

		if (pCammerWidget->m_qDisplayFrameQueue.size() > 16)
		{
			CFrameInfo frameOld;
			if (pCammerWidget->m_qDisplayFrameQueue.get(frameOld))
			{
				free(frameOld.m_pImageBuf);
				frameOld.m_pImageBuf = NULL;
			}
		}

		pCammerWidget->m_qDisplayFrameQueue.push_back(frameInfo);
	}

	pCammerWidget->recvNewFrame(pFrame->frameInfo.size);
}

// 显示线程
// display thread
static unsigned int __stdcall displayThread(void* pUser)
{
	CammerWidget* pCammerWidget = (CammerWidget*)pUser;
	if (!pCammerWidget)
	{
		printf("pCammerWidget is NULL!\n");
		return -1;
	}

	pCammerWidget->display();

	return 0;
}

CammerWidget::CammerWidget(QWidget *parent) :
    QWidget(parent)
    ,ui(new Ui::CammerWidget)
	, m_currentCameraKey("")
	, m_devHandle(NULL)
	, m_nDisplayInterval(0)
	, m_nFirstFrameTime(0)
	, m_nLastFrameTime(0)
	, m_bNeedUpdate(true)
	, m_nTotalFrameCount(0)
	, m_isExitDisplayThread(false)
	, m_threadHandle(NULL)
{
    ui->setupUi(this);

	qRegisterMetaType<uint64_t>("uint64_t");
	connect(this, SIGNAL(signalShowImage(unsigned char*, int, int, uint64_t)), this, SLOT(ShowImage(unsigned char*, int, int, uint64_t)));

	// 默认显示30帧
	// defult display 30 frames 
	setDisplayFPS(DEFAULT_SHOW_RATE);

	m_elapsedTimer.start();

	// 启动显示线程
	// start display thread
	m_threadHandle = (HANDLE)_beginthreadex(NULL,
		0,
		displayThread,
		this,
		CREATE_SUSPENDED,
		NULL);

	if (!m_threadHandle)
	{
		printf("Failed to create display thread!\n");
	}
	else
	{
		ResumeThread(m_threadHandle);

		m_isExitDisplayThread = false;
	}
}

CammerWidget::~CammerWidget()
{
	// 关闭显示线程
	// close display thread
	m_isExitDisplayThread = true;
	WaitForSingleObject(m_threadHandle, INFINITE);
	CloseHandle(m_threadHandle);

	delete ui;
}

// 设置曝光
// set exposeTime
bool CammerWidget::SetExposeTime(double dExposureTime)
{
	int ret = IMV_OK;

	ret = IMV_SetDoubleFeatureValue(m_devHandle, "ExposureTime", dExposureTime);
	if (IMV_OK != ret)
	{
		printf("set ExposureTime value = %0.2f fail, ErrorCode[%d]\n", dExposureTime, ret);
		return false;
	}

	return true;
}

// 设置增益
// set gain
bool CammerWidget::SetAdjustPlus(double dGainRaw)
{
	int ret = IMV_OK;

	ret = IMV_SetDoubleFeatureValue(m_devHandle, "GainRaw", dGainRaw);
	if (IMV_OK != ret)
	{
		printf("set GainRaw value = %0.2f fail, ErrorCode[%d]\n", dGainRaw, ret);
		return false;
	}

	return true;
}

// 打开相机
// open camera
bool CammerWidget::CameraOpen(void)
{
	int ret = IMV_OK;

	if (m_currentCameraKey.length() == 0)
	{
		printf("open camera fail. No camera.\n");
		return false;
	}

	if (m_devHandle)
	{
		printf("m_devHandle is already been create!\n");
		return false;
	}
	QByteArray cameraKeyArray = m_currentCameraKey.toLocal8Bit();
	char* cameraKey = cameraKeyArray.data();

	ret = IMV_CreateHandle(&m_devHandle, modeByCameraKey, (void*)cameraKey);
	if (IMV_OK != ret)
	{
		printf("create devHandle failed! cameraKey[%s], ErrorCode[%d]\n", cameraKey, ret);
		return false;
	}

	// 打开相机 
	// Open camera 
	ret = IMV_Open(m_devHandle);
	if (IMV_OK != ret)
	{
		printf("open camera failed! ErrorCode[%d]\n", ret);
		return false;
	}

	return true;
}

// 关闭相机
// close camera
bool CammerWidget::CameraClose(void)
{
	int ret = IMV_OK;

	if (!m_devHandle)
	{
		printf("close camera fail. No camera.\n");
		return false;
	}

	if (false == IMV_IsOpen(m_devHandle))
	{
		printf("camera is already close.\n");
		return false;
	}

	ret = IMV_Close(m_devHandle);
	if (IMV_OK != ret)
	{
		printf("close camera failed! ErrorCode[%d]\n", ret);
		return false;
	}

	ret = IMV_DestroyHandle(m_devHandle);
	if (IMV_OK != ret)
	{
		printf("destroy devHandle failed! ErrorCode[%d]\n", ret);
		return false;
	}

	m_devHandle = NULL;

	return true;
}

// 开始采集
// start grabbing
bool CammerWidget::CameraStart()
{
	int ret = IMV_OK;

	if (IMV_IsGrabbing(m_devHandle))
	{
		printf("camera is already grebbing.\n");
		return false;
	}


	ret = IMV_AttachGrabbing(m_devHandle, FrameCallback, this);
	if (IMV_OK != ret)
	{
		printf("Attach grabbing failed! ErrorCode[%d]\n", ret);
		return false;
	}

	ret = IMV_StartGrabbing(m_devHandle);
	if (IMV_OK != ret)
	{
		printf("start grabbing failed! ErrorCode[%d]\n", ret);
		return false;
	}

	return true;
}

// 停止采集
// stop grabbing
bool CammerWidget::CameraStop()
{
	int ret = IMV_OK;
	if (!IMV_IsGrabbing(m_devHandle))
	{
		printf("camera is already stop grebbing.\n");
		return false;
	}

	ret = IMV_StopGrabbing(m_devHandle);
	if (IMV_OK != ret)
	{
		printf("Stop grabbing failed! ErrorCode[%d]\n", ret);
		return false;
	}

	// 清空显示队列 
	// clear display queue
	CFrameInfo frameOld;
	while (m_qDisplayFrameQueue.get(frameOld))
	{
		free(frameOld.m_pImageBuf);
		frameOld.m_pImageBuf = NULL;
	}

	m_qDisplayFrameQueue.clear();

	return true;
}

// 切换采集方式、触发方式 （连续采集、外部触发、软件触发）
// Switch acquisition mode and triggering mode (continuous acquisition, external triggering and software triggering)
bool CammerWidget::CameraChangeTrig(ETrigType trigType)
{
	int ret = IMV_OK;

	if (trigContinous == trigType)
	{
		// 设置触发模式
		// set trigger mode
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerMode", "Off");
		if (IMV_OK != ret)
		{
			printf("set TriggerMode value = Off fail, ErrorCode[%d]\n", ret);
			return false;
		}
	}
	else if (trigSoftware == trigType)
	{
		// 设置触发器
		// set trigger
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerSelector", "FrameStart");
		if (IMV_OK != ret)
		{
			printf("set TriggerSelector value = FrameStart fail, ErrorCode[%d]\n", ret);
			return false;
		}

		// 设置触发模式
		// set trigger mode
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerMode", "On");
		if (IMV_OK != ret)
		{
			printf("set TriggerMode value = On fail, ErrorCode[%d]\n", ret);
			return false;
		}

		// 设置触发源为软触发
		// set triggerSource as software trigger
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerSource", "Software");
		if (IMV_OK != ret)
		{
			printf("set TriggerSource value = Software fail, ErrorCode[%d]\n", ret);
			return false;
		}
	}
	else if (trigLine == trigType)
	{
		// 设置触发器
		// set trigger
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerSelector", "FrameStart");
		if (IMV_OK != ret)
		{
			printf("set TriggerSelector value = FrameStart fail, ErrorCode[%d]\n", ret);
			return false;
		}

		// 设置触发模式
		// set trigger mode
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerMode", "On");
		if (IMV_OK != ret)
		{
			printf("set TriggerMode value = On fail, ErrorCode[%d]\n", ret);
			return false;
		}

		// 设置触发源为Line1触发
		// set trigggerSource as Line1 trigger 
		ret = IMV_SetEnumFeatureSymbol(m_devHandle, "TriggerSource", "Line1");
		if (IMV_OK != ret)
		{
			printf("set TriggerSource value = Line1 fail, ErrorCode[%d]\n", ret);
			return false;
		}
	}

	return true;
}

// 执行一次软触发
// execute one software trigger
bool CammerWidget::ExecuteSoftTrig(void)
{
	int ret = IMV_OK;

	ret = IMV_ExecuteCommandFeature(m_devHandle, "TriggerSoftware");
	if (IMV_OK != ret)
	{
		printf("ExecuteSoftTrig fail, ErrorCode[%d]\n", ret);
		return false;
	}

	printf("ExecuteSoftTrig success.\n");
	return true;
}

// 设置当前相机  
// set current camera
void CammerWidget::SetCamera(const QString& strKey)
{
	m_currentCameraKey = strKey;
}

// 显示
// diaplay
bool CammerWidget::ShowImage(unsigned char* pRgbFrameBuf, int nWidth, int nHeight, uint64_t nPixelFormat)
{
	QImage image;
	if (NULL == pRgbFrameBuf ||
		nWidth == 0 ||
		nHeight == 0)
	{
		printf("%s image is invalid.\n", __FUNCTION__);
		return false;
	}
    if (gvspPixelMono8 == nPixelFormat)
	{
		image = QImage(pRgbFrameBuf, nWidth, nHeight, QImage::Format_Grayscale8);
	}
	else
	{
		image = QImage(pRgbFrameBuf,nWidth, nHeight, QImage::Format_RGB888);
	}
    // 将QImage的大小收缩或拉伸，与label的大小保持一致。这样label中能显示完整的图片
	// Shrink or stretch the size of Qimage to match the size of the label. In this way, the complete image can be displayed in the label
	QImage imageScale = image.scaled(QSize(ui->label_Pixmap->width(), ui->label_Pixmap->height()));
	QPixmap pixmap = QPixmap::fromImage(imageScale);
	ui->label_Pixmap->setPixmap(pixmap);
	free(pRgbFrameBuf);

	return true;
}

// 显示线程
// display thread
void CammerWidget::display()
{
	while (!m_isExitDisplayThread)
	{
		CFrameInfo frameInfo;

		if (false == m_qDisplayFrameQueue.get(frameInfo))
		{
			Sleep(1);
			continue;
		}

		// 判断是否要显示。超过显示上限（30帧），就不做转码、显示处理
		// Judge whether to display. If the upper display limit (30 frames) is exceeded, transcoding and display processing will not be performed
		if (!isTimeToDisplay())
		{
			// 释放内存
			// release memory
			free(frameInfo.m_pImageBuf);
			continue;
		}

		// mono8格式可不做转码，直接显示，其他格式需要经过转码才能显示 
		// mono8 format can be displayed directly without transcoding. Other formats can be displayed only after transcoding
		if (gvspPixelMono8 == frameInfo.m_ePixelType)
		{
			// 显示线程中发送显示信号，在主线程中显示图像
			// Send display signal in display thread and display image in main thread
			emit signalShowImage(frameInfo.m_pImageBuf, (int)frameInfo.m_nWidth, (int)frameInfo.m_nHeight, (uint64_t)frameInfo.m_ePixelType);
		}
		else
		{
			// 转码 
			unsigned char* pRGBbuffer = NULL;
			int nRgbBufferSize = 0;
			nRgbBufferSize = frameInfo.m_nWidth * frameInfo.m_nHeight * 3;
			pRGBbuffer = (unsigned char*)malloc(nRgbBufferSize);
			if (pRGBbuffer == NULL)
			{
				// 释放内存
				// release memory
				free(frameInfo.m_pImageBuf);
				printf("RGBbuffer malloc failed.\n");
				continue;
			}

			IMV_PixelConvertParam stPixelConvertParam;
			stPixelConvertParam.nWidth = frameInfo.m_nWidth;
			stPixelConvertParam.nHeight = frameInfo.m_nHeight;
			stPixelConvertParam.ePixelFormat = frameInfo.m_ePixelType;
			stPixelConvertParam.pSrcData = frameInfo.m_pImageBuf;
			stPixelConvertParam.nSrcDataLen = frameInfo.m_nBufferSize;
			stPixelConvertParam.nPaddingX = frameInfo.m_nPaddingX;
			stPixelConvertParam.nPaddingY = frameInfo.m_nPaddingY;
			stPixelConvertParam.eBayerDemosaic = demosaicNearestNeighbor;
			stPixelConvertParam.eDstPixelFormat = gvspPixelRGB8;
			stPixelConvertParam.pDstBuf = pRGBbuffer;
			stPixelConvertParam.nDstBufSize = nRgbBufferSize;

			int ret = IMV_PixelConvert(m_devHandle, &stPixelConvertParam);
			if (IMV_OK != ret)
			{
				// 释放内存 
				// release memory
				printf("image convert to RGB failed! ErrorCode[%d]\n", ret);
				free(frameInfo.m_pImageBuf);
				free(pRGBbuffer);
				continue;
			}

			// 释放内存
			// release memory
			free(frameInfo.m_pImageBuf);

            // 显示线程中发送显示信号，在主线程中显示图像
			// Send display signal in display thread and display image in main thread
			emit signalShowImage(pRGBbuffer, (int)stPixelConvertParam.nWidth, (int)stPixelConvertParam.nHeight, (uint64_t)stPixelConvertParam.eDstPixelFormat);
		}
	}
}

bool CammerWidget::isTimeToDisplay()
{
	m_mxTime.lock();

	// 不显示
	// don't display
	if (m_nDisplayInterval <= 0)
	{
		m_mxTime.unlock();
		return false;
	}

	// 第一帧必须显示
	// the frist frame must be displayed
	if (m_nFirstFrameTime == 0 || m_nLastFrameTime == 0)
	{
		m_nFirstFrameTime = m_elapsedTimer.nsecsElapsed();
		m_nLastFrameTime = m_nFirstFrameTime;

		m_mxTime.unlock();
		return true;
	}

	// 当前帧和上一帧的间隔如果大于显示间隔就显示
	// display if the interval between the current frame and the previous frame is greater than the display interval
	uint64_t nCurTimeTmp = m_elapsedTimer.nsecsElapsed();
	uint64_t nAcquisitionInterval = nCurTimeTmp - m_nLastFrameTime;
	if (nAcquisitionInterval > m_nDisplayInterval)
	{
		m_nLastFrameTime = nCurTimeTmp;
		m_mxTime.unlock();
		return true;
	}

	// 当前帧相对于第一帧的时间间隔
	// Time interval between the current frame and the first frame
	uint64_t nPre = (m_nLastFrameTime - m_nFirstFrameTime) % m_nDisplayInterval;
	if (nPre + nAcquisitionInterval > m_nDisplayInterval)
	{
		m_nLastFrameTime = nCurTimeTmp;
		m_mxTime.unlock();
		return true;
	}

	m_mxTime.unlock();
	return false;
}

// 设置显示频率
// set display frequency
void CammerWidget::setDisplayFPS(int nFPS)
{
	m_mxTime.lock();
	if (nFPS > 0)
	{
		m_nDisplayInterval = 1000 * 1000 * 1000.0 / nFPS;
	}
	else
	{
		m_nDisplayInterval = 0;
	}
	m_mxTime.unlock();
}

// 窗口关闭响应函数
// window close response function
void CammerWidget::closeEvent(QCloseEvent * event)
{
	IMV_DestroyHandle(m_devHandle);
	m_devHandle = NULL;
}

// 状态栏统计信息 开始
// Status bar statistics begin
void CammerWidget::resetStatistic()
{
	QMutexLocker locker(&m_mxStatistic);
	m_nTotalFrameCount = 0;
	m_listFrameStatInfo.clear();
	m_bNeedUpdate = true;
}
QString CammerWidget::getStatistic()
{
	if (m_mxStatistic.tryLock(30))
	{
		if (m_bNeedUpdate)
		{
			updateStatistic();
		}

		m_mxStatistic.unlock();
		return m_strStatistic;
	}
	return "";
}
void CammerWidget::updateStatistic()
{
	size_t nFrameCount = m_listFrameStatInfo.size();
	QString strFPS = DEFAULT_ERROR_STRING;
	QString strSpeed = DEFAULT_ERROR_STRING;

	if (nFrameCount > 1)
	{
		quint64 nTotalSize = 0;
		FrameList::const_iterator it = m_listFrameStatInfo.begin();

		if (m_listFrameStatInfo.size() == 2)
		{
			nTotalSize = m_listFrameStatInfo.back().m_nFrameSize;
		}
		else
		{
			for (++it; it != m_listFrameStatInfo.end(); ++it)
			{
				nTotalSize += it->m_nFrameSize;
			}
		}

		const FrameStatInfo& first = m_listFrameStatInfo.front();
		const FrameStatInfo& last = m_listFrameStatInfo.back();

		qint64 nsecs = last.m_nPassTime - first.m_nPassTime;

		if (nsecs > 0)
		{
			double dFPS = (nFrameCount - 1) * ((double)1000000000.0 / nsecs);
			double dSpeed = nTotalSize * ((double)1000000000.0 / nsecs) / (1000.0) / (1000.0) * (8.0);
			strFPS = QString::number(dFPS, 'f', 2);
			strSpeed = QString::number(dSpeed, 'f', 2);
		}
	}

	m_strStatistic = QString("Stream: %1 images   %2 FPS   %3 Mbps")
		.arg(m_nTotalFrameCount)
		.arg(strFPS)
		.arg(strSpeed);
	m_bNeedUpdate = false;
}

void CammerWidget::recvNewFrame(quint32 frameSize)
{
	QMutexLocker locker(&m_mxStatistic);
	if (m_listFrameStatInfo.size() >= MAX_FRAME_STAT_NUM)
	{
		m_listFrameStatInfo.pop_front();
	}
	m_listFrameStatInfo.push_back(FrameStatInfo(frameSize, m_elapsedTimer.nsecsElapsed()));
	++m_nTotalFrameCount;

	if (m_listFrameStatInfo.size() > MIN_LEFT_LIST_NUM)
	{
		FrameStatInfo infoFirst = m_listFrameStatInfo.front();
		FrameStatInfo infoLast = m_listFrameStatInfo.back();
		while (m_listFrameStatInfo.size() > MIN_LEFT_LIST_NUM && infoLast.m_nPassTime - infoFirst.m_nPassTime > MAX_STATISTIC_INTERVAL)
		{
			m_listFrameStatInfo.pop_front();
			infoFirst = m_listFrameStatInfo.front();
		}
	}

	m_bNeedUpdate = true;
}
// 状态栏统计信息 end 
// Status bar statistics ending