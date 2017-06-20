/****************************************************************************/
/**
 * @file  fjt_turbo_api_extern.h
 * @brief Turbo decoding API External References
 * 
 */
/****************************************************************************/
#ifndef __FJT_TURBO_API_EXT_H__
#define __FJT_TURBO_API_EXT_H__

#include "fjt_turbo_api.h"

/****************************************************************************/
/* extern                                                                   */
/****************************************************************************/
extern uint8_t fjt_api_turbodec_init();

extern uint8_t fjt_api_turbodec_req(fjt_turbodec_inparam_t *param_p);

extern uint8_t fjt_api_turbodec_end_wait(uint8_t mode, uint8_t stream_num, uint32_t *status);

extern uint8_t fjt_api_turbodec_get_result(uint8_t stream_num, fjt_turbodec_outparam_t *param);


#endif /* __FJT_TURBO_API_EXT_H__ */
