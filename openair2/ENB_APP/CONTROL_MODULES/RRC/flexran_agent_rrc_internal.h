/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */ 

/*! \file flexran_agent_mac_internal.c
 * \brief Helper functions for the RRC agent
 * \author shahab SHARIAT BAGHERI
 * \date 2017
 * \version 0.1
 */

#include <pthread.h>

#include <yaml.h>

#include "flexran_agent_common.h"
#include "flexran_agent_defs.h"


int parse_rrc_config(mid_t mod_id, yaml_parser_t *parser);

int parse_handover_config(mid_t mod_id, yaml_parser_t *parser);

int parse_handover_parameters(mid_t mod_id, yaml_parser_t *parser);