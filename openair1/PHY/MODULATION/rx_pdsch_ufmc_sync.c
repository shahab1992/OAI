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
  OpenAirInterface Dev  : openair4g-devel@eurecom.fr

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

 *******************************************************************************/
/*
* @defgroup _PHY_MODULATION_
* @ingroup _physical_layer_ref_implementation_
* @{
\section _phy_modulation_ UFDM Modulation Blocks
This section deals with basic functions for OFDM Modulation.


*/

#include "PHY/defs.h"
#include "PHY/extern.h"
#include "defs.h"
#include "PHY/sse_intrin.h"
//#define DEBUG_DRS

int16_t ul_ref_sigs_ufmc[30][2][33][2][2048<<1];//u,v,MSC_RS,cyclic_shift,dft size

/*generate_drs_pdsch(PHY_vars_UE,0,
                               AMP,subframe,
                               PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->first_rb,
                               PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->nb_rb,
			       drs_gen,
                               0);
generate_drs_pdsch(PHY_vars_UE,0,AMP,subframe,first_rb,nb_rb,drs_gen,0);
*/


int generate_drs_pdsch(PHY_VARS_UE *phy_vars_ue,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       unsigned int first_rb,
                       unsigned int nb_rb,
		       int16_t *drs_array,//output added by myself
                       uint8_t ant)
{

  uint16_t k,l,Msc_RS,Msc_RS_idx,rb,drs_offset;
  uint16_t * Msc_idx_ptr;
  int subframe_offset,re_offset,symbol_offset;

  //uint32_t phase_shift; // phase shift for cyclic delay in DM RS
  //uint8_t alpha_ind;

  int16_t alpha_re[12] = {32767, 28377, 16383,     0,-16384,  -28378,-32768,-28378,-16384,    -1, 16383, 28377};
  int16_t alpha_im[12] = {0,     16383, 28377, 32767, 28377,   16383,     0,-16384,-28378,-32768,-28378,-16384};

  uint8_t cyclic_shift,cyclic_shift0,cyclic_shift1;
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_ue->lte_frame_parms;
  uint32_t u,v,alpha_ind;
  uint32_t u0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1];
  uint32_t u1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)];
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  
  int32_t ref_re,ref_im;
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_ue->frame_tx,subframe);
  //int16_t output[(1<<log2fftSizeFixed)<<3];//1024 complex samples(1024*2 real value)*2(#prb)*2(#slot per frame)  

  cyclic_shift0 = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                   phy_vars_ue->ulsch_ue[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                   phy_vars_ue->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[subframe<<1]+
                   ((phy_vars_ue->ulsch_ue[0]->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  //  printf("PUSCH.cyclicShift %d, n_DMRS2 %d, nPRS %d\n",frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift,phy_vars_ue->ulsch_ue[eNB_id]->n_DMRS2,phy_vars_ue->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[subframe<<1]);
  cyclic_shift1 = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                   phy_vars_ue->ulsch_ue[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                   phy_vars_ue->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+
                   ((phy_vars_ue->ulsch_ue[0]->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
		   

  //       cyclic_shift0 = 0;
  //        cyclic_shift1 = 0;
  Msc_RS = 12*nb_rb;

#ifdef USER_MODE
  Msc_idx_ptr = (uint16_t*) bsearch(&Msc_RS, dftsizes, 33, sizeof(uint16_t), compareints);

  if (Msc_idx_ptr)
    Msc_RS_idx = Msc_idx_ptr - dftsizes;
  else {
    msg("generate_drs_pusch: index for Msc_RS=%d not found\n",Msc_RS);
    return(-1);
  }

#else
  uint8_t b;

  for (b=0; b<33; b++)
    if (Msc_RS==dftsizes[b])
      Msc_RS_idx = b;

#endif
#ifdef DEBUG_DRS
  msg("[PHY] drs_modulation: Msc_RS = %d, Msc_RS_idx = %d,cyclic_shift %d, u0 %d, v0 %d, u1 %d, v1 %d,cshift0 %d,cshift1 %d\n",Msc_RS, Msc_RS_idx,cyclic_shift,u0,v0,u1,v1,cyclic_shift0,cyclic_shift1);

#endif

  subframe_offset=0;
  for (l = (3 - frame_parms->Ncp),u=u0,v=v0,cyclic_shift=cyclic_shift0;
       l<frame_parms->symbols_per_tti;
       l += (7 - frame_parms->Ncp),u=u1,v=v1,cyclic_shift=cyclic_shift1) {

    drs_offset = 0;    
    re_offset = 0;
    symbol_offset = 0+subframe_offset;
    alpha_ind = 0;

    for (rb=0; rb<nb_rb; rb++) { //only 0 and 1 are rb imvolved in drs process
        for (k=0; k<12; k++) {
          ref_re = (int32_t) ul_ref_sigs[u][v][Msc_RS_idx][drs_offset<<1];// the same of ul_ref_sigs-->change with lte_ue_ref_sigs_ufmc
          ref_im = (int32_t) ul_ref_sigs[u][v][Msc_RS_idx][(drs_offset<<1)+1];
	  //printf("IFFT_FPGA_UE rb=%d cyclic_shift=%d k=%d %d\n",rb,cyclic_shift,k,2*(symbol_offset + re_offset));
          ((int16_t*) drs_array)[2*(symbol_offset + re_offset)]   = (int16_t) (((ref_re*alpha_re[alpha_ind]) -
              (ref_im*alpha_im[alpha_ind]))>>15);
          ((int16_t*) drs_array)[2*(symbol_offset + re_offset)+1] = (int16_t) (((ref_re*alpha_im[alpha_ind]) +
              (ref_im*alpha_re[alpha_ind]))>>15);
          ((short*) drs_array)[2*(symbol_offset + re_offset)]   = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)]*(int32_t)amp)>>15);
          ((short*) drs_array)[2*(symbol_offset + re_offset)+1] = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)+1]*(int32_t)amp)>>15);


          alpha_ind = (alpha_ind + cyclic_shift);

          if (alpha_ind > 11)
            alpha_ind-=12;
          re_offset++;
          drs_offset++;
        }
	re_offset=0;
	symbol_offset+=12;
    }
    subframe_offset+=nb_rb*12;
  }
  return(0);
}

int generate_drs_ufmc(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant)
{
  uint16_t k,l,rb,drs_offset;
  int re_offset,symbol_offset;

  int16_t alpha_re[12] = {32767, 28377, 16383,     0,-16384,  -28378,-32768,-28378,-16384,    -1, 16383, 28377};
  int16_t alpha_im[12] = {0,     16383, 28377, 32767, 28377,   16383,     0,-16384,-28378,-32768,-28378,-16384};

  uint8_t cyclic_shift[2];
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,Msc_RS,alpha_ind,v_max;
  int32_t ref_re,ref_im;
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  int16_t log2fftSize2=11,
	  log2fftSize=10;
  int drs_array[12*2*nb_rb];//mux with another variable in order to consider the frame number
  static short temp[2048*4] __attribute__((aligned(16))),temp1[2048*4] __attribute__((aligned(16))),
  Rxdft[2048*4] __attribute__((aligned(16)));
  int16_t mod_vec[1<<(log2fftSize+1)];
  cyclic_shift[0] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  cyclic_shift[1] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;

  for(u=0;u<30;u++){
    for(Msc_RS=0;Msc_RS<33;Msc_RS++){
      v_max = (Msc_RS<2) ? 1 : 2;
      for(v=0;v<v_max;v++){
	symbol_offset=0;
	for (l = 0;l<2;l++) {
	  drs_offset = 0;    
	  re_offset = 0;
	  alpha_ind = 0;
	  memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
	  for (rb=0; rb<nb_rb; rb++) { //only 0 and 1 are rb imvolved in drs process
	      for (k=0; k<12; k++) {
		ref_re = (int32_t) ul_ref_sigs[u][v][Msc_RS][drs_offset<<1];// the same of ul_ref_sigs-->change with lte_ue_ref_sigs_ufmc
		ref_im = (int32_t) ul_ref_sigs[u][v][Msc_RS][(drs_offset<<1)+1];

		((int16_t*) drs_array)[2*(symbol_offset + re_offset)]   = (int16_t) (((ref_re*alpha_re[alpha_ind]) -
		    (ref_im*alpha_im[alpha_ind]))>>15);
		((int16_t*) drs_array)[2*(symbol_offset + re_offset)+1] = (int16_t) (((ref_re*alpha_im[alpha_ind]) +
		    (ref_im*alpha_re[alpha_ind]))>>15);
		((short*) drs_array)[2*(symbol_offset + re_offset)]   = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)]*(int32_t)amp)>>15);
		((short*) drs_array)[2*(symbol_offset + re_offset)+1] = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)+1]*(int32_t)amp)>>15);

		alpha_ind = (alpha_ind + cyclic_shift[l]);

		if (alpha_ind > 11)
		  alpha_ind-=12;
		re_offset++;
		drs_offset++;
	      }
	      re_offset=0;
	      symbol_offset+=12;
	      memset(temp1,0,(1<<log2fftSize)*sizeof(int));
	      memset(temp,0,(1<<log2fftSize2)*sizeof(int)); memcpy(temp1,&drs_array[(12*rb)+(l*12*nb_rb)],6*sizeof(int)); /*memcpy(&temp1[(1<<(log2fftSize+1))-12],&drs_array[(12*rb)+(l*12*nb_rb)+6],6*sizeof(int));//temp is int16_t
	      idft1024((int16_t *)temp1,(int16_t *)temp,1);
	      ii_CreateModvec(rb+1,0,(1<<log2fftSize),(1<<log2fftSize),&mod_vec[0]);
	      multcmplx_add((int16_t *)Rxdft,(int16_t *)temp,mod_vec,(1<<log2fftSize));*/
	      memcpy(&temp1[(1<<(6+1))-12],&drs_array[(12*rb)+(l*12*nb_rb)+6],6*sizeof(int));//temp is int16_t
	      idft64((int16_t *)temp1,(int16_t *)temp,1);
	      ii_CreateModvec(rb+1,0,(1<<log2fftSize),(1<<log2fftSize),&mod_vec[0]);
	      dolph_cheb((int16_t *)temp, // input
		(int16_t *)&Rxdft,
		72,  // (nb_prefix_samples)cyclic prefix length -> it becomes FIR length(multiple of 8)
		1<<6, // input dimension(only real part) -> FFT dimension
		1<<log2fftSize,
		rb+1 //nPRB for filter frequency shifting
		  );
	  }
	  if(u==22 && v==0 && Msc_RS==1 && l==0){
	    write_output("drs_emplRx1.m","drs_emplRx1",(int16_t *)&Rxdft,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
	  }
	  if(u==26 && v==0 && Msc_RS==1 && l==1){
	    write_output("drs_emplRx2.m","drs_emplRx2",(int16_t *)&Rxdft,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
	  }
	  dft2048((int16_t *)Rxdft,(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS][l][0],1);  
	}
      }
    }
  }
  return 0;
}


int generate_drs_pdsch_Rx(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant,
		       uint32_t nsymb
 			)
{
  uint16_t k,l,aa,Msc_RS,Msc_RS_idx,rb,drs_offset;
  uint16_t * Msc_idx_ptr;
  int re_offset,symbol_offset;

  int16_t alpha_re[12] = {32767, 28377, 16383,     0,-16384,  -28378,-32768,-28378,-16384,    -1, 16383, 28377};
  int16_t alpha_im[12] = {0,     16383, 28377, 32767, 28377,   16383,     0,-16384,-28378,-32768,-28378,-16384};
  uint8_t cyclic_shift,cyclic_shift0,cyclic_shift1;
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,alpha_ind,index[2];
  uint32_t u0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1];
  uint32_t u1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)];
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  int32_t ref_re,ref_im;
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  int16_t log2fftSize2=11,
	  log2fftSize=10;
  int16_t mod_vec[1<<(log2fftSize+1)];
  uint32_t offset,offset0,offset1;
  int drs_array[12*2*nb_rb*1];//mux with another variable in order to consider the frame number
  static short temp[2048*4] __attribute__((aligned(16))),temp1[2048*4] __attribute__((aligned(16))),
  Rxdft[2048*4] __attribute__((aligned(16))),output[2048*4] __attribute__((aligned(16)));

  
  cyclic_shift0 = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  cyclic_shift1 = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;

  Msc_RS = 12*nb_rb;

#ifdef USER_MODE
  Msc_idx_ptr = (uint16_t*) bsearch(&Msc_RS, dftsizes, 33, sizeof(uint16_t), compareints);

  if (Msc_idx_ptr)
    Msc_RS_idx = Msc_idx_ptr - dftsizes;
  else {
    msg("generate_drs_pusch: index for Msc_RS=%d not found\n",Msc_RS);
    return(-1);
  }

#else
  uint8_t b;

  for (b=0; b<33; b++)
    if (Msc_RS==dftsizes[b])
      Msc_RS_idx = b;
Rxdft
#endif
#ifdef DEBUG_DRS
  msg("[PHY] drs_modulation: Msc_RS = %d, Msc_RS_idx = %d,cyclic_shift %d, u0 %d, v0 %d, u1 %d, v1 %d,cshift0 %d,cshift1 %d\n",Msc_RS, Msc_RS_idx,cyclic_shift,u0,v0,u1,v1,cyclic_shift0,cyclic_shift1);

#endif
  symbol_offset=0;
  printf("u0=%d u1=%d v0=%d v1=%d cyclic_shift0=%d cyclic_shift1=%d\n",u0,u1,v0,v1,cyclic_shift0,cyclic_shift1);
  for (l = (3 - frame_parms->Ncp),aa=0,u=u0,v=v0,cyclic_shift=cyclic_shift0;
       l<frame_parms->symbols_per_tti;
       l += (7 - frame_parms->Ncp),aa++,u=u1,v=v1,cyclic_shift=cyclic_shift1) {

    drs_offset = 0;    
    re_offset = 0;
    alpha_ind = 0;
    memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));

    for (rb=0; rb<nb_rb; rb++) { //only 0 and 1 are rb imvolved in drs process
        for (k=0; k<12; k++) {
          ref_re = (int32_t) ul_ref_sigs[u][v][Msc_RS_idx][drs_offset<<1];// the same of ul_ref_sigs-->change with lte_ue_ref_sigs_ufmc
          ref_im = (int32_t) ul_ref_sigs[u][v][Msc_RS_idx][(drs_offset<<1)+1];
	  //printf("IFFT_FPGA_UE rb=%d cyclic_shift=%d k=%d %d\n",rb,cyclic_shift,k,2*(symbol_offset + re_offset));
          ((int16_t*) drs_array)[2*(symbol_offset + re_offset)]   = (int16_t) (((ref_re*alpha_re[alpha_ind]) -
              (ref_im*alpha_im[alpha_ind]))>>15);
          ((int16_t*) drs_array)[2*(symbol_offset + re_offset)+1] = (int16_t) (((ref_re*alpha_im[alpha_ind]) +
              (ref_im*alpha_re[alpha_ind]))>>15);
          ((short*) drs_array)[2*(symbol_offset + re_offset)]   = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)]*(int32_t)amp)>>15);
          ((short*) drs_array)[2*(symbol_offset + re_offset)+1] = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)+1]*(int32_t)amp)>>15);
          alpha_ind = (alpha_ind + cyclic_shift);

          if (alpha_ind > 11)
            alpha_ind-=12;
          re_offset++;
          drs_offset++;
        }
	re_offset=0;
	symbol_offset+=12;
	memset(temp1,0,(1<<log2fftSize)*sizeof(int)); //1024 to 0
	memset(temp,0,(1<<log2fftSize2)*sizeof(int)); //2048 to 0
	memcpy((int16_t *)temp1,(int16_t *)&drs_array[(12*rb)+(aa*12*nb_rb)],6*sizeof(int)); 
	memcpy((int16_t *)&temp1[(1<<(log2fftSize+1))-12],(int16_t *)&drs_array[(12*rb)+(aa*12*nb_rb)+6],6*sizeof(int));//temp is int16_t
	idft1024((int16_t *)temp1,(int16_t *)temp,1);
	ii_CreateModvec(rb+1,0,(1<<log2fftSize),(1<<log2fftSize),&mod_vec[0]);
	multcmplx_add((int16_t *)Rxdft,(int16_t *)temp,mod_vec,(1<<log2fftSize));
    }
    dft2048((int16_t *)Rxdft,(int16_t *)&output[(1<<(log2fftSize2+1))*aa],1);
  }
   write_output("Rx.m","Rx",(int16_t *)output,(1<<log2fftSize2)*2,1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
   write_output("drs.m","drs",drs_array,12*nb_rb*2,1,1);//12 symbols*2 slot eachsubframe*nPRBs

  offset0=(phy_vars_eNB->lte_frame_parms.samples_per_tti*subframe)+(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*2))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*3);
  offset1=offset0+(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*6))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*7);
  for(aa=0;aa<frame_parms->nb_antennas_rx;aa++){
    for (l = (3 - frame_parms->Ncp),offset=offset0,k=0;l<frame_parms->symbols_per_tti;l += (7 - frame_parms->Ncp),offset=offset1,k++) {
      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      
      memcpy(temp,&phy_vars_eNB->lte_eNB_common_vars.rxdata[eNB_id][0][offset],(1<<log2fftSize2)*sizeof(int));
      dft2048((int16_t *)temp,(int16_t *)Rxdft,1);

      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      multcmplx_conj((int16_t *)temp,(int16_t *) &output[(1<<(log2fftSize2+1))*k],(int16_t *)&Rxdft,1<<(log2fftSize2));//defined in ufmc_filter.c 
      
      memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      idft2048((int16_t *)temp,(int16_t *)Rxdft,1);
      index[k]=max_vec((int16_t *)Rxdft,(1<<(log2fftSize2+1)));//defined in ufmc_filter.c 
      printf("FUN_MOD_index@frame%d=%d\n",l,index[k]);
    }
  }
  
  return(0);
}

void rx_pdsch_ufmc_sync(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant
		       //,uint32_t nsymb
 			)
{
  uint16_t k,l,aa,Msc_RS,Msc_RS_idx;
  uint16_t * Msc_idx_ptr;

  uint8_t cyclic_shift[2];
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,index;
  uint32_t u0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1];
  uint32_t u1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)];
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  int16_t log2fftSize2=11;
  uint32_t offset,offset0,offset1;
  static short temp[2048*4] __attribute__((aligned(16))),Rxdft[2048*4] __attribute__((aligned(16))),output[2048*4] __attribute__((aligned(16)));
  
  cyclic_shift[0] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  cyclic_shift[1] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  Msc_RS = 12*nb_rb;

#ifdef USER_MODE
  Msc_idx_ptr = (uint16_t*) bsearch(&Msc_RS, dftsizes, 33, sizeof(uint16_t), compareints);

  if (Msc_idx_ptr)
    Msc_RS_idx = Msc_idx_ptr - dftsizes;
  else {
    msg("generate_drs_pusch: index for Msc_RS=%d not found\n",Msc_RS);
  }

#else
  uint8_t b;

  for (b=0; b<33; b++)
    if (Msc_RS==dftsizes[b])
      Msc_RS_idx = b;

#endif
#ifdef DEBUG_DRS
  msg("[PHY] drs_modulation: Msc_RS = %d, Msc_RS_idx = %d,cyclic_shift %d, u0 %d, v0 %d, u1 %d, v1 %d,cshift0 %d,cshift1 %d\n",Msc_RS, Msc_RS_idx,cyclic_shift,u0,v0,u1,v1,cyclic_shift0,cyclic_shift1);

#endif 

  offset0=(phy_vars_eNB->lte_frame_parms.samples_per_tti*subframe)+(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*2))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*3);
  offset1=offset0+(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*6))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*7);
  for(aa=0;aa<frame_parms->nb_antennas_rx;aa++){
    memset(output,0,(1<<log2fftSize2)*sizeof(int));
    for (l = (3 - frame_parms->Ncp),offset=offset0,k=0,u=u0,v=v0;l<frame_parms->symbols_per_tti;l += (7 - frame_parms->Ncp),offset=offset1,u=u1,v=v1,k++) {
      printf("u=%d - v=%d - Msc_RS_idx=%d - k=%d \n",u,v,Msc_RS_idx,k);
      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memcpy(temp,&phy_vars_eNB->lte_eNB_common_vars.rxdata[eNB_id][0][offset],(1<<log2fftSize2)*sizeof(int));
      dft2048((int16_t *)temp,(int16_t *)Rxdft,1);
      if(k==0){
	write_output("dataRx1.m","dataRx1",(int16_t *)&temp,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
	write_output("Rx1.m","Rx1",(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(1<<log2fftSize2)*2,1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
      }else{
	write_output("dataRx2.m","dataRx2",(int16_t *)&temp,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
	write_output("Rx2.m","Rx2",(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(1<<log2fftSize2)*2,1,1);//12 symbols*2 slot eachsubframe*nPRBs*/
      }
      multcmplx_conj((int16_t *)Rxdft,(int16_t *) &ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(int16_t *)Rxdft,1<<(log2fftSize2));
      idft2048((int16_t *)Rxdft,(int16_t *)temp,1);
      add_cpx_vector32((int16_t *)temp,
                     (int16_t *)output,
                     (int16_t *)output,
                     (1<<(log2fftSize2+1)));
    }
    index=max_vec((int16_t *)output,(1<<(log2fftSize2+1)));//defined in ufmc_filter.c 
      printf("index=%d\n",index);
  }
}

