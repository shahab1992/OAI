/****************************************************************************/
/**
 * @file  fjt_turbo_api.c
 * @brief Turbo Decode APIs
 * 
 * API for Turbo Decode Function of ACC Card(FPGA)
 * 
 */
/****************************************************************************/
#include "fjt_turbo_internal.h"

/*	16->8bit Conversion Function	*/
static inline void turbodec_conv_16to8(uint8_t *y8, short *y, unsigned short n);


/****************************************************************************/
/**
 * @brief Turbo Decode Calculation Function Initilization API
 *
 * Initliaze FPGA, make it ready to do Turbo Decode Calculation.
 *
 * @param[in]       none
 * @param[out]      none
 * @retval          FJT_API_OK(0)         Normal End
 * @retval          FJT_API_INIT_ERR(2)   Initialization Failed
 */
/****************************************************************************/
uint8_t fjt_api_turbodec_init()
{
#if (FJT_DBG_PARAM_CHK == 1)
	static uint8_t	init_flag = FJT_FLAG_OFF;		/*	Determination Flag for Initilization Done or not	*/
#endif	/*FJT_DBG_PARAM_CHK*/
	fjt_fpga_uioinfo_t		fpga_uioinfo;			/*	FPGA UIO Information			*/
	fjt_dmabuf_uioinfo_t	dmabuf_uioinfo;			/*	DMA  UIO Information			*/
	fjt_fpga_reg_t			*fpga_reg_addr_virt_p;	/*	FPGA Register Virtual Address	*/
	
	uint64_t	dma_addr_phys0;		/*	Turbo Decode Destination Physical Address for TB0	*/
	uint64_t	dma_addr_phys1;		/*	Turbo Decode Destination Physical Address for TB1	*/
	uint32_t	dma_addr_phys0_L;	/*	TB0 Lower Address									*/
	uint32_t	dma_addr_phys0_H;	/*	TB0 Upper Address									*/
	uint32_t	dma_addr_phys1_L;	/*	TB1 Lower Address									*/
	uint32_t	dma_addr_phys1_H;	/*	TB1 Upper Address									*/
	
	uint64_t	fpga_addr_phys0;	/*	Turbo Decode Origination Physical Address for TB0	*/
	uint64_t	fpga_addr_phys1;	/*	Turbo Decode Origination Physical Address for TB1	*/
	uint32_t	fpga_addr_phys0_L;	/*	TB0 Lower Address									*/
	uint32_t	fpga_addr_phys0_H;	/*	TB0 Upper Address									*/
	uint32_t	fpga_addr_phys1_L;	/*	TB1 Lower Address									*/
	uint32_t	fpga_addr_phys1_H;	/*	TB1 Upper Address									*/
	
	uint8_t		chk_ret;			/*	Address Setting Check Result						*/
	uint32_t	retry_cnt;			/*	Address Setting Retry Counter						*/
	uint8_t		ret;
	
	
	FJT_LOG_INIT();
	FJT_TRACE_LOG(FJT_STREAM0, LOG_LEVEL0, "START");
	/*--------------------------*/
	/*	Double Call Check		*/
	/*--------------------------*/
	FJT_CHECK_FREE_API( ( init_flag != FJT_FLAG_OFF ), FJT_API_INIT_ERR, FJT_STREAM0, API_INIT, RET1 );
	
	
	/*--------------------------*/
	/*	UIO,eventFD OPEN		*/
	/*--------------------------*/
	/*	FPGA	*/
	if( (ret = fjt_open_fpga_uiodev()) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to open fpga area. ret=%d errno=%d",ret, errno);
		FJT_TRACE_LOG(FJT_STREAM0, LOG_LEVEL0, "END(ERR)");
		FJT_STATISTICS_LOG(FJT_STREAM0, API_INIT, RET1);
		return FJT_API_INIT_ERR;
	}
	/*	DMA		*/
	if( (ret = fjt_open_dmabuf_uiodev()) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to open dma area. ret=%d errno=%d",ret, errno);
		goto FJT_INIT_DMA_OPEN_ERR;
	}
	/*	MSI		*/
	if( (ret = fjt_open_msi_eventfd()) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to open msi. ret=%d errno=%d",ret, errno);
		goto FJT_INIT_MSI_OPEN_ERR;
	}
	
	
	/*------------------------------*/
	/*	UIO Information Acquisition	*/
	/*------------------------------*/
	/*	FPGA	*/
	if( (ret = fjt_get_fpga_uiodev(&fpga_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to get fpga uioinfo. ret=%d errno=%d",ret, errno);
		goto FJT_INIT_FPGA_GET_ERR;
	}
	/*	DMA		*/
	if( (ret = fjt_get_dmabuf_uiodev(&dmabuf_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to get dma uioinfo. ret=%d errno=%d",ret, errno);
		goto FJT_INIT_DMA_GET_ERR;
	}
	
	
	/*--------------------------*/
	/*	FPGA Initial Setting	*/
	/*--------------------------*/
	/*	Calculate Turbo Decode Destination Physical Address	*/
	dma_addr_phys0 = (uint64_t)dmabuf_uioinfo.dma_addr_phys;
	dma_addr_phys1 = (uint64_t)dmabuf_uioinfo.dma_addr_phys + sizeof(fjt_fpga_outdt_t);
	
	/*	Divide into Upper/Lower	*/
	dma_addr_phys0_L = (uint32_t)dma_addr_phys0;
	dma_addr_phys0_H = (uint32_t)(dma_addr_phys0 >> 32);
	dma_addr_phys1_L = (uint32_t)dma_addr_phys1;
	dma_addr_phys1_H = (uint32_t)(dma_addr_phys1 >> 32);

	/*	Caculate Turbo Decode Origination Physical Address	*/
	fpga_addr_phys0 = (uint64_t)fpga_uioinfo.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM];
	fpga_addr_phys1 = (uint64_t)fpga_uioinfo.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM] + sizeof(fjt_fpga_outdt_t);
	
	/*	Divide into Upper/Lower	*/
	fpga_addr_phys0_L = (uint32_t)fpga_addr_phys0;
	fpga_addr_phys0_H = (uint32_t)(fpga_addr_phys0 >> 32);
	fpga_addr_phys1_L = (uint32_t)fpga_addr_phys1;
	fpga_addr_phys1_H = (uint32_t)(fpga_addr_phys1 >> 32);
	
	/*	Initialize variable used by check	*/
	fpga_reg_addr_virt_p = (fjt_fpga_reg_t *)fpga_uioinfo.pcie_addr[FJT_FPGA_UIOINFO_REGISTER];
	chk_ret   = FJT_RET_ERR;
	retry_cnt = 0;
	
	do{
		/*--------------------------*/
		/*	Address Notification	*/
		/*--------------------------*/
		/*	TB0 Turbo Decode Destination Address Setting	*/
		fpga_reg_addr_virt_p->outset[0].OST_DEST_TB_L = dma_addr_phys0_L;
		fpga_reg_addr_virt_p->outset[0].OST_DEST_TB_H = dma_addr_phys0_H;
		/*	TB1 Turbo Decode Destination Address Setting	*/
		fpga_reg_addr_virt_p->outset[1].OST_DEST_TB_L = dma_addr_phys1_L;
		fpga_reg_addr_virt_p->outset[1].OST_DEST_TB_H = dma_addr_phys1_H;

		/*	TB0 Turbo Decode Origination Address Setting	*/
		fpga_reg_addr_virt_p->outset[0].OST_SRC_TB_L  = fpga_addr_phys0_L;
		fpga_reg_addr_virt_p->outset[0].OST_SRC_TB_H  = fpga_addr_phys0_H;
		/*	TB1 Turbo Decode Origination Address Setting	*/
		fpga_reg_addr_virt_p->outset[1].OST_SRC_TB_L  = fpga_addr_phys1_L;
		fpga_reg_addr_virt_p->outset[1].OST_SRC_TB_H  = fpga_addr_phys1_H;
		
		/*--------------------------*/
		/*	Setting Value Check		*/
		/*--------------------------*/
		if( (fpga_reg_addr_virt_p->outset[0].OST_SRC_TB_L  == fpga_addr_phys0_L) &&
			(fpga_reg_addr_virt_p->outset[0].OST_SRC_TB_H  == fpga_addr_phys0_H) &&
			(fpga_reg_addr_virt_p->outset[1].OST_SRC_TB_L  == fpga_addr_phys1_L) &&
			(fpga_reg_addr_virt_p->outset[1].OST_SRC_TB_H  == fpga_addr_phys1_H) &&
			(fpga_reg_addr_virt_p->outset[0].OST_DEST_TB_L == dma_addr_phys0_L) &&
			(fpga_reg_addr_virt_p->outset[0].OST_DEST_TB_H == dma_addr_phys0_H) &&
			(fpga_reg_addr_virt_p->outset[1].OST_DEST_TB_L == dma_addr_phys1_L) &&
			(fpga_reg_addr_virt_p->outset[1].OST_DEST_TB_H == dma_addr_phys1_H) ){
			chk_ret = FJT_RET_OK;
		}
		else{
			retry_cnt++;
			if( retry_cnt > FJT_INIT_CHK_RETRY_NUM ){
				FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "address set NG.");
				goto FJT_INIT_CHK_ERR;
			}
		}
		
	}while( chk_ret != FJT_RET_OK );
	
#if (FJT_DBG_PARAM_CHK == 1)
	/*	Set Intialization Done Flag ON	*/
	init_flag = FJT_FLAG_ON;
#endif	/*FJT_DBG_PARAM_CHK*/
	FJT_STATISTICS_LOG(FJT_STREAM0, API_INIT, RET0);
	FJT_TRACE_LOG(FJT_STREAM0, LOG_LEVEL0, "END(OK)");
	return FJT_API_OK;
	
	
/**	Quasi-Normal, Abnormal Post Processing Route	**/
FJT_INIT_CHK_ERR:
FJT_INIT_DMA_GET_ERR:
FJT_INIT_FPGA_GET_ERR:
	if( (ret = fjt_close_msi_eventfd()) != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to close msi. ret=%d errno=%d",ret, errno);
	}
FJT_INIT_MSI_OPEN_ERR:
	if( fjt_close_dmabuf_uiodev() != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to close dma area. ret=%d errno=%d",ret, errno);
	}
FJT_INIT_DMA_OPEN_ERR:
	if( fjt_open_fpga_uiodev() != FJT_RET_OK ){
		FJT_ILG_LOG(FJT_STREAM0, LOG_LEVEL1, "Failed to close fpga area. ret=%d errno=%d",ret, errno);
	}
	
	FJT_TRACE_LOG(FJT_STREAM0, LOG_LEVEL0, "END(ERR)");
	FJT_STATISTICS_LOG(FJT_STREAM0, API_INIT, RET1);
	return FJT_API_INIT_ERR;
}


/****************************************************************************/
/**
 * @brief Turbo Decode Calculation Request API
 *
 * Submit Turbo Decode Calulation Request to FPGA
 *
 * @param[in]       param_p               Pointer to Turbo Decode Calculation Input Parameters
 * @retval          FJT_API_OK(0)         Normal End
 * @retval          FJT_API_PARAM_ERR(3)  Parameter Error
 */
/****************************************************************************/
uint8_t fjt_api_turbodec_req(fjt_turbodec_inparam_t *param_p)
{
	
	fjt_fpga_uioinfo_t		fpga_uioinfo;	/*	FPGA UIO Information			*/
	fjt_dmabuf_uioinfo_t	dmabuf_uioinfo;	/*	DMA  UIO Information			*/
	fjt_fpga_reg_t	*fpga_reg_addr_virt_p;	/*	FPGA Register Virtual Address	*/
	fjt_fpga_mem_t	*fpga_mem_addr_virt_p;	/*	FPGA RAM Virtual Address		*/
	fjt_fpga_mem_t	*dma_addr_virt_p;		/*	DMA Virtual Address				*/
	
	uint8_t		*cb_data_p;		/*	Pointer to the stored CodeBock after conversion	*/
	uint8_t		cb_index;		/*	Stored Codeblockindex							*/
	uint64_t	dst_addr_virt;	/*	DMA Destination Address							*/
	uint8_t		ret;
	
	
	FJT_CHECK_FREE_API( ( param_p == FJT_NULL  ), FJT_API_PARAM_ERR, FJT_STREAM0, API_REQ, RET1 );
	FJT_TRACE_LOG(param_p->stream_number, LOG_LEVEL0, "START");
	/*--------------------------*/
	/*	Input Parameter Check	*/
	/*--------------------------*/
	FJT_CHECK_FREE_API( ( param_p->stream_number >= FJT_MAX_STREAM )    , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->tb_size < FJT_MIN_TB_SIZE )          , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->tb_size > FJT_MAX_TB_SIZE )          , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->cb_seg_num < FJT_MIN_CB_NUM )        , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->cb_seg_num > FJT_MAX_CB_NUM )        , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->cb_number >= FJT_MAX_CB_NUM )        , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->cb_size < FJT_MIN_CB_SIZE )          , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->cb_size > FJT_MAX_CB_SIZE )          , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->max_iteration_num > FJT_MAX_ITR_NUM ), FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->seq_num > FJT_MAX_SEQ_NUM )          , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	FJT_CHECK_FREE_API( ( param_p->inbuf_p == FJT_NULL )                , FJT_API_PARAM_ERR, param_p->stream_number, API_REQ, RET1 );
	
	
	/*------------------------------*/
	/*	UIO Information Acquisition	*/
	/*------------------------------*/
	/*	FPGA	*/
	if( (ret = fjt_get_fpga_uiodev(&fpga_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(param_p->stream_number, LOG_LEVEL1, "Failed to get fpga uioinfo. ret=%d errno=%d",ret, errno);
	}
	/*	DMA		*/
	if( (ret = fjt_get_dmabuf_uiodev(&dmabuf_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(param_p->stream_number, LOG_LEVEL1, "Failed to get dma uioinfo. ret=%d errno=%d",ret, errno);
	}
	fpga_reg_addr_virt_p = (fjt_fpga_reg_t *)fpga_uioinfo.pcie_addr[FJT_FPGA_UIOINFO_REGISTER];
	fpga_mem_addr_virt_p = (fjt_fpga_mem_t *)fpga_uioinfo.pcie_addr[FJT_FPGA_UIOINFO_RAM];
	dma_addr_virt_p      = (fjt_fpga_mem_t *)dmabuf_uioinfo.dma_addr;
	
	
	/*------------------------------------------*/
	/*	Issue Trigger & TB Information Setting	*/
	/*------------------------------------------*/
	if( param_p->cb_number == 0 ){
		
		/*	TB Information Setting	*/
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_TB_SIZE    = param_p->tb_size;
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_CB_SEG_NUM = param_p->cb_seg_num;
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_ITR_NUM    = param_p->max_iteration_num;
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_FIL_BIT    = param_p->filler_bit_num;
		
		/*	Setting Seq Number to TB_ID either in order to decide whether calculation request is busy or not in Result Acquisition API	*/
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_SN         = param_p->seq_num;
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_TB_ID      = param_p->seq_num;
		
		/*	Issue DMA Transmission Trigger (It should be after TB Information Setting)	*/
		wmb();
		fpga_reg_addr_virt_p->inset[param_p->stream_number].IST_DMA_TRG    = FJT_IST_DMA_TRG_ON;
		FJT_STATISTICS_LOG(param_p->stream_number, API_REQ, INF0);
		
	}
	
	/*--------------------------*/
	/*	16bit->8bit Conversion	*/
	/*--------------------------*/
	/*	Calculate the area in which conversion result is stored	*/
	cb_index  = cb_to_index(param_p->cb_number);
	cb_data_p = &dma_addr_virt_p->indt.tb[param_p->stream_number].data[cb_index][0];
	
	/*	Conversion	*/
	turbodec_conv_16to8(	cb_data_p, 							/*	Stored area after conversion	*/
							(short *)param_p->inbuf_p,			/*	Stored area before conversion	*/
							(unsigned short)param_p->cb_size);	/*	CodeBlock Size					*/
	
	/*------------------------------*/
	/*	Calculation Request to FPGA	*/
	/*------------------------------*/
	/*	Calculation Destination Virtual Address	*/
	dst_addr_virt = (uint64_t)&fpga_mem_addr_virt_p->indt.tb[param_p->stream_number].data[cb_index][0];
	
	/*	Calculation Request	*/
	ret = fjt_kick_fpga_dma_read(	param_p->stream_number,	/*	Stream Number					*/
									param_p->cb_number,		/*	CodeBlock Number				*/
									(uint64_t)cb_data_p,	/*	Origination Virtual Address		*/
									dst_addr_virt,			/*	Destination Virtual Address		*/
									param_p->cb_size );		/*	Codeblock Size					*/
	
	if( ret != FJT_RET_OK ){
		FJT_ILG_LOG(param_p->stream_number, LOG_LEVEL1, "Failed to request dma. ret=%d errno=%d",ret, errno);
	}
	
	FJT_TRACE_LOG(param_p->stream_number, LOG_LEVEL0, "END(OK)");
	FJT_STATISTICS_LOG(param_p->stream_number, API_REQ, RET0);
	return FJT_API_OK;
}


/****************************************************************************/
/**
 * @brief Turbo Decode Calculation Completion Monitoring API
 *
 * Monitor FPGA's Turbo Decode Caculation Completion.
 *
 * @param[in]       mode                  End Waiting Mode (0:Async Mode, 1:Sync Mode)
 * @param[in]       stream_num            Stream Number (0:stream0, 1:stream1)
 * @param[in,out]   status                Turbo Decode Calculation Result (0:non-err, else:err(Refer to ODT_STATUS Register))
 * @retval          FJT_API_OK(0)         Normal End (Normal Interruption)
 * @retval          FJT_API_NG(1)         Abnormal End (Abnormal Interruption)
 * @retval          FJT_API_PARAM_ERR(3)  Parameter Error
 * @retval          FJT_API_EMPTY(15)     Not Received(Only when Aync Mode)
 */
/****************************************************************************/
uint8_t fjt_api_turbodec_end_wait(uint8_t mode, uint8_t stream_num, uint32_t *status)
{
	fjt_msi_info_t			msi_info;			/*	MSI Information		*/
	fjt_dmabuf_uioinfo_t	dmabuf_uioinfo;		/*	DMA UIO Information	*/
	fjt_fpga_mem_t			*dma_addr_virt_p;	/*	DMA Virtual Address	*/

	struct epoll_event ret_ev[FJT_WATCH_EVENT_MAX];	/*	Event Result	*/
	uint64_t	efd_cnt;							/*	eventFD Counter				*/
	uint64_t	efd_ok_index;						/*	eventFD Index				*/
	uint64_t	efd_ng_index;						/*	eventFD Index				*/
	int			epoll_timeout;						/*	epoll Timeout Setting		*/
	int			ret_epoll;							/*	Event Occurrence FD Number	*/
	uint8_t		ret;
	
	uint32_t	cb_seg_num;			/*	CodeBlock Division Number		*/
	uint32_t	cb_crc_exp = 0;		/*	CodeBlock CRC Expected Value	*/
	uint32_t	crc_ret;			/*	FPGA Register CRC Result Value	*/
	uint32_t	i;
	
	FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "START");
	/*--------------------------*/
	/*	Input Parameter Check	*/
	/*--------------------------*/
	FJT_CHECK_FREE_API( ( (mode != FJT_API_WATCH_NONSYNC) && (mode != FJT_API_WATCH_SYNC) ), FJT_API_PARAM_ERR, stream_num, API_WAIT, RET3 );
	FJT_CHECK_FREE_API( ( stream_num >= FJT_MAX_STREAM ), FJT_API_PARAM_ERR, stream_num, API_WAIT, RET3 );
	FJT_CHECK_FREE_API( ( status     == FJT_NULL       ), FJT_API_PARAM_ERR, stream_num, API_WAIT, RET3 );
	
	
	/*----------------------------------*/
	/*	UIO,MSI Information Acquisition	*/
	/*----------------------------------*/
	/*	DMA	*/
	if( (ret = fjt_get_dmabuf_uiodev(&dmabuf_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Failed to get dma uioinfo. ret=%d errno=%d",ret, errno);
		FJT_ASSERT();
	}
	/*	MSI	*/
	if( (ret = fjt_get_msi_eventfd(&msi_info)) != FJT_RET_OK ){
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Failed to get msi info. ret=%d errno=%d",ret, errno);
		FJT_ASSERT();
	}
	dma_addr_virt_p = (fjt_fpga_mem_t *)dmabuf_uioinfo.dma_addr;
	
	
	/*----------------------------------*/
	/*	Interruption Check Preparation	*/
	/*----------------------------------*/
	/*	Get eventFD index	*/
	if( stream_num == 0 ){
		efd_ok_index = FJT_EFD_INDEX_STREAM0_OK;
		efd_ng_index = FJT_EFD_INDEX_STREAM0_NG;
	}else{
		efd_ok_index = FJT_EFD_INDEX_STREAM1_OK;
		efd_ng_index = FJT_EFD_INDEX_STREAM1_NG;
	}
	/*	Set epoll block flag by based on mode	*/
	if( mode == FJT_API_WATCH_NONSYNC ){
		/*	Async Mode	*/
		epoll_timeout = 0;
	}else{
		/*	Sync Mode	*/
		epoll_timeout = -1;
	}
	
	
	/*----------------------*/
	/*	Interruption Check	*/
	/*----------------------*/	
	ret_epoll = epoll_wait(msi_info.epollfd[stream_num], ret_ev, FJT_WATCH_EVENT_MAX, epoll_timeout);
	
	if( ret_epoll == 1 ){
		if( ret_ev[0].data.fd == msi_info.eventfd.fd[efd_ok_index] ){
			/*	OK Interruption	*/
			/*	Read in order to get eventfd couter back to 0	*/
			if( read(msi_info.eventfd.fd[efd_ok_index], &efd_cnt, sizeof(uint64_t)) == FJT_FAILED ){
				FJT_ILG_LOG(stream_num, LOG_LEVEL0, "Failed to read eventfd. index=%d,eventfd=%d,errno=%d",
													efd_ok_index, msi_info.eventfd.fd[efd_ok_index], errno);
				FJT_ASSERT();
			}
			*status = 0;
			FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(OK)");
			FJT_STATISTICS_LOG(stream_num, API_WAIT, RET0);
			return FJT_API_OK;
		}
		else{
			/*	NG Interruption	*/
			/*	Read in order to get eventfd couter back to 0 */
			if( read(msi_info.eventfd.fd[efd_ng_index], &efd_cnt, sizeof(uint64_t)) == FJT_FAILED ){
				FJT_ILG_LOG(stream_num, LOG_LEVEL0, "Failed to read eventfd. index=%d,eventfd=%d,errno=%d",
													efd_ng_index, msi_info.eventfd.fd[efd_ng_index], errno);
				FJT_ASSERT();
			}
			
			/*	CRC Result Check Process	*/
			cb_seg_num = dma_addr_virt_p->outdt.tb[stream_num].info.ODT_CB_SEG_NUM;
			crc_ret    = dma_addr_virt_p->outdt.tb[stream_num].info.ODT_CRC_RES;
			for(i = 0 ; i < cb_seg_num ; i++){
				cb_crc_exp |= ( 0x00000001 << i );
			}
			
			if( ((crc_ret & cb_crc_exp) == cb_crc_exp ) && ((crc_ret & FJT_TB_CRC_EXP) == FJT_TB_CRC_EXP) ){
			/*	Considering Timeout and Seq Number NG, when CRC-OK, it returns Normal	*/
				*status = 0;
				FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(OK)");
				FJT_STATISTICS_LOG(stream_num, API_WAIT, RET0);
				return FJT_API_OK;
			}
			else{
			/*	When CRC-NG, it returns Abnormal	*/
				*status = dma_addr_virt_p->outdt.tb[stream_num].info.ODT_STATUS;
				if( *status != 0x00000000 ){
					/*	If the reason of abnormal interruption is other than CRC, logs are saved	*/
					FJT_ILG_LOG(stream_num, LOG_LEVEL0, "NG Interrupt status=%08X",*status);
					FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(NG)");
					FJT_STATISTICS_LOG(stream_num, API_WAIT, RET1);
				}
				else{
					FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(CRC-NG)");
					FJT_STATISTICS_LOG(stream_num, API_WAIT, RET4);
				}
				return FJT_API_NG;
			}
		}
	}else if( ret_epoll == 0 ){
		FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(EMPTY)");
		FJT_STATISTICS_LOG(stream_num, API_WAIT, RET2);
		return FJT_API_EMPTY;
	}
	else{
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Interrupt num NG. ret_poll=%d",ret_epoll);
	}
	
	FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(OK)");
	FJT_STATISTICS_LOG(stream_num, API_WAIT, RET0);
	return FJT_API_OK;
}


/****************************************************************************/
/**
 * @brief Turbo Decode Calculation Result Read API
 *
 * Read Turbo Decode Calculation Result (Turbo Decode Data, CB-CRC Result, TB-CRC Result)
 *
 * @param[in]       stream_num            Stream Number (0:stream0, 1:stream1)
 * @param[in]       param                 Pointer to Turbo Decode Calculation Result Output Area
 * @retval          FJT_API_OK(0)         Normal End
 * @retval          FJT_API_PARAM_ERR(3)  Parameter Error
 */
/****************************************************************************/
uint8_t fjt_api_turbodec_get_result(uint8_t stream_num, fjt_turbodec_outparam_t *param)
{
	fjt_dmabuf_uioinfo_t	dmabuf_uioinfo;		/*	DMA UIO Information	*/
	fjt_fpga_mem_t			*dma_addr_virt_p;	/*	DMA Virtual Address	*/
	
	uint32_t	cb_seg_num;			/*	CodeBlock Division Number		*/
	uint32_t	cb_crc_exp = 0;		/*	CodeBlock CRC Expeceted Value	*/
	uint32_t	crc_ret;			/*	FPGA Register CRC Result Value	*/
	uint32_t	i;
	uint8_t		ret;
	
	
	FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "START");
	/*--------------------------*/
	/*	Input Parameter Check	*/
	/*--------------------------*/
	FJT_CHECK_FREE_API( ( stream_num >= FJT_MAX_STREAM ), FJT_API_PARAM_ERR, stream_num, API_GET, RET1 );
	FJT_CHECK_FREE_API( ( param      == FJT_NULL       ), FJT_API_PARAM_ERR, stream_num, API_GET, RET1 );
	
	/*------------------------------*/
	/*	UIO Information Acquisition	*/
	/*------------------------------*/
	/*	DMA		*/
	if( (ret = fjt_get_dmabuf_uiodev(&dmabuf_uioinfo)) != FJT_RET_OK ){
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Failed to get dma uioinfo. ret=%d errno=%d",ret, errno);
		FJT_ASSERT();
	}
	dma_addr_virt_p = (fjt_fpga_mem_t *)dmabuf_uioinfo.dma_addr;
	/*----------------------------------*/
	/*	Turbo Decode Data Acquisition	*/
	/*----------------------------------*/
	/*	The number of iteration returned by FPGA is the actual interation number minus 1.	*/
	/*	This descripancy is hidden by adding 1 here in API.									*/
	param->iteration_num = (uint8_t)(dma_addr_virt_p->outdt.tb[stream_num].info.ODT_ITR_NUM + 1 );
	param->seq_num       = (uint16_t)dma_addr_virt_p->outdt.tb[stream_num].info.ODT_TB_ID;
	param->outbuf_p      = (void *)&dma_addr_virt_p->outdt.tb[stream_num].decoded_data[0];
	
	/*------------------------------*/
	/*	CRC Check Result Judgement	*/
	/*------------------------------*/
	/* CodeBlock CRC */
	cb_seg_num  = dma_addr_virt_p->outdt.tb[stream_num].info.ODT_CB_SEG_NUM;
	crc_ret     = dma_addr_virt_p->outdt.tb[stream_num].info.ODT_CRC_RES;
	
	/*	Calculate CRC expected value by CodeBlock division number	*/
	for(i = 0 ; i < cb_seg_num ; i++){
		cb_crc_exp |= ( 0x00000001 << i );
	}
	if( (crc_ret & cb_crc_exp) == cb_crc_exp ){
		param->cb_crc_result = FJT_API_CRC_OK;
	}
	else{
		/*	If unmatched with the expected value, bit for CodeBlock with NG is set as 1	*/
		param->cb_crc_result = (uint16_t)((crc_ret & cb_crc_exp) ^ cb_crc_exp);
	}
	
	/* TB CRC */
	if( (crc_ret & FJT_TB_CRC_EXP) == FJT_TB_CRC_EXP ){
		param->tb_crc_result = FJT_API_CRC_OK;
	}
	else{
		param->tb_crc_result = FJT_API_CRC_NG;
	}
	
	FJT_TRACE_LOG(stream_num, LOG_LEVEL0, "END(OK)");
	FJT_STATISTICS_LOG(stream_num, API_GET, RET0);
	return FJT_API_OK;
}


/****************************************************************************/
/**
 * @brief Log-Likelihood Ratio 16bit->8bit Conversion
 *
 * @param[in,out]   y8  Pointer to the Converstion Result Stored Area (DMA Buffer)
 * @param[in]       y   Pointer to the Turbo Decode Input Buffer
 * @param[in]       n   CodeBlock Size
 * @retval          Nothing
 * @memo
 *  Diversion from OAI
 *  File Name: 3gpplte_turbo_decoder_sse_8bit.c 
 *  Function Name: unsigned char phy_threegpplte_turbo_decoder8
 */
/****************************************************************************/
static inline void turbodec_conv_16to8(uint8_t *y8, short *y, unsigned short n){
	
#ifndef RUN_PGR	
	unsigned int	i,j;
	int	n2;
	
	
	/*	Since the calculation in the extraction process is performend in 16 bytes units	*/
	/*	make the size of CodeBlock a multiple of 16										*/
	if ((n&15)>0) {
		n2 = n+8;
	} else
	n2 = n;

#if (FJT_REQ_CONV_16TO8_SIFT == 1)
	/*------------------------------------------*/
	/*	Mode for extracting 8 bits with shift	*/
	/*------------------------------------------*/
	/*	Average Calculation Process	*/
	__m128i avg=_mm_set1_epi32(0);
	for (i=0; i<(3*(n>>4))+1; i++) {
		__m128i tmp=_mm_abs_epi16(_mm_unpackhi_epi16(((__m128i*)y)[i],((__m128i*)y)[i]));
		avg=_mm_add_epi32(_mm_cvtepi16_epi32(_mm_abs_epi16(((__m128i*)y)[i])),avg);
		avg=_mm_add_epi32(_mm_cvtepi16_epi32(tmp),avg);
	}
	int32_t round_avg=(_mm_extract_epi32(avg,0)+_mm_extract_epi32(avg,1)+_mm_extract_epi32(avg,2)+_mm_extract_epi32(avg,3))/(n*3);
	
	/*	8 bit Extraction Process	*/
	if (round_avg < 16 )
		for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(((__m128i *)y)[j],((__m128i *)y)[j+1]);
	else if (round_avg < 32)
		for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(_mm_srai_epi16(((__m128i *)y)[j],1),_mm_srai_epi16(((__m128i *)y)[j+1],1));
	else if (round_avg < 64 )
		for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(_mm_srai_epi16(((__m128i *)y)[j],2),_mm_srai_epi16(((__m128i *)y)[j+1],2));
	else if (round_avg < 128)
		for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(_mm_srai_epi16(((__m128i *)y)[j],3),_mm_srai_epi16(((__m128i *)y)[j+1],3));
	else
		for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(_mm_srai_epi16(((__m128i *)y)[j],3),_mm_srai_epi16(((__m128i *)y)[j+1],4));
#else
	/*----------------------------------------------*/
	/*	Mode for extracting 8 bits without shift	*/
	/*----------------------------------------------*/
	for (i=0,j=0; i<(3*(n2>>4))+1; i++,j+=2)
			((__m128i *)y8)[i] = _mm_packs_epi16(((__m128i *)y)[j],((__m128i *)y)[j+1]);
#endif /*(FJT_REQ_CONV_16TO8_SIFT == 1)*/
#endif	/*	RUN_PGR	*/
	
	return;
}
