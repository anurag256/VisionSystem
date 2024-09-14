#pragma once
#include "GenTL.h"

//Զ���豸��������Ĺ��ܼĴ���
//��Ա������std::string�ȱ�׼�����ͣ����캯����ʹ��memset��ʼ��Ϊ0,���ƻ�std��
typedef struct _cxp_std_features_
{
	//���ֻ����Ϣ����genapi�ӿڶ�ȡ
	char device_vendor_name[128];		//�����˾,��"Adimec" 
	char device_model_name[128];		//����ͺţ���"Q-12A180-Fm/CXP-6"
	char device_serial_number[128];		//������кţ���"100001"
	//�����д�������genapi�ӿڶ�ȡ
	int64_t width;		//��	
	int64_t height;		//��
	int64_t format;		//ͼ���ʽ��ֵ
	char format_str[128];	//ͼ���ʽ���ַ����棨������ǿ������"Mono8" ��Ӧ��ֵ���е��������cxp��׼���е��������PFNC��׼��ֱ��ʹ�����Ƽ�����ǿ
	int64_t offset_x;	//ROI OffsetX ƫ��
	int64_t offset_y;	//ROI OffsetY ƫ��

	//...
}cxp_std_features_t;

//�����ʽ�Ĵ���ʹ��ֵ;
typedef enum _command_gentl_
{
	CMD_GENTL_START = 1
}command_gentl_t;

//�Զ����������
typedef enum _typedef_camera_
{
	ADIMEC_Q_12A180 = 0,
	DAHUA_AX5A22MX050 = 1,
	HAIKANG_CXP12 = 2,
	BASLER =3
}typedef_camera_t;

typedef enum _flex_io_mask_
{
	IIN11 = 0,
	IIN12 = 1,
	IIN13 = 2,
	IIN14 = 3,
	EXT_IIN11 = 4,
	EXT_IIN12 = 5,
	EXT_IIN13 = 6,
	EXT_IIN14 = 7,
	DIN11 = 8,
	DIN12 = 9,
	TTLIO11 = 10,
	TTLIO12 = 11
}flex_io_mask_t;

//�ɼ������ܼĴ���
typedef struct _card_features_
{
	uint32_t mode;					//����ģʽ��//1������ģʽ   2��Ӳ����ģʽ

	//��������
	double pulse_width;				//�ع�ʱ��, unit: 1us
	double pulse_period;			//��������, unit: 1us
	uint32_t pulse_number;			//���ε��ʹ�������������������Ϊ0������һֱѭ������;
	command_gentl_t load_desc_en;	//���1��ʹ���������pulse_number�����塣pulse_numberΪ0ʱ��ʹ������Ĭ��ѭ���Զ�������

	//Ӳ��������
	
	flex_io_mask_t	io_mask;		//�������ѡ��;
	double glitch;					//ȥë�̣���λus��΢��,����0.1us��;�������1.0us
}card_features_t;



static inline bool8_t gentl_is_success(GC_ERROR status)
{
	return (GC_ERR_SUCCESS == status);
}

////�Զ���
class CCapCritical
{
public:
	CCapCritical()
	{
		InitializeCriticalSection(&m_cs);
	}

	virtual ~CCapCritical()
	{
		DeleteCriticalSection(&m_cs);
	}

	void Lock()
	{
		EnterCriticalSection(&m_cs);
	}

	void Unlock()
	{
		LeaveCriticalSection(&m_cs);
	}

	CRITICAL_SECTION m_cs;
};

class CCapAutoLock//�Զ���
{
public:
	CCapCritical* m_csec;
	CCapAutoLock(CCapCritical* critsec)
	{
		m_csec = critsec;
		m_csec->Lock();
	}
	virtual ~CCapAutoLock()
	{
		m_csec->Unlock();
	}
};