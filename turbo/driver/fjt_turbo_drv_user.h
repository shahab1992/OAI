/****************************************************************************/
/**
 * @file  fjt_turbo_drv_user.h
 * @brief Turbo decoding driver UserReference Header file
 */
/****************************************************************************/
#ifndef __FJT_TURBO_DRV_USER_H__
#define __FJT_TURBO_DRV_USER_H__

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif


/****************************************************************************/
/* macro                                                                    */
/****************************************************************************/
#define FJT_MAX_MSI_NUM				(4)

#define FJT_CMD_SET_EVENTFD			(0)

#define FJT_IOC_MAGIC				(0x77)
#define FJT_IOC_SET_EVENTFD		_IO(FJT_IOC_MAGIC, FJT_CMD_SET_EVENTFD)

#define FJT_EFD_INDEX_STREAM0_OK	(0)
#define FJT_EFD_INDEX_STREAM0_NG	(2)
#define FJT_EFD_INDEX_STREAM1_OK	(1)
#define FJT_EFD_INDEX_STREAM1_NG	(3)

/****************************************************************************/
/* typedef                                                                  */
/****************************************************************************/
typedef struct tag_fjt_eventfd {
	int	fd[FJT_MAX_MSI_NUM];
} fjt_eventfd_t;


#endif /* __FJT_TURBO_DRV_USER_H__ */
