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
\section _phy_modulation_ UFMC Modulation Blocks
This section deals with basic functions for UFMC Modulation.


*/

/*! \brief
//  UFMC modulator
//  Carmine Vitiello , 05/2015
//  mail: carmine.vitiello@for.unipi.it
//  
*/

#include "PHY/defs.h"
#include "UTIL/LOG/log.h"

//static short temp2[2048*4] __attribute__((aligned(16)));

#define DEBUG_OFDM_MOD


void normal_prefix_UFMC_mod(int32_t *txdataF,int32_t *txdata,uint8_t nsymb,LTE_DL_FRAME_PARMS *frame_parms,LTE_UL_UE_HARQ_t *ulsch)
{
  uint8_t i;
  int short_offset=0;

  if ((2*nsymb) < frame_parms->symbols_per_tti)
    short_offset = 1;

  //printf("nsymb %d and short_offset %d and symbs per tti %d\n",nsymb,short_offset,frame_parms->samples_per_tti>>1);
  //printf("invocation %d\n",((short_offset)+2*nsymb/frame_parms->symbols_per_tti));
  for (i=0; i<((short_offset)+2*nsymb/frame_parms->symbols_per_tti); i++) {

/*#ifdef DEBUG_OFDM_MOD
    printf("slot i %d (txdata offset %d, txoutput %p)\n",i,(i*(frame_parms->samples_per_tti>>1)),
           txdata+(i*(frame_parms->samples_per_tti>>1)));
#endif
    printf("Parameters normal_prefix_UFMC_mod first:\n -index input= %d\n -index output= %d\n - 4arg=%d\n -prefix_samples=%d\n -carr_offset=%d\n",i*NUMBER_OF_OFDM_CARRIERS*frame_parms->symbols_per_tti>>1,(i*frame_parms->samples_per_tti>>1),1,frame_parms->nb_prefix_samples0,frame_parms->first_carrier_offset);*/

    PHY_UFMC_mod(txdataF+(i*NUMBER_OF_OFDM_CARRIERS*frame_parms->symbols_per_tti>>1),        // input
                 txdata+(i*frame_parms->samples_per_tti>>1),         // output
                 frame_parms->ofdm_symbol_size,                // fft_size
                 1,                 // number of symbols
                 frame_parms->nb_prefix_samples0,               // number of prefix samples
                 frame_parms->nb_prefix_samples,               // filter length
		 frame_parms->first_carrier_offset,		// first resource block
		 ulsch,	   /// ulsch structure
                 CYCLIC_PREFIX);

/*#ifdef DEBUG_OFDM_MOD
    printf("slot i %d (txdata offset %d)\n",i,OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES0+(i*frame_parms->samples_per_tti>>1));
#endif
    printf("Parameters normal_prefix_UFMC_mod others:\n -index input= %d\n -index output= %d\n- 4arg=%d\n -prefix_samples=%d\n -carr_offset=%d\n",NUMBER_OF_OFDM_CARRIERS+(i*NUMBER_OF_OFDM_CARRIERS*(frame_parms->symbols_per_tti>>1)),OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES0+(i*(frame_parms->samples_per_tti>>1)),(short_offset==1) ? 1 :(frame_parms->symbols_per_tti>>1)-1,frame_parms->nb_prefix_samples,frame_parms->first_carrier_offset);*/

    PHY_UFMC_mod(txdataF+NUMBER_OF_OFDM_CARRIERS+(i*NUMBER_OF_OFDM_CARRIERS*(frame_parms->symbols_per_tti>>1)),        // input
                 txdata+OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES0+(i*(frame_parms->samples_per_tti>>1)),         // output
                 frame_parms->ofdm_symbol_size,                // fft_size
                 (short_offset==1) ? 1 :(frame_parms->symbols_per_tti>>1)-1,//6,                 // number of symbols
                 frame_parms->nb_prefix_samples,               // number of prefix samples
                 frame_parms->nb_prefix_samples,               // filter length
		 frame_parms->first_carrier_offset,		// first resource block
		 ulsch,	   	/// ulsch structure
                 CYCLIC_PREFIX);

  }
}

extern int16_t mod_vec[100][2560]  __attribute__((aligned(32)));


void PHY_UFMC_mod(int *input,                       // pointer to complex input
                  int *output,                      // pointer to complex output
                  int fftsize,                      // FFT_SIZE
                  unsigned char nb_symbols,         // number of OFDM symbols
                  unsigned short nb_prefix_samples, // cyclic prefix length
		  unsigned short lFIR,
		  unsigned short first_carrier,	    // first subcarrier offset
		  LTE_UL_UE_HARQ_t *ulsch,	    // ulsch structure
                  Extension_t etype                 // type of extension
                 )
{

  static int temp[2048*2] __attribute__((aligned(16)));
  static int temp1[2048*2] __attribute__((aligned(16)));
  static int temp2[2048*2] __attribute__((aligned(16)));
  unsigned short i,j;
  uint16_t fftSizeFixed=64; //CV:We decided to work with singulat PRB: each subband is composed by 12 subcarrier and nb_rb is also the number of UFMC total subbands 
  int16_t carrierind;

#ifdef DEBUG_OFDM_MOD
  msg("[PHY] OFDM mod (size %d,prefix %d) Symbols %d, input %p, output %p\n",
      fftsize,nb_prefix_samples,nb_symbols,input,output);
#endif
    
  void (*idft)(int16_t *,int16_t *, int);

  switch (fftSizeFixed) {  
  case 64:
    idft = idft64;
    break;
    
  case 128:
    idft = idft128;
    break;

  case 256:
    idft = idft256;
    break;

  case 512:
    idft = idft512;
    break;

  case 1024:
    idft = idft1024;
    break;

  case 2048:
    idft = idft2048;
    break;
  }
  //printf("nb_symbols = %d, nb_rb = %d dim=%d prefix=%d dim_fft=%d\n",nb_symbols,ulsch->nb_rb,log2fftsize,nb_prefix_samples,log2fftSizeFixed);
  for (i=0; i<nb_symbols; i++) {
    //printf("i = %d, nb_rb = %d fftsize=%d prefix=%d\n",i,ulsch->nb_rb,fftsize,nb_prefix_samples);
    //printf("index = %d\n",i*fftsize);
#ifdef DEBUG_OFDM_MOD
    msg("[PHY] symbol %d/%d (%p,%p -> %p)\n",i,nb_symbols,input,&input[i*fftsize],&output[(i*fftsize) + ((i)*nb_prefix_samples)]);
#endif
    memset(temp,0,(fftSizeFixed)*sizeof(int));
    memset(temp1,0,(fftSizeFixed)*sizeof(int));
    memset(temp2,0,(fftSizeFixed+nb_prefix_samples)*sizeof(int));
    memset(&output[i*(fftsize+nb_prefix_samples)],0,(fftsize+nb_prefix_samples)*sizeof(int));

    for (j=0;j<ulsch->nb_rb;j++){
      //write_output("input.m","in",&input[(i*fftsize)],fftsize,1,1);
      carrierind=first_carrier+(12*(j+ulsch->first_rb));
      if (carrierind >= fftsize)
	carrierind = carrierind-fftsize;
      memcpy(&temp[fftSizeFixed-6],&input[(i*fftsize)+carrierind],6*sizeof(int)); 
      memcpy(temp,&input[(i*fftsize)+carrierind+6],6*sizeof(int));
      //write_output("temp.m","tmp",temp,fftSizeFixed,1,1);

      idft((int16_t *)temp,(int16_t *)temp1,1); 
      //write_output("temp1.m","tmp1",temp1,fftSizeFixed,1,1);

      //activate the following lines (with fftSizeFixed=fftsize and disabling dolph_cheb) to test the re-modulation to the right subbands without applying the filter 
      //memcpy(&temp2[nb_prefix_samples],temp1,(fftSizeFixed)*sizeof(int32_t));
      //memcpy(temp2,&temp1[fftSizeFixed-nb_prefix_samples],nb_prefix_samples*sizeof(int32_t));//cyclic prefix
      //write_output("temp2.m","tmp2",temp2,(fftSizeFixed)+nb_prefix_samples,1,1);
      //multcmplx_add(&output[(i*fftsize) + (i*nb_prefix_samples)],&temp2[0],&mod_vec[j+ulsch->first_rb][0],(fftsize)+nb_prefix_samples);
      //write_output("output1.m","out1",&output[(i*fftsize) + (i*nb_prefix_samples)],(fftsize)+nb_prefix_samples,1,1);

      // activate this line (with fftSizeFixed=fftsize and disabling dolph_cheb) to test the original version of UFMC and without the filter
      //idft((int16_t *)&input[i*fftsize],(int16_t *)&output[(i*fftsize) + ((1+i)*nb_prefix_samples)],1); 
      //write_output("output2.m","out2",&output[(i*fftsize) + (i*nb_prefix_samples)],(fftsize)+nb_prefix_samples,1,1);
      
      
      dolph_cheb((int16_t *)temp1, // input
		 (int16_t *)&output[(i*fftsize) + i*nb_prefix_samples],
		 lFIR, //filter length
		 nb_prefix_samples,
		 fftSizeFixed, // input dimension(only real part) -> FFT dimension
		 fftsize,
		 j+ulsch->first_rb, //current PRB index for filter frequency shifting
		 first_carrier );  

      //write_output("output.m","out",&output[(i*fftsize) + (i*nb_prefix_samples)],(fftsize)+nb_prefix_samples,1,1);
    }
  }
}


void do_UFMC_mod(int **txdataF, int32_t **txdata, uint32_t frame,uint16_t next_slot, LTE_DL_FRAME_PARMS *frame_parms, LTE_UL_UE_HARQ_t *ulsch)
{

  int aa, slot_offset, slot_offset_F;

  slot_offset_F = (next_slot)*(frame_parms->ofdm_symbol_size)*((frame_parms->Ncp==1) ? 6 : 7);
  slot_offset = (next_slot)*(frame_parms->samples_per_tti>>1);

  for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
    if (is_pmch_subframe(frame,next_slot>>1,frame_parms)) {
      LOG_E(PHY,"UFMC not supported for PMCH\n");
    } else {
      if (frame_parms->Ncp == EXTENDED)
        PHY_UFMC_mod(&txdataF[aa][slot_offset_F],        // input
                     &txdata[aa][slot_offset],         // output
                     frame_parms->ofdm_symbol_size,                // fft_size
                     6,                 // number of symbols
                     frame_parms->nb_prefix_samples,               // number of prefix samples
                     frame_parms->nb_prefix_samples,               // filter length
		     frame_parms->first_carrier_offset,		// first resource block
		     ulsch,
                     CYCLIC_PREFIX);
      else {
        normal_prefix_UFMC_mod(&txdataF[aa][slot_offset_F],
                          &txdata[aa][slot_offset],
                          7,
                          frame_parms,
			  ulsch);
      }
    }
  }

}

/** @} */

