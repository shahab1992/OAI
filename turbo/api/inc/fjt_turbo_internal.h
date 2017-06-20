/****************************************************************************/
/**
 * @file  fjt_turbo_internal.h
 * @brief Turbo decoding API
 * 
 */
/****************************************************************************/
#ifndef __FJT_TURBO_INTERNAL_H__
#define __FJT_TURBO_INTERNAL_H__

#include "fjt_turbo_config.h"
#include "fjt_turbo_api.h"
#include "../../driver/fjt_turbo_drv_user.h"

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>  
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#ifndef RUN_PGR	
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#endif

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/****************************************************************************/
/* macro                                                                    */
/****************************************************************************/
#define FJT_RET_OK		(0)
#define FJT_RET_ERR		(1)
#define FJT_NULL		(0)
#define FJT_FAILED		(-1)

/*	API/CMD common	*/
#define FJT_FPGA_UIO_NUM			(2)		/*	FPGA UIO nunber		*/
#define FJT_FPGA_UIOINFO_REGISTER	(0)		/*	UIO REGISTER 		*/
#define FJT_FPGA_UIOINFO_RAM		(1)		/*	UIO RAM				*/
#define FJT_FPGA_UIOINFO_DMABUF		(2)		/*	UIO DMA buffer		*/
#define FJT_FPGA_REGISTER_AREA_SIZE	(0x00200000)	/*	FPGA Register size	*/
#define FJT_FPGA_RAM_AREA_SIZE		(0x00200000)	/*	FPGA RAM size		*/
#define FJT_FPGA_AREA_SIZE			(FJT_FPGA_REGISTER_AREA_SIZE + FJT_FPGA_RAM_AREA_SIZE)
#define FJT_FPGA_REGADDR_PHYS_BASE	(0x00000000)
#define FJT_FPGA_RAMADDR_PHYS_BASE	(FJT_FPGA_REGADDR_PHYS_BASE + FJT_FPGA_REGISTER_AREA_SIZE)
#define FJT_DMAMEM_AREA_SIZE		(0x00200000)	/* DMA sizw		*/
#define FJT_IST_CB_TRG_ON			(0x00000001)
#define FJT_FPGA_CB_SIZE_MAX		(0x1FFF)

/*	API Only	*/
#define FJT_INIT_CHK_RETRY_NUM		(1000)			/*	Initialization API number of retries	*/
#define FJT_IST_DMA_TRG_ON			(0x00000001)	/*	Transmission to DMA trigger 			*/
#define FJT_REQ_SEQ_MASK			(0x0000FFFF)	/*	Sequence number effective range			*/
#define FJT_TB_CRC_EXP				(0x00010000)	/*	TB CRC Check result expected value		*/
#define FJT_WATCH_EVENT_MAX			(2)				/*	Number of received events				*/



#if defined(__x86_64__)
#define mb()	 asm volatile("lfence" ::: "memory")
#define rmb()	 mb()
#define wmb()	 asm volatile("" ::: "memory")
#define wc_wmb() asm volatile("sfence" ::: "memory")
#else /* __x86_64__ */
#warning No architecture specific defines found.  Using generic implementation.
#define mb()	 asm volatile("" ::: "memory")
#define rmb()	 mb()
#define wmb()	 mb()
#define wc_wmb() wmb()
#endif /* __x86_64__ */


/*	Common code 	*/
#define	FJT_RET_ERR1	(1)
#define	FJT_RET_ERR2	(2)
#define	FJT_RET_ERR3	(3)
#define	FJT_RET_ERR4	(4)
#define	FJT_RET_ERR5	(5)
#define	FJT_RET_ERR6	(6)
#define	FJT_RET_ERR7	(7)
#define	FJT_RET_ERR8	(8)
#define	FJT_RET_ERR9	(9)
#define	FJT_RET_ERR10	(10)


/* Log related */
#define FJT_LOG_MAX_PATH		(64)
#define FJT_LOG_ON				(1)
#define FJT_LOG_OFF				(0)
#define FJT_LOG_CONF_LINENUM	(3)
#define FJT_LOG_LENGTH			(512)
#define FJT_CTIME_LENGTH		(32)
#define FJT_LOG_MAX_FILESIZE	(0x10000000)	/*	256Mbyte	*/

typedef enum {
	LOG_FILE_TRACEILG = 0,
	LOG_FILE_STATISTICS,
	LOG_FTYPE_NUM
} fjt_logftype_e;

typedef enum {
	LOG_TRACE = 0,
	LOG_ILG,
	LOG_STATISTICS,
	LOG_TYPE_NUM
} fjt_logtype_e;

typedef enum {
	LOG_LEVEL0 = 0,
	LOG_LEVEL1,
	LOG_LEVEL_NUM
} fjt_loglevel_e;

typedef enum {
	API_INIT = 0,
	API_REQ,
	API_WAIT,
	API_GET,
	STATISTICS_CAT_NUM
} fjt_statistics_category_e;

typedef enum {
	TOTAL = 0,
	RET0,
	RET1,
	RET2,
	RET3,
	RET4,
	INF0,
	INF1,
	STATISTICS_KIND_NUM
} fjt_statistics_kind_e;

/****************************************************************************/
/* typedef(FPAG)                                                            */
/****************************************************************************/
typedef struct tag_fjt_fpga_com {
	volatile uint32_t			COM_VERSION;
	volatile uint32_t			rsv0[7];
	volatile uint32_t			COM_SCP;
	volatile uint32_t			rsv1[55];
	volatile uint32_t			COM_STAT;
	volatile uint32_t			rsv2[7];
	volatile uint32_t			COM_IF_STAT;
	volatile uint32_t			rsv3[7];
	volatile uint32_t			COM_IF_STAT_COM;
	volatile uint32_t			rsv4[7];
	volatile uint32_t			COM_IF_STAT0;
	volatile uint32_t			rsv5[7];
	volatile uint32_t			COM_IF_STAT1;
	volatile uint32_t			rsv6[31];
	volatile uint32_t			COM_RCV_STAT0;
	volatile uint32_t			rsv7[7];
	volatile uint32_t			COM_RCV_STAT1;
	volatile uint32_t			rsv8[7];
	volatile uint32_t			COM_DECSTAT0;
	volatile uint32_t			rsv9[7];
	volatile uint32_t			COM_DECSTAT1;
	volatile uint32_t			rsv10[103];
	volatile uint32_t			COM_SW_RST;
	volatile uint32_t			rsv11[63];
	volatile uint32_t			COM_TO_DET_TIME;
	volatile uint32_t			rsv12[191];
	volatile uint32_t			COM_CB_MAXINTIME0;
	volatile uint32_t			rsv13[7];
	volatile uint32_t			COM_CB_MAXINTIME1;
	volatile uint32_t			rsv14[7];
	volatile uint32_t			COM_CB_MAXINVALTIME0;
	volatile uint32_t			rsv15[7];
	volatile uint32_t			COM_CB_MAXINVALTIME1;
	volatile uint32_t			rsv16[7];
	volatile uint32_t			COM_CB_MININVALTIME0;
	volatile uint32_t			rsv17[7];
	volatile uint32_t			COM_CB_MININVALTIME1;
	volatile uint32_t			rsv18[87];
	volatile uint32_t			COM_IN_CNT0;
	volatile uint32_t			rsv19[7];
	volatile uint32_t			COM_IN_CNT1;
	volatile uint32_t			rsv20[7];
	volatile uint32_t			COM_OUT_CNT0;
	volatile uint32_t			rsv21[7];
	volatile uint32_t			COM_OUT_CNT1;
	volatile uint32_t			rsv22[7];
	volatile uint32_t			COM_DISC_CNT0;
	volatile uint32_t			rsv23[7];
	volatile uint32_t			COM_DISC_CNT1;
	volatile uint32_t			rsv24[279];
	volatile uint32_t			COM_TEST_EN;
	volatile uint32_t			rsv25[1];
	volatile uint32_t			COM_MSI_TEST_DATA;
	volatile uint32_t			rsv26[1];
	volatile uint32_t			COM_MSI_TRG;
	volatile uint32_t			rsv27[15419];
} fjt_fpga_com_t;

typedef struct tag_fjt_fpga_outset {
	volatile uint32_t			OST_SRC_TB_L;
	volatile uint32_t			rsv0[1];
	volatile uint32_t			OST_SRC_TB_H;
	volatile uint32_t			rsv1[1];
	volatile uint32_t			OST_DEST_TB_L;
	volatile uint32_t			rsv2[1];
	volatile uint32_t			OST_DEST_TB_H;
	volatile uint32_t			rsv3[1];
	volatile uint32_t			OST_TTB_SIZE;
	volatile uint32_t			rsv4[1];
	volatile uint32_t			OST_TB_TRG;
	volatile uint32_t			rsv5[53];
	volatile uint32_t			OST_SRC_INT_L;
	volatile uint32_t			rsv6[1];
	volatile uint32_t			OST_SRC_INT_H;
	volatile uint32_t			rsv7[1];
	volatile uint32_t			OST_DEST_INT_L;
	volatile uint32_t			rsv8[1];
	volatile uint32_t			OST_DEST_INT_H;
	volatile uint32_t			rsv9[16313];
} fjt_fpga_outset_t;

typedef struct tag_fjt_fpga_inset_dma {
	volatile uint32_t			IST_SRC_CB_L;
	volatile uint32_t			rsv0[1];
	volatile uint32_t			IST_SRC_CB_H;
	volatile uint32_t			rsv1[1];
	volatile uint32_t			IST_DEST_CB_L;
	volatile uint32_t			rsv2[1];
	volatile uint32_t			IST_DEST_CB_H;
	volatile uint32_t			rsv3[1];
	volatile uint32_t			IST_CB_SIZE;
	volatile uint32_t			rsv4[1];
	volatile uint32_t			IST_CB_TRG;
	volatile uint32_t			rsv5[5];
} fjt_fpga_inset_dma_t;

typedef struct tag_fjt_fpga_inset {
	volatile uint32_t			IST_TB_SIZE;
	volatile uint32_t			rsv0[1];
	volatile uint32_t			IST_TB_ID;
	volatile uint32_t			rsv1[1];
	volatile uint32_t			IST_CB_SEG_NUM;
	volatile uint32_t			rsv2[1];
	volatile uint32_t			IST_ITR_NUM;
	volatile uint32_t			rsv3[1];
	volatile uint32_t			IST_FIL_BIT;
	volatile uint32_t			rsv4[1];
	volatile uint32_t			IST_SN;
	volatile uint32_t			rsv5[1];
	volatile uint32_t			IST_DMA_TRG;
	volatile uint32_t			rsv6[1011];
	fjt_fpga_inset_dma_t		dma[FJT_MAX_CB_NUM];
	volatile uint32_t			rsv7[15104];
} fjt_fpga_inset_t;

typedef struct tag_fjt_fpga_reg {
	fjt_fpga_com_t		com;
	uint8_t				reserve0[256*1024];
	fjt_fpga_outset_t	outset[FJT_MAX_STREAM];
	fjt_fpga_inset_t	inset[FJT_MAX_STREAM];
	uint8_t				reserve1[1472*1024];
} fjt_fpga_reg_t;

typedef struct tag_fjt_fpga_mem {
	fjt_turbodec_outbuf_t	outdt;
	uint8_t					reserve[992*1024];
	fjt_turbodec_inbuf_t	indt;
} fjt_fpga_mem_t;

typedef struct tag_fjt_fpga_map {
	fjt_fpga_reg_t		fpga_reg;
	fjt_fpga_mem_t		fpga_mem;
} fjt_fpga_map_t;


/****************************************************************************/
/* typedef(UIO)                                                             */
/****************************************************************************/
typedef struct tag_fjt_fpga_uioinfo {
	void *pcie_addr[FJT_FPGA_UIO_NUM];
	void *pcie_addr_phys[FJT_FPGA_UIO_NUM];
} fjt_fpga_uioinfo_t;

typedef struct tag_fjt_dmabuf_uioinfo {
	void *dma_addr;
	void *dma_addr_phys;
} fjt_dmabuf_uioinfo_t;

typedef struct tag_fjt_msi_info {
	int				epollfd[FJT_MAX_STREAM];
	fjt_eventfd_t	eventfd;
} fjt_msi_info_t;

/****************************************************************************/
/* typedef(log)                                                             */
/****************************************************************************/
typedef struct tag_fjt_logfinfo {
	char		logfile_name[FJT_LOG_MAX_PATH];
	FILE		*fp;
} fjt_logfinfo_t;
typedef struct tag_fjt_loginfo {
	uint32_t		log_sw[LOG_TYPE_NUM];
	fjt_logfinfo_t	finfo[LOG_FTYPE_NUM];
} fjt_loginfo_t;
typedef struct tag_fjt_log {
	fjt_loginfo_t	info[FJT_MAX_STREAM];
} fjt_log_t;

typedef struct tag_fjt_stat_cnt {
	_Atomic int	cnt[STATISTICS_CAT_NUM][STATISTICS_KIND_NUM];
}fjt_stat_cnt_t;

typedef struct tag_fjt_stat_info {
	fjt_stat_cnt_t	info[FJT_MAX_STREAM];
}fjt_stat_info_t;


/****************************************************************************/
/* prototype                                                                */
/****************************************************************************/
/*	common module	*/
void fjt_turbo_loginit();
void fjt_turbo_logout(const char *file, const char *func, int line,  int category, uint8_t stream, int level, char *format, ...);
void fjt_turbo_statistics_logout(uint8_t stream, int category, int kind);
uint8_t fjt_open_fpga_uiodev();
uint8_t fjt_get_fpga_uiodev(fjt_fpga_uioinfo_t *fpga_uioinfo);
uint8_t fjt_close_fpga_uiodev();
uint8_t fjt_open_dmabuf_uiodev();
uint8_t fjt_get_dmabuf_uiodev(fjt_dmabuf_uioinfo_t *dmabuf_uioinfo);
uint8_t fjt_close_dmabuf_uiodev();
uint8_t fjt_open_msi_eventfd();
uint8_t fjt_get_msi_eventfd(fjt_msi_info_t *msi_info_in);
uint8_t fjt_close_msi_eventfd();
uint8_t fjt_kick_fpga_dma_read(uint8_t stream_num, uint8_t cb_num, uint64_t src_addr_virt, uint64_t dst_addr_virt, uint32_t size);

/*	API	*/
uint8_t fjt_api_turbodec_init();
uint8_t fjt_api_turbodec_req(fjt_turbodec_inparam_t *param_p);
uint8_t fjt_api_turbodec_end_wait(uint8_t mode, uint8_t stream_num, uint32_t *status);
uint8_t fjt_api_turbodec_get_result(uint8_t stream_num, fjt_turbodec_outparam_t *param);

static inline uint8_t cb_to_index(uint8_t cb_num);
static inline unsigned long long rdtscp();
/****************************************************************************/
/* inline                                                                   */
/****************************************************************************/
static inline uint8_t cb_to_index(uint8_t cb_num){
	
	/*	cb_num	index	*/
	/*	0		0		*/
	/*	1		2		*/
	/*	2		4		*/
	/*	3		6		*/
	/*	4		8		*/
	/*	5		10		*/
	/*	6		12		*/
	/*	7		14		*/
	/*	8		1		*/
	/*	9		3		*/
	/*	10		5		*/
	/*	11		7		*/
	/*	12		9		*/
	/*	13		11		*/
	/*	14		13		*/
	/*	15		15		*/

	return ( (cb_num & 0x07)*2 + ((cb_num & 0x08) >> 3));
}

/* CPUcycle					*/
#if defined(__x86_64__)
static inline unsigned long long rdtscp() {
    unsigned int hi = 0;
    unsigned int lo = 0;
    
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
    return (unsigned long long)lo | ((unsigned long long)hi << 32);
}
#else /* __x86_64__ */
static inline unsigned long long rdtscp() {	/*pgr0306*/
	return 0;
}
#endif /* __x86_64__ */


/****************************************************************************/
/* log macro                                                          */
/****************************************************************************/

#if ((FJT_DBG_TRACE_ENABLE == 1)||(FJT_DBG_ILG_ENABLE == 1)||(FJT_DBG_STATISTICS_ENABLE == 1))
#define FJT_LOG_INIT()	fjt_turbo_loginit()
#else
#define FJT_LOG_INIT()
#endif

#if	(FJT_DBG_TRACE_ENABLE == 1 )
#define FJT_TRACE_LOG(stream, level, str, args...)		fjt_turbo_logout(__FILE__, __FUNCTION__, __LINE__, (int)LOG_TRACE, stream, (int)level, str, ##args)
#else
#define FJT_TRACE_LOG(stream, level, str, args...)
#endif

#if	(FJT_DBG_ILG_ENABLE == 1 )
#define FJT_ILG_LOG(stream, level, str, args...)		fjt_turbo_logout(__FILE__, __FUNCTION__, __LINE__, (int)LOG_ILG  , stream, (int)level, str, ##args)
#else
#define FJT_ILG_LOG(stream, level, str, args...)
#endif

#if	(FJT_DBG_STATISTICS_ENABLE == 1 )
#define FJT_STATISTICS_LOG(stream, category, kind)		fjt_turbo_statistics_logout(stream, (int)category, (int)kind)
#else
#define FJT_STATISTICS_LOG(stream, category, kind)
#endif

/****************************************************************************/
/* param check macro,inline                                                 */
/****************************************************************************/
#if (FJT_DBG_ASSERT_ENABLE == 1)
#define FJT_ASSERT()														\
	do{																		\
		printf("Assertion failed: file %s,line %d\n",__FILE__, __LINE__);	\
		exit(EXIT_FAILURE);													\
	} while(0)
#else	/* FJT_DBG_ASSERT_ENABLE */
#define FJT_ASSERT()
#endif	/* FJT_DBG_ASSERT_ENABLE */

#if	(FJT_DBG_PARAM_CHK == 1 )
#define FJT_CHECK_RETURN_ERR(expression, code)								\
	do { 																	\
		if(expression == FJT_NULL) {										\
			FJT_ASSERT();													\
			return code;													\
		}																	\
	} while(0)
#define FJT_CHECK_RETURN(expression)										\
	do { 																	\
		if(expression == FJT_NULL) {										\
			FJT_ASSERT();													\
			return;															\
		}																	\
	} while(0)
#define FJT_CHECK_FREE_RETURN_ERR(expression, code, stream)					\
	do { 																	\
		if(expression) {													\
			FJT_ILG_LOG(stream, LOG_LEVEL1, "param-err: '%s'", #expression);\
			FJT_TRACE_LOG(stream, LOG_LEVEL1, "END(ERR)");					\
			FJT_ASSERT();													\
			return code;													\
		}																	\
	} while(0)
#define FJT_CHECK_FREE_API(expression, code, stream, category, kind)		\
	do { 																	\
		if(expression) {													\
			FJT_ILG_LOG(stream, LOG_LEVEL1, "param-err: '%s'", #expression);\
			FJT_STATISTICS_LOG(stream, category, kind);						\
			FJT_TRACE_LOG(stream, LOG_LEVEL0, "END(ERR)");					\
			FJT_ASSERT();													\
			return code;													\
		}																	\
	} while(0)
#else	/* FJT_DBG_PARAM_CHK */
#define FJT_CHECK_RETURN_ERR(expression, code)
#define FJT_CHECK_RETURN(expression)
#define FJT_CHECK_FREE_RETURN_ERR(expression, code, stream)
#define FJT_CHECK_FREE_API(expression, code, stream, category, kind)
#endif	/* FJT_DBG_PARAM_CHK */


#endif /* __FJT_TURBO_INTERNAL_H__ */
