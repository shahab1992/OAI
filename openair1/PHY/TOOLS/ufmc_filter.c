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
#include "log.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define PI 3.141592654

int16_t cos_tab_1024[1024] = {32767,32766,32764,32761,32757,32751,32744,32736,32727,32717,32705,32692,32678,32662,32646,32628,32609,32588,32567,32544,32520,32495,32468,32441,32412,32382,32350,32318,32284,32249,32213,32176,32137,32097,32056,32014,31970,31926,31880,31833,31785,31735,31684,31633,31580,31525,31470,31413,31356,31297,31236,31175,31113,31049,30984,30918,30851,30783,30713,30643,30571,30498,30424,30349,30272,30195,30116,30036,29955,29873,29790,29706,29621,29534,29446,29358,29268,29177,29085,28992,28897,28802,28706,28608,28510,28410,28309,28208,28105,28001,27896,27790,27683,27575,27466,27355,27244,27132,27019,26905,26789,26673,26556,26437,26318,26198,26077,25954,25831,25707,25582,25456,25329,25201,25072,24942,24811,24679,24546,24413,24278,24143,24006,23869,23731,23592,23452,23311,23169,23027,22883,22739,22594,22448,22301,22153,22004,21855,21705,21554,21402,21249,21096,20942,20787,20631,20474,20317,20159,20000,19840,19680,19519,19357,19194,19031,18867,18702,18537,18371,18204,18036,17868,17699,17530,17360,17189,17017,16845,16672,16499,16325,16150,15975,15799,15623,15446,15268,15090,14911,14732,14552,14372,14191,14009,13827,13645,13462,13278,13094,12909,12724,12539,12353,12166,11980,11792,11604,11416,11227,11038,10849,10659,10469,10278,10087,9895,9703,9511,9319,9126,8932,8739,8545,8351,8156,7961,7766,7571,7375,7179,6982,6786,6589,6392,6195,5997,5799,5601,5403,5205,5006,4807,4608,4409,4210,4011,3811,3611,3411,3211,3011,2811,2610,2410,2209,2009,1808,1607,1406,1206,1005,804,603,402,201,0,-202,-403,-604,-805,-1006,-1207,-1407,-1608,-1809,-2010,-2210,-2411,-2611,-2812,-3012,-3212,-3412,-3612,-3812,-4012,-4211,-4410,-4609,-4808,-5007,-5206,-5404,-5602,-5800,-5998,-6196,-6393,-6590,-6787,-6983,-7180,-7376,-7572,-7767,-7962,-8157,-8352,-8546,-8740,-8933,-9127,-9320,-9512,-9704,-9896,-10088,-10279,-10470,-10660,-10850,-11039,-11228,-11417,-11605,-11793,-11981,-12167,-12354,-12540,-12725,-12910,-13095,-13279,-13463,-13646,-13828,-14010,-14192,-14373,-14553,-14733,-14912,-15091,-15269,-15447,-15624,-15800,-15976,-16151,-16326,-16500,-16673,-16846,-17018,-17190,-17361,-17531,-17700,-17869,-18037,-18205,-18372,-18538,-18703,-18868,-19032,-19195,-19358,-19520,-19681,-19841,-20001,-20160,-20318,-20475,-20632,-20788,-20943,-21097,-21250,-21403,-21555,-21706,-21856,-22005,-22154,-22302,-22449,-22595,-22740,-22884,-23028,-23170,-23312,-23453,-23593,-23732,-23870,-24007,-24144,-24279,-24414,-24547,-24680,-24812,-24943,-25073,-25202,-25330,-25457,-25583,-25708,-25832,-25955,-26078,-26199,-26319,-26438,-26557,-26674,-26790,-26906,-27020,-27133,-27245,-27356,-27467,-27576,-27684,-27791,-27897,-28002,-28106,-28209,-28310,-28411,-28511,-28609,-28707,-28803,-28898,-28993,-29086,-29178,-29269,-29359,-29447,-29535,-29622,-29707,-29791,-29874,-29956,-30037,-30117,-30196,-30273,-30350,-30425,-30499,-30572,-30644,-30714,-30784,-30852,-30919,-30985,-31050,-31114,-31176,-31237,-31298,-31357,-31414,-31471,-31526,-31581,-31634,-31685,-31736,-31786,-31834,-31881,-31927,-31971,-32015,-32057,-32098,-32138,-32177,-32214,-32250,-32285,-32319,-32351,-32383,-32413,-32442,-32469,-32496,-32521,-32545,-32568,-32589,-32610,-32629,-32647,-32663,-32679,-32693,-32706,-32718,-32728,-32737,-32745,-32752,-32758,-32762,-32765,-32767,-32767,-32767,-32765,-32762,-32758,-32752,-32745,-32737,-32728,-32718,-32706,-32693,-32679,-32663,-32647,-32629,-32610,-32589,-32568,-32545,-32521,-32496,-32469,-32442,-32413,-32383,-32351,-32319,-32285,-32250,-32214,-32177,-32138,-32098,-32057,-32015,-31971,-31927,-31881,-31834,-31786,-31736,-31685,-31634,-31581,-31526,-31471,-31414,-31357,-31298,-31237,-31176,-31114,-31050,-30985,-30919,-30852,-30784,-30714,-30644,-30572,-30499,-30425,-30350,-30273,-30196,-30117,-30037,-29956,-29874,-29791,-29707,-29622,-29535,-29447,-29359,-29269,-29178,-29086,-28993,-28898,-28803,-28707,-28609,-28511,-28411,-28310,-28209,-28106,-28002,-27897,-27791,-27684,-27576,-27467,-27356,-27245,-27133,-27020,-26906,-26790,-26674,-26557,-26438,-26319,-26199,-26078,-25955,-25832,-25708,-25583,-25457,-25330,-25202,-25073,-24943,-24812,-24680,-24547,-24414,-24279,-24144,-24007,-23870,-23732,-23593,-23453,-23312,-23170,-23028,-22884,-22740,-22595,-22449,-22302,-22154,-22005,-21856,-21706,-21555,-21403,-21250,-21097,-20943,-20788,-20632,-20475,-20318,-20160,-20001,-19841,-19681,-19520,-19358,-19195,-19032,-18868,-18703,-18538,-18372,-18205,-18037,-17869,-17700,-17531,-17361,-17190,-17018,-16846,-16673,-16500,-16326,-16151,-15976,-15800,-15624,-15447,-15269,-15091,-14912,-14733,-14553,-14373,-14192,-14010,-13828,-13646,-13463,-13279,-13095,-12910,-12725,-12540,-12354,-12167,-11981,-11793,-11605,-11417,-11228,-11039,-10850,-10660,-10470,-10279,-10088,-9896,-9704,-9512,-9320,-9127,-8933,-8740,-8546,-8352,-8157,-7962,-7767,-7572,-7376,-7180,-6983,-6787,-6590,-6393,-6196,-5998,-5800,-5602,-5404,-5206,-5007,-4808,-4609,-4410,-4211,-4012,-3812,-3612,-3412,-3212,-3012,-2812,-2611,-2411,-2210,-2010,-1809,-1608,-1407,-1207,-1006,-805,-604,-403,-202,-1,201,402,603,804,1005,1206,1406,1607,1808,2009,2209,2410,2610,2811,3011,3211,3411,3611,3811,4011,4210,4409,4608,4807,5006,5205,5403,5601,5799,5997,6195,6392,6589,6786,6982,7179,7375,7571,7766,7961,8156,8351,8545,8739,8932,9126,9319,9511,9703,9895,10087,10278,10469,10659,10849,11038,11227,11416,11604,11792,11980,12166,12353,12539,12724,12909,13094,13278,13462,13645,13827,14009,14191,14372,14552,14732,14911,15090,15268,15446,15623,15799,15975,16150,16325,16499,16672,16845,17017,17189,17360,17530,17699,17868,18036,18204,18371,18537,18702,18867,19031,19194,19357,19519,19680,19840,20000,20159,20317,20474,20631,20787,20942,21096,21249,21402,21554,21705,21855,22004,22153,22301,22448,22594,22739,22883,23027,23169,23311,23452,23592,23731,23869,24006,24143,24278,24413,24546,24679,24811,24942,25072,25201,25329,25456,25582,25707,25831,25954,26077,26198,26318,26437,26556,26673,26789,26905,27019,27132,27244,27355,27466,27575,27683,27790,27896,28001,28105,28208,28309,28410,28510,28608,28706,28802,28897,28992,29085,29177,29268,29358,29446,29534,29621,29706,29790,29873,29955,30036,30116,30195,30272,30349,30424,30498,30571,30643,30713,30783,30851,30918,30984,31049,31113,31175,31236,31297,31356,31413,31470,31525,31580,31633,31684,31735,31785,31833,31880,31926,31970,32014,32056,32097,32137,32176,32213,32249,32284,32318,32350,32382,32412,32441,32468,32495,32520,32544,32567,32588,32609,32628,32646,32662,32678,32692,32705,32717,32727,32736,32744,32751,32757,32761,32764,32766};


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
  int16_t carrierind = (first_carrier+12*n_rb)+1; //FK: hack for lab

  if (FFTsize!=1024) {
    LOG_E(PHY,"UFMC modulation only supported for FFT size 1024 for the moment\n");
    exit(-1);
  }

  // FK: handle wraparound
  if (carrierind >= FFTsize)
    carrierind = carrierind-FFTsize;
  /*
  for(i=0;i<size_l;i++){
     *(mod_vec+(i<<1))=(int16_t)((float)cos((float)2*PI*i*(carrierind-1)/FFTsize)*((1<<15)-1));
     *(mod_vec+(i<<1)+1)=(int16_t)((float)sin((float)2*PI*i*(carrierind-1)/FFTsize)*((1<<15)-1));
  }
  */
  for(i=0;i<size_l;i++){
    *(mod_vec+(i<<1))   = cos_tab_1024[(i*(carrierind-1))%FFTsize];
    *(mod_vec+(i<<1)+1) = cos_tab_1024[(256-i*(carrierind-1))%FFTsize]; //sin(x) = cos(pi/2-x);
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
