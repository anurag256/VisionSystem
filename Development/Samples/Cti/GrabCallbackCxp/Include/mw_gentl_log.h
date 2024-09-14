#pragma once
#include "GenTL.h"

#if !defined(GC_IMPORT_EXPORT) || !defined(GC_CALLTYPE)
#error Need include GenTL.h before mw_gentl_log.h
#endif

#if !defined(MW_LOG_API)
#define MW_LOG_API GC_IMPORT_EXPORT GC_ERROR GC_CALLTYPE
#define MW_LOG_API_P(function) typedef GC_ERROR( GC_CALLTYPE *function )
#endif

#ifdef __cplusplus
extern "C" {
    namespace GenTL {
        namespace mw_gentl_log {
#endif
            enum _m_log_level_
            {
                MLOG_STOP = 0,           //��д��־
                MLOG_DEBUG = 1,		    //д������־
                MLOG_INFO = 2,			//д������־
                MLOG_WARN = 3,		    //ֻд�澯��Ϣ
                MLOG_ERROR = 4,		    //ֻд������Ϣ
                MLOG_ALL = 99,		    //������Ϣ��д��־
            };
            typedef int32_t m_log_level_t;

            enum _m_log_language_
            {
                MLAN_EN = 0,
                MLAN_CHN = 1,

                MLAN_ALL = 0x1000,
            };
            typedef int32_t m_log_language_t;

            //��ʼģ��
            //log_url: log��־�ļ�Ŀ¼��ַ��
            MW_LOG_API mw_init_log(const char* log_folder, m_log_level_t log_level);
            MW_LOG_API mw_uninit_log();

            //��������,
            MW_LOG_API mw_set_language(m_log_language_t language);


            MW_LOG_API_P(p_mw_init_log)(const char* log_folder, m_log_level_t log_level);
            MW_LOG_API_P(p_mw_uninit_log)();
            MW_LOG_API_P(p_mw_set_language)(m_log_language_t language);
#ifdef __cplusplus
        }
    }
}
#endif
