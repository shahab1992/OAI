/****************************************************************************/
/**
 * @file  fjt_turbo_module.c
 * @brief Turbo Decode Function Sub Module
 * 
 * Turbo Decode Function Sub Module
 * 
 */
/****************************************************************************/
#include "fjt_turbo_internal.h"

/*	Each global variables is dealt with this file	*/
/*	so that declar static							*/

/* UIO FPGA association */
static	fjt_fpga_uioinfo_t		fpga_uioinfo_g;
/* UIO DMA association */
static	fjt_dmabuf_uioinfo_t	dmabuf_uioinfo_g;
/*	MSI association	*/
static	fjt_msi_info_t	msi_info_g;

/* Log management information */
static fjt_log_t		log_g;
static fjt_stat_info_t	statlog_g __attribute__((aligned(64)));


/****************************************************************************/
/**
 * @brief Log Initialization Function
 *
 * Initilize Log Function (Create Log File, Reserve File Descriptor)
 *
 * @param[in]       Nothing
 * @param[out]      Nothing
 * @retval          Nothing
 */
/****************************************************************************/
void fjt_turbo_loginit()
{
	
	static uint8_t loginit_flag = FJT_FLAG_OFF;
	int		stream, filenum;
	FILE	*fp;
	/*	For Configuration Acquisition	*/
	int		sw_stream0;
	int		sw_stream1;
	int		conf_linenum;
	/*	For File Name Creation	*/
	char	log_name[FJT_LOG_MAX_PATH];
	char	str_time[FJT_LOG_MAX_PATH];
	time_t	now_time;
	struct tm	*struct_time;
	
	
	/*----------------------*/
	/*	Double Call Check	*/
	/*----------------------*/
	if( loginit_flag != FJT_FLAG_OFF){
		printf("[FJT_TURBO][WARNING] log_init Multiple.\n");
		return;
	}
	
	/*------------------------------*/
	/*	Configuration Read Process	*/
	/*------------------------------*/
	fp = fopen("/var/tmp/fjt_turbo_config.txt", "r");
	if( fp == FJT_NULL ){
		
		/*	If no configuration, newly created	*/
		fp = fopen("/var/tmp/fjt_turbo_config.txt", "w+");
		if( fp == FJT_NULL ){
			printf("[FJT_TURBO][WARNING] config file make err. path=/var/tmp/fjt_turbo_config.txt\n");
			goto FJT_LOG_CONF_OPEN_ERR;
		}
		if( fputs("TRACE 0 0\nILG 1 1\nSTATISTICS 1 1\n", fp) == EOF){
			printf("[FJT_TURBO][WARNING] config file write err. path=/var/tmp/fjt_turbo_config.txt\n");
			goto FJT_LOG_CONF_WRITE_ERR;
		}
		if( fseek(fp, 0, SEEK_SET) != FJT_RET_OK ){
			printf("[FJT_TURBO][WARNING] config file fseek err. path=/var/tmp/fjt_turbo_config.txt\n");
			goto FJT_LOG_CONF_WRITE_ERR;
		}
	}
	
	/*	Set config value to log SW	*/
	conf_linenum = 0;
	while( fscanf(fp, "%s %d %d", &log_name[0], &sw_stream0, &sw_stream1) != EOF){
		
		conf_linenum++;
		
		if( conf_linenum > FJT_LOG_CONF_LINENUM){
			/*	Format Abnormal	*/
			printf("[FJT_TURBO][WARNING] config file format err.\n");
			goto FJT_LOG_CONF_READ_ERR;
		}
		if( ((sw_stream0 != FJT_LOG_ON) && (sw_stream0 != FJT_LOG_OFF)) ||
			((sw_stream1 != FJT_LOG_ON) && (sw_stream1 != FJT_LOG_OFF)) ){
			/*	SW Value Abnormal	*/
			printf("[FJT_TURBO][WARNING] config file sw value err.\n");
			goto FJT_LOG_CONF_READ_ERR;
		}
		if( strncmp(log_name, "TRACE", strlen(log_name) + 1 ) == 0 ){
			log_g.info[FJT_STREAM0].log_sw[LOG_TRACE] = sw_stream0;
			log_g.info[FJT_STREAM1].log_sw[LOG_TRACE] = sw_stream1;
		}
		else if( strncmp(log_name, "ILG", strlen(log_name) + 1 ) == 0 ){
			log_g.info[FJT_STREAM0].log_sw[LOG_ILG] = sw_stream0;
			log_g.info[FJT_STREAM1].log_sw[LOG_ILG] = sw_stream1;
		}
		else if( strncmp(log_name, "STATISTICS", strlen(log_name) + 1 ) == 0 ){
			log_g.info[FJT_STREAM0].log_sw[LOG_STATISTICS] = sw_stream0;
			log_g.info[FJT_STREAM1].log_sw[LOG_STATISTICS] = sw_stream1;
		}
		else{
			/*	Log Type Abnormal	*/
			printf("[FJT_TURBO][WARNING] config file logname err.\n");
			goto FJT_LOG_CONF_READ_ERR;
		}
	}
	
	if(fclose(fp) != FJT_RET_OK){
		goto FJT_LOG_CONF_CLOSE_ERR;
	}
	
	/*------------------------------*/
	/*	File Name Setting Process	*/
	/*------------------------------*/
	/*	Timestamp Strings Acquisition	*/
	now_time    = time(NULL);
	struct_time = localtime( &now_time );
	
	sprintf(str_time, "%02d%02d%02d%02d%02d",	struct_time->tm_mon + 1,	/*	Month(0-11)	*/
												struct_time->tm_mday,		/*	Day			*/
												struct_time->tm_hour, 		/*	Hour		*/
												struct_time->tm_min, 		/*	Minute		*/
												struct_time->tm_sec );		/*	Secon		*/
	
	sprintf(log_g.info[FJT_STREAM0].finfo[LOG_FILE_TRACEILG].logfile_name, "/tmp/fjt_turbo0_%s.log", str_time);
	sprintf(log_g.info[FJT_STREAM1].finfo[LOG_FILE_TRACEILG].logfile_name, "/tmp/fjt_turbo1_%s.log", str_time);
	sprintf(log_g.info[FJT_STREAM0].finfo[LOG_FILE_STATISTICS].logfile_name, "/tmp/fjt_turbo_stat0_%s.log", str_time);
	sprintf(log_g.info[FJT_STREAM1].finfo[LOG_FILE_STATISTICS].logfile_name, "/tmp/fjt_turbo_stat1_%s.log", str_time);
	
	
	/*----------------------*/
	/*	File OPEN			*/
	/*----------------------*/
	for( stream = 0 ; stream < FJT_MAX_STREAM ; stream++) {		/*	Loop as many as Stream Number	*/
		for(filenum = 0 ; filenum < LOG_FTYPE_NUM ; filenum++) {	/*	Loop as many as File Number	*/
			
			fp = fopen(log_g.info[stream].finfo[filenum].logfile_name, "w");
			if(fp == FJT_NULL) {
				if( filenum == LOG_FILE_TRACEILG ){
					log_g.info[stream].log_sw[LOG_TRACE] = FJT_LOG_OFF;
					log_g.info[stream].log_sw[LOG_ILG]   = FJT_LOG_OFF;
				}
				else{
					log_g.info[stream].log_sw[LOG_STATISTICS] = FJT_LOG_OFF;
				}
				log_g.info[stream].finfo[filenum].fp = FJT_NULL;
				printf("[FJT_TURBO][WARNING] logfile open err. file=%s\n", log_g.info[stream].finfo[filenum].logfile_name);
			}
			else{
				log_g.info[stream].finfo[filenum].fp = fp;
			}
		}
	}
	
	/*----------------------------------*/
	/*	Cache Miss Protection Process	*/
	/*----------------------------------*/
	/* prefetch  ...fjt_turbo_logout() & file-pointer */
	for(stream=0; stream<FJT_MAX_STREAM; stream++) {
		if( log_g.info[stream].log_sw[LOG_TRACE] == FJT_LOG_ON ){
			/* FJT_TRACE_LOG() could be off at compilation setting, so it's not used here.	*/
			fjt_turbo_logout(__FILE__, __FUNCTION__, __LINE__, LOG_TRACE, stream, LOG_LEVEL1, "fjt_turbo_loginit() OK. stream=%d", stream);
		}
		else if( log_g.info[stream].log_sw[LOG_ILG] == FJT_LOG_ON ){
			/* FJT_TRACE_LOG() could be off at compilation setting, so it's not used here.	*/
			fjt_turbo_logout(__FILE__, __FUNCTION__, __LINE__, LOG_ILG, stream, LOG_LEVEL1, "fjt_turbo_loginit() OK. stream=%d", stream);
		}
		else{
			/* When both of TRACE and ILG TRACE are OFF, no prefetch	*/
		}
	}
	
	loginit_flag = FJT_FLAG_ON;
	return;
	
	
FJT_LOG_CONF_READ_ERR:
FJT_LOG_CONF_WRITE_ERR:
	if(fclose(fp) != FJT_RET_OK){
		printf("[FJT_TURBO][WARNING] config file close err.\n");
	}
FJT_LOG_CONF_CLOSE_ERR:
FJT_LOG_CONF_OPEN_ERR:
	for( stream = 0 ; stream < FJT_MAX_STREAM ; stream++) {		/*	Loop as many as Stream Number	*/
		log_g.info[stream].log_sw[LOG_TRACE]      = FJT_LOG_OFF;
		log_g.info[stream].log_sw[LOG_ILG]        = FJT_LOG_OFF;
		log_g.info[stream].log_sw[LOG_STATISTICS] = FJT_LOG_OFF;
	}
	return;
}


/****************************************************************************/
/**
 * @brief Log Output Function
 *
 * Output Log into File
 *
 * @param[in]       file   File Name
 * @param[in]       func   Function Name
 * @param[in]       line   Line Number
 * @param[in]       kind   Log Type (0:TRACE,1:ILG)
 * @param[in]       stream Stream Number
 * @param[in]       level  Log Level
 * @param[in]       format Log Output Format
 * @retval          none
 */
/****************************************************************************/
void fjt_turbo_logout(const char *file, const char *func, int line, int kind, uint8_t stream, int level, char *format, ...)
{
	va_list	args;
	int		len = 0;
	int		ret;
	char	log_tmp[FJT_LOG_LENGTH];
	time_t	now_time;
	char	str_time[FJT_CTIME_LENGTH];
	
	/*------------------------------------------*/
	/*	Check whether you have output or not	*/
	/*------------------------------------------*/
	if( stream >= FJT_MAX_STREAM ){
		return;
	}
	if(log_g.info[stream].log_sw[kind] != FJT_LOG_ON) {
		return;
	}
	
	/*----------------------------------*/
	/*	Output Format Creation Process	*/
	/*----------------------------------*/
	va_start(args, format);
	
	/*	Add Time Information	*/
	now_time = time(NULL);
	ctime_r(&now_time, &str_time[0]);
	str_time[strlen(str_time) - 1] = '\0';
	len += snprintf(log_tmp, FJT_LOG_LENGTH, "[%s]\t", str_time);
	
	/*	Add Header Information	*/
	if(kind == LOG_TRACE) {
		len += snprintf(&log_tmp[len], (FJT_LOG_LENGTH - len), "[TRC][%d]\t", level);
	}
	else {
		len += snprintf(&log_tmp[len], (FJT_LOG_LENGTH - len), "[ILG][%d]\t", level);
	}
	
	/*	Add Input Format	*/
	len += vsnprintf(&log_tmp[len], (FJT_LOG_LENGTH - len), format, args);
	
	/*	Add who calls log function	*/
	len += snprintf(&log_tmp[len], (FJT_LOG_LENGTH - len), "\t[%s:%s:%d]\n", file, func, line);
	
	if(len > FJT_LOG_LENGTH) {
		len = FJT_LOG_LENGTH;
	}
	
	va_end(args);
	
	/*------------------*/
	/*	Output Process	*/
	/*------------------*/	
	ret = fwrite(log_tmp, len, 1, log_g.info[stream].finfo[LOG_FILE_TRACEILG].fp);
	if(ret < 1) {
//		printf("log write err! ret=%d\n", ret);
	}
	else{
		if( ftell(log_g.info[stream].finfo[LOG_FILE_TRACEILG].fp) >= FJT_LOG_MAX_FILESIZE ){
			fseek(log_g.info[stream].finfo[LOG_FILE_TRACEILG].fp, 0, SEEK_SET);
		}
	}
	
	return;
}


/****************************************************************************/
/**
 * @brief Log Output Function (Statistic Log)
 *
 * Output Statistic Log
 *
 * @param[in]       stream   Stream Number
 * @param[in]       category Vertical Axis Index Number
 * @param[in]       kind     Horizontal Axis Index Number
 * @retval          none
 */
/****************************************************************************/
void fjt_turbo_statistics_logout(uint8_t stream, int category, int kind)
{
	static pthread_mutex_t	statlog_mutex[FJT_MAX_STREAM] = {PTHREAD_MUTEX_INITIALIZER,PTHREAD_MUTEX_INITIALIZER};
	char	write_str[FJT_LOG_LENGTH];
	int 	len;
	int		ret;
	
	
	/*------------------------------------------*/
	/*	Check whether you have output or not	*/
	/*------------------------------------------*/
	if( stream >= FJT_MAX_STREAM ){
		return;
	}
	if(log_g.info[stream].log_sw[LOG_STATISTICS] != FJT_LOG_ON) {
		return;
	}
	
	/*----------------------*/
	/*	Count Up Process	*/
	/*----------------------*/
	statlog_g.info[stream].cnt[category][kind]++;
	if( kind < INF0 ){
		/*	When Return Type, Count Up Call Tatal Number	*/
		statlog_g.info[stream].cnt[category][TOTAL]++;
	}
	
#if ( FJT_STATISTIC_OUTPUT_LIMIT == 1 )
	/*	Provisional Coping Of Performance Fluctuation By Statistical Log Output	*/
	/*	Output only when abnormal counter is counted up							*/
	if( ( category == API_INIT && kind == RET1 ) ||
		( category == API_REQ  && kind == RET1 ) ||
		( category == API_WAIT && kind == RET1 ) ||
		( category == API_WAIT && kind == RET3 ) ||
		( category == API_GET  && kind == RET1 ) ){
#endif /*FJT_STATISTIC_OUTPUT_LIMIT*/
		/*----------------------------------*/
		/*	Output Format Creation Process	*/
		/*----------------------------------*/
		len = 0;
		len += snprintf(&write_str[len], (FJT_LOG_LENGTH - len), "fjt_api_turbodec_init       [Return]TOTAL:%d,OK:%d,NG:%d\n",
								statlog_g.info[stream].cnt[API_INIT][TOTAL],	/*	Number of Times of Total Call			*/
								statlog_g.info[stream].cnt[API_INIT][RET0],		/*	Number of Times of Normal End			*/
								statlog_g.info[stream].cnt[API_INIT][RET1]);	/*	Number of Times of Abnormal End			*/
		len += snprintf(&write_str[len], (FJT_LOG_LENGTH - len),"fjt_api_turbodec_req        [Return]TOTAL:%d,OK:%d,NG(PARAM):%d [Info]TB-TRG:%d,CB-TRG:%d\n",
								statlog_g.info[stream].cnt[API_REQ][TOTAL],		/*	Number of Times of Call					*/
								statlog_g.info[stream].cnt[API_REQ][RET0],		/*	Number of Times of Normal End			*/
								statlog_g.info[stream].cnt[API_REQ][RET1],		/*	Number of Times of Parameter Abnormal	*/
								statlog_g.info[stream].cnt[API_REQ][INF0],		/*	Number of Times of TB Trigger			*/
								statlog_g.info[stream].cnt[API_REQ][INF1]);		/*	Number of Times of CB Trigger			*/
		len += snprintf(&write_str[len], (FJT_LOG_LENGTH - len),"fjt_api_turbodec_end_wait   [Return]TOTAL:%d,OK:%d,NG:%d,NG(CRC):%d,NG(EMPTY):%d,NG(PARAM):%d\n",
								statlog_g.info[stream].cnt[API_WAIT][TOTAL],	/*	Number of Times of Call					*/
								statlog_g.info[stream].cnt[API_WAIT][RET0],		/*	Number of Times of OK					*/
								statlog_g.info[stream].cnt[API_WAIT][RET1],		/*	Number of Times of NG other than CRCNG	*/
								statlog_g.info[stream].cnt[API_WAIT][RET4],		/*	Number of Times of CRCNG				*/
								statlog_g.info[stream].cnt[API_WAIT][RET2],		/*	Number of Times of No Interruption		*/
								statlog_g.info[stream].cnt[API_WAIT][RET3]);	/*	Number of Times of Parameter Abnormal	*/
		len += snprintf(&write_str[len], (FJT_LOG_LENGTH - len),"fjt_api_turbodec_get_result [Return]TOTAL:%d,OK:%d,NG(PARAM):%d\n",
								statlog_g.info[stream].cnt[API_GET][TOTAL],		/*	Total Call								*/
								statlog_g.info[stream].cnt[API_GET][RET0],		/*	Number of Times of Normal End			*/
								statlog_g.info[stream].cnt[API_GET][RET1]);		/*	Number of Times of Parameter Abnormal	*/
		/*--------------------------*/
		/*	Output Process			*/
		/*--------------------------*/
		pthread_mutex_lock(&statlog_mutex[stream]);
		fseek(log_g.info[stream].finfo[LOG_FILE_STATISTICS].fp, 0, SEEK_SET);
		ret = fwrite(write_str ,len, 1, log_g.info[stream].finfo[LOG_FILE_STATISTICS].fp);
		pthread_mutex_unlock(&statlog_mutex[stream]);
		if( ret < 0 ){
//			printf("stat log write err! ret=%d\n", ret);
		}
#if ( FJT_STATISTIC_OUTPUT_LIMIT == 1 )
	}
#endif /*FJT_STATISTIC_OUTPUT_LIMIT*/
	
	return;
}


/*	Since no chance to stop log after log initialization, commented out		*/
/*	If needed, remove comment out											*/
/****************************************************************************/
/**
 * @brief Log Output Stop Function
 *
 * Stop Log Output (Set All Log Output Flag OFF)
 *
 * @param[in]       TBD
 * @param[out]      xxx
 * @retval          none
 */
/****************************************************************************/
//void fjt_turbo_logoff_all()
//{
//	int i, j;
//	
//	for(i=0; i<FJT_MAX_STREAM; i++) {
//		for(j=0; j<LOG_TYPE_NUM; j++) {
//			log_g.info[i].log_sw[j] = FJT_LOG_OFF;
//		}
//	}
//
//	return;
//}


/****************************************************************************/
/**
 * @brief FPGA Space UIO Device open & mmap Function
 *
 * open & mmap FPGA Space UIO Device, save mmap value to global variable
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_open_fpga_uiodev()
{
	uint8_t	ret_err;
	void*	ret_mmap;
	int 	ret_munmap;
	int		uio_fpga0_fd;
	uint64_t	value;
	FILE		*fd;
	
	/*----------------------------------*/
	/* mmap Register Space/RAM Space	*/
	/*----------------------------------*/
	if ((uio_fpga0_fd = open("/dev/uio0", O_RDWR)) == FJT_FAILED){
		return FJT_RET_ERR1;
	}

	ret_mmap = mmap(NULL, FJT_FPGA_REGISTER_AREA_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fpga0_fd, (FJT_FPGA_UIOINFO_REGISTER * getpagesize()));
	if(ret_mmap == MAP_FAILED){
		ret_err = FJT_RET_ERR2;
		goto FJT_UIO_REGISER_MMAP_ERR;
	}
	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER] = ret_mmap;

	ret_mmap = mmap(NULL, FJT_FPGA_RAM_AREA_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, uio_fpga0_fd, (FJT_FPGA_UIOINFO_RAM * getpagesize()));
	if(ret_mmap == MAP_FAILED){
		ret_err = FJT_RET_ERR3;
		goto FJT_UIO_RAM_MMAP_ERR;
	}
	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM] = ret_mmap;

	if(close(uio_fpga0_fd) == FJT_FAILED){
		uio_fpga0_fd = FJT_NULL;
		ret_err = FJT_RET_ERR4;
		goto FJT_UIO_FILE_CLOSE_ERR;
	}

	/*--------------------------------------------------*/
	/* Get Register Space/RAM Space Physical Address	*/
	/*--------------------------------------------------*/
	fd = fopen("/sys/class/uio/uio0/maps/map0/addr", "r");
	if(fd == FJT_NULL) {
		ret_err = FJT_RET_ERR5;
		goto FJT_UIO_MAP0_FOPEN_ERR;
	}
	if( fscanf(fd, "0x%lx", &value) == EOF) {
		if(fclose(fd) == FJT_FAILED){
		}
		ret_err = FJT_RET_ERR6;
		goto FJT_UIO_MAP0_FSCANF_ERR;
	}
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER] = (void *)value;

	if(fclose(fd) == FJT_FAILED){
		ret_err = FJT_RET_ERR7;
		goto FJT_UIO_MAP0_FCLOSE_ERR;
	}

	fd = fopen("/sys/class/uio/uio0/maps/map1/addr", "r");
	if(fd == FJT_NULL) {
		ret_err = FJT_RET_ERR8;
		goto FJT_UIO_MAP1_FOPEN_ERR;
	}
	if( fscanf(fd, "0x%lx", &value) == EOF) {
		if(fclose(fd) == FJT_FAILED){
		}
		ret_err = FJT_RET_ERR9;
		goto FJT_UIO_MAP1_FSCANF_ERR;
	}
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM] = (void *)value;

	if(fclose(fd) == FJT_FAILED){
		ret_err = FJT_RET_ERR10;
		goto FJT_UIO_MAP1_FCLOSE_ERR;
	}
	
	return FJT_RET_OK;


FJT_UIO_MAP1_FCLOSE_ERR:
FJT_UIO_MAP1_FSCANF_ERR:
FJT_UIO_MAP1_FOPEN_ERR:
FJT_UIO_MAP0_FSCANF_ERR:
FJT_UIO_MAP0_FCLOSE_ERR:
FJT_UIO_MAP0_FOPEN_ERR:
FJT_UIO_FILE_CLOSE_ERR:
	ret_munmap = munmap(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM],FJT_FPGA_RAM_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
	}
	
FJT_UIO_RAM_MMAP_ERR:
	ret_munmap = munmap(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER],FJT_FPGA_REGISTER_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
	}

FJT_UIO_REGISER_MMAP_ERR:
	if(uio_fpga0_fd != FJT_NULL) {
		if(close(uio_fpga0_fd) == FJT_FAILED){
		}
		uio_fpga0_fd = FJT_NULL;
	}

	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM]           = FJT_NULL;
	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER]      = FJT_NULL;
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM]      = FJT_NULL;
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER] = FJT_NULL;
	
	return ret_err;
}


/****************************************************************************/
/**
 * @brief FPGA Space UIO Device Information Acquisition Function
 *
 * Get FPGA Space UIO Device Information
 *
 * @param[in,out]   fpga_uioinfo
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_get_fpga_uiodev(fjt_fpga_uioinfo_t *fpga_uioinfo)
{
	FJT_CHECK_RETURN_ERR(fpga_uioinfo, FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER], FJT_RET_ERR2);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM], FJT_RET_ERR3);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER], FJT_RET_ERR4);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM], FJT_RET_ERR5);
	
	fpga_uioinfo->pcie_addr[FJT_FPGA_UIOINFO_REGISTER]      = fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER];
	fpga_uioinfo->pcie_addr[FJT_FPGA_UIOINFO_RAM]           = fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM];
	fpga_uioinfo->pcie_addr_phys[FJT_FPGA_UIOINFO_RAM]      = fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM];
	fpga_uioinfo->pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER] = fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER];
	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief FPGA Space UIO Device mumap & close Function
 *
 * mumap & close FPGA Space UIO Device, delete global variable mmap value
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_close_fpga_uiodev()
{
	int ret_munmap;
	
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER], FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM], FJT_RET_ERR2);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER], FJT_RET_ERR3);
	FJT_CHECK_RETURN_ERR(fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM], FJT_RET_ERR4);
	
	/* munmap Register Area */
	ret_munmap = munmap(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER],FJT_FPGA_REGISTER_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
		return FJT_RET_ERR5;
	}
	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_REGISTER]      = FJT_NULL;
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_REGISTER] = FJT_NULL;

	/* munmap RAM Area */
	ret_munmap = munmap(fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM],FJT_FPGA_RAM_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
		return FJT_RET_ERR6;
	}
	fpga_uioinfo_g.pcie_addr[FJT_FPGA_UIOINFO_RAM]      = FJT_NULL;
	fpga_uioinfo_g.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM] = FJT_NULL;

	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief DMA Buffer UIO Device open & mmap Function
 *
 * open & mmap DMA Buffer UIO Device, mmap value is saved in global variable
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_open_dmabuf_uiodev()
{
	uint8_t		ret_err;
	void*		ret_mmap;
	int			uio_dma_fd;
	int			ret_munmap;
	uint64_t	value;
	FILE		*fd;
	
	/* mmap DMA Buffer UIO */
	if ((uio_dma_fd = open("/dev/uio0", O_RDWR)) == FJT_FAILED){
		return FJT_RET_ERR1;
	}
	
	ret_mmap = mmap(NULL, FJT_DMAMEM_AREA_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, uio_dma_fd, (FJT_FPGA_UIOINFO_DMABUF * getpagesize()));
	if(ret_mmap == MAP_FAILED){
		if(close(uio_dma_fd) == FJT_FAILED){
		}
		ret_err = FJT_RET_ERR2;
		goto FJT_UIO_DMAMEM_MMAP_ERR;
	}
	dmabuf_uioinfo_g.dma_addr = ret_mmap;

	if(close(uio_dma_fd) == FJT_FAILED){
		ret_err = FJT_RET_ERR3;
		goto FJT_UIO_DMAMEM_CLOSE_ERR;
	}

	/* Get DMA Buffer Physical Address */
	fd = fopen("/sys/class/uio/uio0/maps/map2/addr", "r");
	if(fd == FJT_NULL) {
		ret_err = FJT_RET_ERR4;
		goto FJT_UIO_DMAMEM_FOPEN_ERR;
	}
	if( fscanf(fd, "0x%lx", &value) == EOF) {
		if(fclose(fd) == FJT_FAILED){
		}
		ret_err = FJT_RET_ERR5;
		goto FJT_UIO_DMAMEM_FSCANF_ERR;
	}
	dmabuf_uioinfo_g.dma_addr_phys = (void *)value;

	if(fclose(fd) == FJT_FAILED){
		ret_err = FJT_RET_ERR6;
		goto FJT_UIO_DMAMEM_FCLOSE_ERR;
	}

	return FJT_RET_OK;


FJT_UIO_DMAMEM_FCLOSE_ERR:
FJT_UIO_DMAMEM_FSCANF_ERR:
FJT_UIO_DMAMEM_FOPEN_ERR:
FJT_UIO_DMAMEM_CLOSE_ERR:
	ret_munmap = munmap(ret_mmap,FJT_DMAMEM_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
	}
	
FJT_UIO_DMAMEM_MMAP_ERR:
	dmabuf_uioinfo_g.dma_addr      = FJT_NULL;
	dmabuf_uioinfo_g.dma_addr_phys = FJT_NULL;
	
	return ret_err;
}


/****************************************************************************/
/**
 * @brief DMA Buffer UIO Device Acquisition Function
 *
 * Get DMA Buffer UIO Device Information
 *
 * @param[in,out]   dmabuf_uioinfo
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_get_dmabuf_uiodev(fjt_dmabuf_uioinfo_t *dmabuf_uioinfo)
{
	FJT_CHECK_RETURN_ERR(dmabuf_uioinfo, FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(dmabuf_uioinfo_g.dma_addr, FJT_RET_ERR2);
	FJT_CHECK_RETURN_ERR(dmabuf_uioinfo_g.dma_addr_phys, FJT_RET_ERR3);	

	dmabuf_uioinfo->dma_addr      = dmabuf_uioinfo_g.dma_addr;
	dmabuf_uioinfo->dma_addr_phys = dmabuf_uioinfo_g.dma_addr_phys;
	
	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief DMA Buffer UIO Device munmap & close Function
 *
 * munmap & close DMA Buffer UIO Device, delete global variable mmap value
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_close_dmabuf_uiodev()
{
	int ret_munmap;
	
	FJT_CHECK_RETURN_ERR(dmabuf_uioinfo_g.dma_addr, FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(dmabuf_uioinfo_g.dma_addr_phys, FJT_RET_ERR2);	
	
	/* DMA Area */
	ret_munmap = munmap(dmabuf_uioinfo_g.dma_addr,FJT_DMAMEM_AREA_SIZE);
	if(ret_munmap == FJT_FAILED){
		return FJT_RET_ERR3;
	}

	dmabuf_uioinfo_g.dma_addr      = FJT_NULL;
	dmabuf_uioinfo_g.dma_addr_phys = FJT_NULL;
	
	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief MSI Interruption EventFD open Function
 *
 * Create MSI Interruption EventFD, notify it to Driver
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_open_msi_eventfd()
{
	uint8_t	ret_err;
	int	i;
	int	msi_drvfd;
	struct epoll_event ev;

	/*--------------------------------------*/
	/*	Create eventFD						*/
	/*--------------------------------------*/
	for(i = 0 ; i< FJT_MAX_MSI_NUM ; i++){
		msi_info_g.eventfd.fd[i] = eventfd(0,0);
		if( msi_info_g.eventfd.fd[i] == FJT_FAILED ){
			ret_err = FJT_RET_ERR1;
			goto FJT_MSI_EVENTFD_CREATE_ERR;
		}
	}
	
	/*--------------------------------------*/
	/*	epoll Setting						*/
	/*--------------------------------------*/
	/*	Create epollfd as many as Stream Number	*/	
	for( i = 0 ; i < FJT_MAX_STREAM ; i++){
		msi_info_g.epollfd[i] = epoll_create1(0);
		if( msi_info_g.epollfd[i] == FJT_FAILED ){
			ret_err = FJT_RET_ERR2;
			goto FJT_EPOLL_CREATE_ERR;
		}
	}
	/*	Register Monitor Contents	*/
	for(i = FJT_EFD_INDEX_STREAM0_OK ; i <= FJT_EFD_INDEX_STREAM0_NG ; i+=2 ){
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events  = EPOLLIN;
		ev.data.fd = msi_info_g.eventfd.fd[i];
		if(  epoll_ctl(msi_info_g.epollfd[FJT_STREAM0], EPOLL_CTL_ADD, msi_info_g.eventfd.fd[i], &ev) == FJT_FAILED ){
			ret_err = FJT_RET_ERR3;
			goto FJT_EPOLL_CTL_ERR;
		}
	}
	for(i = FJT_EFD_INDEX_STREAM1_OK ; i <= FJT_EFD_INDEX_STREAM1_NG ; i+=2 ){
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events  = EPOLLIN;
		ev.data.fd = msi_info_g.eventfd.fd[i];
		if(  epoll_ctl(msi_info_g.epollfd[FJT_STREAM1], EPOLL_CTL_ADD, msi_info_g.eventfd.fd[i], &ev) == FJT_FAILED ){
			ret_err = FJT_RET_ERR4;
			goto FJT_EPOLL_CTL_ERR;
		}
	}
	
	/*--------------------------------------*/
	/*	Notify eventFD to Driver			*/
	/*--------------------------------------*/
	msi_drvfd = open("/dev/fjt_turbo_chrdev", O_RDWR);
	if( msi_drvfd == FJT_FAILED ){
		ret_err = FJT_RET_ERR5;
		goto FJT_MSI_DRVFD_OPEN_ERR;
	}
	if( ioctl(msi_drvfd, FJT_IOC_SET_EVENTFD, &msi_info_g.eventfd) == FJT_FAILED ){
		ret_err = FJT_RET_ERR6;
		goto FJT_MSI_IOCTL_ERR;
	}
	if( close(msi_drvfd) == FJT_FAILED ){
		ret_err = FJT_RET_ERR7;
		goto FJT_MSI_DRVFD_CLOSE_ERR;
	}
	
	return FJT_RET_OK;
	
	
FJT_MSI_DRVFD_CLOSE_ERR:
FJT_MSI_IOCTL_ERR:
	if( close(msi_drvfd) == FJT_FAILED ){
	}
FJT_MSI_DRVFD_OPEN_ERR:
FJT_EPOLL_CTL_ERR:
FJT_EPOLL_CREATE_ERR:
	for(i = 0 ; i < FJT_MAX_STREAM ; i++){
		if( close(msi_info_g.epollfd[i]) == FJT_FAILED ){
		}
	}
FJT_MSI_EVENTFD_CREATE_ERR:
	for(i = 0 ; i < FJT_MAX_MSI_NUM ; i++){
		if( close(msi_info_g.eventfd.fd[i]) == FJT_FAILED ){
		}
	}
	return ret_err;
}


/****************************************************************************/
/**
 * @brief MSI Interruption EventFD Acquisition Function
 *
 * Get MSI Interruption EventFD
 *
 * @param[in]       msi_eventfd_in
 * @param[out]      none
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_get_msi_eventfd(fjt_msi_info_t *msi_info_in)
{
	int	i;
	
	FJT_CHECK_RETURN_ERR(msi_info_in, FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(msi_info_g.epollfd[0], FJT_RET_ERR2);
	FJT_CHECK_RETURN_ERR(msi_info_g.epollfd[1], FJT_RET_ERR3);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[0], FJT_RET_ERR4);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[1], FJT_RET_ERR5);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[2], FJT_RET_ERR6);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[3], FJT_RET_ERR7);
	
	for(i = 0 ; i < FJT_MAX_STREAM ; i++){
		msi_info_in->epollfd[i] = msi_info_g.epollfd[i];
	}
	
	for(i = 0; i < FJT_MAX_MSI_NUM ; i++){
		msi_info_in->eventfd.fd[i] = msi_info_g.eventfd.fd[i];
	}
	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief MSI Interruption EventFD close Function
 *
 * close MSI Ineterruption EventFD
 *
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          Others             Error
 */
/****************************************************************************/
uint8_t fjt_close_msi_eventfd()
{
	int i;
	
	FJT_CHECK_RETURN_ERR(msi_info_g.epollfd[0], FJT_RET_ERR1);
	FJT_CHECK_RETURN_ERR(msi_info_g.epollfd[1], FJT_RET_ERR2);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[0], FJT_RET_ERR3);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[1], FJT_RET_ERR4);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[2], FJT_RET_ERR5);
	FJT_CHECK_RETURN_ERR(msi_info_g.eventfd.fd[3], FJT_RET_ERR6);
	
	
	/*--------------------------------------*/
	/*	epollFD close						*/
	/*--------------------------------------*/
	for(i = 0 ; i < FJT_MAX_STREAM ; i++){
		if( close(msi_info_g.epollfd[i]) == FJT_FAILED ){
			return FJT_RET_ERR7;
		}
		msi_info_g.epollfd[i] = FJT_NULL;
	}
	
	/*--------------------------------------*/
	/*	EventFD close						*/
	/*--------------------------------------*/
	for(i = 0 ; i < FJT_MAX_MSI_NUM ; i++){
		if( close(msi_info_g.eventfd.fd[i]) == FJT_FAILED ){
			return FJT_RET_ERR8;
		}
		msi_info_g.eventfd.fd[i] = FJT_NULL;
	}
	
	return FJT_RET_OK;
}


/****************************************************************************/
/**
 * @brief FPGA DMA Initiation Function (Read)
 *
 * Initiate FPGA's DMA, and issue Read request
 *
 * @param[in]       src_addr_virt
 * @param[in]       dst_addr_virt
 * @param[in]       size
 * @retval          FJT_RET_OK(0)      Normal End
 * @retval          FJT_RET_ERR(1)     Error
 */
/****************************************************************************/
uint8_t fjt_kick_fpga_dma_read(uint8_t stream_num, uint8_t cb_num, uint64_t src_addr_virt, uint64_t dst_addr_virt, uint32_t size)
{
	fjt_fpga_uioinfo_t		pcie_area;
	fjt_dmabuf_uioinfo_t	dma_uioinfo;
	fjt_fpga_reg_t			*fpga_reg_p;
	uint8_t					ret;
	uint64_t				src_addr_phys;
	uint64_t				dst_addr_phys;
	uint64_t				offset;
	
	
	FJT_TRACE_LOG(stream_num, LOG_LEVEL1, "START");
	FJT_CHECK_FREE_RETURN_ERR( ( stream_num >= FJT_MAX_STREAM ), FJT_RET_ERR, stream_num);
	FJT_CHECK_FREE_RETURN_ERR( ( cb_num >= FJT_MAX_CB_NUM ), FJT_RET_ERR, stream_num);
	FJT_CHECK_FREE_RETURN_ERR( ( src_addr_virt == FJT_NULL ), FJT_RET_ERR, stream_num);
	FJT_CHECK_FREE_RETURN_ERR( ( (size == FJT_NULL) || (size > sizeof(fjt_fpga_indt_t)) ), FJT_RET_ERR, stream_num);
	
	/*--------------------------------------*/
	/* Get UIO Information                  */
	/*--------------------------------------*/
	ret = fjt_get_fpga_uiodev(&pcie_area);
	if(ret != FJT_RET_OK) {
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Failed to get fpga uioinfo. ret=%d errno=%d",ret, errno);
		FJT_TRACE_LOG(stream_num, LOG_LEVEL1, "END(ERR)");
		return FJT_RET_ERR;
	}
	ret = fjt_get_dmabuf_uiodev(&dma_uioinfo);
	if(ret != FJT_RET_OK) {
		FJT_ILG_LOG(stream_num, LOG_LEVEL1, "Failed to get dma uioinfo. ret=%d errno=%d",ret, errno);
		FJT_TRACE_LOG(stream_num, LOG_LEVEL1, "END(ERR)");
		return FJT_RET_ERR;
	}
	
	FJT_CHECK_FREE_RETURN_ERR( ( dst_addr_virt < (uint64_t)pcie_area.pcie_addr[FJT_FPGA_UIOINFO_RAM] ), FJT_RET_ERR, stream_num);
	FJT_CHECK_FREE_RETURN_ERR( ( src_addr_virt < (uint64_t)dma_uioinfo.dma_addr ), FJT_RET_ERR, stream_num);
	
	/*----------------------------------------------*/
	/* Calculate FPGA RAM Space Physical Address	*/
	/*----------------------------------------------*/
	offset              = dst_addr_virt - (uint64_t)pcie_area.pcie_addr[FJT_FPGA_UIOINFO_RAM];
	dst_addr_phys       = (uint64_t)pcie_area.pcie_addr_phys[FJT_FPGA_UIOINFO_RAM] + offset;

	/*----------------------------------------------*/
	/* Calculate DMA Memory Physical Address		*/
	/*----------------------------------------------*/
	offset              = src_addr_virt - (uint64_t)dma_uioinfo.dma_addr;
	src_addr_phys       = (uint64_t)dma_uioinfo.dma_addr_phys + offset;
	
	/*----------------------------------------------*/
	/* Initiate DMA									*/
	/*----------------------------------------------*/
	fpga_reg_p = (fjt_fpga_reg_t *)pcie_area.pcie_addr[FJT_FPGA_UIOINFO_REGISTER];
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_SRC_CB_L  = (uint32_t)src_addr_phys;
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_SRC_CB_H  = (uint32_t)(src_addr_phys>>32);
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_DEST_CB_L = (uint32_t)dst_addr_phys;
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_DEST_CB_H = (uint32_t)(dst_addr_phys>>32);
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_CB_SIZE   = size;
	wmb();
	fpga_reg_p->inset[stream_num].dma[cb_num].IST_CB_TRG    = FJT_IST_CB_TRG_ON;
	
	FJT_STATISTICS_LOG(stream_num, API_REQ, INF1);
	FJT_TRACE_LOG(stream_num, LOG_LEVEL1, "END(OK)");
	return FJT_RET_OK;
}

