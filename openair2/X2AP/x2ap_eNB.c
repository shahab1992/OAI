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

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

*******************************************************************************/

/*! \file x2ap_eNB.c
 * \brief x2ap protocol for eNB
 * \author Navid Nikaein
 * \date 2014 - 2015
 * \version 1.0
 * \company Eurecom
 * \email: navid.nikaein@eurecom.fr
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "intertask_interface.h"

#include "assertions.h"
#include "queue.h"

#include "x2ap_eNB.h"
#include "x2ap_eNB_defs.h"
#include "x2ap_eNB_decoder.h"
#include "x2ap_eNB_handler.h"
#include "x2ap_ies_defs.h"
#include "x2ap_eNB_management_procedures.h"
#include "x2ap_eNB_generate_messages.h"

#include "msc.h"

#include "assertions.h"
#include "conversions.h"

struct x2ap_enb_map;
struct x2ap_eNB_data_s;

RB_PROTOTYPE(x2ap_enb_map, x2ap_eNB_data_s, entry, x2ap_eNB_compare_assoc_id);

static
void x2ap_eNB_handle_register_eNB(instance_t instance,
				  x2ap_register_enb_req_t *x2ap_register_eNB);

static
void x2ap_eNB_handle_handover_req(instance_t instance,
				  x2ap_handover_req_t *x2ap_handover_req);

static
void x2ap_eNB_handle_handover_req_ack(instance_t instance,
				      x2ap_handover_req_ack_t *x2ap_handover_req_ack);

static
void x2ap_eNB_register_eNB(x2ap_eNB_instance_t *instance_p,
			   net_ip_address_t    *target_eNB_ip_addr,
			   net_ip_address_t    *local_ip_addr,
			   uint16_t             in_streams,
			   uint16_t             out_streams);
static
void x2ap_eNB_handle_sctp_association_resp(instance_t instance,
					   sctp_new_association_resp_t *sctp_new_association_resp);


static
void x2ap_eNB_handle_sctp_data_ind(instance_t instance,
				   sctp_data_ind_t *sctp_data_ind) {

  int result;

  DevAssert(sctp_data_ind != NULL);

  x2ap_eNB_handle_message(sctp_data_ind->assoc_id, sctp_data_ind->stream,
                          sctp_data_ind->buffer, sctp_data_ind->buffer_length);

  result = itti_free(TASK_UNKNOWN, sctp_data_ind->buffer);
  AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);

}

static
void x2ap_eNB_handle_sctp_association_resp(instance_t instance, sctp_new_association_resp_t *sctp_new_association_resp)
{
  x2ap_eNB_instance_t *instance_p;
  x2ap_eNB_data_t *x2ap_enb_data_p;

  DevAssert(sctp_new_association_resp != NULL);

printf("x2ap_eNB_handle_sctp_association_resp at 1\n");
dump_trees();

  instance_p = x2ap_eNB_get_instance(instance);
  DevAssert(instance_p != NULL);

  /* if the assoc_id is already known, it is certainly because an IND was received
   * before. In this case, just update streams and return
   */
  if (sctp_new_association_resp->assoc_id != -1) {
    x2ap_enb_data_p = x2ap_get_eNB(instance_p, sctp_new_association_resp->assoc_id,
                                       sctp_new_association_resp->ulp_cnx_id);
    if (x2ap_enb_data_p != NULL) {
      /* some sanity check - to be refined at some point */
      if (sctp_new_association_resp->sctp_state != SCTP_STATE_ESTABLISHED) {
        X2AP_ERROR("x2ap_enb_data_p not NULL and sctp state not SCTP_STATE_ESTABLISHED, what to do?\n");
        abort();
      }
      x2ap_enb_data_p->in_streams  = sctp_new_association_resp->in_streams;
      x2ap_enb_data_p->out_streams = sctp_new_association_resp->out_streams;
      return;
    }
  }

  x2ap_enb_data_p = x2ap_get_eNB(instance_p, -1,
                                     sctp_new_association_resp->ulp_cnx_id);
  DevAssert(x2ap_enb_data_p != NULL);

printf("x2ap_eNB_handle_sctp_association_resp at 2\n");
dump_trees();

  if (sctp_new_association_resp->sctp_state != SCTP_STATE_ESTABLISHED) {
    X2AP_WARN("Received unsuccessful result for SCTP association (%u), instance %d, cnx_id %u\n",
              sctp_new_association_resp->sctp_state,
              instance,
              sctp_new_association_resp->ulp_cnx_id);

    x2ap_handle_x2_setup_message(x2ap_enb_data_p,
				 sctp_new_association_resp->sctp_state == SCTP_STATE_SHUTDOWN);

    return;
  }

printf("x2ap_eNB_handle_sctp_association_resp at 3\n");
dump_trees();

  /* Update parameters */
  x2ap_enb_data_p->assoc_id    = sctp_new_association_resp->assoc_id;
  x2ap_enb_data_p->in_streams  = sctp_new_association_resp->in_streams;
  x2ap_enb_data_p->out_streams = sctp_new_association_resp->out_streams;

printf("x2ap_eNB_handle_sctp_association_resp at 4\n");
dump_trees();

  /* Prepare new x2 Setup Request */
  x2ap_eNB_generate_x2_setup_request(instance_p, x2ap_enb_data_p);
}

static
void x2ap_eNB_handle_sctp_association_ind(instance_t instance, sctp_new_association_ind_t *sctp_new_association_ind)
{
  x2ap_eNB_instance_t *instance_p;
  x2ap_eNB_data_t *x2ap_enb_data_p;

printf("x2ap_eNB_handle_sctp_association_ind at 1 (called for instance %d)\n", instance);
dump_trees();
  DevAssert(sctp_new_association_ind != NULL);

  instance_p = x2ap_eNB_get_instance(instance);
  DevAssert(instance_p != NULL);

  x2ap_enb_data_p = x2ap_get_eNB(instance_p, sctp_new_association_ind->assoc_id, -1);
  if (x2ap_enb_data_p != NULL) abort();
//  DevAssert(x2ap_enb_data_p != NULL);
  if (x2ap_enb_data_p == NULL) {
    /* Create new eNB descriptor */
    x2ap_enb_data_p = calloc(1, sizeof(*x2ap_enb_data_p));
    DevAssert(x2ap_enb_data_p != NULL);

    x2ap_enb_data_p->cnx_id                = x2ap_eNB_fetch_add_global_cnx_id();

    x2ap_enb_data_p->x2ap_eNB_instance = instance_p;

    /* Insert the new descriptor in list of known eNB
     * but not yet associated.
     */
    RB_INSERT(x2ap_enb_map, &instance_p->x2ap_enb_head, x2ap_enb_data_p);
    x2ap_enb_data_p->state = X2AP_ENB_STATE_CONNECTED;
    instance_p->x2_target_enb_nb++;
    if (instance_p->x2_target_enb_pending_nb > 0) {
      instance_p->x2_target_enb_pending_nb--;
    }
  } else {
    X2AP_WARN("x2ap_enb_data_p already exists\n");
  }

printf("x2ap_eNB_handle_sctp_association_ind at 2\n");
dump_trees();
  /* Update parameters */
  x2ap_enb_data_p->assoc_id    = sctp_new_association_ind->assoc_id;
  x2ap_enb_data_p->in_streams  = sctp_new_association_ind->in_streams;
  x2ap_enb_data_p->out_streams = sctp_new_association_ind->out_streams;

printf("x2ap_eNB_handle_sctp_association_ind at 3\n");
dump_trees();
}

int x2ap_eNB_init_sctp (x2ap_eNB_instance_t *instance_p,
			net_ip_address_t    *local_ip_addr)
{
  // Create and alloc new message
  MessageDef                             *message;
  sctp_init_t                            *sctp_init  = NULL;

  DevAssert(instance_p != NULL);
  DevAssert(local_ip_addr != NULL);

  message = itti_alloc_new_message (TASK_X2AP, SCTP_INIT_MSG);
  sctp_init = &message->ittiMsg.sctp_init;

  sctp_init->port = X2AP_PORT_NUMBER;
  sctp_init->ppid = X2AP_SCTP_PPID;
  sctp_init->ipv4 = 1;
  sctp_init->ipv6 = 0;
  sctp_init->nb_ipv4_addr = 1;

#if 0
  memcpy(&sctp_init->ipv4_address,
         local_ip_addr,
         sizeof(*local_ip_addr));
#endif
  sctp_init->ipv4_address[0] = inet_addr(local_ip_addr->ipv4_address);

  /*
   * SR WARNING: ipv6 multi-homing fails sometimes for localhost.
   * * * * Disable it for now.
   */
  sctp_init->nb_ipv6_addr = 0;
  sctp_init->ipv6_address[0] = "0:0:0:0:0:0:0:1";

  return itti_send_msg_to_task (TASK_SCTP, instance_p->instance, message);

}

static void x2ap_eNB_register_eNB(x2ap_eNB_instance_t *instance_p,
                                  net_ip_address_t    *target_eNB_ip_address,
                                  net_ip_address_t    *local_ip_addr,
                                  uint16_t             in_streams,
                                  uint16_t             out_streams)
{

  MessageDef                 *message                   = NULL;
  sctp_new_association_req_t *sctp_new_association_req  = NULL;
  x2ap_eNB_data_t            *x2ap_enb_data             = NULL;

  DevAssert(instance_p != NULL);
  DevAssert(target_eNB_ip_address != NULL);

  message = itti_alloc_new_message(TASK_X2AP, SCTP_NEW_ASSOCIATION_REQ);

  sctp_new_association_req = &message->ittiMsg.sctp_new_association_req;

  sctp_new_association_req->port = X2AP_PORT_NUMBER;
  sctp_new_association_req->ppid = X2AP_SCTP_PPID;

  sctp_new_association_req->in_streams  = in_streams;
  sctp_new_association_req->out_streams = out_streams;

  memcpy(&sctp_new_association_req->remote_address,
         target_eNB_ip_address,
         sizeof(*target_eNB_ip_address));

  memcpy(&sctp_new_association_req->local_address,
         local_ip_addr,
         sizeof(*local_ip_addr));

  /* Create new eNB descriptor */
  x2ap_enb_data = calloc(1, sizeof(*x2ap_enb_data));
  DevAssert(x2ap_enb_data != NULL);

  x2ap_enb_data->cnx_id                = x2ap_eNB_fetch_add_global_cnx_id();
  sctp_new_association_req->ulp_cnx_id = x2ap_enb_data->cnx_id;

  x2ap_enb_data->assoc_id          = -1;
  x2ap_enb_data->x2ap_eNB_instance = instance_p;

  /* Insert the new descriptor in list of known eNB
   * but not yet associated.
   */
  RB_INSERT(x2ap_enb_map, &instance_p->x2ap_enb_head, x2ap_enb_data);
  x2ap_enb_data->state = X2AP_ENB_STATE_WAITING;
  instance_p->x2_target_enb_nb ++;
  instance_p->x2_target_enb_pending_nb ++;

  itti_send_msg_to_task(TASK_SCTP, instance_p->instance, message);
}

static
void x2ap_eNB_handle_register_eNB(instance_t instance,
				  x2ap_register_enb_req_t *x2ap_register_eNB)
{

  x2ap_eNB_instance_t *new_instance;
  uint8_t index;

  DevAssert(x2ap_register_eNB != NULL);

  /* Look if the provided instance already exists */
  new_instance = x2ap_eNB_get_instance(instance);

  if (new_instance != NULL) {
    /* Checks if it is a retry on the same eNB */
    DevCheck(new_instance->eNB_id == x2ap_register_eNB->eNB_id, new_instance->eNB_id, x2ap_register_eNB->eNB_id, 0);
    DevCheck(new_instance->cell_type == x2ap_register_eNB->cell_type, new_instance->cell_type, x2ap_register_eNB->cell_type, 0);
    DevCheck(new_instance->tac == x2ap_register_eNB->tac, new_instance->tac, x2ap_register_eNB->tac, 0);
    DevCheck(new_instance->mcc == x2ap_register_eNB->mcc, new_instance->mcc, x2ap_register_eNB->mcc, 0);
    DevCheck(new_instance->mnc == x2ap_register_eNB->mnc, new_instance->mnc, x2ap_register_eNB->mnc, 0);

  }
  else {
    new_instance = calloc(1, sizeof(x2ap_eNB_instance_t));
    DevAssert(new_instance != NULL);

    RB_INIT(&new_instance->x2ap_enb_head);

    /* Copy usefull parameters */
    new_instance->instance         = instance;
    new_instance->eNB_name         = x2ap_register_eNB->eNB_name;
    new_instance->eNB_id           = x2ap_register_eNB->eNB_id;
    new_instance->cell_type        = x2ap_register_eNB->cell_type;
    new_instance->tac              = x2ap_register_eNB->tac;
    new_instance->mcc              = x2ap_register_eNB->mcc;
    new_instance->mnc              = x2ap_register_eNB->mnc;
    new_instance->mnc_digit_length = x2ap_register_eNB->mnc_digit_length;

    /* Add the new instance to the list of eNB (meaningfull in virtual mode) */
    x2ap_eNB_insert_new_instance(new_instance);

    X2AP_INFO("Registered new eNB[%d] and %s eNB id %u\n",
               instance,
               x2ap_register_eNB->cell_type == CELL_MACRO_ENB ? "macro" : "home",
               x2ap_register_eNB->eNB_id);
  }

  DevCheck(x2ap_register_eNB->nb_x2 <= X2AP_MAX_NB_ENB_IP_ADDRESS,
           X2AP_MAX_NB_ENB_IP_ADDRESS, x2ap_register_eNB->nb_x2, 0);


  /* Trying to connect to the provided list of eNB ip address */

  for (index = 0; index < x2ap_register_eNB->nb_x2; index++) {

    if (x2ap_register_eNB->target_enb_x2_ip_address[index].active == 1 ){
      X2AP_INFO("eNB[%d] eNB id %u acting as an initiator (client)\n",
		instance, x2ap_register_eNB->eNB_id);
      x2ap_eNB_register_eNB(new_instance,
			    &x2ap_register_eNB->target_enb_x2_ip_address[index],
			    &x2ap_register_eNB->enb_x2_ip_address,
			    x2ap_register_eNB->sctp_in_streams,
			    x2ap_register_eNB->sctp_out_streams);
    }
    else {
      /* initiate the SCTP listener */
      if (x2ap_eNB_init_sctp(new_instance,&x2ap_register_eNB->enb_x2_ip_address) <  0 ) {
	X2AP_ERROR ("Error while sending SCTP_INIT_MSG to SCTP \n");
	return;
      }
      X2AP_INFO("eNB[%d] eNB id %u acting as a listner (server)\n",
		instance, x2ap_register_eNB->eNB_id);
    }
  }

}

/* TODO: remove this hack, it's to map rnti to X2 ID in the source eNB, only 1 UE supported */
/* so we directly use index 0, no search no lock no nothing */
int x2id_to_source_rnti[1];

/* TODO: remove that, it's for the hack below */
#include "enb_config.h"

static
void x2ap_eNB_handle_handover_req(instance_t instance,
				  x2ap_handover_req_t *x2ap_handover_req)
{
  /* TODO: remove this hack (the goal is to find the correct
   * eNodeB structure for the target) - we need a proper way for RRC
   * and X2AP to identify eNodeBs
   * RRC knows about mod_id and X2AP knows about eNB_id (eNB_ID in
   * the configuration file)
   * as far as I understand.. CROUX
   */
  x2ap_eNB_instance_t *instance_p;
  x2ap_eNB_data_t     *target;
  const Enb_properties_array_t *enb_properties = enb_config_get();
  int target_enb_id = enb_properties->properties[x2ap_handover_req->target_mod_id]->eNB_id;

  instance_p = x2ap_eNB_get_instance(instance);
  DevAssert(instance_p != NULL);

  target = x2ap_is_eNB_id_in_list(target_enb_id);
  DevAssert(target != NULL);

  /* store rnti at index 0 */
  x2id_to_source_rnti[0] = x2ap_handover_req->source_rnti;
  x2ap_eNB_generate_x2_handover_request(instance_p, target, 0);
}

static
void x2ap_eNB_handle_handover_req_ack(instance_t instance,
				      x2ap_handover_req_ack_t *x2ap_handover_req_ack)
{
  /* TODO: remove this hack (the goal is to find the correct
   * eNodeB structure for the other end) - we need a proper way for RRC
   * and X2AP to identify eNodeBs
   * RRC knows about mod_id and X2AP knows about eNB_id (eNB_ID in
   * the configuration file)
   * as far as I understand.. CROUX
   */
  x2ap_eNB_instance_t *instance_p;
  x2ap_eNB_data_t     *target;
  const Enb_properties_array_t *enb_properties = enb_config_get();
  int target_enb_id = enb_properties->properties[x2ap_handover_req_ack->target_mod_id]->eNB_id;

  instance_p = x2ap_eNB_get_instance(instance);
  DevAssert(instance_p != NULL);

  target = x2ap_is_eNB_id_in_list(target_enb_id);
  DevAssert(target != NULL);

  x2ap_eNB_generate_x2_handover_req_ack(instance_p, target, x2ap_handover_req_ack->source_x2id);
}

void *x2ap_task(void *arg)
{
  MessageDef *received_msg = NULL;
  int         result;

  X2AP_DEBUG("Starting X2AP layer\n");

  x2ap_eNB_prepare_internal_data();

  itti_mark_task_ready(TASK_X2AP);

  while (1) {
    itti_receive_msg(TASK_X2AP, &received_msg);

    switch (ITTI_MSG_ID(received_msg)) {
    case TERMINATE_MESSAGE:
      itti_exit_task();
      break;

    case X2AP_REGISTER_ENB_REQ:
      x2ap_eNB_handle_register_eNB(ITTI_MESSAGE_GET_INSTANCE(received_msg),
                                   &X2AP_REGISTER_ENB_REQ(received_msg));
      break;

    case X2AP_HANDOVER_REQ:
      x2ap_eNB_handle_handover_req(ITTI_MESSAGE_GET_INSTANCE(received_msg),
                                   &X2AP_HANDOVER_REQ(received_msg));
      break;

    case X2AP_HANDOVER_REQ_ACK:
      x2ap_eNB_handle_handover_req_ack(ITTI_MESSAGE_GET_INSTANCE(received_msg),
				       &X2AP_HANDOVER_REQ_ACK(received_msg));
      break;

    case SCTP_NEW_ASSOCIATION_RESP:
      x2ap_eNB_handle_sctp_association_resp(ITTI_MESSAGE_GET_INSTANCE(received_msg),
					    &received_msg->ittiMsg.sctp_new_association_resp);
      break;

    case SCTP_NEW_ASSOCIATION_IND:
      x2ap_eNB_handle_sctp_association_ind(ITTI_MESSAGE_GET_INSTANCE(received_msg),
					   &received_msg->ittiMsg.sctp_new_association_ind);
      break;

    case SCTP_DATA_IND:
      x2ap_eNB_handle_sctp_data_ind(ITTI_MESSAGE_GET_INSTANCE(received_msg),
				    &received_msg->ittiMsg.sctp_data_ind);
      break;

    default:
      X2AP_ERROR("Received unhandled message: %d:%s\n",
                 ITTI_MSG_ID(received_msg), ITTI_MSG_NAME(received_msg));
      break;
    }

    result = itti_free (ITTI_MSG_ORIGIN_ID(received_msg), received_msg);
    AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);

    received_msg = NULL;
  }

  return NULL;
}


