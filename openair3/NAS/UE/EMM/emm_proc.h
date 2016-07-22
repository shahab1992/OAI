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
Source      emm_proc.h

Version     0.1

Date        2012/10/16

Product     NAS stack

Subsystem   EPS Mobility Management

Author      Frederic Maurel

Description Defines the EPS Mobility Management procedures executed at
        the EMM Service Access Points.

*****************************************************************************/
#ifndef __EMM_PROC_H__
#define __EMM_PROC_H__

#include "commonDef.h"
#include "OctetString.h"
#include "LowerLayer.h"
#include "user_defs.h"

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

/*
 *---------------------------------------------------------------------------
 *              EMM status procedure
 *---------------------------------------------------------------------------
 */
int emm_proc_status_ind(unsigned int ueid, int emm_cause);
int emm_proc_status(nas_user_t *user, int emm_cause);

/*
 *---------------------------------------------------------------------------
 *              Lower layer procedure
 *---------------------------------------------------------------------------
 */
int emm_proc_lowerlayer_initialize(lowerlayer_success_callback_t success,
                                   lowerlayer_failure_callback_t failure,
                                   lowerlayer_release_callback_t release,
                                   void *args);
int emm_proc_lowerlayer_success(void);
int emm_proc_lowerlayer_failure(int is_initial);
int emm_proc_lowerlayer_release(void);

/*
 *---------------------------------------------------------------------------
 *              UE's Idle mode procedure
 *---------------------------------------------------------------------------
 */
int emm_proc_initialize(nas_user_t *user);
int emm_proc_plmn_selection(nas_user_t *user, int index);
int emm_proc_plmn_selection_end(nas_user_t *user, int found, tac_t tac, ci_t ci, AcT_t rat);

/*
 * --------------------------------------------------------------------------
 *              Attach procedure
 * --------------------------------------------------------------------------
 */
int emm_proc_attach(nas_user_t *user, emm_proc_attach_type_t type);
int emm_proc_attach_request(void *args);
int emm_proc_attach_accept(nas_user_t *user, long T3412, long T3402, long T3423, int n_tais,
                           tai_t *tai, GUTI_t *guti, int n_eplmns, plmn_t *eplmn,
                           const OctetString *esm_msg);
int emm_proc_attach_reject(nas_user_t *user, int emm_cause, const OctetString *esm_msg);
int emm_proc_attach_complete(void *args);
int emm_proc_attach_failure(int is_initial, void *args);
int emm_proc_attach_release(void *args);
int emm_proc_attach_restart(nas_user_t *user);

int emm_proc_attach_set_emergency(emm_data_t *emm_data);
int emm_proc_attach_set_detach(void *user);



/*
 * --------------------------------------------------------------------------
 *              Detach procedure
 * --------------------------------------------------------------------------
 */
int emm_proc_detach(nas_user_t *user, emm_proc_detach_type_t type, int switch_off);
int emm_proc_detach_request(void *args);
int emm_proc_detach_accept(void *args);
int emm_proc_detach_failure(int is_initial, void *args);
int emm_proc_detach_release(void *args);


/*
 * --------------------------------------------------------------------------
 *              Identification procedure
 * --------------------------------------------------------------------------
 */
int emm_proc_identification_request(nas_user_t *user, emm_proc_identity_type_t type);


/*
 * --------------------------------------------------------------------------
 *              Authentication procedure
 * --------------------------------------------------------------------------
 */
int emm_proc_authentication_request(nas_user_t *user, int native_ksi, int ksi,
                                    const OctetString *rand, const OctetString *autn);
int emm_proc_authentication_reject(nas_user_t *user);
int emm_proc_authentication_delete(nas_user_t *user);


/*
 * --------------------------------------------------------------------------
 *          Security mode control procedure
 * --------------------------------------------------------------------------
 */
int emm_proc_security_mode_command(nas_user_t *user, int native_ksi, int ksi, int seea, int seia,
                                   int reea, int reia);

/*
 *---------------------------------------------------------------------------
 *             Network indication handlers
 *---------------------------------------------------------------------------
 */
int emm_proc_registration_notify(user_api_id_t *user_api_id, emm_data_t *emm_data, Stat_t status);
int emm_proc_location_notify(user_api_id_t *user_api_id, emm_data_t *emm_data, tac_t tac, ci_t ci, AcT_t rat);
int emm_proc_network_notify(emm_plmn_list_t *emm_plmn_list, user_api_id_t *user_api_id, emm_data_t *emm_data, int index);

#endif /* __EMM_PROC_H__*/
