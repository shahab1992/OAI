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

int generate_drs_ufmc(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant)
{
  uint16_t k,l,rb,drs_offset,fftSize;
  int lCP,re_offset,symbol_offset;

  int16_t alpha_re[12] = {32767, 28377, 16383,     0,-16384,  -28378,-32768,-28378,-16384,    -1, 16383, 28377};
  int16_t alpha_im[12] = {0,     16383, 28377, 32767, 28377,   16383,     0,-16384,-28378,-32768,-28378,-16384};
  
  switch(phy_vars_eNB->lte_frame_parms.N_RB_UL){
    case 6:
      fftSize=128;
      lCP=9;
      break;
    case 15:
      fftSize=256;
      lCP=18;
      break;
    case 25:
      lCP=36;
      fftSize=512;
      break;
    case 50:
      lCP=72;
      fftSize=1024;
      break;
    case 75:
      lCP=108;
      fftSize=1536;
      break;
    case 100:
      lCP=144;
      fftSize=2048;
      break;
    default:
      lCP=72;
      fftSize=1024;
      break;
  }

  uint8_t cyclic_shift[2];
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,Msc_RS,alpha_ind,v_max;
  int32_t ref_re,ref_im;
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  uint16_t fftSize2=2*fftSize;
  int drs_array[12*2*nb_rb];//mux with another variable in order to consider the frame number
  static short temp[2048*4] __attribute__((aligned(16))),temp1[2048*4] __attribute__((aligned(16))),
  Rxdft[2048*4] __attribute__((aligned(16)));
  cyclic_shift[0] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  cyclic_shift[1] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+
                   ant*6) % 12;
  printf("lCP=%d,phy_vars_eNB->lte_frame_parms.nb_prefix_samples=%d\n",lCP,phy_vars_eNB->lte_frame_parms.nb_prefix_samples);
  for(u=0;u<30;u++){
    for(Msc_RS=0;Msc_RS<33;Msc_RS++){
      v_max = (Msc_RS<2) ? 1 : 2;
      for(v=0;v<v_max;v++){
	symbol_offset=0;
	for (l = 0;l<2;l++) {
	  drs_offset = 0;    
	  re_offset = 0;
	  alpha_ind = 0;
	  memset(Rxdft,0,fftSize2*sizeof(int));
	  for (rb=0; rb<nb_rb; rb++) { //only 0 and 1 are rb imvolved in drs process
	      for (k=0; k<12; k++) {
		ref_re = (int32_t) ul_ref_sigs[u][v][Msc_RS][drs_offset<<1];// the same of ul_ref_sigs-->change with lte_ue_ref_sigs_ufmc
		ref_im = (int32_t) ul_ref_sigs[u][v][Msc_RS][(drs_offset<<1)+1];

		((int16_t*) drs_array)[2*(symbol_offset + re_offset)]   = (int16_t) (((ref_re*alpha_re[alpha_ind]) -
		    (ref_im*alpha_im[alpha_ind]))>>15);
		((int16_t*) drs_array)[2*(symbol_offset + re_offset)+1] = (int16_t) (((ref_re*alpha_im[alpha_ind]) +
		    (ref_im*alpha_re[alpha_ind]))>>15);
		((short*) drs_array)[2*(symbol_offset + re_offset)]   = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)]*(int32_t)amp)>>15); ////amp=4096 as defined in ufmcsim -->scale by 12
		((short*) drs_array)[2*(symbol_offset + re_offset)+1] = (short) ((((short*) drs_array)[2*(symbol_offset + re_offset)+1]*(int32_t)amp)>>15); ////amp=4096 as defined in ufmcsim -->scale by 12

		alpha_ind = (alpha_ind + cyclic_shift[l]);

		if (alpha_ind > 11)
		  alpha_ind-=12;
		re_offset++;
		drs_offset++;
	      }
	      re_offset=0;
	      symbol_offset+=12;
	      memset(temp1,0,fftSize*sizeof(int));
	      memset(temp,0,fftSize2*sizeof(int));
	      memcpy(temp1,&drs_array[(12*rb)+(l*12*nb_rb)+6],6*sizeof(int));
	      memcpy(&temp1[(1<<(6+1))-12],&drs_array[(12*rb)+(l*12*nb_rb)],6*sizeof(int));//temp is int16_t
	      idft64((int16_t *)temp1,(int16_t *)temp,1);
	      dolph_cheb((int16_t *)temp, // input
			 (int16_t *)&Rxdft,
			 lCP,  // (nb_prefix_samples)cyclic prefix length -> it becomes FIR length(multiple of 8)
			 1<<6, // input dimension(only real part) -> FFT dimension
			 fftSize,
			 rb, //nPRB for filter frequency shifting
			 frame_parms->first_carrier_offset);
	  }
	  dft2048((int16_t *)Rxdft,(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS][l][0],1);  
	}
      }
    }
  }
  return 0;
}

int rx_pusch_ufmc_sync(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant /*,
		       uint32_t length*/
 			)
{
  uint16_t k,l,aa,Msc_RS,Msc_RS_idx,j;
  uint16_t * Msc_idx_ptr;
  uint8_t scale=10;
  uint8_t scale_adj=0;
  uint8_t cyclic_shift[2];
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,i,flag_energy=0,index=0;
  uint32_t u0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1];
  uint32_t u1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)];
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  int16_t log2fftSize2=11;
  uint32_t offset,offset0,offset1;
  int16_t index_int,index_start;
  int32_t energy,energy2;
  static short temp[2048*4] __attribute__((aligned(32))),Rxdft[2048*4] __attribute__((aligned(32))),temp1[2048*4] __attribute__((aligned(32)));
  static int32_t output[2048*2] __attribute__((aligned(32)));
  
  cyclic_shift[0] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+ant*6) % 12;
  cyclic_shift[1] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+ant*6) % 12;
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
    
  // Timing synchronization
  for(aa=0;aa<frame_parms->nb_antennas_rx;aa++){   
    memset(output,0,(1<<log2fftSize2)*sizeof(int));
    for (l = (3 - frame_parms->Ncp),offset=offset0,k=0,u=u0,v=v0;l<frame_parms->symbols_per_tti;l += (7 - frame_parms->Ncp),offset=offset1,u=u1,v=v1,k++) {
      printf("u=%d - v=%d - Msc_RS_idx=%d - k=%d offset=%d\n",u,v,Msc_RS_idx,k,offset);
      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      memcpy(temp,&phy_vars_eNB->lte_eNB_common_vars.rxdata[eNB_id][aa][offset],(1<<log2fftSize2)*sizeof(int));
      dft2048((int16_t *)temp,(int16_t *)Rxdft,1);     
      index_int=128;
      index_start=(index_int*3+(1<<(log2fftSize2-1)))*2;
      index_start= (index_start%8)>0 ? 2*(index_start+8-(index_start%8)) : index_start; //for SIMD computation
      // Energy estimation in order to set scale of crosscorrelation based only on slot 3
      // right shift based on Peak:11 on energy:10 (ceil(log2(sqrt(mean(abs(drs_x).^2))))) around 9.1
      energy=0; //initialization
      //energy=sum_square_abs_cmplx_SIMD((int16_t *)Rxdft,(1<<log2fftSize2+1));//&Rxdft[index_start]    
      energy=sum_square_abs_cmplx((int16_t *)&Rxdft[index_start],index_int>>1);//&Rxdft[index_start]    
      j=log2_approx(iSqrt(energy));
      j= (j<=0 || j>=12) ? 12 : j+1;
      scale_adj=(12-j);
      printf("SIGNAL_ENERGY 1: %d - adj_factor : %d scale_factor_OVER_%d=%d\n",energy,j,scale,scale_adj);
      multcmplx_conj((int16_t *)&temp1[index_start],(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][index_start],(int16_t *)&Rxdft[index_start],index_int,11); // shifted right of 11 bits because this is the dynamic range of ul_ref */
      //multcmplx_conj((int16_t *)temp1,(int16_t *) &ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(int16_t *)Rxdft,1<<(log2fftSize2+1),scale-scale_adj);// shifted right of 11 bits because this is the dynamic range of ul_ref
      /*memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memcpy(&temp[index_start],&temp1[index_start],(int)(index_int)*sizeof(int));
      if(k==0){
	write_output("conj1_m.m","conj1",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);
	write_output("conj1_1m.m","conj1_1",(int16_t *)&temp1[0],(1<<log2fftSize2),1,1);
      }else{
	write_output("conj2_m.m","conj2",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);
	write_output("conj2_1m.m","conj2_1",(int16_t *)&temp1[0],(1<<log2fftSize2),1,1);
      }*/
      
      //memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      idft2048((int16_t *)temp1,(int16_t *)Rxdft,1);
      /*if(k==0){
	write_output("idft1_m.m","idft1",(int16_t *)&Rxdft[0],(1<<log2fftSize2),1,1);
      }else{
	write_output("idft2_m.m","idft2",(int16_t *)&Rxdft[0],(1<<log2fftSize2),1,1);
      }*/
      
      /*add_cpx_vector32((int16_t *)Rxdft,
                     (int16_t *)output,
                     (int16_t *)output,
                     (1<<(log2fftSize2+1)));*/
      square_abs_cmplx_add(output,(int16_t *)Rxdft,(1<<log2fftSize2));
      /*if(k==0){
	write_output("out1_m.m","out1",(int *)&output[0],(1<<log2fftSize2),1,2);
      }else{
	write_output("out2_m.m","out2",(int *)&output[0],(1<<log2fftSize2),1,2);
      }*/
    }
    // write_output("delay_est_m.m","delay_est",output,1<<(log2fftSize2),1,2);
    index=max_vec(output,(1<<log2fftSize2));//defined in ufmc_filter.c 
    printf("TIME SYNCHRO 1: index_found=%d - corrected index=%d\n",index,index-9);
    index= (index-9<0) ? 0 : index;  
  }
  return index;
}

int rx_pusch_ufmc_sync_7_5kHz(PHY_VARS_eNB *phy_vars_eNB,
                       uint8_t eNB_id,
                       short amp,
                       unsigned int subframe,
                       uint8_t ant /*,
		       uint32_t length*/
 			)
{
  uint16_t k,l,aa,Msc_RS,Msc_RS_idx,j;
  uint16_t * Msc_idx_ptr;
  uint8_t scale=10;
  uint8_t scale_adj=0;
  uint8_t cyclic_shift[2];
  LTE_DL_FRAME_PARMS *frame_parms = &phy_vars_eNB->lte_frame_parms;
  uint32_t u,v,i,flag_energy=0,index=0;
  uint32_t u0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[subframe<<1];
  uint32_t u1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.grouphop[1+(subframe<<1)];
  uint32_t v0=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[subframe<<1];
  uint32_t v1=frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.seqhop[1+(subframe<<1)];
  uint8_t harq_pid = subframe2harq_pid(frame_parms,phy_vars_eNB->proc[subframe].frame_rx,subframe);
  uint16_t nb_rb=phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->nb_rb;
  int16_t log2fftSize2=11;
  uint32_t offset,offset0,offset1;
  int16_t index_int,index_start;
  int32_t energy,energy2;
  static short temp[2048*4] __attribute__((aligned(32))),Rxdft[2048*4] __attribute__((aligned(32))),temp1[2048*4] __attribute__((aligned(32)));
  static int32_t output[2048*2] __attribute__((aligned(32)));
  
  cyclic_shift[0] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)]+((phy_vars_eNB->cooperation_flag==2)?10:0)+ant*6) % 12;
  cyclic_shift[1] = (frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift +
                  phy_vars_eNB->ulsch_eNB[eNB_id]->harq_processes[harq_pid]->n_DMRS2 +
                  frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[(subframe<<1)+1]+((phy_vars_eNB->cooperation_flag==2)?10:0)+ant*6) % 12;
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

  offset0=(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*2))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*3);
  offset1=offset0+(frame_parms->nb_prefix_samples0+(frame_parms->nb_prefix_samples*6))+(phy_vars_eNB->lte_frame_parms.ofdm_symbol_size*7);
    
  // Timing synchronization
  for(aa=0;aa<frame_parms->nb_antennas_rx;aa++){   
    memset(output,0,(1<<log2fftSize2)*sizeof(int));
    for (l = (3 - frame_parms->Ncp),offset=offset0,k=0,u=u0,v=v0;l<frame_parms->symbols_per_tti;l += (7 - frame_parms->Ncp),offset=offset1,u=u1,v=v1,k++) {
      //printf("u=%d - v=%d - Msc_RS_idx=%d - k=%d offset=%d\n",u,v,Msc_RS_idx,k,offset);
      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      memcpy(temp,&phy_vars_eNB->lte_eNB_common_vars.rxdata_7_5kHz[eNB_id][aa][offset],(1<<log2fftSize2)*sizeof(int));
      dft2048((int16_t *)temp,(int16_t *)Rxdft,1);
      /*if(k==0){
	write_output("dataRx1_m.m","dataRx1",(int16_t *)&Rxdft,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
	write_output("Rx1_m.m","Rx1",(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
	write_output("tmp1_m.m","tmp1",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
      }else{
	write_output("dataRx2_m.m","dataRx2",(int16_t *)&Rxdft,(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
	write_output("Rx2_m.m","Rx2",(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
	write_output("tmp2_m.m","tmp2",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);//12 symbols*2 slot eachsubframe*nPRBs
      }*/
      index_int=128;
      index_start=(index_int*3+(1<<(log2fftSize2-1)))*2;
      index_start= (index_start%8)>0 ? 2*(index_start+8-(index_start%8)) : index_start; //for SIMD computation
      // Energy estimation in order to set scale of crosscorrelation based only on slot 3
      // right shift based on Peak:11 on energy:10 (ceil(log2(sqrt(mean(abs(drs_x).^2))))) around 9.1
      energy=0; //initialization
      //energy=sum_square_abs_cmplx_SIMD((int16_t *)Rxdft,(1<<log2fftSize2+1));//&Rxdft[index_start]    
      energy=sum_square_abs_cmplx((int16_t *)&Rxdft[index_start],index_int>>1);//&Rxdft[index_start]    
      j=log2_approx(iSqrt(energy));
      j= (j<=0 || j>=12) ? 12 : j+1;
      scale_adj=(12-j);
      //printf("SIGNAL_ENERGY : %d - adj_factor : %d scale_factor_OVER_%d=%d\n",energy,j,scale,scale_adj);
      multcmplx_conj((int16_t *)&temp1[index_start],(int16_t *)&ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][index_start],(int16_t *)&Rxdft[index_start],index_int,11); // shifted right of 11 bits because this is the dynamic range of ul_ref */
      multcmplx_conj((int16_t *)temp1,(int16_t *) &ul_ref_sigs_ufmc[u][v][Msc_RS_idx][k][0],(int16_t *)Rxdft,1<<(log2fftSize2+1),scale-scale_adj);// shifted right of 11 bits because this is the dynamic range of ul_ref
      memset(temp,0,(1<<log2fftSize2)*sizeof(int));
      memcpy(&temp[index_start],&temp1[index_start],(int)(index_int)*sizeof(int));
      /*if(k==0){
	write_output("conj1_m.m","conj1",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);
	write_output("conj1_1m.m","conj1_1",(int16_t *)&temp1[0],(1<<log2fftSize2),1,1);
      }else{
	write_output("conj2_m.m","conj2",(int16_t *)&temp[0],(1<<log2fftSize2),1,1);
	write_output("conj2_1m.m","conj2_1",(int16_t *)&temp1[0],(1<<log2fftSize2),1,1);
      }*/
      //memset(Rxdft,0,(1<<log2fftSize2)*sizeof(int));
      idft2048((int16_t *)temp1,(int16_t *)Rxdft,1);
      /*if(k==0){
	write_output("idft1_m.m","idft1",(int16_t *)&Rxdft[0],(1<<log2fftSize2),1,1);
      }else{
	write_output("idft2_m.m","idft2",(int16_t *)&Rxdft[0],(1<<log2fftSize2),1,1);
      }*/
      /*add_cpx_vector32((int16_t *)Rxdft,
                     (int16_t *)output,
                     (int16_t *)output,
                     (1<<(log2fftSize2+1)));*/
      square_abs_cmplx_add(output,(int16_t *)Rxdft,(1<<log2fftSize2));
      /*if(k==0){
	write_output("out1_m.m","out1",(int *)&output[0],(1<<log2fftSize2),1,2);
      }else{
	write_output("out2_m.m","out2",(int *)&output[0],(1<<log2fftSize2),1,2);
      }*/
    }
    //write_output("delay_est_m.m","delay_est",output,1<<(log2fftSize2),1,2);
    index=max_vec(output,(1<<log2fftSize2));//defined in ufmc_filter.c 
    //printf("TIME SYNCHRO: index_found=%d, corrected index=%d\n",index,index-1);
    index= (index-1<0) ? 0 : index;  
  }
  return index;
}
