#pragma once
#include "load_gentl_cti.h"
#include "cxp_standard.h"

#define DMA_BUF_NUM 256         //ָ���ɼ����ɰ󶨻���������(��ǰ���°�ʵ�ʿɲ�����500)



typedef void (*mw_ezgentl_frame_callback_t)(void* p_cb_contex, void* p_base, uint32_t size);

class CGentlCap
{
public:
    CGentlCap();
    virtual ~CGentlCap();

    //-------------0,��ʼ��sdk�ӿ�-----------------------------------------------------------------------------
    bool8_t init_sdk_ex();                  //����1���Զ�������cti�ļ�������GenICam��׼λ���ҵ��⣩
    bool8_t init_sdk(const char* cti_url);  //����2��ֱ�Ӽ���cti���ļ���url

    //-------------1,���ӡ��ر�gentl�豸-----------------------------------------------------------------------
    //���Ӳ���ʼ��GenTL system�㡢interface�㡢device�㡢remote device�㡢data stream��;
    bool8_t connect_gentl();
    //�ر�SDK
    bool8_t close_gentl();

    //-------------2,���òɼ��������--------------------------------------------------------------------------
        //�������������ģʽ��
        bool8_t set_camera_params();
        //���á���ȡ�ɼ�����Ϣ��device��
        //���òɼ�������
        bool8_t set_card_params(card_features_t* in_card_features_ptr);
		//��ȡ��
		int64_t getWidth();
		//��ȡ��
		int64_t getHeight();

    //-------------3,��ʼ��ֹͣ�ɼ�-----------------------------------------------------------------------------
	/*
	* �����ɼ�,
	* @param[in] frame_cb           capture callback function
	* @param[in] p_cb_context       callback parameter
	* @param[in] buf_count          alloc buffer num
	* @param[in] cap_count          �ɼ�ͼƬ������ -1������������
	*/
	bool8_t start_gentl_capture(mw_ezgentl_frame_callback_t frame_cb, void* p_cb_context, uint32_t buf_count, uint64_t cap_count = -1);

	bool8_t capture_wait(uint64_t i_time);

    //ֹͣ�ɼ�
    bool8_t stop_gentl_capture();
    //��ȡ���ݵ��߳�
    int play_get_threader(void* p_cb_context=NULL);
 
private:

    bool8_t create_get_thread();
    bool8_t create_get_thread(void* p_cb_context);

    void    clear_data();

public:
    cxp_std_features_t  m_std_features;//�����׼����
    TL_HANDLE	m_h_system;         //GenTLϵͳ����
    IF_HANDLE	m_h_if;             //Interface����
    DEV_HANDLE  m_h_device;         //Device����
    PORT_HANDLE m_h_rmt_device;     //remote device(�����)���
    DS_HANDLE	m_h_ds;             //��Ƶ������

    mw_ezgentl_frame_callback_t     m_cb_frame;		//�ص�����
    uint64_t                        m_cap_count;	//��ȡ����

    CStdLoadCti m_gentl_api; //�ӿ����ʼ��
private:  
    bool8_t     m_api_is_init;

    card_features_t     m_card_features;
    cxp_std_features_t  in_data;

    EVENT_HANDLE            m_new_buffer_event;             //ͼ��ɼ��¼�
    EVENT_NEW_BUFFER_DATA   m_event_buffer;                 //�ɼ��¼���Ϣ
    //BUFFER_HANDLE           m_buffer_array[DMA_BUF_NUM];    //�ɼ�ͼ�񻺴�������(�ɰ棬������)
    BUFFER_HANDLE*          m_p_hbuf_array;                 //�ɼ�ͼ�񻺴�������ָ�룬���ڴ��
    char**                  m_pp_user_alloc;                //�û������ڴ棬
    uint32_t                m_p_buf_num;         //����Ļ������,����С��DMA_BUF_NUM��
    size_t                  m_buffer_len;        //ÿ֡ͼ���С

    HANDLE	m_play_thread_id;
    bool8_t m_run_status;
    CCapCritical		m_locker;		//�Զ���
};