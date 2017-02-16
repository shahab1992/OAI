#include "defs.h"
#include "PHY/CUDA/extern.h"
#include "PHY/LTE_TRANSPORT/extern.h"

#include <stdio.h>


int device_count;
dl_cu_t dl_cu[10];
ul_cu_t ul_cu[10];

void init_cufft( void );
void free_cufft( void );
void init_cuda(PHY_VARS_eNB *phy_vars_eNB, LTE_DL_FRAME_PARMS frame_parms )
{
  
  int i,j,k;
  int u,v,Msc_RS;
  cudaGetDeviceCount(&device_count);
  printf("[CUDA] now we have %d device\n",device_count);
  LTE_DL_FRAME_PARMS* const frame_parm = &phy_vars_eNB->lte_frame_parms;
  LTE_eNB_COMMON* const eNB_common_vars = &phy_vars_eNB->lte_eNB_common_vars;
  LTE_eNB_PUSCH** const eNB_pusch_vars  = phy_vars_eNB->lte_eNB_pusch_vars;
  LTE_eNB_SRS* const eNB_srs_vars       = phy_vars_eNB->lte_eNB_srs_vars;
  LTE_eNB_PRACH* const eNB_prach_vars   = &phy_vars_eNB->lte_eNB_prach_vars;
  for ( i = 0; i < device_count; i++ )
  {
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, i);
    printf("[CUDA] device number= %d, device name= %s\n",i, deviceProp.name);
  }
  
  for ( i = 0; i < 10; i++ )
  {
    ul_cu[i].CP = frame_parms.nb_prefix_samples;
    ul_cu[i].CP0= frame_parms.nb_prefix_samples0;
	
	ul_cu[i].fftsize = frame_parms.ofdm_symbol_size;
	ul_cu[i].Ncp = frame_parms.Ncp;
	ul_cu[i].symbols_per_tti         = frame_parms.symbols_per_tti;
	ul_cu[i].samples_per_tti         = frame_parms.samples_per_tti;
	ul_cu[i].nb_antennas_rx          = frame_parms.nb_antennas_rx;
	ul_cu[i].N_RB_UL                 = frame_parms.N_RB_UL;
	
	ul_cu[i].d_rxdata                = ( int **)malloc( frame_parms.nb_antennas_rx * sizeof( int *) );
	ul_cu[i].d_rxdata_fft            = ( float2 **)malloc( frame_parms.nb_antennas_rx * sizeof( float2 *) );
	ul_cu[i].d_rxdataF               = ( int **)malloc( frame_parms.nb_antennas_rx * sizeof( int *) );
	for ( k = 0; k < ul_cu[i].nb_antennas_rx; k++ )
	{
	  if(cudaMalloc(( void **)&ul_cu[i].d_rxdata[k]         , sizeof( int )* frame_parms.samples_per_tti))
            printf("error\n");
	  cudaMalloc(( void **)&ul_cu[i].d_rxdata_fft[k]     , sizeof( float2 )* ul_cu[i].symbols_per_tti* ul_cu[i].fftsize);
	  cudaMalloc(( void **)&ul_cu[i].d_rxdataF[k]        , 2* sizeof( int )* ul_cu[i].symbols_per_tti* ul_cu[i].fftsize );
	}
	
    dl_cu[i].CP = frame_parms.nb_prefix_samples;
    dl_cu[i].CP0= frame_parms.nb_prefix_samples0;
	
	dl_cu[i].ifftsize = frame_parms.ofdm_symbol_size;
	dl_cu[i].Ncp = frame_parms.Ncp;
	dl_cu[i].symbols_per_tti = frame_parms.symbols_per_tti;
	dl_cu[i].samples_per_tti = frame_parms.samples_per_tti;
  }
  printf("[CUDA] CP0=%d, CP=%d, fftsize=%d, symbols_per_tti=%d, samples_per_tti=%d\n",ul_cu[i].CP0,ul_cu[i].CP,frame_parms.ofdm_symbol_size,frame_parms.symbols_per_tti,frame_parms.samples_per_tti);

  init_cufft( );
  
}

void init_cufft( void )
{
  int i,j;
  short fftsize = ul_cu[i].fftsize; 
  short Ncp = ul_cu[i].Ncp; 
  short symbols_per_tti = ul_cu[i].symbols_per_tti; 
  short samples_per_tti = ul_cu[i].samples_per_tti;
  for ( i = 0; i < 10; i++ )
  {
  //for ul cuda
    cudaStreamCreateWithFlags( &( ul_cu[i].stream_ul ), cudaStreamNonBlocking );
	
	cufftPlan1d( &( ul_cu[i].fft ) , fftsize ,CUFFT_C2C, symbols_per_tti);
	cufftSetStream( ul_cu[i].fft , ul_cu[i].stream_ul );
	
  //for dl cuda
    cudaStreamCreateWithFlags( &( dl_cu[i].stream_dl ), cudaStreamNonBlocking );
	cufftPlan1d( &( dl_cu[i].ifft ) , fftsize ,CUFFT_C2C, symbols_per_tti);
	cudaMalloc((void **)&(dl_cu[i].d_txdata)     , sizeof( short )*(symbols_per_tti+1)* 2* symbols_per_tti*fftsize);
    cudaMalloc((void **)&(dl_cu[i].d_txdata_o)   , sizeof( short )* samples_per_tti* 2 );
    cudaMalloc((void **)&(dl_cu[i].d_txdata_ifft), sizeof( float2 )* symbols_per_tti* fftsize);
	cudaMallocHost((void **)&(dl_cu[i].h_txdata) , sizeof( short )* symbols_per_tti* 2* fftsize);  
    cufftSetStream( dl_cu[i].ifft , dl_cu[i].stream_dl );	
  }
}

void free_cuda(void)
{
  int i, j, k;
  for ( i = 0; i < 10; i++ )
  {
    cudaFree(ul_cu[i].d_rxdata);
	cudaFree(ul_cu[i].d_rxdata_fft);
	cufftDestroy(ul_cu[i].fft);
	for ( k = 0; k < ul_cu[i].nb_antennas_rx; k++ )
	{
	  cudaFree(ul_cu[i].d_rxdataF[k]);
	  cudaFree(ul_cu[i].d_rxdata[k]);
	  cudaFree(ul_cu[i].d_rxdata_fft[k]);
	}
	cudaStreamDestroy(ul_cu[i].stream_ul);
	cudaStreamDestroy(dl_cu[i].stream_dl);
	free(ul_cu[i].d_rxdata);
	free(ul_cu[i].d_rxdata_fft);
	free(ul_cu[i].d_rxdataF);
	cudaFreeHost(dl_cu[i].h_txdata);
    cudaFree(dl_cu[i].d_txdata);
    cudaFree(dl_cu[i].d_txdata_o);
    cudaFree(dl_cu[i].d_txdata_ifft);
	cufftDestroy(dl_cu[i].ifft);
  }

  printf("end cuda\n");
}












