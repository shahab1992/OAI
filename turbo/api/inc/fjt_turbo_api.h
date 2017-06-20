/****************************************************************************/
/**
 * @file  fjt_turbo_api.h
 * @brief Turbo decoding API
 */
/****************************************************************************/
#ifndef __FJT_TURBO_API_H__
#define __FJT_TURBO_API_H__

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/****************************************************************************/
/* macro                                                                    */
/****************************************************************************/
#define FJT_FLAG_ON			(1)
#define FJT_FLAG_OFF		(0)
#define FJT_API_OK			(0)		/* API OK								*/
#define FJT_API_NG			(1)		/* API NG								*/
#define FJT_API_INIT_ERR	(2)		/* API Initialization NG				*/
#define FJT_API_PARAM_ERR	(3)		/* API Parameters check NG				*/
#define FJT_API_EMPTY		(15)	/* Turbo decoding API Incomplete		*/
#define FJT_API_CRC_OK		(0)		/* Turbo decoding CRC OK				*/
#define FJT_API_CRC_NG		(1)		/* Turbo decoding CRC NG				*/

#define FJT_MAX_STREAM		(2)		/* Max Stream number					*/
#define FJT_MIN_CB_NUM		(1)		/* Min CodeBlock number					*/
#define FJT_MAX_CB_NUM		(16)	/* Min CodeBlock number					*/
#define FJT_MIN_TB_SIZE		(16)	/* Min TransportBlock size(bit)			*/
#define FJT_MAX_TB_SIZE		(97896)	/* Max TransportBlock size(bit)			*/
#define FJT_MIN_CB_SIZE		(40)	/* Min CodeBlock size(bit)				*/
#define FJT_MAX_CB_SIZE		(6144)	/* Max CodeBlock size(bit)				*/
#define FJT_MAX_ITR_NUM		(8)		/* Max Iteration times					*/
#define FJT_MAX_SEQ_NUM		(65535)	/* Max Sequence number					*/

#define FJT_STREAM0			(0)		/* stream#0								*/
#define FJT_STREAM1			(1)		/* stream#1								*/

#define FJT_API_WATCH_NONSYNC	(0)	/* Turbo decoding API Asynchronous mode */
#define FJT_API_WATCH_SYNC		(1)	/* Turbo decoding API Synchronous mode  */

#define FJT_CB_DT_SIZE		(32768)	/* CB Buffer size			*/
#define FJT_TB_INFO_SIZE	(32)	/* TB Buffer size			*/
#define FJT_TB_DT_SIZE		(16384-FJT_TB_INFO_SIZE) /* TB Buffer size */

/****************************************************************************/
/* typedef                                                                  */
/****************************************************************************/

typedef struct tag_fjt_fpga_indt {
	uint8_t data[FJT_MAX_CB_NUM][FJT_CB_DT_SIZE];
} fjt_fpga_indt_t;

typedef struct tag_fjt_turbodec_inbuf {
	fjt_fpga_indt_t tb[FJT_MAX_STREAM];
} fjt_turbodec_inbuf_t;

typedef struct tag_fjt_fpga_outdt_info {
	volatile uint32_t	ODT_TB_SIZE;
	volatile uint32_t	ODT_TB_ID;
	volatile uint32_t	ODT_CB_SEG_NUM;
	volatile uint32_t	ODT_CRC_RES;
	volatile uint32_t	ODT_CB_INTIME;
	volatile uint32_t	ODT_ITR_NUM;
	volatile uint32_t	ODT_FIL_BIT;
	volatile uint32_t	ODT_STATUS;
}fjt_fpga_outinfo_t;

typedef struct tag_fjt_fpga_outdt {
	fjt_fpga_outinfo_t	info;
	uint8_t 			decoded_data[FJT_TB_DT_SIZE];
} fjt_fpga_outdt_t;

typedef struct tag_fjt_turbodec_outbuf {
	fjt_fpga_outdt_t tb[FJT_MAX_STREAM];
} fjt_turbodec_outbuf_t;

typedef struct tag_fjt_turbodec_inparam {
	uint8_t		stream_number;		/* stream number								*/
	uint32_t	tb_size;			/* transport block size							*/
	uint8_t		cb_seg_num;			/* code block segment number					*/
	uint8_t		cb_number;			/* code block number							*/
	uint16_t	cb_size;			/* code block size								*/
	uint8_t		filler_bit_num;		/* filler bit number							*/
	uint8_t		max_iteration_num;	/* max iteration number							*/
	uint16_t	seq_num;			/* Sequence number								*/
	void		*inbuf_p;			/* Turbo decoding data pointer					*/
} fjt_turbodec_inparam_t;

typedef struct tag_fjt_turbodec_outparam {
	uint16_t	cb_crc_result;	/* code block CRC check result(0=OK)					*/
	uint8_t		tb_crc_result;	/* transport block CRC check result(0=OK,1=NG)			*/
	uint8_t		iteration_num;	/* iteration times										*/
	uint16_t	seq_num;		/* Sequence number										*/
	void		*outbuf_p;		/* turbo decoding data pointer							*/
} fjt_turbodec_outparam_t;


/****************************************************************************/
/* inline                                                                   */
/****************************************************************************/

/****************************************************************************/
/* prototype                                                                */
/****************************************************************************/

#endif /* __FJT_TURBO_API_H__ */
