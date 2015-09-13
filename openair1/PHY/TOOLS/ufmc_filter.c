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
#include "defs.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define PI 3.141592654

/*! \brief
//  Filtering function for UFMC waveform 
//  Carmine Vitiello , 05/2015
//  mail: carmine.vitiello@for.unipi.it
//  
*/

double cheby_poly(int n, double x){ //chebyshev polyomial T_n(x)
    double res;
    if (fabs(x) <= 1) res = cos(n*acos(x));
    else              res = cosh(n*acosh(x));
    return res;
}

/***************************************************************************
 calculate a Dolph-Chebyshev window of size N, store coeffs in out as in A.Antoniou - "Digital Filters" - 2000 - McGrawHill
- out should be a float array of size N 
- atten is the required sidelobe attenuation (e.g. if you want -60dB atten, use '60')
 P.S.: Results are equal to matlab/octave algorithm, which operate in frequency domain, with slight difference for even filter order
***************************************************************************/
void f_cheby_win(float *out, int N, float atten){ //Floating-point real taps Dolph-Chebyshev filter
    int nn, i;
    double M, n, sum = 0, max=0;
    double tg = pow(10,atten/20);  /* 1/r term [2], 10^gamma [2] */
    double x0 = cosh((1.0/(N-1))*acosh(tg));
    M = (N-1)/2;
    if(N%2==0) M = M + 0.5; /* handle even length windows */
    for(nn=0; nn<(N/2+1); nn++){
        n = nn-M;
        sum = 0;
        for(i=1; i<=M; i++){
            sum += cheby_poly(N-1,x0*cos(PI*i/N))*cos(2.0*n*PI*i/N);
        }
        *(out+nn) = tg + 2*sum;
        *(out+N-nn-1) = *(out+nn);
        if(out[nn]>max)max=out[nn];
    }
    for(nn=0; nn<N; nn++) *(out+nn) /= max; /* normalise everything */
    return;
}

void i_cheby_win(int16_t *out, int N, float atten){ //Fixed-point real taps Dolph-Chebyshev filter
    int nn, i;
    float y[N];
    double M, n, sum = 0, max=0;
    double tg = pow(10,atten/20);  /* 1/r term [2], 10^gamma [2] */
    double x0 = cosh((1.0/(N-1))*acosh(tg));
    M = (N-1)/2;
    if(N%2==0) M = M + 0.5; /* handle even length windows */
    for(nn=0; nn<(N/2+1); nn++){
        n = nn-M;
        sum = 0;
        for(i=1; i<=M; i++){
            sum += cheby_poly(N-1,x0*cos(PI*i/N))*cos(2.0*n*PI*i/N);
        }
        y[nn] = tg + 2*sum;
        y[N-nn-1] = y[nn];
        if(y[nn]>max)max=y[nn];
    }
    for(nn=0; nn<N; nn++) {
      *(out+nn) = (int16_t)((y[nn]/ max)*((1<<15)-1)); // normalise everything and scale 
    }
    return;
}
/***************************************************************************
Vector Multiplication SIMD
***************************************************************************/

static short reflip[8]  __attribute__((aligned(16))) = {1,-1,1,-1,1,-1,1,-1};  

void multcmplx(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN){
  /*!\fn void multcmplx_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN)
This function performs multiplication between complex vector fact1 and fact2 and store the result into out
@param out Output vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact1 Factor 1 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact2 Factor 2 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param N Length of x WARNING: N>=8

The function implemented is : \f$\mathbf{out} = \mathbf{fact1} * \mathbf{fact2}\f$
*/
  int16_t i;
  //int16_t *temps;
  __m128i mmtmpD0,mmtmpD1,mmtmpD2,mmtmpD3;
  __m128i *inX,*outX,*vec;
  outX= (__m128i *)&out[0];
  inX=(__m128i *)&fact1[0];
  vec=(__m128i *)&fact2[0];
  for(i=0;i<lIN>>2;i++){
    //temps = (int16_t *)inX;
    //printf("inX : %d,%d,%d,%d,%d,%d,%d,%d\n",temps[0],temps[1],temps[2],temps[3],temps[4],temps[5],temps[6],temps[7]);
    // Real part
    // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
    mmtmpD0 = _mm_madd_epi16(vec[0],_mm_sign_epi16(inX[0],*(__m128i*)&reflip[0]));
    mmtmpD1 = _mm_shufflelo_epi16(vec[0],_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
    //mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&conjugate[0]);
    // Imaginary part
    // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
    mmtmpD1 = _mm_madd_epi16(mmtmpD1,inX[0]);
    // take higher 15 bit
    mmtmpD0 = _mm_srai_epi32(mmtmpD0,15);
    mmtmpD1 = _mm_srai_epi32(mmtmpD1,15);
    // pack
    mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
    mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
    outX[0] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
    // Increase the pointers
    outX+=1;
    inX+=1;
    vec+=1;
  } 
}

void multcmplx_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN){
  /*!\fn void multcmplx_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN)
This function performs multiplication between complex vector fact1 and fact2 and store the result into out, summing with the previous value
@param out Output vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact1 Factor 1 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact2 Factor 2 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param N Length of x WARNING: N>=8

The function implemented is : \f$\mathbf{out} = out + (\mathbf{fact1} * \mathbf{fact2})\f$
*/
  int16_t i;
  //int16_t *temps;
  __m128i mmtmpD0,mmtmpD1,mmtmpD2,mmtmpD3;
  __m128i *inX,*outX,*vec;
  outX= (__m128i *)&out[0];
  inX=(__m128i *)&fact1[0];
  vec=(__m128i *)&fact2[0];
  for(i=0;i<lIN>>2;i++){
    //temps = (int16_t *)inX;
    //printf("inX : %d,%d,%d,%d,%d,%d,%d,%d\n",temps[0],temps[1],temps[2],temps[3],temps[4],temps[5],temps[6],temps[7]);
    // Real part
    // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
    mmtmpD0 = _mm_madd_epi16(vec[0],_mm_sign_epi16(inX[0],*(__m128i*)&reflip[0]));
    mmtmpD1 = _mm_shufflelo_epi16(vec[0],_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
    //mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&conjugate[0]);
    // Imaginary part
    // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
    mmtmpD1 = _mm_madd_epi16(mmtmpD1,inX[0]);
    // take higher 15 bit
    mmtmpD0 = _mm_srai_epi32(mmtmpD0,15);
    mmtmpD1 = _mm_srai_epi32(mmtmpD1,15);
    // pack
    mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
    mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
    outX[0] = _mm_adds_epi16(outX[0] ,_mm_packs_epi32(mmtmpD2,mmtmpD3)); //is necessary shift right to 1? _mm_srai_epi16(,1)
    // Increase the pointers
    outX+=1;
    inX+=1;
    vec+=1;
  } 
}

void multcmplx_conj(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN){
  /*!\fn void multcmplx_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN)
This function performs multiplication between complex vector fact1 and fact2 and store the result into out
@param out Output vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact1 Factor 1 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact2 Factor 2 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param N Length of x WARNING: N>=8

The function implemented is : \f$\mathbf{out} = \mathbf{fact1} * \mathbf{fact2}\f$
*/
  int16_t i;
  //int16_t *temps;
  __m128i mmtmpD0,mmtmpD1,mmtmpD2,mmtmpD3;
  __m128i *inX,*outX,*vec;
  outX= (__m128i *)&out[0];
  inX=(__m128i *)&fact1[0];
  vec=(__m128i *)&fact2[0];
  for(i=0;i<lIN>>2;i++){
    //temps = (int16_t *)inX;
    //printf("inX : %d,%d,%d,%d,%d,%d,%d,%d\n",temps[0],temps[1],temps[2],temps[3],temps[4],temps[5],temps[6],temps[7]);
    // Real part
    // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
    mmtmpD0 = _mm_madd_epi16(vec[0],inX[0]);
    mmtmpD1 = _mm_shufflelo_epi16(vec[0],_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&reflip[0]);
    // Imaginary part
    // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
    mmtmpD1 = _mm_madd_epi16(mmtmpD1,inX[0]);
    // take higher 15 bit
    mmtmpD0 = _mm_srai_epi32(mmtmpD0,15);
    mmtmpD1 = _mm_srai_epi32(mmtmpD1,15);
    // pack
    mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
    mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
    outX[0] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
    // Increase the pointers
    outX+=1;
    inX+=1;
    vec+=1;
  } 
}

void multcmplx_conj_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN){
  /*!\fn void multcmplx_add(int16_t *out,int16_t *fact1,int16_t *fact2,int16_t lIN)
This function performs multiplication between complex vector fact1 and fact2 and store the result into out, summing with the previous value
@param out Output vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact1 Factor 1 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param fact2 Factor 2 vector (Q1.15) in the format  |Re0  Im0|, |Re1 Im1|,......,|Re(N-1) Im(N-1)|
@param N Length of x WARNING: N>=8

The function implemented is : \f$\mathbf{out} = out + (\mathbf{fact1} * \mathbf{fact2})\f$
*/
  int16_t i;
  //int16_t *temps;
  __m128i mmtmpD0,mmtmpD1,mmtmpD2,mmtmpD3;
  __m128i *inX,*outX,*vec;
  outX= (__m128i *)&out[0];
  inX=(__m128i *)&fact1[0];
  vec=(__m128i *)&fact2[0];
  for(i=0;i<lIN>>2;i++){
    //temps = (int16_t *)inX;
    //printf("inX : %d,%d,%d,%d,%d,%d,%d,%d\n",temps[0],temps[1],temps[2],temps[3],temps[4],temps[5],temps[6],temps[7]);
    // Real part
    // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
   mmtmpD0 = _mm_madd_epi16(vec[0],inX[0]);
    mmtmpD1 = _mm_shufflelo_epi16(vec[0],_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
    mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&reflip[0]);
    // Imaginary part
    // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
    mmtmpD1 = _mm_madd_epi16(mmtmpD1,inX[0]);
    // take higher 15 bit
    mmtmpD0 = _mm_srai_epi32(mmtmpD0,15);
    mmtmpD1 = _mm_srai_epi32(mmtmpD1,15);
    // pack
    mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
    mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
    outX[0] = _mm_adds_epi16(outX[0] ,_mm_packs_epi32(mmtmpD2,mmtmpD3)); //is necessary shift right to 1? _mm_srai_epi16(,1)
    // Increase the pointers
    outX+=1;
    inX+=1;
    vec+=1;
  } 
}

int max_vec(int16_t *in,uint16_t lin){
  uint16_t i,index_v;
  int32_t a,b,abs_v,max_v;
  //  int32_t value[lin>>1];

  max_v=in[0]*in[0]+in[1]*in[1];
  index_v=0;
  for(i=0;i<lin>>1;i++){
    a=in[(i<<1)];
    b=in[(i<<1)+1];
    abs_v=a*a+b*b;
    //    value[i]=abs_v;
    max_v = (max_v>abs_v) ? max_v : abs_v; 
    index_v = (max_v>abs_v) ? index_v : i;
  }
  //write_output("AbsVector.m","AbsVec",value,lin>>1,1,0);
  return index_v;
}

/***************************************************************************
Upsampling Operation
***************************************************************************/

void ff_UpSampler(float *in, float *out, uint16_t lIN, uint16_t Up_factor){ //Upsamping float input float output
  uint16_t i;
  memset(out,0,lIN*Up_factor*sizeof(int16_t));
  for (i=0;i<lIN;i++){
    *(out+i*Up_factor)=*(in+i);
  }
}
void ii_UpSampler(int16_t *in, int16_t *out, uint16_t lIN, uint16_t Up_factor){ //Upsampling fixedpoint 16 bit input & output
  uint16_t i;
  memset(out,0,lIN*Up_factor*sizeof(int16_t));
  for (i=0;i<lIN;i++){
    *(out+i*Up_factor)=*(in+i);
  }
}
void ff_complex_UpSampler(float *in, float *out, uint16_t lIN, uint16_t Up_factor){ //Upsampling fixedpoint 16 bit input & output
  uint16_t i;
  memset(out,0,2*lIN*Up_factor*sizeof(float));
  for (i=0;i<lIN;i++){
    *(out+i*Up_factor)=*(in+i);
    *(out+i*Up_factor+1)=*(in+i+1);
  }
}
void ii_complex_UpSampler(int16_t *in, int16_t *out, uint16_t lIN, uint16_t Up_factor){ //Upsampling fixedpoint 16 bit input & output
  uint16_t i;
  memset(out,0,2*lIN*Up_factor*sizeof(int16_t));
  for (i=0;i<lIN;i++){
    *(out+i*Up_factor)=*(in+i);
    *(out+i*Up_factor+1)=*(in+i+1);
  }
}

/***************************************************************************
Upsampling Filtering Operation
***************************************************************************/

void ff_UpFilter(float *in, float *out , uint16_t lIN, float *hFIR , uint16_t lFIR, uint16_t Up_factor){ //Upsampling filter in floating point
  uint16_t i,j;
  for (i=0;i<(lIN*Up_factor+lFIR-1);i++){
    *(out+i)=0.0;
  }
  for (i=0;i<lIN;i++){
    for(j=0;j<lFIR;j++){
      *(out+i*Up_factor+j)+=(*(in+i))*(*(hFIR+j));
    }
  }
}

void ii_complx_UpFilter(int16_t *in, int16_t *out , uint16_t lIN, int16_t *hFIR , uint32_t lFIR, uint16_t Up_factor){ //Filtering function 
  // Processing on output, fixed input
  uint16_t i;
  memset(out,0,((lIN*Up_factor)+lFIR-1)*2*sizeof(int16_t));
  for (i=0;i<lIN;i++){
    multadd_real_vector_complex_scalar(hFIR,//*x-->filter length must be multiple of 8
                                        &in[i<<1],//alpha
                                        &out[Up_factor * (i<<1)],//*y
                                        lFIR);//N
  }
}

/***************************************************************************
Modulation Operation
***************************************************************************/
void CreateModvec(uint16_t n_rb,// number of resource block
		  uint16_t first_rb,// number of resource block
		  uint32_t FFTsize, //FFTsize
		  uint32_t size_l,
		  float *mod_vec){
  uint16_t i;
  float carrierind =(first_rb+12*(n_rb-1))+(float)(12+1)/2;
  for(i=0;i<size_l;i+=2){
     *(mod_vec+i)=(float)cos((float)2*PI*i*(carrierind-1)/FFTsize);
     *(mod_vec+i+1)=(float)sin((float)2*PI*i*(carrierind-1)/FFTsize);
  }
}

int16_t mod_vec[100][2560]  __attribute__((aligned(32)));

void ii_CreateModvec(uint16_t n_rb,// current resource block index
		  uint16_t first_carrier,// first subcarrier offset
		  uint32_t FFTsize, //FFTsize
		  uint32_t size_l, //array dimension
		  int16_t *mod_vec) //output array
{
  int16_t i;
  //float carrierind = (first_carrier+12*n_rb)+(float)(12+1)/2; //band are more or less superimposed if I put 6 instead of 12 
  float carrierind = (first_carrier+12*n_rb)+1; //FK: hack for lab
  // FK: handle wraparound
  if (carrierind >= FFTsize)
    carrierind = carrierind-FFTsize;
  for(i=0;i<size_l;i++){
     *(mod_vec+(i<<1))=(int16_t)((float)cos((float)2*PI*i*(carrierind-1)/FFTsize)*((1<<15)-1));
     *(mod_vec+(i<<1)+1)=(int16_t)((float)sin((float)2*PI*i*(carrierind-1)/FFTsize)*((1<<15)-1));
  }
}


/***************************************************************************
UFMC Modulation - Upsampling+Dolph-Chebyshev+FrequencyShilfting
***************************************************************************/

int16_t hFIR[152]  __attribute__((aligned(32))); // 152 is closest multiple of 8 to 145

void ufmc_init(uint32_t lFIR,  // (nb_prefix_samples)cyclic prefix length -> it becomes FIR length(multiple of 8)
	       int size, // input dimension(only real part) -> FFT dimension
	       int FFT_size,
	       int n_rb_max,
	       int first_carrier) {// dimensione of standard FFT

  float atten=60;
  uint16_t lOUT, 
	   lFIR_padded,
           quotient;

  int n_rb;

  lOUT=(FFT_size+lFIR)<<1; //output length(complex);
  if ((lFIR%0x0A)>0){ //lFIR!=10 || lFIR!=20 || lFIR!=40 || lFIR!=80 || lFIR!=120 || lFIR!=160 longer prefix from 1st symbol
    lFIR+=1;                   // 37 for 25 PRB, 73 for 50 PRB, 145 for 10 PRB
  }else{
    lFIR=(0x09*(lFIR/0x0A))+1; // bring down to : 37 for 25 PRB, 73 for 50 PRB, 145 for 10 PRB
  }
  quotient=lFIR/8;
  lFIR_padded = (lFIR%8)>0 ? (quotient+1)<<3 : quotient<<3;

  // Filter Impulse Response creation
  memset(hFIR,0,lFIR_padded*sizeof(int16_t));
  i_cheby_win(hFIR, lFIR, atten);
  //write_output("h_filter.m","h_filter",hFIR,lFIR_padded,1,0);
  for (n_rb=1;n_rb<n_rb_max;n_rb++) {
    ii_CreateModvec(n_rb,first_carrier,(1<<10),lOUT>>1,&mod_vec[n_rb-1][0]);
    //write_output("mod_vec.m","mod_vec",mod_vec, lOUT>>1,1,1);
  }
}

void dolph_cheb(int16_t *in, // input array-->length=(size+lFIR)*2
		int16_t *out, // output array-->length=(FFT_size+lFIR)*2
		uint32_t lFIR,  // (nb_prefix_samples)cyclic prefix length -> it becomes FIR length(multiple of 8)
		int size, // input dimension(only real part) -> FFT dimension
		int FFT_size, // dimensione of standard FFT
		int n_rb, //current resource block index 
		int first_carrier //first subcarrier offset
	       ) 
{
  
  uint16_t Up_factor=FFT_size/size, //Upsampling factor-->multiple of 4
	   lOUT, //output length(complex)
	   lFIR_padded,
	   quotient,
	   lOUT2;



  lOUT=(FFT_size+lFIR)<<1; //output length(complex);
  if ((lFIR%0x0A)>0){ //lFIR!=10 || lFIR!=20 || lFIR!=40 || lFIR!=80 || lFIR!=120 || lFIR!=160 EXTENDED cyclic prefix
    lFIR+=1;
  }else{
    lFIR=(0x09*(lFIR/0x0A))+1;
  }
  quotient=lFIR/8;
  lFIR_padded = (lFIR%8)>0 ? (quotient+1)<<3 : quotient<<3;
  lOUT2=(FFT_size+lFIR_padded)<<1; //filter output length (complex);

  int16_t out2[lOUT2] __attribute__((aligned(32)));

  memset(out2,0,lOUT2*sizeof(int16_t));
  // Upsampling and Filtering
  ii_complx_UpFilter(&in[0],&out2[0], size, &hFIR[0] , lFIR_padded, Up_factor);
  //write_output("FIR_out.m","FIR_out",out2, lOUT2>>1,1,1);
  // Modulation
  multcmplx_add(&out[0],&out2[0],&mod_vec[n_rb][0],lOUT>>1);
}

#ifdef MAIN

main()
{
  uint32_t lFIR=72; //filter length but see as cyclic prefix length --> 72 or 80
  uint16_t lIN=32, //length of only real part-->length of reduced FFT
	   FFT_size=1<<10, //length of entire FFT
	   Up_factor=FFT_size/lIN, //Upsampling factor-->multiple of 4
	   lOUT=(FFT_size+lFIR)*2, //output length
	   nb_rb=1; //first resource block (for frequency shifting)
  int16_t in1[2*lIN],in[2*lIN],out[lOUT];
  int16_t i;    

  // Random generation of source bit
  memset(in1,0,(lIN<<1)*sizeof(int16_t));
  for(i=0;i<lIN<<1;i+=2){
    in1[i<<1]=(rand()-(RAND_MAX>>1))>0 ? -32768 : 32767; //BPSK
  }
  
  idft64(&in1[0],&in[0],1);
  
  // Upsampling+Filtering+FrequencyShifting
  dolph_cheb(&in[0], // input
	     &out[0], // output
	     lFIR,  // (nb_prefix_samples)cyclic prefix length -> it becomes FIR length(multiple of 8)
	     lIN, // input dimension(only real part) -> FFT dimension
	     FFT_size,
	     nb_rb //first resource block index 
	       );
  // storing into file
  //write_output("Output.m","Output",out,lOUT,1,1);
  
}
#endif
