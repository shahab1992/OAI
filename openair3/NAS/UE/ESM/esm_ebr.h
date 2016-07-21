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
Source      esm_ebr.h

Version     0.1

Date        2013/01/29

Product     NAS stack

Subsystem   EPS Session Management

Author      Frederic Maurel

Description Defines functions used to handle state of EPS bearer contexts
        and manage ESM messages re-transmission.

*****************************************************************************/
#ifndef __ESM_EBR_H__
#define __ESM_EBR_H__

#include "OctetString.h"

#include "networkDef.h"
#include "esmData.h"

#include "nas_timer.h"
#include "user_defs.h"

/****************************************************************************/
/*********************  G L O B A L    C O N S T A N T S  *******************/
/****************************************************************************/

/* Unassigned EPS bearer identity value */
#define ESM_EBI_UNASSIGNED  (EPS_BEARER_IDENTITY_UNASSIGNED)

/****************************************************************************/
/************************  G L O B A L    T Y P E S  ************************/
/****************************************************************************/

/*
 * User notification callback, executed whenever a change of status with
 * respect of PDN connection or EPS bearer context is notified by the EPS
 * Session Management sublayer
 */
typedef int (*esm_indication_callback_t) (user_api_id_t *user_api_id, int, network_pdn_state_t);

/****************************************************************************/
/********************  G L O B A L    V A R I A B L E S  ********************/
/****************************************************************************/

/****************************************************************************/
/******************  E X P O R T E D    F U N C T I O N S  ******************/
/****************************************************************************/

void esm_ebr_register_callback(esm_indication_callback_t cb);

int esm_ebr_is_reserved(esm_ebr_data_t *esm_ebr_data, int ebi);

esm_ebr_data_t *esm_ebr_initialize(void);
int esm_ebr_assign(esm_ebr_data_t *esm_ebr_data, int ebi, int cid, int default_ebr);
int esm_ebr_release(esm_ebr_data_t *esm_ebr_data, int ebi);

int esm_ebr_set_status(user_api_id_t *user_api_id, esm_ebr_data_t *esm_ebr_data, int ebi, esm_ebr_state status, int ue_requested);
esm_ebr_state esm_ebr_get_status(esm_ebr_data_t *esm_ebr_data, int ebi);

int esm_ebr_is_not_in_use(esm_ebr_data_t *esm_ebr_data, int ebi);

#endif /* __ESM_EBR_H__*/
