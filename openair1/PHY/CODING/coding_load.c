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

/*! \file openair1/PHY/CODING
 * \brief: load library implementing coding/decoding algorithms
 * \author Francois TABURET
 * \date 2017
 * \version 0.1
 * \company NOKIA BellLabs France
 * \email: francois.taburet@nokia-bell-labs.com
 * \note
 * \warning
 */
#include "common/utils/load_module_shlib.h" 

  loader_shlibfunc_t shlib_fdesc[6];

int (*decoder16)(void);
int (*decoder8)(void);


extern int _may_i_use_cpu_feature(unsigned __int64);

void nofunc(void) { printf(".");return;};
void decoding_setmode (int mode) {
printf("decoding_setmode %i\n",mode);
   switch (mode) {
       case 2:
          decoder8=nofunc;
          decoder16=nofunc;
       break;
       case 1:
          decoder8=shlib_fdesc[2].fptr; 
          decoder16=shlib_fdesc[3].fptr;         
       break;
       case 0:
          decoder16=shlib_fdesc[4].fptr;
          decoder8=shlib_fdesc[4].fptr;   
       break;
   }

}


int load_codinglib(void) {
 int ret;

 
     shlib_fdesc[0].fname="init_td8";
     shlib_fdesc[1].fname="init_td16";

     shlib_fdesc[2].fname="phy_threegpplte_turbo_decoder8";

     
     shlib_fdesc[3].fname="phy_threegpplte_turbo_decoder16";


     shlib_fdesc[4].fname="phy_threegpplte_turbo_decoder_scalar";


     shlib_fdesc[5].fname="init_td16avx2";

    
     ret=load_module_shlib("coding",shlib_fdesc,6);
     if (ret < 0) exit_fun();
     
     shlib_fdesc[0].fptr();
     shlib_fdesc[1].fptr();
     shlib_fdesc[5].fptr();
     decoding_setmode(1);
     
return 0;
}
