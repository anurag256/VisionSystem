//#include "stdafx.h"
#include "gentl_capture.h"
#include <process.h> 
#include <stdio.h>
#include <string>
#include <vector>
#include "cxp_driver.h"
#include "cxp_standard.h"


CGentlCap::CGentlCap()
	:m_h_system(NULL),
	m_h_if(NULL),
	m_h_device(NULL),
	m_h_rmt_device(NULL),
	m_h_ds(NULL),
	m_api_is_init(false),
	m_play_thread_id(NULL),
	m_run_status(false),
	m_buffer_len(0),
	m_p_hbuf_array(NULL),
	m_p_buf_num(0),
	m_cb_frame(NULL),
	m_cap_count(0)
{
	memset(&m_std_features, 0, sizeof(m_std_features));
}

CGentlCap::~CGentlCap()
{
	stop_gentl_capture();
	close_gentl();
	if (m_api_is_init)
		CStdLoadCti::m_GCCloseLib();
}

bool8_t CGentlCap::init_sdk_ex()
{
	//��ȡFlex I/O �ɼ����� MVProducerCXP.cti��urlλ��,
	//�ӱ�׼�������� KEY_GENICAM_CTI64_PATH
	char url_name[1024] = {};
	char* t_url = getenv("MV_GENICAM_64");
	if (!t_url)
	{
		printf("no  MV_GENICAM_64\n");
			return false;
	}
	memcpy(url_name, t_url, strlen(t_url));
	strcat(url_name, "MVProducerCXP.cti");

	//����cxplink_gentl.cti�� ����ʼ���ӿ�
	bool8_t b_res = init_sdk(url_name);
	if (b_res)
		printf("load %s ok\n", url_name);
	else
		printf("load %s error\n", url_name);

	return b_res;
}

bool8_t CGentlCap::init_sdk(const char* cti_url)
{
	if (m_api_is_init)
		return true;

	//��̬��ʾ���غ���ָ��
	if (!m_gentl_api.load_sdk(cti_url))
		return false;

	GC_ERROR res = GC_ERR_SUCCESS;

	//��ʼ��gentl��
	res = CStdLoadCti::m_GCInitLib();

	if (gentl_is_success(res))
		m_api_is_init = true;

	return gentl_is_success(res);
}

bool8_t CGentlCap::connect_gentl()
{
	GC_ERROR res = GC_ERR_SUCCESS;
	bool8_t pbChanged = NULL;
	uint64_t iTimeout = 5000;

	if (!m_api_is_init)
	{
		return false;
	}

	// ��system����Ӧcxp ������
	res = CStdLoadCti::m_TLOpen(&m_h_system);
	if (!gentl_is_success(res))
	{
		printf("Open system failed! res [%d]\n", res);
		close_gentl();
		return false;
	}

	// ���½ӿڲ�
	CStdLoadCti::m_TLUpdateInterfaceList(m_h_system, &pbChanged, iTimeout);

	// ��ȡ�ӿڲ�����
	uint32_t num = 0;
	res = CStdLoadCti::m_TLGetNumInterfaces(m_h_system, &num);
	if (num < 1)
	{
		res = GC_ERR_ERROR;
	}

	size_t id_len = 128;
	char  if_id[128] = {};
	res = CStdLoadCti::m_TLGetInterfaceID(m_h_system, 0, if_id, &id_len);//��ȡindexΪ0��interface id;
	if (!gentl_is_success(res))
	{
		printf("get interface id failed!\n");
		close_gentl();
		return false;
	}

	// ��interface�������Ӧһ��cxp-6/12�ɼ�����
	res = CStdLoadCti::m_TLOpenInterface(m_h_system, if_id, &m_h_if);//����id���� ��interface�� ��ȡ���;
	if (!gentl_is_success(res))
	{
		printf("get interface handle failed!\n");
		close_gentl();
		return false;
	}

	// �����豸��;
	pbChanged = NULL;
	iTimeout = 5000;
	CStdLoadCti::m_IFUpdateDeviceList(m_h_if, &pbChanged, iTimeout); 
	if (!gentl_is_success(res))
	{
		printf("get interface handle failed!\n");
		close_gentl();
		return false;
	}

	// ��ȡ�豸������
	UINT32 device_num = 0;
	res = CStdLoadCti::m_IFGetNumDevices(m_h_if, &device_num);
	if (device_num < 1)
	{
		printf("device num < 1\n");
		close_gentl();
		return false;
	}

	//��ȡindexΪ0��device id;
	id_len = 128;
	char  device_id[128] = { 0 };
	res = CStdLoadCti::m_IFGetDeviceID(m_h_if, 0, device_id, &id_len);
	if (!gentl_is_success(res))
	{
		printf("get device id failed!\n");
		close_gentl();
		return false;
	}

	// ��dma�豸��1��device��Ӧ1��remote device
	DEVICE_ACCESS_FLAGS iOpenFlag = DEVICE_ACCESS_CONTROL;
	res = CStdLoadCti::m_IFOpenDevice(m_h_if, device_id, iOpenFlag, &m_h_device);
	if (!gentl_is_success(res))
	{
		printf("open device failed!\n");
		close_gentl();
		return false;
	}

	printf("Open Device: %d\n", res);

	//�����豸�������
	res = CStdLoadCti::m_DevGetPort(m_h_device, &m_h_rmt_device);
	if (!gentl_is_success(res))
	{
		printf("connect device failed!\n");
		close_gentl();
		return false;
	}
	
	printf("Open Device Success. ret:%d \n", res);

	//date stream ����Ҫupdatelist
	UINT32 stream_num = 0;
	res = CStdLoadCti::m_DevGetNumDataStreams(m_h_device, &stream_num);
	if (stream_num < 1)
	{
		printf("stream num < 1\n");
		close_gentl();
		return false;
	}

	//��ȡindexΪ0��data stream id;Ŀǰ�̼�֧��1 data stream-1/2/4link;
	id_len = 128;
	char  ds_id[128] = { 0 };
	res = CStdLoadCti::m_DevGetDataStreamID(m_h_device, 0, ds_id, &id_len);
	if (!gentl_is_success(res))
	{
		printf("get data stream id faild!\n");
		close_gentl();
		return false;
	}

	// �� data stream�����ƿ���
	res = CStdLoadCti::m_DevOpenDataStream(m_h_device, ds_id, &m_h_ds);
	if (!gentl_is_success(res))
	{
		printf("open data stream faild!\n");
		close_gentl();
		return false;
	}
	printf("Open DS: %d\n", res);
	printf("load gentl ok\n");

	return true;
}

bool8_t CGentlCap::close_gentl()
{
	if (!m_api_is_init)
		return false;

	printf("close ds\n");
	if(m_h_ds)
		CStdLoadCti::m_DSClose(m_h_ds);
	m_h_ds = NULL;

	printf("close dev\n");
	if(m_h_device)
		CStdLoadCti::m_DevClose(m_h_device);
	m_h_device = NULL;

	printf("close if\n");
	if (m_h_if)
		CStdLoadCti::m_IFClose(m_h_if);
	m_h_if = NULL;

	printf("close system\n");
	if (m_h_system)
		CStdLoadCti::m_TLClose(m_h_system);
	m_h_system = NULL;

	return true;
}

bool8_t CGentlCap::set_camera_params()
{
	GC_ERROR res = GC_ERR_SUCCESS;

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_rmt_device, "TriggerSelector", "FrameStart");
	if (!gentl_is_success(res))
	{
		printf("set TriggerSelector FrameStart error! Error code is [%d]\n", res);
		return false;
	}

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_rmt_device, "TriggerMode", "Off");
	if (!gentl_is_success(res))
	{
		printf("set TriggerMode Off error! Error code is [%d]\n", res);
		return false;
	}

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_rmt_device, "TriggerSelector", "AcquisitionStart");
	if (!gentl_is_success(res))
	{
		printf("set TriggerSelector AcquisitionStart error! Error code is [%d]\n", res);
		return false;
	}

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_rmt_device, "TriggerMode", "Off");
	if (!gentl_is_success(res))
	{
		printf("set TriggerMode Off error! Error code is [%d]\n", res);
		return false;
	}

	return gentl_is_success(res);
}

bool8_t CGentlCap::set_card_params(card_features_t* in_card_features_ptr)
{
	if (!m_api_is_init)
		return false;
	GC_ERROR res = GC_ERR_SUCCESS;

#if 0 //�ɼ���������ģʽ
	res = CStdLoadCti::m_mw_genapi_set_string(m_h_device, "DeviceTriggerMode", "Software");

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_device, "DeviceExposureTime", "1000"); //1000us�ع�ʱ�䣨���ο�ȣ�

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_device, "DeviceFramePeriod", "5347.59"); // 187֡��֡������ 5347.59 = 1000*1000/187��


#else //�ɼ�����Ӳ����ģʽ
	res = CStdLoadCti::m_mw_genapi_set_string(m_h_device, "DeviceTriggerMode", "Hardware");	//ADDR_trigger_mode

	res = CStdLoadCti::m_mw_genapi_set_integer(m_h_device, "CardioInMask", EXT_IIN11);		//����IO�ӿ�

	res = CStdLoadCti::m_mw_genapi_set_string(m_h_device, "CardioGlitchTime", "1");			//ȥë��1us�� ������set_float������set_string

#endif
	//��ӡ�ɼ�������ģʽ
	char t_str[128] = {};
	size_t str_len = sizeof(t_str);
	res = CStdLoadCti::m_mw_genapi_get_string(m_h_device, "DeviceTriggerMode", t_str, &str_len);
	printf("DeviceTriggerMode: %s\n", t_str);

	return true;
}

bool8_t CGentlCap::start_gentl_capture(mw_ezgentl_frame_callback_t frame_cb, void* p_cb_context, uint32_t buf_count, uint64_t cap_count)
{
	if (!m_api_is_init)//δ��ʼ��
		return false;
	else if (m_run_status)//�ѿ���
		return true;
	else if (buf_count <= 0 || buf_count > 256)
		return false;
	else if (cap_count == 0)
		return false;

	GC_ERROR res = GC_ERR_SUCCESS;

	if (!m_p_hbuf_array)
	{
		m_p_hbuf_array = new(std::nothrow) BUFFER_HANDLE[buf_count];
		if (!m_p_hbuf_array)
			res = GC_ERR_OUT_OF_MEMORY;
	}

	//ע��ɼ��¼�
	EVENT_TYPE iEventID = EVENT_NEW_BUFFER;
	if (gentl_is_success(res))
		res = CStdLoadCti::m_GCRegisterEvent(m_h_ds, iEventID, &m_new_buffer_event);

	//��ȡһ֡ͼ����ڴ��С
	INFO_DATATYPE iType = NULL;
	size_t buffer_len = 0;
	size_t buffer_type_bytes = sizeof(size_t);
	if (gentl_is_success(res))
	{
		res = CStdLoadCti::m_DSGetInfo(m_h_ds, STREAM_INFO_PAYLOAD_SIZE, &iType, &buffer_len, &buffer_type_bytes);
		m_buffer_len = buffer_len;
	}
#ifdef Use_alloc_and_announce_buffer
	//���벢����ע��ɼ��ڴ��;
	void* pPrivate = NULL;
	for (uint32_t i = 0; i < buf_count; i++)
	{
		if (gentl_is_success(res))
			res = CStdLoadCti::m_DSAllocAndAnnounceBuffer(m_h_ds, m_buffer_len, pPrivate, &m_p_hbuf_array[i]);//������ע��, ��ȡ�ɼ������buf�������
	}
	//����GenTL�淶��ʹ��������������ݣ���Ҫ���¼����Ŷӣ��ɼ����ſ��Լ�����仺��
	for (uint32_t i = 0; i < buf_count; i++)
	{
		if (gentl_is_success(res))
			res = CStdLoadCti::m_DSQueueBuffer(m_h_ds, m_p_hbuf_array[i]);
	}
#else
	//�����ڴ�ָ������
	m_pp_user_alloc = new(std::nothrow) char* [buf_count];
	m_p_buf_num = buf_count;
	//�����ڴ�
	for (uint32_t i = 0; i < buf_count; i++)
	{
		m_pp_user_alloc[i] = new(std::nothrow) char[m_buffer_len];
	}

	//ע��buffer, ��ȡ��Ӧ�Ĳɼ�������
	//[��ע]�� ����DSRevokeBuffer���buffer֮�� �û��ſ����ͷ�buffer��
	for (uint32_t i = 0; i < buf_count; i++)
	{
		if (gentl_is_success(res))
			res = CStdLoadCti::m_DSAnnounceBuffer(m_h_ds, m_pp_user_alloc[i], m_buffer_len, NULL, &m_p_hbuf_array[i]);
	}
	//����GenTL�淶��ʹ��������������ݣ���Ҫ���¼����Ŷӣ��ɼ����ſ��Լ�����仺��
	for (uint32_t i = 0; i < buf_count; i++)
	{
		if (gentl_is_success(res))
			res = CStdLoadCti::m_DSQueueBuffer(m_h_ds, m_p_hbuf_array[i]);
	}

#endif

	//�����ɼ��߳�
	if (gentl_is_success(res))
	{
		m_p_buf_num = buf_count;
		CCapAutoLock lock(&m_locker);
		m_cb_frame = frame_cb;
		m_cap_count = cap_count;
		m_run_status = true;
		if (!create_get_thread(p_cb_context))
			res = GC_ERR_ERROR;
	}

	if (gentl_is_success(res))
	{
		//��ʼ
		ACQ_START_FLAGS iStartFlags = ACQ_START_FLAGS_DEFAULT;
		uint64_t iNumToAcquire = -1;
		res = CStdLoadCti::m_DSStartAcquisition(m_h_ds, iStartFlags, iNumToAcquire);
	}
	//�������AcquistionStart
	CStdLoadCti::m_mw_genapi_call_command(m_h_rmt_device, "AcquisitionStart");

	if (!gentl_is_success(res))
	{
		m_run_status = false;
		clear_data();
	}
	return gentl_is_success(res);
}

bool8_t CGentlCap::capture_wait(uint64_t i_time)
{
	printf("wait for thread\n");
	if (m_play_thread_id)
	{
		DWORD ret = WaitForSingleObject(m_play_thread_id, (DWORD)i_time); //INFINITEһֱ�ȴ�
		if (ret == WAIT_OBJECT_0)
		{
			CloseHandle(m_play_thread_id);
			m_play_thread_id = NULL;
		}
		else if (ret == WAIT_TIMEOUT)//��ʱ����
		{
			stop_gentl_capture();
		}
	}
	return true;
}

int CGentlCap::play_get_threader(void* p_cb_context)
{
	//Sleep(100);
	printf("Set play_threader\n");
	GC_ERROR res = GC_ERR_SUCCESS;
	uint64_t i_timeout = 2*1000;//ѭ���ȴ��¼�2s,��ʱ��������
	size_t event_size = sizeof(EVENT_NEW_BUFFER_DATA);
	INFO_DATATYPE type = INFO_DATATYPE_UNKNOWN;
	void*	data_ptr=NULL;
	size_t	data_type_len = sizeof(void*);
	m_event_buffer.BufferHandle = NULL;
	m_event_buffer.pUserPointer = NULL;
	uint32_t get_num = 0;
	uint32_t time = 0;
	while (true)
	{
		if (p_cb_context)//�ص���ͼ����
		{
			if (get_num >= m_cap_count)
				break;
		}
		if(true)
		{
			CCapAutoLock lock(&m_locker);
			if (m_run_status == false)
				break;
		} 
		//��ȡ��ͼ���¼�
		res = CStdLoadCti::m_EventGetData(m_new_buffer_event, &m_event_buffer, &event_size, i_timeout);
		//��ȡ����ָ��
		if (gentl_is_success(res))
		{		
			res = CStdLoadCti::m_DSGetBufferInfo(m_h_ds, m_event_buffer.BufferHandle, BUFFER_INFO_BASE, &type, &data_ptr, &data_type_len);
		}

		//��������
		if (gentl_is_success(res))
		{
#if 1 //��ʾ�ص�
			if(m_cb_frame)
				m_cb_frame(p_cb_context, data_ptr, (uint32_t) m_buffer_len);
			get_num++;

#else //��ʾֱ�Ӳ�ͼ
			//ͼ���ڴ��ַ��data_ptr
			//ͼ��ռ�ô�С��m_buffer_len
			//save_pic(data_ptr, m_buffer_len);//������ʾ��������
			++get_num;
			if (get_num % 10 == 0)
				printf("\n get num=%d", get_num);//������ʾÿ���ʮ��ͼƬ��ӡһ�λ�ȡ��ͼ������
#endif
		}			

		//ʹ�������ݣ���buffer�����Ŷ�;
		if (gentl_is_success(res))
		{
			res = CStdLoadCti::m_DSQueueBuffer(m_h_ds, m_event_buffer.BufferHandle);
		}
	}
	printf("exit get buf thread\n");
	return 0;
}

unsigned __stdcall PlayThreaderWork(LPVOID param)
{
	CGentlCap* player_ptr = (CGentlCap*)param;
	if (player_ptr)
		player_ptr->play_get_threader(player_ptr);
	else
		return -1;
	return 0;
}

bool8_t CGentlCap::create_get_thread()
{
	m_play_thread_id = (HANDLE)_beginthreadex(NULL, 0, &PlayThreaderWork, (void*)this, 0, NULL);
	if (m_play_thread_id == NULL)
		return false;
	else
		return true;
}

bool8_t CGentlCap::create_get_thread(void* p_cb_context)
{
	m_play_thread_id = (HANDLE)_beginthreadex(NULL, 0, &PlayThreaderWork, p_cb_context, 0, NULL);
	if (m_play_thread_id == NULL)
		return false;
	else
		return true;
}


void CGentlCap::clear_data()
{
	printf("clear_data: stop\n");
	ACQ_START_FLAGS iStartFlags = ACQ_QUEUE_INPUT_TO_OUTPUT;
	CStdLoadCti::m_DSStopAcquisition(m_h_ds, iStartFlags);

	//�������AcquistionStop
	CStdLoadCti::m_mw_genapi_call_command(m_h_rmt_device, "AcquisitionStop");

	printf("UnregisterEvent\n");
	CStdLoadCti::m_GCUnregisterEvent(m_h_ds, EVENT_NEW_BUFFER);

	CStdLoadCti::m_DSFlushQueue(m_h_ds, ACQ_QUEUE_ALL_DISCARD);

	printf("clear_data: wait for thread\n");
	if (m_play_thread_id)
	{
		WaitForSingleObject(m_play_thread_id, INFINITE);
		CloseHandle(m_play_thread_id);
		m_play_thread_id = NULL;
	}
	//DSRevokeBuffer���buffer֮�� �û��ſ����ͷ�buffer
	printf("clear_data: revoke\n");
	if (m_p_hbuf_array)
	{
		//���buffer�� �ɼ�����Ӧ�ľ��ֵ���ͷ�
		for (uint32_t i = 0; i < m_p_buf_num; i++)
		{
			if (m_p_hbuf_array[i])
			{
				CStdLoadCti::m_DSRevokeBuffer(m_h_ds, m_p_hbuf_array[i], NULL, NULL);
				m_p_hbuf_array[i] = NULL;
			}
		}
		//���ٲɼ����������
		delete[] m_p_hbuf_array;
		m_p_hbuf_array = NULL;
	}
	m_cb_frame = NULL;
	m_cap_count = 0;
	
	//�����û��ڴ棬����ڴ�������κ������ط��ͷţ�
	if (m_pp_user_alloc)
	{
		for (uint32_t i = 0; i < m_p_buf_num; i++)
		{
			if (m_pp_user_alloc[i])
			{
				delete[] m_pp_user_alloc[i];
				m_pp_user_alloc[i] = NULL;
			}
		}
		delete[] m_pp_user_alloc;
		m_pp_user_alloc = NULL;
	}
}
bool8_t CGentlCap::stop_gentl_capture()
{
	if (!m_api_is_init)
		return false;
	else if (!m_run_status)
		return true;//�ѹر�

	if (true)
	{
		CCapAutoLock lock(&m_locker);
		m_run_status = false;
	}
	clear_data();
	return true;
}

int64_t CGentlCap::getWidth()
{
	GC_ERROR res = GC_ERR_SUCCESS;
	int64_t width = 0;

	res = CStdLoadCti::m_mw_genapi_get_integer(m_h_rmt_device, "Width", &width);
	if (!gentl_is_success(res))
	{
		printf("get width failed! res is [%d]\n", res);
		return 0;
	}

	return width;
}

int64_t CGentlCap::getHeight()
{
	GC_ERROR res = GC_ERR_SUCCESS;
	int64_t height = 0;

	res = CStdLoadCti::m_mw_genapi_get_integer(m_h_rmt_device, "Height", &height);
	if (!gentl_is_success(res))
	{
		printf("get height failed! res is [%d]\n", res);
		return 0;
	}

	return height;
}