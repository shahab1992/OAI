#include <stdint.h>
#include <stdio.h>

#ifndef CUFFT_H
#define CUFFT_H
#include "cufft.h"
#endif

typedef struct {
  cufftHandle fft;
  cudaStream_t stream_ul;
  int    **d_rxdata;
  float2 **d_rxdata_fft;
  int    **d_rxdataF;
  short  N_RB_UL;
  short  nb_antennas_rx;
  short  symbols_per_tti;
  short  samples_per_tti;
  short  Ncp;
  short  fftsize;
  short  CP;
  short  CP0;
} ul_cu_t;

typedef struct {
  cufftHandle ifft;
  cudaStream_t stream_dl;
  short  *d_txdata;
  short  *d_txdata_o;
  float2 *d_txdata_ifft;
  short  *h_txdata;
  short  symbols_per_tti;
  short  samples_per_tti;
  short  Ncp;
  short  ifftsize;
  short  CP;
  short  CP0;
} dl_cu_t;
