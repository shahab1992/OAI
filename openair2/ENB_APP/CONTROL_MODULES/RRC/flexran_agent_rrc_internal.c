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


#include <string.h>
#include <dlfcn.h>

#include "flexran_agent_common_internal.h"
#include "flexran_agent_rrc_internal.h"

int parse_rrc_config(mid_t mod_id, yaml_parser_t *parser) {
  
  yaml_event_t event;
  
  int done = 0;

  int sequence_started = 0;
  int mapping_started = 0;

  while (!done) {

    if (!yaml_parser_parse(parser, &event))
      goto error;
   
    switch (event.type) {
    case YAML_SEQUENCE_START_EVENT:
    /*TODO ENB_APP*/
      // LOG_D(ENB_APP, "A sequence just started as expected\n");
      sequence_started = 1;
      break;
    case YAML_SEQUENCE_END_EVENT:
      // LOG_D(ENB_APP, "A sequence ended\n");
      sequence_started = 0;
      break;
    case YAML_MAPPING_START_EVENT:
      if (!sequence_started) {
	goto error;
      }
      // LOG_D(ENB_APP, "A mapping started\n");
      mapping_started = 1;
      break;
    case YAML_MAPPING_END_EVENT:
      if (!mapping_started) {
	goto error;
      }
      // LOG_D(ENB_APP, "A mapping ended\n");
      mapping_started = 0;
      break;
    case YAML_SCALAR_EVENT:
      if (!mapping_started) {
	goto error;
      }
      // Check the types of subsystems offered and handle their values accordingly
      if (strcmp((char *) event.data.scalar.value, "x2_ho") == 0) {
	// LOG_D(ENB_APP, "This is for the dl_scheduler subsystem\n");
	// Call the proper handler
	if (parse_handover_config(mod_id, parser) == -1) {
	  // LOG_D(ENB_APP, "An error occured\n");
	  goto error;
	}      
      } else {
	// Unknown subsystem
        	goto error;
      }
      break;
    default: // We expect nothing else at this level of the hierarchy
      goto error;
    }
   
    done = (event.type == YAML_SEQUENCE_END_EVENT);

    yaml_event_delete(&event);
 
  }
  
  return 0;

  error:
  yaml_event_delete(&event);
  return -1;

}

int parse_handover_config(mid_t mod_id, yaml_parser_t *parser) {
  
  yaml_event_t event;

  int done = 0;
  int mapping_started = 0;

  while (!done) {
    
    if (!yaml_parser_parse(parser, &event))
      goto error;

    switch (event.type) {
      // We are expecting a mapping (behavior and parameters)
    case YAML_MAPPING_START_EVENT:
      // LOG_D(ENB_APP, "The mapping of the subsystem started\n");
      mapping_started = 1;
      break;
    case YAML_MAPPING_END_EVENT:
      // LOG_D(ENB_APP, "The mapping of the subsystem ended\n");
      mapping_started = 0;
      break;
    case YAML_SCALAR_EVENT:
      if (!mapping_started) {
	goto error;
      }
    if (strcmp((char *) event.data.scalar.value, "parameters") == 0) {
	// LOG_D(ENB_APP, "Now it is time to set the parameters for this subsystem\n");
	if (parse_handover_parameters(mod_id, parser) == -1) {
	  goto error;
	}
      }
      break;
    default:
      goto error;
    }

    done = (event.type == YAML_MAPPING_END_EVENT);
    yaml_event_delete(&event);
  }

  return 0;

 error:
  yaml_event_delete(&event);
  return -1;
}

int parse_handover_parameters(mid_t mod_id, yaml_parser_t *parser) {
  yaml_event_t event;
  
  void *param;
  
  int done = 0;
  int mapping_started = 0;

  while (!done) {
    
    if (!yaml_parser_parse(parser, &event))
      goto error;

    switch (event.type) {
      // We are expecting a mapping of parameters
    case YAML_MAPPING_START_EVENT:
      // LOG_D(ENB_APP, "The mapping of the parameters started\n");
      mapping_started = 1;
      break;
    case YAML_MAPPING_END_EVENT:
      // LOG_D(ENB_APP, "The mapping of the parameters ended\n");
      mapping_started = 0;
      break;
    case YAML_SCALAR_EVENT:
      if (!mapping_started) {
	goto error;
      }
      // Check what key needs to be set
      if (rrc_agent_registered[mod_id]) {
	// LOG_D(ENB_APP, "Setting parameter %s\n", event.data.scalar.value);
	param = dlsym(NULL, (char *) event.data.scalar.value);
	if (param == NULL) {
	  goto error;
	}
	apply_parameter_modification(param, parser);
      } else {
	goto error;
      }
      break;
    default:
      goto error;
    }

    done = (event.type == YAML_MAPPING_END_EVENT);
    yaml_event_delete(&event);
  }

  return 0;
  
 error:
  yaml_event_delete(&event);
  return -1;
}
