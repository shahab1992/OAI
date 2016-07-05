/*******************************************************************************
    OpenAirInterface
    Copyright(c) 1999 - 2014 Eurecom

    OpenAirInterface is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.


    OpenAirInterface is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenAirInterface.The full GNU General Public License is
   included in this distribution in the file called "COPYING". If not,
   see <http://www.gnu.org/licenses/>.

  Contact Information
  OpenAirInterface Admin: openair_admin@eurecom.fr
  OpenAirInterface Tech : openair_tech@eurecom.fr
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Compus SophiaTech 450, route des chappes, 06451 Biot, France.

 *******************************************************************************/
/*****************************************************************************
Source      nas_proc.h

Version     0.1

Date        2012/09/20

Product     NAS stack

Subsystem   NAS main process

Author      Frederic Maurel

Description NAS procedure call manager

*****************************************************************************/
#ifndef __NAS_PROC_H__
#define __NAS_PROC_H__

#include "commonDef.h"
#include "networkDef.h"
#include "user_defs.h"
#include "emm_main.h"
#include "esm_ebr.h"

/****************************************************************************/
/*********************  G L O B A L    C O N S T A N T S  *******************/
/****************************************************************************/

/****************************************************************************/
/************************  G L O B A L    T Y P E S  ************************/
/****************************************************************************/

/****************************************************************************/
/********************  G L O B A L    V A R I A B L E S  ********************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

void nas_proc_initialize(nas_user_t *user, emm_indication_callback_t emm_cb,
                         esm_indication_callback_t esm_cb, const char *imei);

void nas_proc_cleanup(nas_user_t *user);

/*
 * --------------------------------------------------------------------------
 *          NAS procedures triggered by the user
 * --------------------------------------------------------------------------
 */

int nas_proc_enable_s1_mode(nas_user_t *user);
int nas_proc_disable_s1_mode(nas_user_t *user);
int nas_proc_get_eps(nas_user_t *user, int *stat);

int nas_proc_get_imsi(char *imsi_str);
int nas_proc_get_msisdn(char *msisdn_str, int *ton_npi);

int nas_proc_get_signal_quality(nas_user_t *user, int *rsrq, int *rsrp);

int nas_proc_register(nas_user_t *user, int mode, int format, const network_plmn_t *oper, int AcT);
int nas_proc_deregister(nas_user_t *user);
int nas_proc_get_reg_data(nas_user_t *user, int *mode, int *selected, int format,
                          network_plmn_t *oper, int *AcT);
int nas_proc_get_oper_list(nas_user_t *user, const char **oper_list);

int nas_proc_get_reg_status(nas_user_t *user, int *stat);
int nas_proc_get_loc_info(nas_user_t *user, char *tac, char *ci, int *AcT);

int nas_proc_detach(nas_user_t *user, int switch_off);
int nas_proc_attach(nas_user_t *user);
int nas_proc_get_attach_status(nas_user_t *user);

int nas_proc_reset_pdn(nas_user_t *user, int cid);
int nas_proc_set_pdn(nas_user_t *user, int cid, int type, const char *apn, int ipv4_addr,
                     int emergency, int p_cscf, int im_cn_signal);
int nas_proc_get_pdn_range(nas_user_t *user);
int nas_proc_get_pdn_status(nas_user_t *user, int *cids, int *states, int n_pdn_max);
int nas_proc_get_pdn_param(nas_user_t *user, int *cids, int *types, const char **apns,
                           int n_pdn_max);
int nas_proc_get_pdn_addr(nas_user_t *user, int cid, int *cids, const char **addr1,
                          const char **addr2, int n_addr_max);
int nas_proc_deactivate_pdn(nas_user_t *user, int cid);
int nas_proc_activate_pdn(nas_user_t *user, int cid);

/*
 * --------------------------------------------------------------------------
 *      NAS procedures triggered by the network
 * --------------------------------------------------------------------------
 */

int nas_proc_cell_info(nas_user_t *user, int found, tac_t tac, ci_t ci, AcT_t rat, uint8_t rsrp,
                       uint8_t rsrq);

int nas_proc_establish_cnf(nas_user_t *user, const Byte_t *data, uint32_t len);
int nas_proc_establish_rej(nas_user_t *user);

int nas_proc_release_ind(nas_user_t *user, int cause);

int nas_proc_ul_transfer_cnf(nas_user_t *user);
int nas_proc_ul_transfer_rej(nas_user_t *user);
int nas_proc_dl_transfer_ind(nas_user_t *user, const Byte_t *data, uint32_t len);



#endif /* __NAS_PROC_H__*/
