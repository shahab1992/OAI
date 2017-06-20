/****************************************************************************/
/**
 * @file  fjt_turbo_config.h
 * @brief Turbo decoding config
 * 
 * @author      H.naoi
 * @date        2016.07.30
 * @version     1.0
 * @copyright   All Rights Reserved, Copyright (C) FUJITSU LIMITED, 2016
 * 
 * @par 変更履歴:
 * - 2016.07.30: H.naoi  : 新規作成
 */
/****************************************************************************/
#ifndef __FJT_TURBO_CONFIG_H__
#define __FJT_TURBO_CONFIG_H__

/****************************************************************************/
/* log switch(default : #if 1)                          */
/****************************************************************************/

#if 0
#define FJT_DBG_TRACE_ENABLE			(0)
#else
#define FJT_DBG_TRACE_ENABLE			(1)
#endif

#if 1
#define FJT_DBG_ILG_ENABLE				(1)
#else
#define FJT_DBG_ILG_ENABLE				(0)
#endif

#if 1
#define FJT_DBG_STATISTICS_ENABLE		(1)
#else
#define FJT_DBG_STATISTICS_ENABLE		(0)
#endif

#if 1
#define FJT_STATISTIC_OUTPUT_LIMIT		(1)
#else
#define FJT_STATISTIC_OUTPUT_LIMIT		(0)
#endif

#if 1
#define FJT_DBG_PRINT_ENABLE			(0)
#else
#define FJT_DBG_PRINT_ENABLE			(1)
#endif

/****************************************************************************/
/* parameter check switch (default : #if 1)           */
/****************************************************************************/

#if 1
#define FJT_DBG_PARAM_CHK				(0)
#else
#define FJT_DBG_PARAM_CHK				(1)
#endif

#if 1
#define FJT_DBG_ASSERT_ENABLE			(0)
#else
#define FJT_DBG_ASSERT_ENABLE			(1)
#endif

#endif /* __FJT_TURBO_CONFIG_H__ */


/****************************************************************************/
/* 16bit->8bit likelihood conversion function switch(default : #if 1)        */
/****************************************************************************/
#if 1
#define FJT_REQ_CONV_16TO8_SIFT				(1)
#else
#define FJT_REQ_CONV_16TO8_SIFT				(0)
#endif



