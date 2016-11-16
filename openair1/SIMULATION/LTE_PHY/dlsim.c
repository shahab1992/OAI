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
  OpenAirInterface Dev  : openair4g-devel@lists.eurecom.fr

  Address      : Eurecom, Campus SophiaTech, 450 Route des Chappes, CS 50193 - 06904 Biot Sophia Antipolis cedex, FRANCE

 *******************************************************************************/

/*! \file dlsim.c
 \brief Top-level DL simulator
 \author R. Knopp
 \date 2011 - 2014
 \version 0.1
 \company Eurecom
 \email: knopp@eurecom.fr
 \note
 \warning
*/

//#define DEBUG_HARQ
//#define PRINT_THROUGHPUT

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>

#include "SIMULATION/TOOLS/defs.h"
#include "PHY/types.h"
#include "PHY/defs.h"
#include "PHY/vars.h"

#include "SCHED/defs.h"
#include "SCHED/vars.h"
#include "LAYER2/MAC/vars.h"
#include "OCG_vars.h"
#include "UTIL/LOG/log.h"
#include "UTIL/LISTS/list.h"

extern unsigned int dlsch_tbs25[27][25],TBStable[27][110];
extern unsigned char offset_mumimo_llr_drange_fix;

extern uint8_t interf_unaw_shift0;
extern uint8_t interf_unaw_shift1;
extern uint8_t interf_unaw_shift;

#include "PHY/TOOLS/lte_phy_scope.h"

#define PRINT_BYTES

PHY_VARS_eNB *PHY_vars_eNB;
PHY_VARS_UE *PHY_vars_UE;

int otg_enabled=0;
/*the following parameters are used to control the processing times calculations*/
double t_tx_max = -1000000000; /*!< \brief initial max process time for tx */
double t_rx_max = -1000000000; /*!< \brief initial max process time for rx */
double t_tx_min = 1000000000; /*!< \brief initial min process time for tx */
double t_rx_min = 1000000000; /*!< \brief initial min process time for rx */
int n_tx_dropped = 0; /*!< \brief initial max process time for tx */
int n_rx_dropped = 0; /*!< \brief initial max process time for rx */

void handler(int sig)
{
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}



//DCI2_5MHz_2A_M10PRB_TDD_t DLSCH_alloc_pdu2_2A[2];

DCI1E_5MHz_2A_M10PRB_TDD_t  DLSCH_alloc_pdu2_1E[2];
uint64_t DLSCH_alloc_pdu_1[2];

#define UL_RB_ALLOC 0x1ff;
#define CCCH_RB_ALLOC computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,0,2)
//#define DLSCH_RB_ALLOC 0x1fbf // igore DC component,RB13
//#define DLSCH_RB_ALLOC 0x0001
void do_OFDM_mod_l(int32_t **txdataF, int32_t **txdata, uint16_t next_slot, LTE_DL_FRAME_PARMS *frame_parms)
{

  int aa, slot_offset, slot_offset_F;

  slot_offset_F = (next_slot)*(frame_parms->ofdm_symbol_size)*((frame_parms->Ncp==1) ? 6 : 7);
  slot_offset = (next_slot)*(frame_parms->samples_per_tti>>1);

  for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
    //    printf("Thread %d starting ... aa %d (%llu)\n",omp_get_thread_num(),aa,rdtsc());

    if (frame_parms->Ncp == 1)
      PHY_ofdm_mod(&txdataF[aa][slot_offset_F],        // input
                   &txdata[aa][slot_offset],         // output
                   frame_parms->ofdm_symbol_size,
                   6,                 // number of symbols
                   frame_parms->nb_prefix_samples,               // number of prefix samples
                   CYCLIC_PREFIX);
    else {
      normal_prefix_mod(&txdataF[aa][slot_offset_F],
                        &txdata[aa][slot_offset],
                        7,
                        frame_parms);
    }


  }

}

int main(int argc, char **argv)
{

  int c;
  int k,i,aa,aarx,aatx;

  int s,Kr,Kr_bytes;

  double sigma2, sigma2_dB=10,SNR,snr0=-2.0,snr1,rate[2];
  double snr_step=1,input_snr_step=1, snr_int=30;

  LTE_DL_FRAME_PARMS *frame_parms;
  double **s_re,**s_im,**r_re,**r_im;
  double forgetting_factor=0.0; //in [0,1] 0 means a new channel every time, 1 means keep the same channel
  double iqim=0.0;

  uint8_t extended_prefix_flag=0,transmission_mode=1,n_tx=1,n_rx=1;
  uint16_t Nid_cell=0;

  int8_t eNB_id = 0, eNB_id_i = 1;
  unsigned char mcs1=0,mcs2=0,mcs_i=0,awgn_flag=0,dci_flag=0;
  unsigned char i_mod = 2;
  unsigned short NB_RB;
  unsigned char Ns,l,m;
  uint16_t tdd_config=3;
  uint16_t n_rnti=0x1234;
  int n_users = 1;
  int TB=0;
  RX_type_t rx_type=rx_standard;
  unsigned char  cur_harq_pid;


  SCM_t channel_model=Rayleigh1;
  //  unsigned char *input_data,*decoded_output;

  unsigned char *input_buffer0[2],*input_buffer1[2];
  unsigned short input_buffer_length0,input_buffer_length1;

  /* A variable "ret" is a nuumber of turbo iterations, that are performed in the turbo-decoder for each CW.
  The decoder checks CRC, and if CRC test fails, it increments "ret". Ret is between 1 and 4, where 4 is for
  the max number of turbo iterations. If the CRC is passed, ret is equal to a current iteration.
  This is done separately for each CW inside decoding process.
  Again: this is not a HARQ retransmission!*/
  unsigned int ret[2];
  unsigned int coded_bits_per_codeword[2],nsymb,dci_cnt,tbs[2];

  unsigned int tx_lev=0, tx_lev_dB=0, round=0, trials, errs[2][4]={{0,0,0,0},{0,0,0,0}}, round_trials[2][4]={{0,0,0,0},{0,0,0,0}}, round_sic=0;
  unsigned int dci_errors=0, dlsch_active=0, num_layers;

  int re_allocated;
  char fname[32],vname[32];
  FILE *bler_fd;
  char bler_fname[256];
  FILE *time_meas_fd;
  char time_meas_fname[256];

  //qFILE *tikz_fd;
  //char tikz_fname[256];

  FILE *input_trch_fd=NULL;
  unsigned char input_trch_file=0;
  FILE *input_fd=NULL;
  unsigned char input_file=0;
  //  char input_val_str[50],input_val_str2[50];

  char input_trch_val[16];
  double channelx,channely;

  //  unsigned char pbch_pdu[6];

  DCI_ALLOC_t dci_alloc[8],dci_alloc_rx[8];
  int num_common_dci=0,num_ue_spec_dci=0,num_dci=0;

  //  FILE *rx_frame_file;

  int n_frames;
  int n_ch_rlz = 1;
  channel_desc_t *eNB2UE[8];
  uint8_t num_pdcch_symbols=1,num_pdcch_symbols_2=0;
  uint8_t pilot1,pilot2,pilot3;
  uint8_t rx_sample_offset = 0;
  //char stats_buffer[4096];
  //int len;

  uint8_t num_rounds = 4;
  uint8_t subframe=7;
  int u;
  int n=0;
  int abstx=0;
  int iii;
  FILE *csv_fd=NULL;
  char csv_fname[512];
  int ch_realization;
  int pmi_feedback=0;
  int hold_channel=0;

  // temporarily for retransmissions:
  unsigned char resend_cw1=0; //if 0 resend only cw0
  unsigned char resend_cw0_cw1=1; //if 0 resend both cw in a normal way

  // void *data;
  // int ii;
  int bler;
  double blerr[2][4],uncoded_ber,avg_ber;
  short *uncoded_ber_bit=NULL;
  uint8_t N_RB_DL=25,osf=1;
  uint8_t fdd_flag = 0;
  frame_t frame_type = FDD;
  int xforms=0;
  FD_lte_phy_scope_ue *form_ue = NULL;
  char title[255];
  uint32_t DLSCH_RB_ALLOC = 0x1fff;
  int numCCE=0;
  int dci_length_bytes=0,dci_length=0;
  int common_flag=0,TPC=0;

  double cpu_freq_GHz;
  //time_stats_t ts;//,sts,usts;
  int avg_iter[2],iter_trials[2];
  int rballocset=0;
  int print_perf=0;
  int test_perf=0;
  int dump_table=0;
  int llr8_flag=0;

  double effective_rate=0.0;
  double thr_cw0_tm4 = 0.0;
  double thr_cw0_tm4_nonconst = 0.0;
  double thr_cw0[4]={0,0,0,0}, thr_cw1[4]={0,0,0,0}, thr_cw0_tot = 0.0, thr_cw1_tot = 0.0;
  unsigned int tbs0_init=0, tbs1_init=0;
  double rate0_init=0.0, rate1_init=0.0;
  int mcs0_init=0, mcs1_init=0, mod_order0_init = 0, mod_order1_init=0;
  char channel_model_input[17]="I";

  int TB0_active = 1;
  uint32_t perfect_ce = 0;

  LTE_DL_UE_HARQ_t *dlsch0_ue_harq;
  LTE_DL_eNB_HARQ_t *dlsch0_eNB_harq;
  uint8_t Kmimo;

  int32_t **sic_buffer;
  int8_t cw_to_decode_interf;
  int8_t cw_to_decode_interf_free;
  int8_t  cw_non_sic;
  int8_t  cw_sic;
  FILE    *proc_fd = NULL;
  char buf[64];
  uint8_t ue_category=4;
  uint32_t Nsoft;

  int CCE_table[800];

  int threequarter_fs=0;



  opp_enabled=1; // to enable the time meas

#if defined(__arm__)
  FILE    *proc_fd = NULL;
  char buf[64];

  proc_fd = fopen("/sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq", "r");
  if(!proc_fd)
     printf("cannot open /sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq");
  else {
     while(fgets(buf, 63, proc_fd))
        printf("%s", buf);
  }
  fclose(proc_fd);
  cpu_freq_GHz = ((double)atof(buf))/1e6;
#else
  cpu_freq_GHz = get_cpu_freq_GHz();
#endif
  printf("Detected cpu_freq %f GHz\n",cpu_freq_GHz);

  //signal(SIGSEGV, handler);
  //signal(SIGABRT, handler);

  logInit();
  set_glog(LOG_INFO, LOG_MED);

  // default parameters
  n_frames = 1000;
  snr0 = 0;
  //  num_layers = 1;
  perfect_ce = 0;

  while ((c = getopt (argc, argv, "ahdpZDe:Em:n:o:s:f:t:c:g:r:F:x:y:z:AM:N:I:i:O:R:S:C:T:b:u:v:w:B:PLl:XYv:W:J:")) != -1) {

    switch (c) {
    case 'a':
      awgn_flag = 1;
      channel_model = AWGN;
      break;

    case 'A':
      abstx = 1;
      break;

    case 'b':
      tdd_config=atoi(optarg);
      break;

    case 'B':
      N_RB_DL=atoi(optarg);
      break;

    case 'c':
      num_pdcch_symbols=atoi(optarg);
      break;

    case 'C':
      Nid_cell = atoi(optarg);
      break;

    case 'd':
      dci_flag = 1;
      break;

    case 'D':
      frame_type=TDD;
      break;

    case 'e':
      num_rounds=1;
      common_flag = 1;
      TPC = atoi(optarg);
      break;

    case 'E':
      threequarter_fs=1;
      break;

    case 'f':
      input_snr_step= atof(optarg);
      break;

    case 'F':
      forgetting_factor = atof(optarg);
      break;

    case 'i':
      input_fd = fopen(optarg,"r");
      input_file=1;
      dci_flag = 1;
      break;

    case 'I':
      input_trch_fd = fopen(optarg,"r");
      input_trch_file=1;
      break;

    case 'L':
      llr8_flag=1;
      break;

    case 'l':
      offset_mumimo_llr_drange_fix=atoi(optarg);
      break;

    case 'm':
      mcs1 = atoi(optarg);
      break;

    case 'M':
      mcs2 = atoi(optarg);
      //i_mod = get_Qm(mcs2); /// think here again!!!
      break;

    case 'O':
      test_perf=atoi(optarg);
      //print_perf =1;
      break;

    case 't':
      mcs_i = atoi(optarg);
      i_mod = get_Qm(mcs_i);
      break;

    case 'n':
      n_frames = atoi(optarg);
      break;

    case 'o':
      rx_sample_offset = atoi(optarg);
      break;

    case 'r':
      DLSCH_RB_ALLOC = atoi(optarg);
      rballocset = 1;
      break;

    case 's':
      snr0 = atof(optarg);
      break;

    case 'w':
      snr_int = atof(optarg);
      break;

    case 'N':
      n_ch_rlz= atof(optarg);
      break;

    case 'p':
      extended_prefix_flag=1;
      break;

    case 'g':
      memcpy(channel_model_input,optarg,17);

      switch((char)*optarg) {
      case 'A':
        channel_model=SCM_A;
        break;
      case 'B':
        channel_model=SCM_B;
        break;
      case 'C':
        channel_model=SCM_C;
        break;
      case 'D':
        channel_model=SCM_D;
        break;
      case 'E':
        channel_model=EPA;
        break;
      case 'F':
        channel_model=EVA;
        break;
      case 'G':
        channel_model=ETU;
        break;
      case 'H':
        channel_model=Rayleigh8;
        break;
      case 'I':
        channel_model=Rayleigh1;
        break;
      case 'J':
        channel_model=Rayleigh1_corr;
        break;
      case 'K':
        channel_model=Rayleigh1_anticorr;
        break;
      case 'L':
        channel_model=Rice8;
        break;
      case 'M':
        channel_model=Rice1;
        break;
      case 'N':
        channel_model=AWGN;
        break;
      case 'P':
        channel_model=Rayleigh1_orthogonal;
        break;
      case 'Q':
        channel_model=Rayleigh1_orth_eff_ch_TM4_prec_real; // for DUALSTREAM_UNIFORM_PRECODING1 when interf is precancelled
        break;
      case 'R':
        channel_model=Rayleigh1_orth_eff_ch_TM4_prec_imag; // for DUALSTREAM_UNIFORM_PRECODINGj when interf is precancelled
        break;
      case 'S':
        channel_model=Rayleigh8_orth_eff_ch_TM4_prec_real;//when interf is precancelled
        break;
      case 'T':
        channel_model=Rayleigh8_orth_eff_ch_TM4_prec_imag;//when interf is precancelled
        break;
      case 'U':
        channel_model = TS_SHIFT;
        break;
      default:
        msg("Unsupported channel model!\n");
        exit(-1);
      }
      break;
      case 'x':
      transmission_mode=atoi(optarg);
      if ((transmission_mode!=1) &&
          (transmission_mode!=2) &&
          (transmission_mode!=3) &&
          (transmission_mode!=4) &&
          (transmission_mode!=5) &&
          (transmission_mode!=6)) {
        msg("Unsupported transmission mode %d\n",transmission_mode);
        exit(-1);
      }
      if (transmission_mode>1) {
        n_tx = 2;
      }
      break;
      case 'y':
      n_tx=atoi(optarg);
      if ((n_tx==0) || (n_tx>2)) {
        msg("Unsupported number of tx antennas %d\n",n_tx);
        exit(-1);
      }
      break;
      case 'z':
      n_rx=atoi(optarg);
      if ((n_rx==0) || (n_rx>2)) {
        msg("Unsupported number of rx antennas %d\n",n_rx);
        exit(-1);
      }
      break;

      case 'R':
        num_rounds=atoi(optarg);
        break;
      case 'S':
        subframe=atoi(optarg);
        break;
      case 'T':
        n_rnti=atoi(optarg);
        break;
      case 'u':
        rx_type = (RX_type_t) atoi(optarg);
        if (rx_type<rx_standard || rx_type>rx_SIC_dual_stream) {
          printf("Unsupported rx type %d\n",rx_type);
          exit(-1);
        }
        break;
      case 'v':
        i_mod = atoi(optarg);
        if (i_mod!=2 && i_mod!=4 && i_mod!=6) {
          msg("Wrong i_mod %d, should be 2,4 or 6\n",i_mod);
          exit(-1);
        }
        break;
      case 'P':
        print_perf=1;
        break;
      case 'X':
        xforms = 1;
        break;
      case 'Z':
        dump_table=1;
        break;
      case 'Y':
        perfect_ce=1;
        break;
      case 'V':
        interf_unaw_shift0=atoi(optarg);
        break;
      case 'W':
        interf_unaw_shift1=atoi(optarg);
        break;
      case 'J':
        interf_unaw_shift=atoi(optarg);
        break;
      case 'h':
      default:
      printf("%s -h(elp) -a(wgn on) -d(ci decoding on) -p(extended prefix on) -m mcs1 -M mcs2 -n n_frames -s snr0 -x transmission mode (1,2,3,5,6) -y TXant -z RXant -I trch_file\n",argv[0]);
      printf("-h This message\n");
      printf("-a Use AWGN channel and not multipath\n");
      printf("-c Number of PDCCH symbols\n");
      printf("-m MCS1 for TB 1\n");
      printf("-M MCS2 for TB 2\n");
      printf("-d Transmit the DCI and compute its error statistics and the overall throughput\n");
      printf("-p Use extended prefix mode\n");
      printf("-n Number of frames to simulate\n");
      printf("-o Sample offset for receiver\n");
      printf("-s Starting SNR, runs from SNR to SNR+%.1fdB in steps of %.1fdB. If n_frames is 1 then just SNR is simulated and MATLAB/OCTAVE output is generated\n", snr_int, snr_step);
      printf("-f step size of SNR, default value is 1.\n");
      printf("-r resource block allocation (see  section 7.1.6.3 in 36.213\n");
      printf("-g Channel model, possible values are 3GPP 25.814 SCM-A/B/C/D('A','B','C','D'), 36-101 EPA('E'), EVA ('F'),ETU('G'), Rayghleigh8 ('H'), Rayleigh1('I'), Rayleigh1_corr('J'), Rayleigh1_anticorr('K'), Rice8('L'), Rice1('M'), AWGN('N'), Rayleigh1_orthogonal('P'), Rayleigh1_orth_eff_ch_TM4_prec_real ('Q'), Rayleigh1_orth_eff_ch_TM4_prec_imag ('R'), Rayleigh8_orth_eff_ch_TM4_prec_real ('S'),Rayleigh8_orth_eff_ch_TM4_prec_imag ('T')   \n");
      printf("-F forgetting factor (0 new channel every trial, 1 channel constant\n");
      printf("-x Transmission mode (1,2,6 for the moment)\n");
      printf("-y Number of TX antennas used in eNB\n");
      printf("-z Number of RX antennas used in UE\n");
      printf("-t MCS of interfering UE\n");
      printf("-R Number of HARQ rounds (fixed)\n");
      printf("-A Turns on calibration mode for abstraction.\n");
      printf("-N Determines the number of Channel Realizations in Abstraction mode. Default value is 1. \n");
      printf("-O Set the percentage of effective rate to testbench the modem performance (typically 30 and 70, range 1-100) \n");
      printf("-I Input filename for TrCH data (binary)\n");
      printf("-u Receiver type: 0=standard, 1 = single stream IC (for TM3,4,5,6), 2 = dual stream IC (for TM3,4), 3 = SIC (for TM3,4) \n");
      exit(1);
      break;

    }
  }

  if (common_flag == 0) {
    switch (N_RB_DL) {
    case 6:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x3f;
      num_pdcch_symbols = 3;
      break;

    case 25:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1fff;
      break;

    case 50:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1ffff;
      break;

    case 100:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1ffffff;
      break;
    }

    NB_RB=conv_nprb(0,DLSCH_RB_ALLOC,N_RB_DL);
  } else
    NB_RB = 4;

  NB_RB=conv_nprb(0,DLSCH_RB_ALLOC,N_RB_DL);

  if ((transmission_mode > 1) && (n_tx != 2)) {

    printf("n_tx must be >1 for transmission_mode %d\n",transmission_mode);
    exit(-1);
  }

  if (((transmission_mode==1) || (transmission_mode==2)) && (rx_type != rx_standard)) {
    printf("only standard rx available for TM1 and TM2\n");
    exit(-1);
  }
  if (((transmission_mode==5) || (transmission_mode==6)) && (rx_type > rx_IC_single_stream)) {
    printf("only standard rx or single stream IC available for TM5 and TM6\n");
    exit(-1);
  }


  if (xforms==1) {
  fl_initialize (&argc, argv, NULL, 0, 0);
  form_ue = create_lte_phy_scope_ue();
  sprintf (title, "LTE PHY SCOPE eNB");
  fl_show_form (form_ue->lte_phy_scope_ue, FL_PLACE_HOTSPOT, FL_FULLBORDER, title);

  /*
  if (rx_type==rx_IC_single_stream) {
    openair_daq_vars.use_ia_receiver = 1;
    fl_set_button(form_ue->button_0,1);
    fl_set_object_label(form_ue->button_0, "IA Receiver ON");
    fl_set_object_color(form_ue->button_0, FL_GREEN, FL_GREEN);
  }

  */
  }

  if (transmission_mode==5) {
    n_users = 2;
    eNB_id_i = PHY_vars_UE->n_connected_eNB;
    //eNB_id_i=1;
  }
  else
    eNB_id_i = eNB_id;

  lte_param_init(n_tx,
                 n_rx,
                 transmission_mode,
                 extended_prefix_flag,
                 frame_type,
                 Nid_cell,
                 tdd_config,
                 N_RB_DL,
                 threequarter_fs,
                 osf,
                 perfect_ce);


  printf("Setting mcs1 = %d\n",mcs1);
  printf("Setting mcs2 = %d\n",mcs2);
  printf("NPRB = %d\n",NB_RB);
  printf("n_frames = %d\n",n_frames);
  printf("Transmission mode %d with %dx%d antenna configuration, Extended Prefix %d\n",transmission_mode,n_tx,n_rx,extended_prefix_flag);
  printf("Using receiver type %d\n", rx_type);
  printf("TM1 shift %d\n", interf_unaw_shift);
  //printf("Using I_UA rec shift layer 1  %d\n", interf_unaw_shift0);
  //printf("Using I_UA rec shift layer 2  %d\n", interf_unaw_shift1);
  snr1 = snr0+snr_int;
  printf("SNR0 %f, SNR1 %f\n",snr0,snr1);

  /*
    txdataF    = (int **)malloc16(2*sizeof(int*));
    txdataF[0] = (int *)malloc16(FRAME_LENGTH_BYTES);
    txdataF[1] = (int *)malloc16(FRAME_LENGTH_BYTES);

    txdata    = (int **)malloc16(2*sizeof(int*));
    txdata[0] = (int *)malloc16(FRAME_LENGTH_BYTES);
    txdata[1] = (int *)malloc16(FRAME_LENGTH_BYTES);
  */

  frame_parms = &PHY_vars_eNB->lte_frame_parms;

  s_re = malloc(2*sizeof(double*)); //transmitted signal (Sent)
  s_im = malloc(2*sizeof(double*));
  r_re = malloc(2*sizeof(double*)); //received signal
  r_im = malloc(2*sizeof(double*));
  //  r_re0 = malloc(2*sizeof(double*));
  //  r_im0 = malloc(2*sizeof(double*));

  nsymb = (PHY_vars_eNB->lte_frame_parms.Ncp == 0) ? 14 : 12;

  printf("Channel Model= (%s,%d)\n",channel_model_input, channel_model);
  printf("SCM-A=%d, SCM-B=%d, SCM-C=%d, SCM-D=%d, EPA=%d, EVA=%d, ETU=%d, Rayleigh8=%d, Rayleigh1=%d, Rayleigh1_corr=%d, Rayleigh1_anticorr=%d, Rice1=%d, Rice8=%d, Rayleigh1_orthogonal=%d, Rayleigh1_orth_eff_ch_TM4_prec_real22=%d, Rayleigh1_orth_eff_ch_TM4_prec_imag=%d, Rayleigh8_orth_eff_ch_TM4_prec_real=%d,  Rayleigh8_orth_eff_ch_TM4_prec_imag=%d , TS_SHIFT=%d\n",
   SCM_A, SCM_B, SCM_C, SCM_D, EPA, EVA, ETU, Rayleigh8, Rayleigh1, Rayleigh1_corr, Rayleigh1_anticorr, Rice1, Rice8, Rayleigh1_orthogonal, Rayleigh1_orth_eff_ch_TM4_prec_real, Rayleigh1_orth_eff_ch_TM4_prec_imag, Rayleigh8_orth_eff_ch_TM4_prec_real, Rayleigh8_orth_eff_ch_TM4_prec_imag, TS_SHIFT);
  if(transmission_mode==5)
    sprintf(bler_fname,"bler_tx%d_rec%d_chan%d_nrx%d_mcs%d_mcsi%d_u%d_imod%d.csv",transmission_mode,rx_type,channel_model,n_rx,mcs1,mcs_i,rx_type,i_mod);
  else if (abstx == 1)
    if (perfect_ce==1)
   sprintf(bler_fname,"bler_tx%d_r%d_ch%d_%d_nrx%d_rnd%d_mcs%d_mcsi%d_ab_pce_sh%d_rpmi4.csv",transmission_mode,rx_type,channel_model,n_frames, n_rx, num_rounds, mcs1, mcs2,interf_unaw_shift );
    else
      sprintf(bler_fname,"bler_tx%d_r%d_ch%d_%d_nrx%d_rnd%d_mcs%d_mcsi%d_ab_sh%d_rtpmi4.csv",transmission_mode,rx_type,channel_model, n_frames, n_rx, num_rounds, mcs1, mcs2,interf_unaw_shift );
  else //abstx=0
    if (perfect_ce==1)
      sprintf(bler_fname,"bler_tx%d_r%d_ch%d_%d_nrx%d_rnd%d_mcs%d_mcsi%d_pce_sh%d_rtpmi4.csv",transmission_mode,rx_type,channel_model,n_frames, n_rx, num_rounds, mcs1, mcs2, interf_unaw_shift);
   else
    sprintf(bler_fname,"bler_tx%d_r%d_ch%d_%d_nrx%d_rnd%d_mcs%d_mcsi%d_sh%d_pnort_rtpmi4.csv",transmission_mode,rx_type,channel_model,n_frames,n_rx, num_rounds, mcs1, mcs2, interf_unaw_shift);

  bler_fd = fopen(bler_fname,"w");
  if (bler_fd==NULL) {
    fprintf(stderr,"Cannot create file %s!\n",bler_fname);
    exit(-1);
  }
  if ((transmission_mode != 3) && (transmission_mode != 4))
    fprintf(bler_fd,"SNR; MCS1; MCS2; TBS1; TBS2; rate 0; rate 1; err0_st1; err0_st2 trials0; err1_st1; err1_st2; trials1; err2_st1; err2_st2; trials2; err3_st1; err3_st2; trials3; throug 0; throug 1; sum throug; dci_err\n");
  else
    fprintf(bler_fd,"SNR; MCS1; MCS2; TBS1; TBS2; rate 0; rate 1; err0_st1; err0_st2; trials0 r0; trials1 r0; err1_st1; err1_st2; trials0_r1; trials1_r1;; err2_st1; err2_st2; trials0_r2; trials1_r2; err3_st1; err3_st2; trials0_r3; trials1_r3; th0_r0; th1_r0; th_sum_r0; th0_r1; th1_r1; th_sum_r1; th0_r2; th1_r2; th_sum_r2; th0_r3; th1_r3; th_sum_r3; tot_th\n");


  if (test_perf != 0) {

    char hostname[1024];

    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Hostname: %s\n", hostname);
    //char dirname[FILENAME_MAX];
    //sprintf(dirname, "%s/SIMU/USER/pre-ci-logs-%s", getenv("OPENAIR_TARGETS"),hostname );
    sprintf(time_meas_fname,"time_meas_prb%d_mcs%d_anttx%d_antrx%d_pdcch%d_channel%s_tx%d.csv",
            N_RB_DL,mcs1,n_tx,n_rx,num_pdcch_symbols,channel_model_input,transmission_mode);
    //mkdir(dirname,0777);
    time_meas_fd = fopen(time_meas_fname,"w");
    if (time_meas_fd==NULL) {
      fprintf(stderr,"Cannot create file %s!\n",time_meas_fname);
      exit(-1);
    }
  }

  if(abstx){
    // CSV file  // add here second stream mcs

    if (transmission_mode == 5)
      sprintf(csv_fname,"dataout_tx%d_u2%d_mcs%d_chan%d_nsimus%d_R%d_abstr.m",transmission_mode,rx_type,mcs1,channel_model,n_frames,num_rounds);

    else
      if (perfect_ce==1)
        sprintf(csv_fname,"dout_tx%d_r%d_mcs%d_mcsi%d_ch%d_ns%d_R%d_pce_sh%d_%d_pnort_r45.m",transmission_mode,rx_type,mcs1,mcs2,channel_model,n_frames,num_rounds, interf_unaw_shift, n_ch_rlz);
      else
        sprintf(csv_fname,"dout_tx%d_r%d_mcs%d_mcsi%d_ch%d_ns%d_R%d_sh%d_%d_pnort_r45.m",transmission_mode,rx_type,mcs1,mcs2,channel_model,n_frames,num_rounds, interf_unaw_shift, n_ch_rlz);

    csv_fd = fopen(csv_fname,"w");
    fprintf(csv_fd,"data_all%d=[",mcs1);
    if (csv_fd==NULL) {
      fprintf(stderr,"Cannot create file %s!\n",csv_fname);
      exit(-1);
    }
  }

  for (i=0; i<2; i++) {
    s_re[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    s_im[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    r_re[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    r_im[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    r_re0[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    bzero(r_re0[i],FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    r_im0[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    bzero(r_im0[i],FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
  }


  PHY_vars_UE->lte_ue_pdcch_vars[0]->crnti = n_rnti;
  PHY_vars_UE->transmission_mode[eNB_id]=transmission_mode;
  if (PHY_vars_UE->transmission_mode[eNB_id] !=4) // add TM3 exception
      PHY_vars_UE->PHY_measurements.rank[eNB_id]=0;
  else
      PHY_vars_UE->PHY_measurements.rank[eNB_id]=1;
  // Fill in UL_alloc
  UL_alloc_pdu.type    = 0;
  UL_alloc_pdu.hopping = 0;
  UL_alloc_pdu.rballoc = UL_RB_ALLOC;
  UL_alloc_pdu.mcs     = 1;
  UL_alloc_pdu.ndi     = 1;
  UL_alloc_pdu.TPC     = 0;
  UL_alloc_pdu.cqi_req = 1;

  CCCH_alloc_pdu.type               = 0;
  CCCH_alloc_pdu.vrb_type           = 0;
  CCCH_alloc_pdu.rballoc            = CCCH_RB_ALLOC;
  CCCH_alloc_pdu.ndi                = 1;
  CCCH_alloc_pdu.mcs                = 1;
  CCCH_alloc_pdu.harq_pid           = 0;

  DLSCH_alloc_pdu2_1E[0].rah              = 0;
  DLSCH_alloc_pdu2_1E[0].rballoc          = DLSCH_RB_ALLOC;
  DLSCH_alloc_pdu2_1E[0].TPC              = 0;
  DLSCH_alloc_pdu2_1E[0].dai              = 0;
  DLSCH_alloc_pdu2_1E[0].harq_pid         = 0;
  //DLSCH_alloc_pdu2_1E[0].tb_swap        = 0;
  DLSCH_alloc_pdu2_1E[0].mcs              = mcs1;
  DLSCH_alloc_pdu2_1E[0].ndi              = 1;
  DLSCH_alloc_pdu2_1E[0].rv               = 0;
  // Forget second codeword
  DLSCH_alloc_pdu2_1E[0].tpmi             = (transmission_mode>=5 ? 5 : 0);  // precoding
  DLSCH_alloc_pdu2_1E[0].dl_power_off     = (transmission_mode==5 ? 0 : 1);

  DLSCH_alloc_pdu2_1E[1].rah              = 0;
  DLSCH_alloc_pdu2_1E[1].rballoc          = DLSCH_RB_ALLOC;
  DLSCH_alloc_pdu2_1E[1].TPC              = 0;
  DLSCH_alloc_pdu2_1E[1].dai              = 0;
  DLSCH_alloc_pdu2_1E[1].harq_pid         = 0;
  //DLSCH_alloc_pdu2_1E[1].tb_swap          = 0;
  DLSCH_alloc_pdu2_1E[1].mcs              = mcs_i;
  DLSCH_alloc_pdu2_1E[1].ndi              = 1;
  DLSCH_alloc_pdu2_1E[1].rv               = 0;
  // Forget second codeword
  DLSCH_alloc_pdu2_1E[1].tpmi             = (transmission_mode>=5 ? 5 : 0) ;  // precoding
  DLSCH_alloc_pdu2_1E[1].dl_power_off     = (transmission_mode==5 ? 0 : 1);


    eNB2UE[0] = new_channel_desc_scm(PHY_vars_eNB->lte_frame_parms.nb_antennas_tx,
                                     PHY_vars_UE->lte_frame_parms.nb_antennas_rx,
                                     channel_model,
                                     N_RB2sampling_rate(PHY_vars_eNB->lte_frame_parms.N_RB_DL),
                                     N_RB2channel_bandwidth(PHY_vars_eNB->lte_frame_parms.N_RB_DL),
                                     forgetting_factor,
                                     rx_sample_offset,
                                     0);

    if(num_rounds>1) { // 8 is because the round counters are independent across the CW
      for(n=1; n<8; n++)
        eNB2UE[n] = new_channel_desc_scm(PHY_vars_eNB->lte_frame_parms.nb_antennas_tx,
                                         PHY_vars_UE->lte_frame_parms.nb_antennas_rx,
                                         channel_model,
                                         N_RB2sampling_rate(PHY_vars_eNB->lte_frame_parms.N_RB_DL),
                                         N_RB2channel_bandwidth(PHY_vars_eNB->lte_frame_parms.N_RB_DL),
                                         forgetting_factor,
                                         rx_sample_offset,
                                         0);
  }



  if (eNB2UE[0]==NULL) {
    msg("Problem generating channel model. Exiting.\n");
    exit(-1);
  }

  if ((transmission_mode == 3) || (transmission_mode==4) || (transmission_mode==8))
    Kmimo=2;
  else
    Kmimo=1;

  switch (ue_category) {
  case 1:
    Nsoft = 250368;
    break;
  case 2:
  case 3:
    Nsoft = 1237248;
    break;
  case 4:
    Nsoft = 1827072;
    break;
  default:
    printf("Unsupported UE category %d\n",ue_category);
    exit(-1);
    break;
  }

  for (k=0; k<n_users; k++) {
    // Create transport channel structures for 2 transport blocks (MIMO)
    for (i=0; i<2; i++) { //i is a CW
      PHY_vars_eNB->dlsch_eNB[k][i] = new_eNB_dlsch(Kmimo,8,Nsoft,N_RB_DL,0);

      if (!PHY_vars_eNB->dlsch_eNB[k][i]) {
        printf("Can't get eNB dlsch structures\n");
        exit(-1);
      }

      PHY_vars_eNB->dlsch_eNB[k][i]->rnti = n_rnti+k;
    }
  }

  for (i=0; i<2; i++) {
    PHY_vars_UE->dlsch_ue[0][i]  = new_ue_dlsch(Kmimo,8,Nsoft,MAX_TURBO_ITERATIONS,N_RB_DL,0);

    if (!PHY_vars_UE->dlsch_ue[0][i]) {
      printf("Can't get ue dlsch structures\n");
      exit(-1);
    }

    PHY_vars_UE->dlsch_ue[0][i]->rnti   = n_rnti;
  }

  // structure for SIC at UE
  PHY_vars_UE->dlsch_eNB[0] = new_eNB_dlsch(Kmimo,8,Nsoft,N_RB_DL,0);

  if (DLSCH_alloc_pdu2_1E[0].tpmi == 5) {

    PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single = (unsigned short)(taus()&0xffff);

    if (n_users>1)
      PHY_vars_eNB->eNB_UE_stats[1].DL_pmi_single = (PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single ^ 0x1555); //opposite PMI
  } else {
    PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single = 0;

    if (n_users>1)
      PHY_vars_eNB->eNB_UE_stats[1].DL_pmi_single = 0;
  }


  sic_buffer = (int32_t **) malloc16(frame_parms->nb_antennas_tx*sizeof(int32_t *) );
  for (i=0; i<frame_parms->nb_antennas_tx; i++) {
    sic_buffer[i] = malloc16_clear(FRAME_LENGTH_COMPLEX_SAMPLES_NO_PREFIX*sizeof(int32_t));
  }

  if (input_fd==NULL) {

    // UE specific DCI
    for(k=0; k<n_users; k++) {
      switch(transmission_mode) {
      case 1:
      case 2:
        if (common_flag == 0) {

          if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1_1_5MHz_TDD_t;
              dci_length_bytes = sizeof(DCI1_1_5MHz_TDD_t);
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1_5MHz_TDD_t;
              dci_length_bytes = sizeof(DCI1_5MHz_TDD_t);
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1_10MHz_TDD_t;
              dci_length_bytes = sizeof(DCI1_10MHz_TDD_t);
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              dci_length = sizeof_DCI1_20MHz_TDD_t;
              dci_length_bytes = sizeof(DCI1_20MHz_TDD_t);
              break;
            }
          }
          else { //fdd
            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_1_5MHz_FDD_t);
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_5MHz_FDD_t);
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_10MHz_FDD_t);
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              dci_length = sizeof_DCI1_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_20MHz_FDD_t);
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            }
          }
          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = n_rnti+k;
          dci_alloc[num_dci].format     = format1;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
                     subframe,
                     &DLSCH_alloc_pdu_1[0],
                     n_rnti+k,
                     format1,
                     PHY_vars_eNB->dlsch_eNB[0],
                     &PHY_vars_eNB->lte_frame_parms,
                     PHY_vars_eNB->pdsch_config_dedicated,
                     SI_RNTI,
                     0,
                     P_RNTI,
                     PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);
          num_dci++;
          num_ue_spec_dci++;
        }
        else { //common flag =1
          if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          }
          else { // fdd
            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            }
          }
          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = SI_RNTI;
          dci_alloc[num_dci].format     = format1A;
          dci_alloc[num_dci].firstCCE       = 0;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
                     subframe,
                     &DLSCH_alloc_pdu_1[0],
                     SI_RNTI,
                     format1A,
                     PHY_vars_eNB->dlsch_eNB[0],
                     &PHY_vars_eNB->lte_frame_parms,
                     PHY_vars_eNB->pdsch_config_dedicated,
                     SI_RNTI,
                     0,
                     P_RNTI,
                     PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

          num_common_dci++;
          num_dci++;

        }
        break;
      case 3: //LARGE CDD
        if (common_flag == 0) {

          if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx == 2) {// here DCI does not contain any precoder information. matrix is fixed as identity, but dont forget aboy cycling

            if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

              switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
              case 6:
          dci_length = sizeof_DCI2A_1_5MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2A_1_5MHz_2A_TDD_t);
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 25:
          dci_length = sizeof_DCI2A_5MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2A_5MHz_2A_TDD_t);
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 50:
          dci_length = sizeof_DCI2A_10MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2A_10MHz_2A_TDD_t);
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 100:
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          dci_length = sizeof_DCI2A_20MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2A_20MHz_2A_TDD_t);
          break;
              }
            }

            else { // fdd
              switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
              case 6:
          dci_length = sizeof_DCI2A_1_5MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2A_1_5MHz_2A_FDD_t);
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 25:
          dci_length = sizeof_DCI2A_5MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2A_5MHz_2A_FDD_t);
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 50:
          dci_length = sizeof_DCI2A_10MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2A_10MHz_2A_FDD_t);
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              case 100:
          dci_length = sizeof_DCI2A_20MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2A_20MHz_2A_FDD_t);
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          break;
              }
            }
          }
          else if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx == 4) { //4 antenna, needs diff precoding codebook index depeneding on layers

          }

          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = n_rnti+k;
          dci_alloc[num_dci].format     = format2A;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

          printf("Generating dlsch params for user %d / format 2A (%d)\n",k,format2A);
          generate_eNB_dlsch_params_from_dci(0,
                     subframe,
                     &DLSCH_alloc_pdu_1[0],
                     n_rnti+k,
                     format2A,
                     PHY_vars_eNB->dlsch_eNB[0],
                     &PHY_vars_eNB->lte_frame_parms,
                     PHY_vars_eNB->pdsch_config_dedicated,
                     SI_RNTI,
                     0,
                     P_RNTI,
                     PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

          num_dci++;
          num_ue_spec_dci++;
        }
        else { //commonn flag 1
          if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) { //tdd

            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          }
          else { //fdd
            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            }
          }
          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = SI_RNTI;
          dci_alloc[num_dci].format     = format1A;
          dci_alloc[num_dci].firstCCE       = 0;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
                     subframe,
                     &DLSCH_alloc_pdu_1[0],
                     SI_RNTI,
                     format1A,
                     PHY_vars_eNB->dlsch_eNB[0],
                     &PHY_vars_eNB->lte_frame_parms,
                     PHY_vars_eNB->pdsch_config_dedicated,
                     SI_RNTI,
                     0,
                     P_RNTI,
                     PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

          num_common_dci++;
          num_dci++;

        }
        printf("Generated DCI format 2A (Transmission Mode 3)\n");
        break;

      case 4: // !!! this dci format contains precoder information
        if (common_flag == 0) {

          if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx == 2) {

            if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

              switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
              case 6:
          dci_length = sizeof_DCI2_1_5MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2_1_5MHz_2A_TDD_t);
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc        = DLSCH_RB_ALLOC;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC            = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai            = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid       = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1           = mcs1;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1           = 1;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1            = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2           = mcs2;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2           = 1;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2            = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap        = 0;
          ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi           = 2;
          break;
              case 25:
          dci_length = sizeof_DCI2_5MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2_5MHz_2A_TDD_t);
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi            = 2;

          break;
              case 50:
          dci_length = sizeof_DCI2_10MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2_10MHz_2A_TDD_t);
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 2;

          break;
              case 100:
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 2;
          dci_length = sizeof_DCI2_20MHz_2A_TDD_t;
          dci_length_bytes = sizeof(DCI2_20MHz_2A_TDD_t);
          break;
              }
            }

            else {
              switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
              case 6:
          dci_length = sizeof_DCI2_1_5MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2_1_5MHz_2A_FDD_t);
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi              = 2;
          break;
              case 25:
          dci_length = sizeof_DCI2_5MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2_5MHz_2A_FDD_t);
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi            = 2;
          break;
              case 50:
          dci_length = sizeof_DCI2_10MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2_10MHz_2A_FDD_t);
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 2;
          break;
              case 100:
          dci_length = sizeof_DCI2_20MHz_2A_FDD_t;
          dci_length_bytes = sizeof(DCI2_20MHz_2A_FDD_t);
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = 1;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = 1;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tb_swap          = 0;
          ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 2;
          break;
              }
            }
          }
          else if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx == 4) {

          }

           printf ("TM4 with tpmi =%d\n", ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi);
           if ((((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi == 2) || (((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi == 2)) {

            PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single = (unsigned short)(taus()&0xffff);

           }

          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = n_rnti+k;
          dci_alloc[num_dci].format     = format2;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
                     subframe,
                     &DLSCH_alloc_pdu_1[0],
                     n_rnti+k,
                     format2,
                     PHY_vars_eNB->dlsch_eNB[0],
                     &PHY_vars_eNB->lte_frame_parms,
                     PHY_vars_eNB->pdsch_config_dedicated,
                     SI_RNTI,
                     0,
                     P_RNTI,
                     PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

          num_dci++;
          num_ue_spec_dci++;
        }
        else { //common_flag==1
          if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          }
          else {
            switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs              = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi              = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv               = 0;
              break;
            }
          }
          memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[num_dci].dci_length = dci_length;
          dci_alloc[num_dci].L          = 1;
          dci_alloc[num_dci].rnti       = SI_RNTI;
          dci_alloc[num_dci].format     = format1A;
          dci_alloc[num_dci].firstCCE       = 0;
          dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);

            printf("Generating dlsch params for user %d\n",k);
            generate_eNB_dlsch_params_from_dci(0,
                       subframe,
                       &DLSCH_alloc_pdu_1[0],
                       SI_RNTI,
                       format1A,
                       PHY_vars_eNB->dlsch_eNB[0],
                       &PHY_vars_eNB->lte_frame_parms,
                       PHY_vars_eNB->pdsch_config_dedicated,
                       SI_RNTI,
                       0,
                       P_RNTI,
                       PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

          num_common_dci++;
          num_dci++;

        }
        break;

      case 5:
      case 6:
        memcpy(&dci_alloc[num_dci].dci_pdu[0],&DLSCH_alloc_pdu2_1E[k],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
        dci_alloc[num_dci].dci_length = sizeof_DCI1E_5MHz_2A_M10PRB_TDD_t;
        dci_alloc[num_dci].L          = 1;
        dci_alloc[num_dci].rnti       = n_rnti+k;
        dci_alloc[num_dci].format     = format1E_2A_M10PRB;
        dci_alloc[num_dci].firstCCE       = 4*k;
        printf("Generating dlsch params for user %d\n",k);
        generate_eNB_dlsch_params_from_dci(0,
                                           subframe,
                                           &DLSCH_alloc_pdu2_1E[k],
                                           n_rnti+k,
                                           format1E_2A_M10PRB,
                                           PHY_vars_eNB->dlsch_eNB[k],
                                           &PHY_vars_eNB->lte_frame_parms,
                                           PHY_vars_eNB->pdsch_config_dedicated,
                                           SI_RNTI,
                                           0,
                                           P_RNTI,
                                           PHY_vars_eNB->eNB_UE_stats[k].DL_pmi_single);

        dump_dci(&PHY_vars_eNB->lte_frame_parms,&dci_alloc[num_dci]);
        num_ue_spec_dci++;
        num_dci++;

        break;

      default:
        printf("Unsupported Transmission Mode!!!");
        exit(-1);
        break;
      }




      /*
      memcpy(&dci_alloc[1].dci_pdu[0],&UL_alloc_pdu,sizeof(DCI0_5MHz_TDD0_t));
      dci_alloc[1].dci_length = sizeof_DCI0_5MHz_TDD_0_t;
      dci_alloc[1].L          = 2;
      dci_alloc[1].rnti       = n_rnti;
      */
    }

    if (n_frames==1)
      printf("num_pdcch_symbols %d, numCCE %d => ",num_pdcch_symbols,numCCE);

    numCCE = get_nCCE(num_pdcch_symbols,&PHY_vars_eNB->lte_frame_parms,get_mi(&PHY_vars_eNB->lte_frame_parms,subframe));

    if (n_frames==1)
      printf("%d\n",numCCE);

    // apply RNTI-based nCCE allocation
    memset(CCE_table,0,800*sizeof(int));

    for (i=num_common_dci; i<num_dci; i++) {

      dci_alloc[i].firstCCE = get_nCCE_offset_l1(CCE_table,
                                                 1<<dci_alloc[i].L,
                                                 numCCE,
                                                 (dci_alloc[i].rnti==SI_RNTI)? 1 : 0,
                                                 dci_alloc[i].rnti,
                                                 subframe);

      if (n_frames==1)
        printf("dci %d: rnti %x, format %d : nCCE %d/%d\n",i,dci_alloc[i].rnti, dci_alloc[i].format,
               dci_alloc[i].firstCCE,numCCE);
    }

    for (k=0; k<n_users; k++) {

      input_buffer_length0 = PHY_vars_eNB->dlsch_eNB[k][0]->harq_processes[0]->TBS/8;
      input_buffer0[k] = (unsigned char *)malloc(input_buffer_length0+4);
      memset(input_buffer0[k],0,input_buffer_length0+4);
      input_buffer_length1 = PHY_vars_eNB->dlsch_eNB[k][1]->harq_processes[0]->TBS/8;
      input_buffer1[k] = (unsigned char *)malloc(input_buffer_length1+4);
      memset(input_buffer1[k],0,input_buffer_length1+4);

      if (input_trch_file==0) {
        for (i=0; i<input_buffer_length0; i++) {
          //input_buffer0[k][i] = (unsigned char)(i&0xff);
          input_buffer0[k][i] = (unsigned char)(taus()&0xff);
        }

        for (i=0; i<input_buffer_length1; i++) {
          input_buffer1[k][i]= (unsigned char)(taus()&0xff);
        }
      }

      else {
        i=0;

        while ((!feof(input_trch_fd)) && (i<input_buffer_length0<<3)) {
          ret[0]=fscanf(input_trch_fd,"%s",input_trch_val);

          if (input_trch_val[0] == '1')
            input_buffer0[k][i>>3]+=(1<<(7-(i&7)));

          if (i<16)
            printf("input_trch_val %d : %c\n",i,input_trch_val[0]);

          i++;

          if (((i%8) == 0) && (i<17))
            printf("%x\n",input_buffer0[k][(i-1)>>3]);
        }

        printf("Read in %d bits\n",i);
      }
    }
  }

  snr_step = input_snr_step;
  PHY_vars_UE->high_speed_flag = 1;
  PHY_vars_UE->ch_est_alpha=0;

  for (ch_realization=0; ch_realization<n_ch_rlz; ch_realization++) {
    if(abstx) {
      printf("**********************Channel Realization Index = %d **************************\n", ch_realization);
    }

    for (SNR=snr0; SNR<snr1; SNR+=snr_step) {
      PHY_vars_UE->frame_rx=0;
      for (i=0; i<4; i++) {
        errs[0][i]=0; //CW_0
        errs[1][i]=0; //CW_1

        round_trials[0][i] = 0;  // CW_0
        round_trials[1][i] = 0;  // CW_1
      }
      dci_errors=0;

      round = 0;
      avg_iter[0] = 0;
      avg_iter[1] = 0;
      iter_trials[0]=0;
      iter_trials[1]=0;
      unsigned int resend_one[4]={0,0,0,0}, resend_both[4]={0,0,0,0};
      reset_meas(&PHY_vars_eNB->phy_proc_tx); // total eNB tx
      reset_meas(&PHY_vars_eNB->dlsch_scrambling_stats);
      reset_meas(&PHY_vars_UE->dlsch_unscrambling_stats);
      reset_meas(&PHY_vars_eNB->ofdm_mod_stats);
      reset_meas(&PHY_vars_eNB->dlsch_modulation_stats);
      reset_meas(&PHY_vars_eNB->dlsch_encoding_stats);
      reset_meas(&PHY_vars_eNB->dlsch_interleaving_stats);
      reset_meas(&PHY_vars_eNB->dlsch_rate_matching_stats);
      reset_meas(&PHY_vars_eNB->dlsch_turbo_encoding_stats);

      reset_meas(&PHY_vars_UE->phy_proc_rx); // total UE rx
      reset_meas(&PHY_vars_UE->ofdm_demod_stats);
      reset_meas(&PHY_vars_UE->dlsch_channel_estimation_stats);
      reset_meas(&PHY_vars_UE->dlsch_freq_offset_estimation_stats);
      reset_meas(&PHY_vars_UE->rx_dft_stats);
      reset_meas(&PHY_vars_UE->dlsch_decoding_stats);
      reset_meas(&PHY_vars_UE->dlsch_turbo_decoding_stats);
      reset_meas(&PHY_vars_UE->dlsch_deinterleaving_stats);
      reset_meas(&PHY_vars_UE->dlsch_rate_unmatching_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_init_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_alpha_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_beta_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_gamma_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_ext_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_intl1_stats);
      reset_meas(&PHY_vars_UE->dlsch_tc_intl2_stats);
      // initialization
      struct list time_vector_tx;
      initialize(&time_vector_tx);
      struct list time_vector_tx_ifft;
      initialize(&time_vector_tx_ifft);
      struct list time_vector_tx_mod;
      initialize(&time_vector_tx_mod);
      struct list time_vector_tx_enc;
      initialize(&time_vector_tx_enc);

      struct list time_vector_rx;
      initialize(&time_vector_rx);
      struct list time_vector_rx_fft;
      initialize(&time_vector_rx_fft);
      struct list time_vector_rx_demod;
      initialize(&time_vector_rx_demod);
      struct list time_vector_rx_dec;
      initialize(&time_vector_rx_dec);

      for (trials = 0;trials<n_frames;trials++) {
      //printf("Trial %d\n",trials);
        fflush(stdout);
        round = 0;
#ifdef DEBUG_HARQ
        printf("[DLSIM] TRIAL %d\n", trials);
#endif

        for (i=0; i<frame_parms->nb_antennas_tx; i++) {
          memset(sic_buffer[i], 0, FRAME_LENGTH_COMPLEX_SAMPLES_NO_PREFIX*sizeof(int32_t));
        }
         //PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
        //memset(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[0], 0, (8*((3*8*6144)+12))*sizeof(int16_t));
        //memset(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[1], 0, (8*((3*8*6144)+12))*sizeof(int16_t));
        //if (trials%100==0)
        eNB2UE[0]->first_run = 1;

        ret[0] = PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations+1;
        ret[1] = PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations+1;

        resend_cw0_cw1=1;
        resend_cw1=0;
        TB0_active=1;

        while (((transmission_mode == 3 || transmission_mode == 4) &&
               ((round < num_rounds) && ((ret[0] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations) ||
               (ret[1] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations)))) ||
               ((transmission_mode!=4 && transmission_mode != 3) && ((round< num_rounds) &&
               (ret[0] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations)))) {
#ifdef DEBUG_HARQ
        printf("\n [DLSIM] On top round is %d\n", round);
#endif


          round_trials[0][round]++;
          round_trials[1][round]++;

          //printf("Trial %d, round %d , ret[0] %d, ret[1] %d, round_trials %d\n",trials,round, ret[0], ret[1], round_trials[round]);

        /*if (ret[0] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations) {
          round_trials[0][round]++;
          round_trials[1][round]++;
        } else if ((ret[1] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations) && (ret[0] <= PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations))
          round_trials[1][round]++;*/


#ifdef DEBUG_HARQ
        printf("[DLSIM] Just after while loop TB0 status %d round %d\n", TB0_active, round);
#endif

        // printf("Trial %d, round %d , ret[0] %d, ret[1] %d, round_trials TB0 = %d, round_trials TB1 = %d \n",trials,round, ret[0], ret[1], round_trials[0][round], round_trials[1][round]);


        /*printf("Trial %d, round cw0 =  %d , round cw1 = %d, ret[0] = %d, ret[1] = %d, round_trials cw0 [%d]= %d, round_trials cw1 [%d]= %d\n",trials, round, round, \
          ret[0], ret[1], round, round_trials[0][round], round, round_trials[1][round]);*/

          //printf("round_trials %d round %d\n", round_trials[round], round);

          if (transmission_mode == 4 || transmission_mode == 5 || transmission_mode == 6)
            pmi_feedback=1;
          else
            pmi_feedback=0;

          if (abstx) {
            if (trials==0 && round==0 && SNR==snr0)  //generate a new channel
              hold_channel = 0;
            else
              hold_channel = 1;
          }
          else
            hold_channel = 0;//(round==0) ? 0 : 1;

        PMI_FEEDBACK:

          //printf("Trial %d : Round %d, pmi_feedback %d \n",trials,round,pmi_feedback);
          for (aa=0; aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_tx;aa++) {
            memset(&PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id][aa][0],0,FRAME_LENGTH_COMPLEX_SAMPLES_NO_PREFIX*sizeof(int32_t));
          }

          if (input_fd==NULL) {

            start_meas(&PHY_vars_eNB->phy_proc_tx);

            // Simulate HARQ procedures!!!
            if (common_flag == 0) {


              PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->rvidx = round&3;
              if (transmission_mode == 3 || transmission_mode == 4)
                  PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->rvidx = round&3;

                //printf("TB0 status %d round %d \n ", TB0_active, round);

              if (round == 0) {   // First round
                TB0_active = 1;

             /* PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->rvidx = round&3;
              if (transmission_mode == 3 || transmission_mode == 4)
                  PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->rvidx = round&3;*/



                if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {

                  switch (transmission_mode) {
                  case 1:
                  case 2:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_1_5MHz_TDD_t));
                      break;
                    case 25:
                      ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_5MHz_TDD_t));
                      break;
                    case 50:
                      ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_10MHz_TDD_t));
                      break;
                    case 100:
                      ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_20MHz_TDD_t));
                      break;
                    }
                    break;
                  case 3:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_1_5MHz_2A_TDD_t));
                      break;
                    case 25:
                      ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_5MHz_2A_TDD_t));
                      break;
                    case 50:
                      ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_10MHz_2A_TDD_t));
                      break;
                    case 100:
                      ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_20MHz_2A_TDD_t));
                      break;
                    }
                    break;
                  case 4:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                     case 6:
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_1_5MHz_2A_TDD_t));
                      break;
                    case 25:
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_5MHz_2A_TDD_t));
                      break;
                    case 50:
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_10MHz_2A_TDD_t));
                      break;
                    case 100:
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_20MHz_2A_TDD_t));
                      break;
                    }
                    break;

                  case 5:
                  case 6:
                    DLSCH_alloc_pdu2_1E[0].ndi             = trials&1;
                    DLSCH_alloc_pdu2_1E[0].rv              = 0;
                    memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu2_1E[0],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
                    break;
                  }
                }
                else { // FDD
                  switch (transmission_mode) {
                    case 1:
                    case 2:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_1_5MHz_FDD_t));
                      break;
                    case 25:
                      ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_5MHz_FDD_t));
                      break;
                    case 50:
                      ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_10MHz_FDD_t));
                      break;
                    case 100:
                      ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_20MHz_FDD_t));
                      break;
                    }
                    break;
                  case 3:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_1_5MHz_2A_FDD_t));
                      break;
                    case 25:
                      ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_5MHz_2A_FDD_t));
                      break;
                    case 50:
                      ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_10MHz_2A_FDD_t));
                      break;
                    case 100:
                      ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_20MHz_2A_FDD_t));
                      break;
                    }
                    break;
                  case 4:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                     case 6:
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_1_5MHz_2A_FDD_t));
                      break;
                    case 25:
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_5MHz_2A_FDD_t));
                      break;
                    case 50:
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_10MHz_2A_FDD_t));
                      break;
                    case 100:
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 0;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                      ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = 0;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_20MHz_2A_FDD_t));
                      break;
                    }
                    break;
                  case 5:
                  case 6:
                    DLSCH_alloc_pdu2_1E[0].ndi             = trials&1;
                    DLSCH_alloc_pdu2_1E[0].rv              = 0;
                    memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu2_1E[0],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
                    break;
                  }

                }
              }
              else {

                if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {
                  switch (transmission_mode) {
                  case 1:
                  case 2:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_1_5MHz_TDD_t));
                      break;
                    case 25:
                      ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_5MHz_TDD_t));
                      break;
                    case 50:
                      ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_10MHz_TDD_t));
                      break;
                    case 100:
                      ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_20MHz_TDD_t));
                      break;
                    }
                    break;
                  case 3:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      if (TB0_active==1) {
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      else {  // deactivate TB0
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_1_5MHz_2A_TDD_t));
                      break;
                    case 25:
                      if (TB0_active==1) {
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      else {  // deactivate TB0
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_5MHz_2A_TDD_t));
                      break;
                    case 50:
                      if (TB0_active==1) {
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      else {  // deactivate TB0
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_10MHz_2A_TDD_t));
                      break;
                    case 100:
                      if (TB0_active==1) {
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_20MHz_2A_TDD_t));
                      break;
                    }
                    break;
                  case 4:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      if (TB0_active==1) {
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 2;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi             = 4;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                        }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_1_5MHz_2A_TDD_t));
                      break;
                    case 25:
                      if (TB0_active==1) {
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
#ifdef DEBUG_HARQ
                        printf("\n[DLSIM] Requesting only TB1 from temp DCI\n");
#endif
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_5MHz_2A_TDD_t));
                      generate_eNB_dlsch_params_from_dci(0,
                                                         subframe,
                                                         &DLSCH_alloc_pdu_1[0],
                                                         n_rnti+k,
                                                         format2,
                                                         PHY_vars_eNB->dlsch_eNB[0],
                                                         &PHY_vars_eNB->lte_frame_parms,
                                                         PHY_vars_eNB->pdsch_config_dedicated,
                                                         SI_RNTI,
                                                         0,
                                                         P_RNTI,
                                                         PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);

                      break;
                    case 50:
                      if (TB0_active==1) {
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_10MHz_2A_TDD_t));
                      break;
                    case 100:
                      if (TB0_active==1) {
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_20MHz_2A_TDD_t));
                      break;
                    }
                    break;

                  case 5:
                  case 6:
                    DLSCH_alloc_pdu2_1E[0].ndi             = trials&1;
                    DLSCH_alloc_pdu2_1E[0].rv              = round&3;
                    memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu2_1E[0],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
                    break;
                  }
                }
                else {
                  switch (transmission_mode) {
                  case 1:
                  case 2:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_1_5MHz_FDD_t));
                      break;
                    case 25:
                      ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_5MHz_FDD_t));
                      break;
                    case 50:
                      ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_10MHz_FDD_t));
                      break;
                    case 100:
                      ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi             = trials&1;
                      ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv              = round&3;
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI1_20MHz_FDD_t));
                      break;
                    }
                    break;
                  case 3:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      if (TB0_active==1) {
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_1_5MHz_2A_FDD_t));
                      break;
                    case 25:
                      if (TB0_active==1) {
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_5MHz_2A_FDD_t));
                      break;
                    case 50:
                      if (TB0_active==1) {
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_10MHz_2A_FDD_t));
                      break;
                    case 100:
                      if (TB0_active==1) {
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2A_20MHz_2A_FDD_t));
                      break;
                    }
                    break;
                  case 4:
                    switch (PHY_vars_eNB->lte_frame_parms.N_RB_DL) {
                    case 6:
                      if (TB0_active==1) {
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_1_5MHz_2A_FDD_t));
                      break;
                    case 25:
                      if (TB0_active==1) {
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
#ifdef DEBUG_HARQ
                        printf("\n [DLSIM] Requesting only TB1 from temp DCI\n");
#endif
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_5MHz_2A_FDD_t));
                      generate_eNB_dlsch_params_from_dci(0,
                                                         subframe,
                                                         &DLSCH_alloc_pdu_1[0],
                                                         n_rnti+k,
                                                         format2,
                                                         PHY_vars_eNB->dlsch_eNB[0],
                                                         &PHY_vars_eNB->lte_frame_parms,
                                                         PHY_vars_eNB->pdsch_config_dedicated,
                                                         SI_RNTI,
                                                         0,
                                                         P_RNTI,
                                                         PHY_vars_eNB->eNB_UE_stats[0].DL_pmi_single);
                      break;
                    case 50:
                      if (TB0_active==1) {
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_10MHz_2A_FDD_t));
                      break;
                    case 100:
                      if (TB0_active==1) {
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = mcs1;
                        ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[0])->mcs2             = mcs2;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 2;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi1             = trials&1;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = round&3;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      else {  // deactivate TB0
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->tpmi             = 4;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->mcs1             = 0;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv1              = 1;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->ndi2             = trials&1;
                        ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[0])->rv2              = round&3;
                      }
                      memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu_1[0],sizeof(DCI2_20MHz_2A_FDD_t));
                      break;
                    }
                    break;


                  case 5:
                  case 6:
                    DLSCH_alloc_pdu2_1E[0].ndi             = trials&1;
                    DLSCH_alloc_pdu2_1E[0].rv              = round&3;
                    memcpy(&dci_alloc[0].dci_pdu[0],&DLSCH_alloc_pdu2_1E[0],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
                    break;
                  }
                }
              }
            }
            num_pdcch_symbols_2 = generate_dci_top(num_ue_spec_dci,
                                                   num_common_dci,
                                                   dci_alloc,
                                                   0,
                                                   AMP,
                                                   &PHY_vars_eNB->lte_frame_parms,
                                                   PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                                                   subframe);

            if (num_pdcch_symbols_2 > num_pdcch_symbols) {
              msg("Error: given num_pdcch_symbols not big enough (%d > %d)\n",num_pdcch_symbols_2,num_pdcch_symbols);
              exit(-1);
            }

            for (k=0;k<n_users;k++) {
              for (int TB=0; TB<Kmimo; TB++) {
                if (TB0_active == 0){
#ifdef DEBUG_HARQ
                  printf("[DLSIM ENC] Skip TB0 \n");
#endif
                  TB++;

                }
#ifdef DEBUG_HARQ
            printf("[DLSIM ENC] process TB %d \n", TB);
#endif

                coded_bits_per_codeword[TB] = get_G(&PHY_vars_eNB->lte_frame_parms,
                                                PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->nb_rb,
                                                PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->rb_alloc,
                                                get_Qm(PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->mcs),
                                                PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->Nl,
                                                num_pdcch_symbols,
                                                0,subframe);
      #ifdef TBS_FIX   // This is for MESH operation!!!
                tbs[TB] = (double)3*TBStable[get_I_TBS(PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->mcs)][PHY_vars_eNB->dlsch_eNB[k][TB]->nb_rb-1]/4;
      #else
                tbs[TB] = PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->TBS;
      #endif

                rate[TB] = (double)tbs[TB]/(double)coded_bits_per_codeword[TB];

                if ((SNR == snr0) && (trials == 0) && (round == 0) && (pmi_feedback == 0))
                  printf("User %d, TB %d: Rate = %f (%f bits/dim) (G %d, TBS %d, mod %d, pdcch_sym %d, ndi %d)\n",
                         k,TB,rate[TB],rate[TB]*get_Qm(PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->mcs),
                         coded_bits_per_codeword[TB],
                         tbs[TB],
                         get_Qm(PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->mcs),
                         num_pdcch_symbols,
                         PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->round);

              // use the PMI from previous trial
                if (DLSCH_alloc_pdu2_1E[0].tpmi == 5) {
                  PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->pmi_alloc = quantize_subband_pmi(&PHY_vars_UE->PHY_measurements,0,PHY_vars_eNB->lte_frame_parms.N_RB_DL);
                  PHY_vars_UE->dlsch_ue[0][0]->pmi_alloc = quantize_subband_pmi(&PHY_vars_UE->PHY_measurements,0,PHY_vars_UE->lte_frame_parms.N_RB_DL);
                  if (n_users>1)
                    PHY_vars_eNB->dlsch_eNB[1][0]->harq_processes[0]->pmi_alloc = (PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->pmi_alloc ^ 0x1555);
                  /*
                    if ((trials<10) && (round==0)) {
                    printf("tx PMI UE0 %x (pmi_feedback %d)\n",pmi2hex_2Ar1(PHY_vars_eNB->dlsch_eNB[0][0]->pmi_alloc),pmi_feedback);
                    if (transmission_mode ==5)
                    printf("tx PMI UE1 %x\n",pmi2hex_2Ar1(PHY_vars_eNB->dlsch_eNB[1][0]->pmi_alloc));
                    }
          */
                }

                //Here need to add cases for TPMI 6 and TPMI 5 and check for both TB.
                if (transmission_mode == 4 && ((((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi == 2) ||(((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->tpmi == 2))){
                  PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->pmi_alloc = quantize_subband_pmi(&PHY_vars_UE->PHY_measurements,0,PHY_vars_eNB->lte_frame_parms.N_RB_DL);
                  PHY_vars_UE->dlsch_ue[0][TB]->pmi_alloc = quantize_subband_pmi(&PHY_vars_UE->PHY_measurements,0,PHY_vars_UE->lte_frame_parms.N_RB_DL);
                }




                start_meas(&PHY_vars_eNB->dlsch_encoding_stats);
                if (dlsch_encoding(((TB==0) ? input_buffer0[k] : input_buffer1[k]),
                                   &PHY_vars_eNB->lte_frame_parms,
                                   num_pdcch_symbols,
                                   PHY_vars_eNB->dlsch_eNB[k][TB],
                                   0,subframe,
                                   &PHY_vars_eNB->dlsch_rate_matching_stats,
                                   &PHY_vars_eNB->dlsch_turbo_encoding_stats,
                                   &PHY_vars_eNB->dlsch_interleaving_stats)<0)
                  exit(-1);

                stop_meas(&PHY_vars_eNB->dlsch_encoding_stats);

                PHY_vars_eNB->dlsch_eNB[k][TB]->rnti = (common_flag==0) ? n_rnti+k : SI_RNTI;
                start_meas(&PHY_vars_eNB->dlsch_scrambling_stats);
                dlsch_scrambling(&PHY_vars_eNB->lte_frame_parms,
                                 0,
                                 PHY_vars_eNB->dlsch_eNB[k][TB],
                                 coded_bits_per_codeword[TB],
                                 TB,
                                 subframe<<1);
                stop_meas(&PHY_vars_eNB->dlsch_scrambling_stats);

                if (n_frames==2) {
                  for (s=0;s<PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->C;s++) {
                    if (s<PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->Cminus)
                      Kr = PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->Kminus;
                    else
                      Kr = PHY_vars_eNB->dlsch_eNB[k][TB]->harq_processes[0]->Kplus;
                    Kr_bytes = Kr>>3;
                  }
                }
              }

              start_meas(&PHY_vars_eNB->dlsch_modulation_stats);
              re_allocated = dlsch_modulation(PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                                              AMP,
                                              subframe,
                                              &PHY_vars_eNB->lte_frame_parms,
                                              num_pdcch_symbols,
                                              ((TB0_active == 1)? PHY_vars_eNB->dlsch_eNB[k][0]: NULL),
                                              PHY_vars_eNB->dlsch_eNB[k][1]);
              stop_meas(&PHY_vars_eNB->dlsch_modulation_stats);
            } //n_users


                if (((transmission_mode == 3) || (transmission_mode == 4)) && (SNR == snr0) && (trials == 0) && (round == 0)){
                  rate0_init = rate[0];
                  rate1_init = rate[1];
                  tbs0_init=tbs[0];
                  tbs1_init=tbs[1];
                  mod_order0_init=get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs);
                  mod_order1_init=get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs);
                  mcs0_init=PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs;
                  mcs1_init=PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs;
                }

            generate_pilots(PHY_vars_eNB,
                            PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                            AMP,
                            LTE_NUMBER_OF_SUBFRAMES_PER_FRAME);

            start_meas(&PHY_vars_eNB->ofdm_mod_stats);

            do_OFDM_mod_l(PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                          PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id],
                          (subframe*2),
                          &PHY_vars_eNB->lte_frame_parms);

            do_OFDM_mod_l(PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                          PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id],
                          (subframe*2)+1,
                          &PHY_vars_eNB->lte_frame_parms);

            stop_meas(&PHY_vars_eNB->ofdm_mod_stats);
            stop_meas(&PHY_vars_eNB->phy_proc_tx);

            do_OFDM_mod_l(PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id],
                          PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id],
                          (subframe*2)+2,
                          &PHY_vars_eNB->lte_frame_parms);

            if (n_frames==2) {
              write_output("txsigF0.m","txsF0", &PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id][0][subframe*nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size],nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size,1,1);
              if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx>1)
                write_output("txsigF1.m","txsF1", &PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id][1][subframe*nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size],nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size,1,1);
            }

            tx_lev = 0;
            for (aa=0; aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_tx; aa++) {
              tx_lev += signal_energy(&PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]
                                      [subframe*PHY_vars_eNB->lte_frame_parms.samples_per_tti],
                                      PHY_vars_eNB->lte_frame_parms.samples_per_tti);
            }
            tx_lev_dB = (unsigned int) dB_fixed(tx_lev);

            if (n_frames==2) {
              write_output("txsigF0.m","txsF0", &PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id][0][subframe*nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size],nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size,1,1);
              if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx>1)
                write_output("txsigF1.m","txsF1", &PHY_vars_eNB->lte_eNB_common_vars.txdataF[eNB_id][1][subframe*nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size],nsymb*PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size,1,1);
            }

            tx_lev = 0;
            for (aa=0; aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_tx; aa++) {
              tx_lev += signal_energy(&PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]
                                      [subframe*PHY_vars_eNB->lte_frame_parms.samples_per_tti],
                                      PHY_vars_eNB->lte_frame_parms.samples_per_tti);
            }
            tx_lev_dB = (unsigned int) dB_fixed(tx_lev);

            if (n_frames==2) {
              printf("tx_lev = %d (%d dB)\n",tx_lev,tx_lev_dB);
              write_output("txsig0.m","txs0", &PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][0][subframe* PHY_vars_eNB->lte_frame_parms.samples_per_tti], PHY_vars_eNB->lte_frame_parms.samples_per_tti,1,1);
            }
          }
          /*
            else {  // Read signal from file
            i=0;
            while (!feof(input_fd)) {
            fscanf(input_fd,"%s %s",input_val_str,input_val_str2);

            if ((i%4)==0) {
            ((short*)txdata[0])[i/2] = (short)((1<<15)*strtod(input_val_str,NULL));
            ((short*)txdata[0])[(i/2)+1] = (short)((1<<15)*strtod(input_val_str2,NULL));
            if ((i/4)<100)
            printf("sample %d => %e + j%e (%d +j%d)\n",i/4,strtod(input_val_str,NULL),strtod(input_val_str2,NULL),((short*)txdata[0])[i/4],((short*)txdata[0])[(i/4)+1]);//1,input_val2,);
            }
            i++;
            if (i>(FRAME_LENGTH_SAMPLES))
            break;
            }
            printf("Read in %d samples\n",i/4);
            write_output("txsig0.m","txs0", txdata[0],2*frame_parms->samples_per_tti,1,1);
            //    write_output("txsig1.m","txs1", txdata[1],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
            tx_lev = signal_energy(&txdata[0][0],
            OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES);
            tx_lev_dB = (unsigned int) dB_fixed(tx_lev);
            }
          */

          //    printf("Copying tx ..., nsymb %d (n_tx %d), awgn %d\n",nsymb,PHY_vars_eNB->lte_frame_parms.nb_antennas_tx,awgn_flag);
          for (i=0;i<2*frame_parms->samples_per_tti;i++) {
            for (aa=0;aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_tx;aa++) {
              if (awgn_flag == 0) {
                s_re[aa][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) + (i<<1)]);
                s_im[aa][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1]);
              }
              else {
                if (transmission_mode==4) {
                  r_re[0][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][0]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)])+((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][1]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)]);
                  r_im[0][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][0]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1])+((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][1]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1]);

                  r_re[1][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][0]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)])-((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][1]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)]);
                  r_im[1][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][0]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1])-((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][1]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1]);
            //printf("r_re0 = %d\n",r_re[0][i]);
            //printf("r_im0 = %d\n",r_im[0][i]);
            //printf("r_re1 = %d\n",r_re[1][i]);
            //printf("r_im1 = %d\n",r_im[1][i]);

                }
                else {
                  for (aarx=0;aarx<PHY_vars_UE->lte_frame_parms.nb_antennas_rx;aarx++) {
                    if (aa==0) {
                      r_re[aarx][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)]);
                      r_im[aarx][i] = ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1]);
                    }
                    else {
                      r_re[aarx][i] += ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)]);
                      r_im[aarx][i] += ((double)(((short *)PHY_vars_eNB->lte_eNB_common_vars.txdata[eNB_id][aa]))[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti) +(i<<1)+1]);
                    }
                  }
                }
              }
            }
          }

          // Multipath channel
          if (awgn_flag == 0) {
            multipath_channel(eNB2UE[round],s_re,s_im,r_re,r_im,
                              2*frame_parms->samples_per_tti,hold_channel);
            //      printf("amc: ****************** eNB2UE[%d]->n_rx = %d,dd %d\n",round,eNB2UE[round]->nb_rx,eNB2UE[round]->channel_offset);
            if(abstx==1 && num_rounds>1)
              if(round==0 && hold_channel==0){
                random_channel(eNB2UE[1],0);
                random_channel(eNB2UE[2],0);
                random_channel(eNB2UE[3],0);
              }
            if (PHY_vars_UE->perfect_ce==1){
              freq_channel(eNB2UE[round],PHY_vars_UE->lte_frame_parms.N_RB_DL,12*PHY_vars_UE->lte_frame_parms.N_RB_DL + 1);
                      //  write_output("channel.m","ch",eNB2UE[round]->ch[0],eNB2UE[round]->channel_length,1,8);
                      //  write_output("channelF.m","chF",eNB2UE[round]->chF[0],12*PHY_vars_UE->lte_frame_parms.N_RB_DL +1,1,8);
            }
          }

        if(abstx){ // TODO: check the rounds here!!
          if (trials==0 && round==0) {
            // calculate freq domain representation to compute SINR
            freq_channel(eNB2UE[0], NB_RB,2*NB_RB + 1);
            // snr=pow(10.0,.1*SNR);
            fprintf(csv_fd,"%f,",SNR);

            for (u=0;u<2*NB_RB;u++){
              for (aarx=0;aarx<eNB2UE[0]->nb_rx;aarx++) {
                for (aatx=0;aatx<eNB2UE[0]->nb_tx;aatx++) {
                  channelx = eNB2UE[0]->chF[aarx+(aatx*eNB2UE[0]->nb_rx)][u].x;
                  channely = eNB2UE[0]->chF[aarx+(aatx*eNB2UE[0]->nb_rx)][u].y;
                  fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
                }
              }
            }

            if(num_rounds>1){
              freq_channel(eNB2UE[1], NB_RB,2*NB_RB + 1);

              for (u=0;u<2*NB_RB;u++){
                for (aarx=0;aarx<eNB2UE[1]->nb_rx;aarx++) {
                  for (aatx=0;aatx<eNB2UE[1]->nb_tx;aatx++) {
                    channelx = eNB2UE[1]->chF[aarx+(aatx*eNB2UE[1]->nb_rx)][u].x;
                    channely = eNB2UE[1]->chF[aarx+(aatx*eNB2UE[1]->nb_rx)][u].y;
                    fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
                  }
                }
              }
              freq_channel(eNB2UE[2], NB_RB,2*NB_RB + 1);

              for (u=0;u<2*NB_RB;u++){
                for (aarx=0;aarx<eNB2UE[2]->nb_rx;aarx++) {
                  for (aatx=0;aatx<eNB2UE[2]->nb_tx;aatx++) {
                    channelx = eNB2UE[2]->chF[aarx+(aatx*eNB2UE[2]->nb_rx)][u].x;
                    channely = eNB2UE[2]->chF[aarx+(aatx*eNB2UE[2]->nb_rx)][u].y;
                    fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
                  }
                }
              }

              freq_channel(eNB2UE[3], NB_RB,2*NB_RB + 1);

              for (u=0;u<2*NB_RB;u++){
                for (aarx=0;aarx<eNB2UE[3]->nb_rx;aarx++) {
                  for (aatx=0;aatx<eNB2UE[3]->nb_tx;aatx++) {
                    channelx = eNB2UE[3]->chF[aarx+(aatx*eNB2UE[3]->nb_rx)][u].x;
                    channely = eNB2UE[3]->chF[aarx+(aatx*eNB2UE[3]->nb_rx)][u].y;
                    fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
                  }
                }
              }
            }
          }
        }

        //AWGN
        // This is the SNR on the PDSCH for OFDM symbols without pilots -> rho_A
        sigma2_dB = 10*log10((double)tx_lev) +10*log10((double)PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size/(double)(NB_RB*12)) - SNR - get_pa_dB(PHY_vars_eNB->pdsch_config_dedicated);
        sigma2 = pow(10,sigma2_dB/10);
        if (n_frames==2)
          printf("Sigma2 %f (sigma2_dB %f,%f,%f )\n",sigma2,sigma2_dB,10*log10((double)PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size/(double)(NB_RB*12)),get_pa_dB(PHY_vars_eNB->pdsch_config_dedicated));

        for (i=0; i<2*frame_parms->samples_per_tti; i++) {
          for (aa=0;aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_rx;aa++) {
            //printf("s_re[0][%d]=> %f , r_re[0][%d]=> %f\n",i,s_re[aa][i],i,r_re[aa][i]);
            ((short*) PHY_vars_UE->lte_ue_common_vars.rxdata[aa])[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti)+2*i] =
              (short) (r_re[aa][i] + sqrt(sigma2/2)*gaussdouble(0.0,1.0));
            ((short*) PHY_vars_UE->lte_ue_common_vars.rxdata[aa])[(2*subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti)+2*i+1] =
              (short) (r_im[aa][i] + (iqim*r_re[aa][i]) + sqrt(sigma2/2)*gaussdouble(0.0,1.0));
          }
        }

        //    lte_sync_time_init(PHY_vars_eNB->lte_frame_parms,lte_ue_common_vars);
        //    lte_sync_time(lte_ue_common_vars->rxdata, PHY_vars_eNB->lte_frame_parms);
        //    lte_sync_time_free();

        /*
          // optional: read rx_frame from file
          if ((rx_frame_file = fopen("rx_frame.dat","r")) == NULL)
          {
          printf("Cannot open rx_frame.m data file\n");
          exit(0);
          }

          result = fread((void *)PHY_vars->rx_vars[0].RX_DMA_BUFFER,4,FRAME_LENGTH_COMPLEX_SAMPLES,rx_frame_file);
          printf("Read %d bytes\n",result);
          result = fread((void *)PHY_vars->rx_vars[1].RX_DMA_BUFFER,4,FRAME_LENGTH_COMPLEX_SAMPLES,rx_frame_file);
          printf("Read %d bytes\n",result);

          fclose(rx_frame_file);
        */

        if (n_frames==2) {
          printf("RX level in null symbol %d\n",dB_fixed(signal_energy(&PHY_vars_UE->lte_ue_common_vars.rxdata[0][160+OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES],OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2)));
          printf("RX level in data symbol %d\n",dB_fixed(signal_energy(&PHY_vars_UE->lte_ue_common_vars.rxdata[0][160+(2*OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES)],OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2)));
          printf("rx_level Null symbol %f\n",10*log10(signal_energy_fp(r_re,r_im,1,OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2,256+(OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES))));
          printf("rx_level data symbol %f\n",10*log10(signal_energy_fp(r_re,r_im,1,OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2,256+(2*OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES))));
        }

        if (PHY_vars_eNB->lte_frame_parms.Ncp == 0) {  // normal prefix
          pilot1 = 4;
          pilot2 = 7;
          pilot3 = 11;
        }
        else {  // extended prefix
          pilot1 = 3;
          pilot2 = 6;
          pilot3 = 9;
        }

        start_meas(&PHY_vars_UE->phy_proc_rx);
        // Inner receiver scheduling for 3 slots
          for (Ns=(2*subframe);Ns<((2*subframe)+3);Ns++) {
            for (l=0;l<pilot2;l++) {
              if (n_frames==2)
          printf("Ns %d, l %d, l2 %d\n",Ns, l, l+(Ns%2)*pilot2);
              /*
          This function implements the OFDM front end processor (FEP).

          Parameters:
          frame_parms   LTE DL Frame Parameters
          ue_common_vars   LTE UE Common Vars
          l   symbol within slot (0..6/7)
          Ns   Slot number (0..19)
          sample_offset   offset within rxdata (points to beginning of subframe)
          no_prefix   if 1 prefix is removed by HW

              */

              start_meas(&PHY_vars_UE->ofdm_demod_stats);
              slot_fep(PHY_vars_UE,
                       l,
                       Ns%20,
                       0,
                       0,
                       0);
              stop_meas(&PHY_vars_UE->ofdm_demod_stats);

              if (PHY_vars_UE->perfect_ce==1) {
                if (awgn_flag==0) {
                  for(k=0; k<NUMBER_OF_eNB_MAX; k++) {
                    for(aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
                      for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
                        for (i=0; i<frame_parms->N_RB_DL*12; i++) {
                          ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[k][(aa<<1)+aarx])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=
                          (int16_t)(eNB2UE[round]->chF[aarx+(aa*frame_parms->nb_antennas_rx)][i].x*AMP);
                          ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[k][(aa<<1)+aarx])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=
                          (int16_t)(eNB2UE[round]->chF[aarx+(aa*frame_parms->nb_antennas_rx)][i].y*AMP);
                        }
                      }
                    }
                  }
              }else {
                if (transmission_mode==4) {
                  for (i=0; i<frame_parms->N_RB_DL*12; i++) {
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][0])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=(short)(AMP);
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][0])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=0;
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][1])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=(short)(AMP);
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][1])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=0;
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][2])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=(short)(AMP);
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][2])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=0;
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][3])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=-(short)(AMP);
                    ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][3])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=0;
                  }
                } else {
                    for(aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
                      for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {
                        for (i=0; i<frame_parms->N_RB_DL*12; i++) {
                          ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][(aa<<1)+aarx])[2*i+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=(short)(AMP);
                          ((int16_t *) PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[0][(aa<<1)+aarx])[2*i+1+((l+(Ns%2)*pilot2)*frame_parms->ofdm_symbol_size+LTE_CE_FILTER_LENGTH)*2]=0/2;
                        }
                       }
                    }
                }
              }
              }

              if ((Ns==((2*subframe))) && (l==0)) {
                lte_ue_measurements(PHY_vars_UE,
                                    subframe*PHY_vars_UE->lte_frame_parms.samples_per_tti,
                                    1,
                                    0);
                //printf ("Trial %d, Measurements are done \n", trials);
                      /*
                        debug_msg("RX RSSI %d dBm, digital (%d, %d) dB, linear (%d, %d), avg rx power %d dB (%d lin), RX gain %d dB\n",
                        PHY_vars_UE->PHY_measurements.rx_rssi_dBm[0] - ((PHY_vars_UE->lte_frame_parms.nb_antennas_rx==2) ? 3 : 0),
                        PHY_vars_UE->PHY_measurements.wideband_cqi_dB[0][0],
                        PHY_vars_UE->PHY_measurements.wideband_cqi_dB[0][1],
                        PHY_vars_UE->PHY_measurements.wideband_cqi[0][0],
                        PHY_vars_UE->PHY_measurements.wideband_cqi[0][1],
                        PHY_vars_UE->PHY_measurements.rx_power_avg_dB[0],
                        PHY_vars_UE->PHY_measurements.rx_power_avg[0],
                        PHY_vars_UE->rx_total_gain_dB);
                        debug_msg("N0 %d dBm digital (%d, %d) dB, linear (%d, %d), avg noise power %d dB (%d lin)\n",
                        PHY_vars_UE->PHY_measurements.n0_power_tot_dBm,
                        PHY_vars_UE->PHY_measurements.n0_power_dB[0],
                        PHY_vars_UE->PHY_measurements.n0_power_dB[1],
                        PHY_vars_UE->PHY_measurements.n0_power[0],
                        PHY_vars_UE->PHY_measurements.n0_power[1],
                        PHY_vars_UE->PHY_measurements.n0_power_avg_dB,
                        PHY_vars_UE->PHY_measurements.n0_power_avg);
                        debug_msg("Wideband CQI tot %d dB, wideband cqi avg %d dB\n",
                        PHY_vars_UE->PHY_measurements.wideband_cqi_tot[0],
                        PHY_vars_UE->PHY_measurements.wideband_cqi_avg[0]);
                      */

                if (transmission_mode == 4 || transmission_mode == 5 || transmission_mode == 6) {
                  if (pmi_feedback == 1) {
                    pmi_feedback = 0;
                    hold_channel = 1;
                    //printf ("trial %d pmi_feedback %d \n", trials, pmi_feedback);
                    //printf ("go to PMI feedback\n");
                    goto PMI_FEEDBACK;
                  }
                }

              }


              if ((Ns==(2*subframe)) && (l==pilot1)) {// process symbols 0,1,2
                if (dci_flag == 1) {
                  PHY_vars_UE->UE_mode[0] = PUSCH;
                  start_meas(&PHY_vars_UE->dlsch_rx_pdcch_stats);

                  rx_pdcch(&PHY_vars_UE->lte_ue_common_vars,
                           PHY_vars_UE->lte_ue_pdcch_vars,
                           &PHY_vars_UE->lte_frame_parms,
                           subframe,
                           0,
                           (PHY_vars_UE->lte_frame_parms.mode1_flag == 1) ? SISO : ALAMOUTI,
                           PHY_vars_UE->high_speed_flag,
                           0);

                  stop_meas(&PHY_vars_UE->dlsch_rx_pdcch_stats);
                  // overwrite number of pdcch symbols
                  PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols = num_pdcch_symbols;

                  dci_cnt = dci_decoding_procedure(PHY_vars_UE,
                                                   dci_alloc_rx,1,
                                                   eNB_id,
                                                   subframe);
                  printf("dci_cnt %d\n",dci_cnt);

                  if (dci_cnt==0) {
                    dlsch_active = 0;
                    if (round==0) {
                      dci_errors++;
                      round=5; // this is meant to stop the "while" loop if DCI is wrong;
                      errs[0][0]++;

                      if (n_frames==2)
                        printf("DCI error trial %d errs[0][0] %d\n",trials,errs[0][0]);
                    }
                  }

                  for (i=0;i<dci_cnt;i++) {
                    //printf("Generating dlsch parameters for RNTI %x\n",dci_alloc_rx[i].rnti);
                     if (round == 0) {
                        PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->first_tx=1;
                      }
                      if ((transmission_mode == 3 || transmission_mode ==4) && (round == 0)) {
                        PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->first_tx=1;
                      }
                    if ((dci_alloc_rx[i].rnti == n_rnti) &&
                        (generate_ue_dlsch_params_from_dci(0,
                                                           subframe,
                                                           dci_alloc_rx[i].dci_pdu,
                                                           dci_alloc_rx[i].rnti,
                                                           dci_alloc_rx[i].format,
                                                           PHY_vars_UE->dlsch_ue[0],
                                                           &PHY_vars_UE->lte_frame_parms,
                                                           PHY_vars_UE->pdsch_config_dedicated,
                                                           SI_RNTI,
                                                           0,
                                                           P_RNTI)==0)) {
                      dump_dci(&PHY_vars_UE->lte_frame_parms,&dci_alloc_rx[i]);
                      coded_bits_per_codeword[0]= get_G(&PHY_vars_eNB->lte_frame_parms,
                                                      PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->nb_rb,
                                                      PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->rb_alloc_even,
                                                      get_Qm(PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->mcs),
                                                      PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->Nl,
                                                      PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols,
                                                      0,
                                                      subframe);
                      if (transmission_mode == 3 || transmission_mode == 4) {
                        coded_bits_per_codeword[1]= get_G(&PHY_vars_eNB->lte_frame_parms,
                                                      PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->nb_rb,
                                                      PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->rb_alloc_even,
                                                      get_Qm(PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->mcs),
                                                      PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->Nl,
                                                      PHY_vars_UE->lte_ue_pdcch_vars[1]->num_pdcch_symbols,
                                                      0,
                                                      subframe);
                      }
                      /*
                      rate = (double)dlsch_tbs25[get_I_TBS(PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->mcs)][PHY_vars_UE->dlsch_ue[0][0]->nb_rb-1]/(coded_bits_per_codeword);
                      rate*=get_Qm(PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->mcs);
                      */
                      printf("num_pdcch_symbols %d, G %d, TBS %d\n",PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols,coded_bits_per_codeword [0],PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->TBS);

                      dlsch_active = 1; // what does it indicates???
                    } else {
                        dlsch_active = 0;
                        if (round==0) {
                          dci_errors++;
                          errs[0][0]++;
                          round=5;

                          if (n_frames==2)
                            printf("DCI misdetection trial %d\n",trials);

                        }
                      }
                  }
                } else { //dci_flag == 0
                      PHY_vars_UE->lte_ue_pdcch_vars[0]->crnti = n_rnti;
                      PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols = num_pdcch_symbols;
                      if (round == 0) {
                        PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->first_tx=1;
                      }
                      if (round == 0) {
                        PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->first_tx=1;
                      }

                      switch (transmission_mode) {
                      case 1:
                      case 2:
                        generate_ue_dlsch_params_from_dci(0,
                                                          subframe,
                                                          &DLSCH_alloc_pdu_1[0],
                                                          (common_flag==0)? C_RNTI : SI_RNTI,
                                                          (common_flag==0)? format1 : format1A,
                                                          PHY_vars_UE->dlsch_ue[0],
                                                          &PHY_vars_UE->lte_frame_parms,
                                                          PHY_vars_UE->pdsch_config_dedicated,
                                                          SI_RNTI,
                                                          0,
                                                          P_RNTI);
                        break;
                      case 3:
                        //printf("Rate: TM3 (before) round %d (%d) first_tx %d\n",round,PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->round,PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->first_tx);
                        generate_ue_dlsch_params_from_dci(0,
                                                          subframe,
                                                          &DLSCH_alloc_pdu_1[0],
                                                          (common_flag==0)? C_RNTI : SI_RNTI,
                                                          (common_flag==0)? format2A : format1A,
                                                          PHY_vars_UE->dlsch_ue[0],
                                                          &PHY_vars_UE->lte_frame_parms,
                                                          PHY_vars_UE->pdsch_config_dedicated,
                                                          SI_RNTI,
                                                          0,
                                                          P_RNTI);
                        //printf("Rate: TM3 (after) round %d (%d) first_tx %d\n",round,PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->round,PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->first_tx);
                        break;
                      case 4:
                        generate_ue_dlsch_params_from_dci(0,
                                                          subframe,
                                                          &DLSCH_alloc_pdu_1[0],
                                                          (common_flag==0)? C_RNTI : SI_RNTI,
                                                          (common_flag==0)? format2 : format1A,//format1A only for a codeblock
                                                          PHY_vars_UE->dlsch_ue[0],
                                                          &PHY_vars_UE->lte_frame_parms,
                                                          PHY_vars_UE->pdsch_config_dedicated,
                                                          SI_RNTI,
                                                          0,
                                                          P_RNTI);

                        break;
                      case 5:
                      case 6:
                        generate_ue_dlsch_params_from_dci(0,
                                                          subframe,
                                                          &DLSCH_alloc_pdu2_1E[0],
                                                          C_RNTI,
                                                          format1E_2A_M10PRB,
                                                          PHY_vars_UE->dlsch_ue[0],
                                                          &PHY_vars_UE->lte_frame_parms,
                                                          PHY_vars_UE->pdsch_config_dedicated,
                                                          SI_RNTI,
                                                          0,
                                                          P_RNTI);
                        break;
                      }
                      dlsch_active = 1;
                    } // if dci_flag == 1
              }

              if (dlsch_active == 1) {
                if (TB0_active==1)
                  cur_harq_pid =PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid;
                else
                  cur_harq_pid =PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid;

                if ((Ns==(1+(2*subframe))) && (l==0)) {// process PDSCH symbols 1,2,3,4,5,(6 Normal Prefix
                  if (transmission_mode == 5) {
                    if ((PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[cur_harq_pid]->dl_power_off==0) &&
                        (openair_daq_vars.use_ia_receiver ==1)) {
                      rx_type = rx_IC_single_stream;
                    } else {
                      rx_type = rx_standard;
                    }
                  }


                  start_meas(&PHY_vars_UE->dlsch_llr_stats);

                  for (m=PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols; m<pilot2; m++) {

                    if (rx_pdsch(PHY_vars_UE,
                                 PDSCH,
                                 eNB_id,
                                 eNB_id_i,
                                 subframe,
                                 m,
                                 (m==PHY_vars_UE->lte_ue_pdcch_vars[0]->num_pdcch_symbols)?1:0,
                                 rx_type,
                                 i_mod,
                                 cur_harq_pid)==-1){
                      dlsch_active = 0;
                      break;
                    }
                  }
                  stop_meas(&PHY_vars_UE->dlsch_llr_stats);
                }

                if ((Ns==(1+(2*subframe))) && (l==pilot1)){// process symbols (6 Extended Prefix),7,8,9
                    start_meas(&PHY_vars_UE->dlsch_llr_stats);
                    for (m=pilot2;m<pilot3; m++) {
                      if (rx_pdsch(PHY_vars_UE,
                             PDSCH,
                             eNB_id,
                             eNB_id_i,
                             subframe,
                             m,
                             0,
                             rx_type,
                             i_mod,
                             cur_harq_pid)==-1){
                        dlsch_active=0;
                        break;
                        }
                      }
                    stop_meas(&PHY_vars_UE->dlsch_llr_stats);
                  }

                if ((Ns==(2+(2*subframe))) && (l==0)) { // process symbols 10,11,(12,13 Normal Prefix) do deinterleaving for TTI
                  start_meas(&PHY_vars_UE->dlsch_llr_stats);
                  for (m=pilot3; m<PHY_vars_UE->lte_frame_parms.symbols_per_tti; m++) {
                    if (rx_pdsch(PHY_vars_UE,
                                 PDSCH,
                                 eNB_id,
                                 eNB_id_i,
                                 subframe,
                                 m,
                                 0,
                                 rx_type,
                                 i_mod,
                                 cur_harq_pid)==-1) {
                    dlsch_active=0;
                    break;
                    }
                  }
                  stop_meas(&PHY_vars_UE->dlsch_llr_stats);
                }
              }
            }
          }

          //saving PMI in case of Transmission Mode > 5

          if(abstx){
            if (trials==0 && round==0 && transmission_mode>=4){
              for (iii=0; iii<NB_RB; iii++){
                //fprintf(csv_fd, "%d, %d", (PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->pmi_ext[iii]),(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id_i]->pmi_ext[iii]));
                fprintf(csv_fd,"%x,",(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->pmi_ext[iii]));
                //printf("%x ",(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->pmi_ext[iii]));
              }
            }
          }

          /*if (rx_type==rx_SIC_dual_stream){
            cw_to_decode_interf=1;
            cw_to_decode_interf_free=1;
          }
            else {
            cw_to_decode_interf=Kmimo;
            }*/

          for (TB=0; TB<Kmimo; TB++){ // may be we ll have to swap CW

           /* if (PHY_vars_UE->dlsch_ue[0][TB] == NULL){
              printf("[DLSIM] PHY_vars_UE->dlsch_ue[0][TB] == NULL\n");
              printf("HERE HERE HERE HERE\n");
              continue;
            }*/

              if (TB0_active == 0){
#ifdef DEBUG_HARQ
              printf("[DLSIM] Skip TB0 \n");
#endif
              TB++;

            }
#ifdef DEBUG_HARQ
            printf("[DLSIM] process TB %d \n", TB);
#endif

            PHY_vars_UE->dlsch_ue[0][TB]->rnti = (common_flag==0) ? n_rnti: SI_RNTI;
            coded_bits_per_codeword[TB] = get_G(&PHY_vars_eNB->lte_frame_parms,
                                            PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->nb_rb,
                                            PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->rb_alloc,
                                            get_Qm(PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->mcs),
                                            PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->Nl,
                                            num_pdcch_symbols,
                                            0,subframe);

            PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->G = coded_bits_per_codeword[TB];
            PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->Qm = get_Qm(PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->mcs);

            if (n_frames==2) {
              printf("Kmimo=%d, TB=%d, G=%d, TBS=%d\n",Kmimo,TB,coded_bits_per_codeword[TB],
                     PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->TBS);

              // calculate uncoded BER
              uncoded_ber_bit = (short*) malloc(sizeof(short)*coded_bits_per_codeword[TB]);

              AssertFatal(uncoded_ber_bit, "uncoded_ber_bit==NULL");

              sprintf(fname,"dlsch%d_rxF_r%d_cw%d_llr.m",eNB_id,round, TB);
              sprintf(vname,"dl%d_r%d_cw%d_llr",eNB_id,round, TB);
              write_output(fname,vname, PHY_vars_UE->lte_ue_pdsch_vars[0]->llr[PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->codeword],coded_bits_per_codeword[TB],1,0);
              sprintf(fname,"dlsch_cw%d_e.m", TB);
              sprintf(vname,"dlschcw%d_e", TB);
              write_output(fname, vname,PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->e,coded_bits_per_codeword[TB],1,4);
              uncoded_ber=0;
              printf("trials=%d\n", trials);

              for (i=0;i<coded_bits_per_codeword[TB];i++)
                if (PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->e[i] != (PHY_vars_UE->lte_ue_pdsch_vars[0]->llr[PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->codeword][i]<0)) {
                  uncoded_ber_bit[i] = 1;
                  uncoded_ber++;
                }
                else
                  uncoded_ber_bit[i] = 0;

              uncoded_ber/=coded_bits_per_codeword[TB];
              avg_ber += uncoded_ber;
              sprintf(fname,"cw%d_uncoded_ber_bit.m", TB);
              sprintf(vname,"uncoded_ber_bit_cw%d", TB);
              write_output(fname, vname,uncoded_ber_bit,coded_bits_per_codeword[TB],1,0);
              printf("cw %d, uncoded ber %f\n",TB,uncoded_ber);


              free(uncoded_ber_bit);
              uncoded_ber_bit = NULL;

            }

            start_meas(&PHY_vars_UE->dlsch_unscrambling_stats);
            dlsch_unscrambling(&PHY_vars_UE->lte_frame_parms,
                               0,
                               PHY_vars_UE->dlsch_ue[0][TB],
                               coded_bits_per_codeword[TB],
                               PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->codeword],
                               TB,
                               subframe<<1);
            stop_meas(&PHY_vars_UE->dlsch_unscrambling_stats);

            start_meas(&PHY_vars_UE->dlsch_decoding_stats);
            ret[TB] = dlsch_decoding(PHY_vars_UE,
                                     PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid]->codeword],
                                     &PHY_vars_UE->lte_frame_parms,
                                     PHY_vars_UE->dlsch_ue[0][TB],
                                     PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid],
                                    subframe,
                                    PHY_vars_UE->dlsch_ue[0][TB]->current_harq_pid,
                                    1,llr8_flag);
            stop_meas(&PHY_vars_UE->dlsch_decoding_stats);
#ifdef DEBUG_HARQ
            printf("[DLSIM] ret[%d] = %d\n", TB, ret[TB]);
#endif

            //printf("retr cw 0 = %d\n", ret[0]);
            //printf("current round = %d\n", PHY_vars_UE->dlsch_ue[0][cw_non_sic]->harq_processes[PHY_vars_UE->dlsch_ue[0][cw_non_sic]->current_harq_pid]->round);



            if (ret[TB] <= PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations ) { //if CW0 is decoded, approach CW1
#ifdef DEBUG_HARQ
            printf("[DLSIM] TB%d is decoded\n", TB);
#endif

                /*avg_iter[TB] += ret[TB];
                iter_trials[TB]++;*/

              if (n_frames==2) {
                printf("cw non sic %d, round %d: No DLSCH errors found, uncoded ber %f\n",TB,round,uncoded_ber);
#ifdef PRINT_BYTES
                for (s=0;s<PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->C;s++) {
                  if (s<PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Cminus)
                    Kr = PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Kminus;
                  else
                    Kr = PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Kplus;

                  Kr_bytes = Kr>>3;

                  printf("Decoded_output (Segment %d):\n",s);
                  for (i=0;i<Kr_bytes;i++)
                    printf("%d : %x (%x)\n",i,PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->c[s][i],
                           PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->c[s][i]^PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->c[s][i]);
                }
#endif
              }

              PHY_vars_UE->total_TBS[eNB_id] =  PHY_vars_UE->total_TBS[eNB_id] + PHY_vars_UE->dlsch_ue[eNB_id][TB]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][TB]->current_harq_pid]->TBS;

              // If the  receiver is NOT SIC, Here we are done with both CW, now only to calculate BLER
              //If the receiver IS SIC, we are done only with CW0, CW1 was only compensated by this moment (y1' obtained)
              if (PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode == LARGE_CDD) {   //try to decode second stream using SIC
              /*
              //for (round = 0 ; round < PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->round ; round++) {
              // we assume here that the second stream has a lower MCS and is thus more likely to be decoded
              // re-encoding of second stream
              dlsch0_ue_harq = PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid];
              dlsch0_eNB_harq = PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid];
              dlsch0_eNB_harq->mimo_mode    = LARGE_CDD;
              dlsch0_eNB_harq->rb_alloc[0]  = dlsch0_ue_harq->rb_alloc[0];
              dlsch0_eNB_harq->nb_rb        = dlsch0_ue_harq->nb_rb;
              dlsch0_eNB_harq->mcs          = dlsch0_ue_harq->mcs;
              dlsch0_eNB_harq->rvidx        = dlsch0_ue_harq->rvidx;
              dlsch0_eNB_harq->Nl           = dlsch0_ue_harq->Nl;
              dlsch0_eNB_harq->round        = dlsch0_ue_harq->round;

              dlsch0_eNB_harq->TBS          = dlsch0_ue_harq->TBS;
              dlsch0_eNB_harq->dl_power_off = dlsch0_ue_harq->dl_power_off;
              dlsch0_eNB_harq->status       = dlsch0_ue_harq->status;

              PHY_vars_UE->dlsch_eNB[eNB_id]->active       = PHY_vars_UE->dlsch_ue[eNB_id][0]->active;
              PHY_vars_UE->dlsch_eNB[eNB_id]->rnti         = PHY_vars_UE->dlsch_ue[eNB_id][0]->rnti;
              PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid         = PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid;

              dlsch_encoding(PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->b,
                 &PHY_vars_UE->lte_frame_parms,
                 num_pdcch_symbols,
                 PHY_vars_UE->dlsch_eNB[eNB_id],
                 0,subframe,
                 &PHY_vars_UE->dlsch_rate_matching_stats,
                 &PHY_vars_UE->dlsch_turbo_encoding_stats,
                 &PHY_vars_UE->dlsch_interleaving_stats
                 );

              coded_bits_per_codeword = get_G(&PHY_vars_UE->lte_frame_parms,
                      PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->nb_rb,
                      PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->rb_alloc,
                      get_Qm(PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->mcs),
                      PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->Nl,
                      num_pdcch_symbols,
                      0,subframe);


              //scrambling
              dlsch_scrambling(&PHY_vars_UE->lte_frame_parms,
                   0,
                   PHY_vars_UE->dlsch_eNB[eNB_id],
                   coded_bits_per_codeword,
                   0,
                   subframe<<1);


              //modulation
              re_allocated = dlsch_modulation(sic_buffer,
                      AMP,
                      subframe,
                      &PHY_vars_UE->lte_frame_parms,
                      num_pdcch_symbols,
                      &PHY_vars_UE->dlsch_eNB[0][0],
                      NULL);
              // sic_buffer is a vector of size nb_antennas_tx, but both contain the same signal, since we do modulation without precoding
              // precoding is contained in effective channel estimate
              // compute the norm of the effective channel for both receive antennas -> alphha[0], alphha[2]
              // multiply with the norm of the effective channnel
              */

              //stripping (from matched filter output of first stream = rxdataF_comp0)
              // this is pseudocode
              /*
          for (i=0; i<frame_parms->nb_antennas_rx; i++) {
          PHY_vars_UE->lte_ue_pdsch_vars[eNB_id].rxdataF_ext[i] -=   alpha[i].*sic_buffer[i];
          }
              */

              //apply rho to output
              /*
          dlsch_channel_compensation(PHY_vars_UE->lte_ue_pdsch_vars[eNB_id].rxdataF_ext,
                   PHY_vars_UE->lte_ue_pdsch_vars[eNB_id].dl_ch_rho_ext[harq_pid][round],
                   PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_mag1,
                   PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_magb1,
                   PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->rxdataF_comp1,
                   NULL,
                   frame_parms,
                   symbol,
                   first_symbol_flag,
                   i_mod,
                   nb_rb,
                   lte_ue_pdsch_vars[eNB_id]->log2_maxh,
                   phy_measurements); // log2_maxh+I0_shift
          */


              //detection of second stream
              //}

              }

              if ((PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode >=DUALSTREAM_UNIFORM_PRECODING1) &&
                  (PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode <=DUALSTREAM_PUSCH_PRECODING) && (TB0_active == 1) &&
                  (rx_type==rx_SIC_dual_stream)) {
#ifdef DEBUG_HARQ
                printf("[DLSIM] Starting SIC procedure\n");
#endif
               // printf("current round = %d\n", PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->round);


              //printf("\n CW 0 is decoded, i go for , round %d\n", round);
              //printf("\n ret[TB0] = %d  round %d\n", ret[TB], round);

                for (round_sic = 0 ; round_sic < (round +1); round_sic++) {

#ifdef DEBUG_HARQ
                printf("[DLSIM] 0 Round sic = %d\n", round_sic);
#endif
                //printf("I enter round_sic loop \n");
                //printf("round_sic= %d\n", round_sic);
                dlsch0_ue_harq = PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][1]->current_harq_pid];
                dlsch0_eNB_harq = PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][1]->current_harq_pid];

                dlsch0_eNB_harq->mimo_mode    = PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode;
                dlsch0_eNB_harq->rb_alloc[0]  = dlsch0_ue_harq->rb_alloc_even[0];
                dlsch0_eNB_harq->nb_rb        = dlsch0_ue_harq->nb_rb;
                dlsch0_eNB_harq->mcs          = dlsch0_ue_harq->mcs;
                dlsch0_eNB_harq->rvidx        = dlsch0_ue_harq->rvidx;
                dlsch0_eNB_harq->Nl           = dlsch0_ue_harq->Nl;
                dlsch0_eNB_harq->round        = dlsch0_ue_harq->round;
                dlsch0_eNB_harq->TBS          = dlsch0_ue_harq->TBS;
                dlsch0_eNB_harq->dl_power_off = dlsch0_ue_harq->dl_power_off;
                dlsch0_eNB_harq->status       = dlsch0_ue_harq->status;

                PHY_vars_UE->dlsch_eNB[eNB_id]->active                   = PHY_vars_UE->dlsch_ue[eNB_id][0]->active;
                PHY_vars_UE->dlsch_eNB[eNB_id]->rnti                     = PHY_vars_UE->dlsch_ue[eNB_id][0]->rnti;
                PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid         = PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid;

                dlsch_encoding(input_buffer0[0], //PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->b,
                               &PHY_vars_UE->lte_frame_parms,
                               num_pdcch_symbols,
                               PHY_vars_UE->dlsch_eNB[eNB_id],
                               0,
                               subframe,
                               &PHY_vars_UE->dlsch_rate_matching_stats,
                               &PHY_vars_UE->dlsch_turbo_encoding_stats,
                               &PHY_vars_UE->dlsch_interleaving_stats);


                coded_bits_per_codeword[0]= get_G(&PHY_vars_UE->lte_frame_parms,
                                                PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->nb_rb,
                                                PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->rb_alloc,
                                                get_Qm(PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->mcs),
                                                PHY_vars_UE->dlsch_eNB[eNB_id]->harq_processes[PHY_vars_UE->dlsch_eNB[eNB_id]->current_harq_pid]->Nl,
                                                num_pdcch_symbols,
                                                0,
                                                subframe);

                dlsch_scrambling(&PHY_vars_UE->lte_frame_parms,
                                 0,
                                 PHY_vars_UE->dlsch_eNB[eNB_id],
                                 coded_bits_per_codeword[TB],
                                 0,
                                 subframe<<1);

                re_allocated = dlsch_modulation_SIC(sic_buffer,
                                                    subframe,
                                                    &PHY_vars_UE->lte_frame_parms,
                                                    num_pdcch_symbols,
                                                    &PHY_vars_UE->dlsch_eNB[0][0],
                                                    NULL,
                                                    coded_bits_per_codeword[0]);

               // write_output("sic_buffer.m","sic", *sic_buffer,re_allocated,1,1);
               // write_output("rxdataF_comp1.m","rxF_comp1", *PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->rxdataF_comp1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round],14*12*25,1,1);
               // write_output("rxdataF_rho.m","rho", *PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_rho_ext[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round],14*12*25,1,1);

                switch  (get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)){


                  case 2:

                    dlsch_qpsk_llr_SIC(&PHY_vars_UE->lte_frame_parms,
                                       PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->rxdataF_comp1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                       sic_buffer,
                                       PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_rho_ext[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                       PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
                                       num_pdcch_symbols,
                                       dlsch0_eNB_harq->nb_rb,
                                       subframe,
                                       dlsch0_eNB_harq->rb_alloc[0],
                                       get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs),
                                       PHY_vars_UE->dlsch_ue[eNB_id][0]);
                  break;

                  case 4:

                    dlsch_16qam_llr_SIC(&PHY_vars_UE->lte_frame_parms,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->rxdataF_comp1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        sic_buffer,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_rho_ext[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
                                        num_pdcch_symbols,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_mag1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        dlsch0_eNB_harq->nb_rb,
                                        subframe,
                                        dlsch0_eNB_harq->rb_alloc[0],
                                        get_Qm(PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->mcs),
                                        PHY_vars_UE->dlsch_ue[eNB_id][0]);
                  break;
                  case 6:
                    dlsch_64qam_llr_SIC(&PHY_vars_UE->lte_frame_parms,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->rxdataF_comp1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        sic_buffer,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_rho_ext[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
                                        num_pdcch_symbols,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_mag1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->dl_ch_magb1[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid][round_sic],
                                        dlsch0_eNB_harq->nb_rb,
                                        subframe,
                                        dlsch0_eNB_harq->rb_alloc[0],
                                        get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs),
                                        PHY_vars_UE->dlsch_ue[eNB_id][TB]);
                  break;
                    }
                  //}// rouns sic
#ifdef DEBUG_HARQ
                  printf("[DLSIM] TB1 is mapped into CW%d\n", PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword);
#endif

                    //  write_output("rxdata_llr1.m","llr1", PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[1],re_allocated*2,1,0);

                  // replace cw_sic with TB+1
                  PHY_vars_UE->dlsch_ue[0][1]->rnti = (common_flag==0) ? n_rnti: SI_RNTI;
                  coded_bits_per_codeword[1]= get_G(&PHY_vars_eNB->lte_frame_parms,
                                                         PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->nb_rb,
                                                         PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->rb_alloc,
                                                         get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs),
                                                         PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->Nl,
                                                         num_pdcch_symbols,
                                                         0,
                                                         subframe);

                  PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->G = coded_bits_per_codeword[1];
                  PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->Qm = get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs);

                  if (n_frames==2) {
                    printf("Kmimo=%d, cw=%d, G=%d, TBS=%d\n",Kmimo,1,coded_bits_per_codeword[1],
                    PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->TBS);

                  // calculate uncoded BER
                    uncoded_ber_bit = (short*) malloc(sizeof(short)*coded_bits_per_codeword[1]);
                    AssertFatal(uncoded_ber_bit, "uncoded_ber_bit==NULL");
                    sprintf(fname,"dlsch%d_rxF_r%d_cw%d_llr.m",eNB_id,round,1);
                    sprintf(vname,"dl%d_r%d_cw%d_llr",eNB_id,round, 1);
                    write_output(fname,vname, PHY_vars_UE->lte_ue_pdsch_vars[0]->llr[1],coded_bits_per_codeword[1],1,0);
                    sprintf(fname,"dlsch_cw%d_e.m", 1);
                    sprintf(vname,"dlschcw%d_e", 1);
                    write_output(fname, vname,PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->e,coded_bits_per_codeword[1],1,4);
                    uncoded_ber=0;
                    printf("trials=%d\n", trials);
                    for (i=0;i<coded_bits_per_codeword[1];i++)
                    if (PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->e[i] != (PHY_vars_UE->lte_ue_pdsch_vars[0]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword][i]<0)) {
                      uncoded_ber_bit[i] = 1;
                      uncoded_ber++;
                    }
                    else
                    uncoded_ber_bit[i] = 0;

                   uncoded_ber/=coded_bits_per_codeword[1];
                   avg_ber += uncoded_ber;
                   sprintf(fname,"cw%d_uncoded_ber_bit.m", 1);
                   sprintf(vname,"uncoded_ber_bit_cw%d", 1);
                   write_output(fname, vname,uncoded_ber_bit,coded_bits_per_codeword[1],1,0);
                   printf("cw %d, uncoded ber %f\n",1,uncoded_ber);
                         free(uncoded_ber_bit);
                   uncoded_ber_bit = NULL;
                }

                start_meas(&PHY_vars_UE->dlsch_unscrambling_stats);
                dlsch_unscrambling(&PHY_vars_UE->lte_frame_parms,
                                   0,
                                   PHY_vars_UE->dlsch_ue[0][1],
                                   coded_bits_per_codeword[1],
                                   PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
                                   1,
                                   subframe<<1);
                stop_meas(&PHY_vars_UE->dlsch_unscrambling_stats);

                start_meas(&PHY_vars_UE->dlsch_decoding_stats);
                ret[1] = dlsch_decoding(PHY_vars_UE,
                                        PHY_vars_UE->lte_ue_pdsch_vars[eNB_id]->llr[PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid]->codeword],
                                        &PHY_vars_UE->lte_frame_parms,
                                        PHY_vars_UE->dlsch_ue[0][1],
                                        PHY_vars_UE->dlsch_ue[0][1]->harq_processes[PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid],
                                        subframe,
                                        PHY_vars_UE->dlsch_ue[0][1]->current_harq_pid,
                                        1,llr8_flag);
                stop_meas(&PHY_vars_UE->dlsch_decoding_stats);

#ifdef DEBUG_HARQ
                printf("[DLSIM] Decoding TB1 in SIC: ret[1] = %d,  round sic %d\n", ret[1], round_sic);
#endif

                //printf("ret TB 1 = %d round %d \n", ret[1], round);

                if (ret[1] <= PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations ) {
                  round_sic = round+1; // to exit round_sic
#ifdef DEBUG_HARQ
                  printf("[DLSIM] TB1 is decoded in SIC loop\n");
#endif
                  avg_iter[1] += ret[1];
                  iter_trials[1]++;


                  if (n_frames==2) {
                    printf("cw sic %d, round %d: No DLSCH errors found, uncoded ber %f\n",1,round,uncoded_ber);

                    #ifdef PRINT_BYTES
                    for (s=0;s<PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->C;s++) {
                    if (s<PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Cminus)
                      Kr = PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Kminus;
                    else
                      Kr = PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Kplus;

                    Kr_bytes = Kr>>3;

                    printf("Decoded_output (Segment %d):\n",s);

                    for (i=0;i<Kr_bytes;i++)
                      printf("%d : %x (%x)\n",i,PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->c[s][i],
                      PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->c[s][i]^PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->c[s][i]);
                    }
                    #endif
                  }
                }
              } //round_sic


            if (ret[1] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations ){
              errs[1][round]++;
#ifdef DEBUG_HARQ
                  printf("[DLSIM] TB1 is not decoded in SIC loop, errs[TB1][round %d] = %d\n",round, errs[1][round]);
#endif

                 // exit(0);

                  avg_iter[1] += ret[1]-1;
                  iter_trials[1]++;

                  if (n_frames==2) {
                    //if ((n_frames==1) || (SNR>=30)) {
                    printf("cw sic %d, round %d: DLSCH errors found, uncoded ber %f\n",1,round,uncoded_ber);
#ifdef PRINT_BYTES
                    for (s=0;s<PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->C;s++) {
                      if (s<PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Cminus)
                        Kr = PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Kminus;
                      else
                        Kr = PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->Kplus;

                      Kr_bytes = Kr>>3;

                      printf("Decoded_output (Segment %d):\n",s);
                      for (i=0;i<Kr_bytes;i++)
                        printf("%d : %x (%x)\n",i,PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->c[s][i],
                         PHY_vars_UE->dlsch_ue[0][1]->harq_processes[0]->c[s][i]^PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->c[s][i]);
                    }
#endif
                  } //n_frames==1
                 // exit(0);
              } //if (ret > PHY_vars_UE->dlsch_ue[0][1]->max_turbo_iterations )
            }//if SIC
          } else {

            errs[TB][round]++;
#ifdef DEBUG_HARQ
            printf("[DLSIM] TB%d is not decoded outside SIC loop, errs[TB%d][round %d] = %d\n", TB, TB, round, errs[TB][round]);
#endif


              /*if (cw_non_sic==0) {
          avg_iter[0] += ret[0]-1;
          iter_trials[0]++;
              }*/

            if ((PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode >=DUALSTREAM_UNIFORM_PRECODING1) &&
              (PHY_vars_UE->dlsch_ue[eNB_id][0]->harq_processes[PHY_vars_UE->dlsch_ue[eNB_id][0]->current_harq_pid]->mimo_mode <=DUALSTREAM_PUSCH_PRECODING) &&
                (rx_type==rx_SIC_dual_stream) && (TB0_active ==1)) {
                errs[1][round]++;
#ifdef DEBUG_HARQ
              printf("[DLSIM] TB%d is not decoded outside SIC loop, errs[TB%d][round %d] = %d\n", 1, 1, round, errs[1][round]);
#endif
              }


                /*if (cw_non_sic==1) {
          avg_iter[1] += ret[1]-1;
          iter_trials[1]++;
              }*/


              if (n_frames==2) {
          //if ((n_frames==1) || (SNR>=30)) {
                printf("cw %d, round %d: DLSCH errors found, uncoded ber %f\n",TB,round,uncoded_ber);
#ifdef PRINT_BYTES
                for (s=0;s<PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->C;s++) {
                  if (s<PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Cminus)
                    Kr = PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Kminus;
                  else
                    Kr = PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->Kplus;

                  Kr_bytes = Kr>>3;

                  printf("Decoded_output (Segment %d):\n",s);
                  for (i=0;i<Kr_bytes;i++)
                    printf("%d : %x (%x)\n",i,PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->c[s][i],
                     PHY_vars_UE->dlsch_ue[0][TB]->harq_processes[0]->c[s][i]^PHY_vars_eNB->dlsch_eNB[0][TB]->harq_processes[0]->c[s][i]);
                }
#endif
              }
            }
            TB++; // to terminate the loop over TB
          }

          stop_meas(&PHY_vars_UE->phy_proc_rx);

          if (n_frames==2) {

            //rxsig
            sprintf(fname,"rxsig0_r%d.m",round);
            sprintf(vname,"rxs0_r%d",round);
            write_output(fname,vname, &PHY_vars_UE->lte_ue_common_vars.rxdata[0][0],10*PHY_vars_UE->lte_frame_parms.samples_per_tti,1,1);
            sprintf(fname,"rxsigF0_r%d.m",round);
            sprintf(vname,"rxs0F_r%d",round);
            write_output(fname,vname, &PHY_vars_UE->lte_ue_common_vars.rxdataF[0][0],2*PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,2,1);
            if (PHY_vars_UE->lte_frame_parms.nb_antennas_rx>1) {
              sprintf(fname,"rxsig1_r%d.m",round);
              sprintf(vname,"rxs1_r%d",round);
              write_output(fname,vname, PHY_vars_UE->lte_ue_common_vars.rxdata[1],PHY_vars_UE->lte_frame_parms.samples_per_tti,1,1);
              sprintf(fname,"rxsig1F_r%d.m",round);
              sprintf(vname,"rxs1F_r%d",round);
              write_output(fname,vname, PHY_vars_UE->lte_ue_common_vars.rxdataF[1],2*PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,2,1);
            }

            //channel
            write_output("chanF11.m","chF11",eNB2UE[0]->chF[0],12*NB_RB,1,8);
            write_output("chan11.m","ch11",eNB2UE[0]->ch[0],eNB2UE[0]->channel_length,1,8);
            if ( PHY_vars_eNB->lte_frame_parms.nb_antennas_rx==2 && PHY_vars_eNB->lte_frame_parms.nb_antennas_tx==1 ){
              write_output("chan21.m","ch21",eNB2UE[0]->ch[1],eNB2UE[0]->channel_length,1,8);
            }
            if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx>1){
              write_output("chan12.m","ch12",eNB2UE[0]->ch[1],eNB2UE[0]->channel_length,1,8);
              if ( PHY_vars_eNB->lte_frame_parms.nb_antennas_rx>1){
          write_output("chan21.m","ch21",eNB2UE[0]->ch[2],eNB2UE[0]->channel_length,1,8);
          write_output("chan22.m","ch22",eNB2UE[0]->ch[3],eNB2UE[0]->channel_length,1,8);
              }
            }

            //channel estimates
            sprintf(fname,"dlsch00_r%d.m",round);
            sprintf(vname,"dl00_r%d",round);
            write_output(fname,vname,
             &(PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[eNB_id][0][0]),
             PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,1,1);
            if (PHY_vars_UE->lte_frame_parms.nb_antennas_rx>1) {
              sprintf(fname,"dlsch01_r%d.m",round);
              sprintf(vname,"dl01_r%d",round);
              write_output(fname,vname,
               &(PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[eNB_id][1][0]),
               PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,1,1);
            }
            if (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx>1) {
              sprintf(fname,"dlsch10_r%d.m",round);
              sprintf(vname,"dl10_r%d",round);
              write_output(fname,vname,
               &(PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[eNB_id][2][0]),
               PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,1,1);
            }
            if ((PHY_vars_UE->lte_frame_parms.nb_antennas_rx>1) && (PHY_vars_eNB->lte_frame_parms.nb_antennas_tx>1)) {
              sprintf(fname,"dlsch11_r%d.m",round);
              sprintf(vname,"dl11_r%d",round);
              write_output(fname,vname,
               &(PHY_vars_UE->lte_ue_common_vars.dl_ch_estimates[eNB_id][3][0]),
               PHY_vars_UE->lte_frame_parms.ofdm_symbol_size*nsymb,1,1);
            }
            //pdsch_vars
            dump_dlsch2(PHY_vars_UE,eNB_id,coded_bits_per_codeword[0],round,PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid);
            /*
              write_output("dlsch_e.m","e",PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->e,coded_bits_per_codeword,1,4);
              write_output("dlsch_ber_bit.m","ber_bit",uncoded_ber_bit,coded_bits_per_codeword,1,0);
              write_output("dlsch_eNB_w.m","w",PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->w[0],3*(tbs+64),1,4);
              write_output("dlsch_UE_w.m","w",PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->w[0],3*(tbs+64),1,0);
            */

            //pdcch_vars
            write_output("pdcchF0_ext.m","pdcchF_ext", PHY_vars_UE->lte_ue_pdcch_vars[eNB_id]->rxdataF_ext[0],2*3*PHY_vars_UE->lte_frame_parms.ofdm_symbol_size,1,1);
            write_output("pdcch00_ch0_ext.m","pdcch00_ch0_ext",PHY_vars_UE->lte_ue_pdcch_vars[eNB_id]->dl_ch_estimates_ext[0],300*3,1,1);

            write_output("pdcch_rxF_comp0.m","pdcch0_rxF_comp0",PHY_vars_UE->lte_ue_pdcch_vars[eNB_id]->rxdataF_comp[0],4*300,1,1);
            write_output("pdcch_rxF_llr.m","pdcch_llr",PHY_vars_UE->lte_ue_pdcch_vars[eNB_id]->llr,2400,1,4);


            if (round == 3) exit(-1);
          }

          if (xforms==1) {
            phy_scope_UE(form_ue,
             PHY_vars_UE,
             eNB_id,
             0,// UE_id
             subframe);
          }
#ifdef DEBUG_HARQ
          printf("[DLSIM] Errors errs[TB0][round %d] = %d, errs[TB1][round %d] = %d\n ", round, errs[0][round], round, errs[1][round]);
#endif

          if ((transmission_mode != 3) && (transmission_mode !=4) && (ret[0] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations)){
            //printf("flag 1 \n");
            round++;
          }


          if ((transmission_mode == 3 || transmission_mode == 4 ) && (ret[0] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations) && (ret[1] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations)){
            resend_both[round]++;
            round++;
            resend_cw0_cw1=1;  //resend both cws
            resend_cw1=0;
            TB0_active=1;

          }

          if ((transmission_mode == 3 || transmission_mode == 4 ) && (ret[1] > PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations ) && (ret[0] <= PHY_vars_UE->dlsch_ue[0][0]->max_turbo_iterations)){
            resend_one[round]++;
            resend_cw0_cw1=0;
              TB0_active=0;
#ifdef DEBUG_HARQ
            printf("[DLSIM] ret[TB0] =%d, ret[TB1] =%d, trial %d \n", ret[0], ret[1], trials);
            printf("[DLSIM] TB0 deactivated\n");
#endif
            round++;
          }
#ifdef DEBUG_HARQ
          printf("[DLSIM] Now round is %d, trial %d\n" , round, trials);
#endif


        }


        if(transmission_mode != 3 && transmission_mode !=4 ){
          if ((errs[0][0]>=n_frames/10) && (trials>(n_frames/2)) )
          break;
        }

        //len = chbch_stats_read(stats_buffer,NULL,0,4096);
        //printf("%s\n\n",stats_buffer);

        if (PHY_vars_UE->frame_rx % 10 == 0) {
          PHY_vars_UE->bitrate[eNB_id] = (PHY_vars_UE->total_TBS[eNB_id] - PHY_vars_UE->total_TBS_last[eNB_id])*10;
          LOG_D(PHY,"[UE %d] Calculating bitrate: total_TBS = %d, total_TBS_last = %d, bitrate = %d kbits/s\n",PHY_vars_UE->Mod_id,PHY_vars_UE->total_TBS[eNB_id],PHY_vars_UE->total_TBS_last[eNB_id],PHY_vars_UE->bitrate[eNB_id]/1000);
          PHY_vars_UE->total_TBS_last[eNB_id] = PHY_vars_UE->total_TBS[eNB_id];
        }


        PHY_vars_UE->frame_rx++;

        /* calculate the total processing time for each packet,
         * get the max, min, and number of packets that exceed t>2000us
         */
        double t_tx = (double)PHY_vars_eNB->phy_proc_tx.p_time/cpu_freq_GHz/1000.0;
        double t_tx_ifft = (double)PHY_vars_eNB->ofdm_mod_stats.p_time/cpu_freq_GHz/1000.0;
        double t_tx_mod = (double)PHY_vars_eNB->dlsch_modulation_stats.p_time/cpu_freq_GHz/1000.0;
        double t_tx_enc = (double)PHY_vars_eNB->dlsch_encoding_stats.p_time/cpu_freq_GHz/1000.0;


        double t_rx = (double)PHY_vars_UE->phy_proc_rx.p_time/cpu_freq_GHz/1000.0;
        double t_rx_fft = (double)PHY_vars_UE->ofdm_demod_stats.p_time/cpu_freq_GHz/1000.0;
        double t_rx_demod = (double)PHY_vars_UE->dlsch_rx_pdcch_stats.p_time/cpu_freq_GHz/1000.0;
        double t_rx_dec = (double)PHY_vars_UE->dlsch_decoding_stats.p_time/cpu_freq_GHz/1000.0;

              if (t_tx > t_tx_max)
          t_tx_max = t_tx;
              if (t_tx < t_tx_min)
          t_tx_min = t_tx;
              if (t_rx > t_rx_max)
          t_rx_max = t_rx;
              if (t_rx < t_rx_min)
          t_rx_min = t_rx;
              if (t_tx > 2000)
          n_tx_dropped++;
              if (t_rx > 2000)
          n_rx_dropped++;


              push_front(&time_vector_tx, t_tx);
              push_front(&time_vector_tx_ifft, t_tx_ifft);
              push_front(&time_vector_tx_mod, t_tx_mod);
              push_front(&time_vector_tx_enc, t_tx_enc);

              push_front(&time_vector_rx, t_rx);
              push_front(&time_vector_rx_fft, t_rx_fft);
              push_front(&time_vector_rx_demod, t_rx_demod);
              push_front(&time_vector_rx_dec, t_rx_dec);


            }   //trials
#ifdef DEBUG_HARQ
        printf("\n both failed round 0 = %d, both failed round 1 = %d, both failed round 2 = %d, both failed round 3 = %d\n", resend_both[0], resend_both[1], resend_both[2], resend_both[3]);
        printf(" one failed round 0  = %d, one failed round 1  = %d, one failed round 2  = %d, one failed round 3  = %d\n", resend_one[0], resend_one[1], resend_one[2], resend_one[3]);
#endif
      // round_trials[0]: number of code word : goodput the protocol
      double table_tx[time_vector_tx.size];
      totable(table_tx, &time_vector_tx);
      double table_tx_ifft[time_vector_tx_ifft.size];
      totable(table_tx_ifft, &time_vector_tx_ifft);
      double table_tx_mod[time_vector_tx_mod.size];
      totable(table_tx_mod, &time_vector_tx_mod);
      double table_tx_enc[time_vector_tx_enc.size];
      totable(table_tx_enc, &time_vector_tx_enc);

      double table_rx[time_vector_rx.size];
      totable(table_rx, &time_vector_rx);
      double table_rx_fft[time_vector_rx_fft.size];
      totable(table_rx_fft, &time_vector_rx_fft);
      double table_rx_demod[time_vector_rx_demod.size];
      totable(table_rx_demod, &time_vector_rx_demod);
      double table_rx_dec[time_vector_rx_dec.size];
      totable(table_rx_dec, &time_vector_rx_dec);


      // sort table
      qsort (table_tx, time_vector_tx.size, sizeof(double), &compare);
      qsort (table_rx, time_vector_rx.size, sizeof(double), &compare);

      if (dump_table == 1 ) {
        set_component_filelog(USIM);  // file located in /tmp/usim.txt
        int n;
        LOG_F(USIM,"The transmitter raw data: \n");

        for (n=0; n< time_vector_tx.size; n++) {
          printf("%f ", table_tx[n]);
          LOG_F(USIM,"%f ", table_tx[n]);
        }

        LOG_F(USIM,"\n");
        LOG_F(USIM,"The receiver raw data: \n");

        for (n=0; n< time_vector_rx.size; n++) {
          // printf("%f ", table_rx[n]);
          LOG_F(USIM,"%f ", table_rx[n]);
        }

        LOG_F(USIM,"\n");
      }

      double tx_median = table_tx[time_vector_tx.size/2];
      double tx_q1 = table_tx[time_vector_tx.size/4];
      double tx_q3 = table_tx[3*time_vector_tx.size/4];

      double tx_ifft_median = table_tx_ifft[time_vector_tx_ifft.size/2];
      double tx_ifft_q1 = table_tx_ifft[time_vector_tx_ifft.size/4];
      double tx_ifft_q3 = table_tx_ifft[3*time_vector_tx_ifft.size/4];

      double tx_mod_median = table_tx_mod[time_vector_tx_mod.size/2];
      double tx_mod_q1 = table_tx_mod[time_vector_tx_mod.size/4];
      double tx_mod_q3 = table_tx_mod[3*time_vector_tx_mod.size/4];

      double tx_enc_median = table_tx_enc[time_vector_tx_enc.size/2];
      double tx_enc_q1 = table_tx_enc[time_vector_tx_enc.size/4];
      double tx_enc_q3 = table_tx_enc[3*time_vector_tx_enc.size/4];

      double rx_median = table_rx[time_vector_rx.size/2];
      double rx_q1 = table_rx[time_vector_rx.size/4];
      double rx_q3 = table_rx[3*time_vector_rx.size/4];

      double rx_fft_median = table_rx_fft[time_vector_rx_fft.size/2];
      double rx_fft_q1 = table_rx_fft[time_vector_rx_fft.size/4];
      double rx_fft_q3 = table_rx_fft[3*time_vector_rx_fft.size/4];

      double rx_demod_median = table_rx_demod[time_vector_rx_demod.size/2];
      double rx_demod_q1 = table_rx_demod[time_vector_rx_demod.size/4];
      double rx_demod_q3 = table_rx_demod[3*time_vector_rx_demod.size/4];

      double rx_dec_median = table_rx_dec[time_vector_rx_dec.size/2];
      double rx_dec_q1 = table_rx_dec[time_vector_rx_dec.size/4];
      double rx_dec_q3 = table_rx_dec[3*time_vector_rx_dec.size/4];

      double std_phy_proc_tx=0;
      double std_phy_proc_tx_ifft=0;
      double std_phy_proc_tx_mod=0;
      double std_phy_proc_tx_enc=0;

      double std_phy_proc_rx=0;
      double std_phy_proc_rx_fft=0;
      double std_phy_proc_rx_demod=0;
      double std_phy_proc_rx_dec=0;

      if (transmission_mode != 3 && transmission_mode !=4) {
        effective_rate = ((double)(round_trials[0][0]-dci_errors)/((double)round_trials[0][0] + round_trials[0][1] + round_trials[0][2] + round_trials[0][3]));
      }
      else {
        effective_rate = ((double)(round_trials[0][0]-dci_errors)/((double)round_trials[0][0] + round_trials[0][1] + round_trials[0][2] + round_trials[0][3]))+
        ((double)(round_trials[1][0])/((double)round_trials[1][0] + round_trials[1][1] + round_trials[1][2] + round_trials[1][3]));
      }

      /*
      Here we compute throughput per cw based on the formula
      T=P_suc[r1]R(mcs) + sum[r=2 ..r=4] (P_(suc r, fail r-1)*R/r).
      The non-constr formula should not be used, when there are some errors on the last round,
      meaning when not all the packages are finally decoded.
      */

      if (transmission_mode == 3 || transmission_mode == 4) {
        // FOR CW0
        thr_cw0[0] = rate0_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)*(1-((double)errs[0][0]/(double)round_trials[0][0]));
        thr_cw0[1] = (rate0_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)/2)*(((double)errs[0][0] - (double)errs[0][1])/(double)round_trials[0][0]);
        thr_cw0[2] = (rate0_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)/3)*(((double)errs[0][1] - (double)errs[0][2])/(double)round_trials[0][0]);
        thr_cw0[3] = (rate0_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)/4)*(((double)errs[0][2] - (double)errs[0][3])/(double)round_trials[0][0]);
        thr_cw0_tot = (double)thr_cw0[0]+(double)thr_cw0[1]+(double)thr_cw0[2]+(double)thr_cw0[3];
#ifdef PRINT_THROUGHPUT
        printf("rate  %f \n", rate0_init);
        printf("rate*mod_order  %f \n", rate0_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs));
        printf("Probability  %f \n", (1-((double)errs[0][0]/(double)round_trials[0][0])));
        printf("Throughput cw0 sum =  %f \n", thr_cw0_tot);
        printf("Throughput cw0 round 0 =  %f \n", thr_cw0[0]);
        printf("Throughput cw0 round 1 =  %f \n", thr_cw0[1]);
        printf("Throughput cw0 round 2 =  %f \n", thr_cw0[2]);
        printf("Throughput cw0 round 3 =  %f \n", thr_cw0[3]);
        printf("round_trials =  %d, errs[0][0] = %d, round_trials[0][1] = %d, errs[0][1] = %d, round_trials[0][2] = %d, errs[0][2] = %d, \
        round_trials[0][3] = %d, errs[0][3] = %d \n", round_trials[0][0], errs[0][0],round_trials[0][1], errs[0][1], round_trials[0][2], \
        errs[0][2], round_trials[0][3], errs[0][3]);
#endif

        thr_cw1[0] = rate1_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)*(1-((double)errs[1][0]/(double)round_trials[1][0]));
        thr_cw1[1] = (rate1_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)/2)*(((double)errs[1][0] - (double)errs[1][1])/(double)round_trials[1][0]);
        thr_cw1[2] = (rate1_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)/3)*(((double)errs[1][1] - (double)errs[1][2])/(double)round_trials[1][0]);
        thr_cw1[3] = (rate1_init*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)/4)*(((double)errs[1][2] - (double)errs[1][3])/(double)round_trials[1][0]);
        thr_cw1_tot = (double)thr_cw1[0]+(double)thr_cw1[1]+(double)thr_cw1[2]+(double)thr_cw1[3];
#ifdef PRINT_THROUGHPUT
        printf("Throughput cw1 sum =  %f \n", thr_cw1_tot);
        printf("Throughput cw1 round 0 =  %f \n", thr_cw1[0]);
        printf("Throughput cw1 round 1 =  %f \n", thr_cw1[1]);
        printf("Throughput cw1 round 2 =  %f \n", thr_cw1[2]);
        printf("Throughput cw1 round 3 =  %f \n", thr_cw1[3]);
        printf("round_trials[1][0] =  %d, errs[1][0] = %d, round_trials[1][1] = %d, errs[1][1] = %d, round_trials[1][2] = %d, errs[1][2] = %d, \
          round_trials[1][3] = %d, errs[1][3] = %d \n", round_trials[1][0], errs[1][0], round_trials[1][1], errs[1][1], round_trials[1][2], \
          errs[1][2], round_trials[1][3], errs[1][3]);
#endif
      }

#if 0
        thr_cw0_tm4_nonconst = rate[0]*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)* \
        ((double)(round_trials[0][0]-dci_errors)/((double)round_trials[0][0] + round_trials[0][1] + round_trials[0][2] + round_trials[0][3]));
        printf("Throughput cw0 noncnstr =  %f \n", thr_cw0_tm4_nonconst);
#endif
        //FOR CW1
       /*thr_cw1[0] = rate[1]*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)*(1-((double)errs[0][0]/(double)round_trials[0][0])) \
       *(1-((double)errs[1][0]/(double)round_trials[1][0]));
       printf("thr cw1 round 0 = %f\n", thr_cw1[0]);
        thr_cw1[1]=(rate[1]*get_Qm(PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->mcs)/2)*\
        (((double)errs[1][0]-(double)errs[1][1])*((double)round_trials[0][0] - (double)errs[0][1])/((double)round_trials[0][0]*(double)round_trials[1][0]));
        printf("thr cw1 round 1 = %f\n", thr_cw1[1]);
        thr_cw1[2] = (rate[0]*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)/3)*\
       ((double)errs[1][1]-(double)errs[1][2])*((double)round_trials[0][0]-(double)errs[0][2])/((double)round_trials[0][0]*(double)round_trials[1][0]);
        printf("thr cw1 round 2 = %f\n", thr_cw1[2]);
        thr_cw1[3] = (rate[0]*get_Qm(PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->mcs)/4)*\
        ((double)errs[1][2]-(double)errs[1][3])*((double)round_trials[0][0] - (double)errs[0][3])/((double)round_trials[0][0]*(double)round_trials[1][0]);
        resid_errs=
        printf("thr cw1 round 3 = %f\n", thr_cw1[3]);
        thr_cw1_tot =thr_cw1[0]+thr_cw1[1]+thr_cw1[2]+thr_cw1[3];
        printf("Throughput cw1 sum =  %f \n", thr_cw1_tot);
        printf("round_trials =  %d, errs[1][0] = %d, round_trials[1][1] = %d, errs[1][1] = %d, round_trials[1][2] = %d, errs[1][2] = %d, \
          round_trials[1][3] = %d, errs[1][3] = %d \n", round_trials[1][0], errs[1][0],round_trials[1][1], errs[1][1], round_trials[1][2], \
          errs[1][2], round_trials[1][3], errs[1][3]);
      }*/


      printf("\n**********************SNR = %f dB (tx_lev %f, sigma2_dB %f)**************************\n",
             SNR,
             (double)tx_lev_dB+10*log10(PHY_vars_UE->lte_frame_parms.ofdm_symbol_size/(NB_RB*12)),
              sigma2_dB);
      if ((transmission_mode != 3) && (transmission_mode != 4)){
        printf("Errors (%d(%d)/%d %d(%d)/%d %d(%d)/%d %d(%d)/%d), Pe = (%e(%e),%e(%e),%e(%e),%e(%e)),"
                "dci_errors %d/%d, Pe = %e => effective rate %f  (%2.1f%%,%f, %f), normalized delay %f (%f), "
                "throughput stream 0 = %f , throughput stream 1 = %f, system throughput = %f , rate 0 = %f , rate 1 = %f \n",
         errs[0][0],
         errs[1][0],
         round_trials[0][0],
         errs[0][1],
         errs[1][1],
         round_trials[0][0],
         errs[0][2],
         errs[1][2],
         round_trials[0][0],
         errs[0][3],
         errs[1][3],
         round_trials[0][0],
         (double)errs[0][0]/(round_trials[0][0]),
         (double)errs[1][0]/(round_trials[0][0]),
         (double)errs[0][1]/(round_trials[0][0]),
         (double)errs[1][1]/(round_trials[0][0]),
         (double)errs[0][2]/(round_trials[0][0]),
         (double)errs[1][2]/(round_trials[0][0]),
         (double)errs[0][3]/(round_trials[0][0]),
         (double)errs[1][3]/(round_trials[0][0]),
         dci_errors,
         round_trials[0][0],
         (double)dci_errors/(round_trials[0][0]),
         rate[0]*effective_rate,
         100*effective_rate,
         rate[0],
         rate[0]*get_Qm(PHY_vars_UE->dlsch_ue[0][0]->harq_processes[PHY_vars_UE->dlsch_ue[0][0]->current_harq_pid]->mcs),
         (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])+
          4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0])/(double)PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
         (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])
          +4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0]),
         thr_cw0_tot,
         thr_cw1_tot,
         thr_cw0_tot + thr_cw1_tot,
         rate[0],
         rate[1]);
      }else{
        printf("Errors (%d(%d)/%d %d(%d)/%d %d(%d)/%d %d(%d)/%d),"
                "dci_errors %d/%d, thr TB0 = %f , thr TB1 = %f, overall thr = %f , rate 0 = %f , rate 1 = %f \n",
         errs[0][0],
         errs[1][0],
         round_trials[0][0],
         errs[0][1],
         errs[1][1],
         round_trials[0][0],
         errs[0][2],
         errs[1][2],
         round_trials[0][0],
         errs[0][3],
         errs[1][3],
         round_trials[0][0],
         dci_errors,
         round_trials[0][0],
         thr_cw0_tot,
         thr_cw1_tot,
         thr_cw0_tot + thr_cw1_tot,
         rate0_init,
         rate1_init);
      }

      if (print_perf==1) {
        printf("eNB TX function statistics (per 1ms subframe)\n\n");
        std_phy_proc_tx = sqrt((double)PHY_vars_eNB->phy_proc_tx.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                               2)/PHY_vars_eNB->phy_proc_tx.trials - pow((double)PHY_vars_eNB->phy_proc_tx.diff/PHY_vars_eNB->phy_proc_tx.trials/cpu_freq_GHz/1000,2));
        printf("Total PHY proc tx                 :%f us (%d trials)\n",(double)PHY_vars_eNB->phy_proc_tx.diff/PHY_vars_eNB->phy_proc_tx.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->phy_proc_tx.trials);
        printf("|__ Statistcs                           std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus n_dropped: %d packet \n",std_phy_proc_tx, t_tx_max, t_tx_min, tx_median, tx_q1, tx_q3,
               n_tx_dropped);
        std_phy_proc_tx_ifft = sqrt((double)PHY_vars_eNB->ofdm_mod_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                    2)/PHY_vars_eNB->ofdm_mod_stats.trials - pow((double)PHY_vars_eNB->ofdm_mod_stats.diff/PHY_vars_eNB->ofdm_mod_stats.trials/cpu_freq_GHz/1000,2));
        printf("OFDM_mod time                     :%f us (%d trials)\n",(double)PHY_vars_eNB->ofdm_mod_stats.diff/PHY_vars_eNB->ofdm_mod_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->ofdm_mod_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_ifft, tx_ifft_median, tx_ifft_q1, tx_ifft_q3);
        std_phy_proc_tx_mod = sqrt((double)PHY_vars_eNB->dlsch_modulation_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_eNB->dlsch_modulation_stats.trials - pow((double)PHY_vars_eNB->dlsch_modulation_stats.diff/PHY_vars_eNB->dlsch_modulation_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH modulation time             :%f us (%d trials)\n",(double)PHY_vars_eNB->dlsch_modulation_stats.diff/PHY_vars_eNB->dlsch_modulation_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_eNB->dlsch_modulation_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_mod, tx_mod_median, tx_mod_q1, tx_mod_q3);
        printf("DLSCH scrambling time             :%f us (%d trials)\n",(double)PHY_vars_eNB->dlsch_scrambling_stats.diff/PHY_vars_eNB->dlsch_scrambling_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_eNB->dlsch_scrambling_stats.trials);
        std_phy_proc_tx_enc = sqrt((double)PHY_vars_eNB->dlsch_encoding_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_eNB->dlsch_encoding_stats.trials - pow((double)PHY_vars_eNB->dlsch_encoding_stats.diff/PHY_vars_eNB->dlsch_encoding_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH encoding time               :%f us (%d trials)\n",(double)PHY_vars_eNB->dlsch_encoding_stats.diff/PHY_vars_eNB->dlsch_encoding_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_eNB->dlsch_modulation_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_enc, tx_enc_median, tx_enc_q1, tx_enc_q3);
        printf("|__ DLSCH turbo encoding time         :%f us (%d trials)\n",
               ((double)PHY_vars_eNB->dlsch_turbo_encoding_stats.trials/PHY_vars_eNB->dlsch_encoding_stats.trials)*(double)
               PHY_vars_eNB->dlsch_turbo_encoding_stats.diff/PHY_vars_eNB->dlsch_turbo_encoding_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->dlsch_turbo_encoding_stats.trials);
        printf("|__ DLSCH rate-matching time          :%f us (%d trials)\n",
               ((double)PHY_vars_eNB->dlsch_rate_matching_stats.trials/PHY_vars_eNB->dlsch_encoding_stats.trials)*(double)
               PHY_vars_eNB->dlsch_rate_matching_stats.diff/PHY_vars_eNB->dlsch_rate_matching_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->dlsch_rate_matching_stats.trials);
        printf("|__ DLSCH sub-block interleaving time :%f us (%d trials)\n",
               ((double)PHY_vars_eNB->dlsch_interleaving_stats.trials/PHY_vars_eNB->dlsch_encoding_stats.trials)*(double)
               PHY_vars_eNB->dlsch_interleaving_stats.diff/PHY_vars_eNB->dlsch_interleaving_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->dlsch_interleaving_stats.trials);

        printf("\n\nUE RX function statistics (per 1ms subframe)\n\n");
        std_phy_proc_rx = sqrt((double)PHY_vars_UE->phy_proc_rx.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                               2)/PHY_vars_UE->phy_proc_rx.trials - pow((double)PHY_vars_UE->phy_proc_rx.diff/PHY_vars_UE->phy_proc_rx.trials/cpu_freq_GHz/1000,2));
        printf("Total PHY proc rx                                   :%f us (%d trials)\n",(double)PHY_vars_UE->phy_proc_rx.diff/PHY_vars_UE->phy_proc_rx.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->phy_proc_rx.trials*2/3);
        printf("|__Statistcs                                            std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus n_dropped: %d packet \n", std_phy_proc_rx, t_rx_max, t_rx_min, rx_median,
               rx_q1, rx_q3, n_rx_dropped);
        std_phy_proc_rx_fft = sqrt((double)PHY_vars_UE->ofdm_demod_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_UE->ofdm_demod_stats.trials - pow((double)PHY_vars_UE->ofdm_demod_stats.diff/PHY_vars_UE->ofdm_demod_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH OFDM demodulation and channel_estimation time :%f us (%d trials)\n",(nsymb)*(double)PHY_vars_UE->ofdm_demod_stats.diff/PHY_vars_UE->ofdm_demod_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->ofdm_demod_stats.trials*2/3);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_fft, rx_fft_median, rx_fft_q1, rx_fft_q3);
        printf("|__ DLSCH rx dft                                        :%f us (%d trials)\n",
               (nsymb*PHY_vars_UE->lte_frame_parms.nb_antennas_rx)*(double)PHY_vars_UE->rx_dft_stats.diff/PHY_vars_UE->rx_dft_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_UE->rx_dft_stats.trials*2/3);
        printf("|__ DLSCH channel estimation time                       :%f us (%d trials)\n",
               (4.0)*(double)PHY_vars_UE->dlsch_channel_estimation_stats.diff/PHY_vars_UE->dlsch_channel_estimation_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_UE->dlsch_channel_estimation_stats.trials*2/3);
        printf("|__ DLSCH frequency offset estimation time              :%f us (%d trials)\n",
               (4.0)*(double)PHY_vars_UE->dlsch_freq_offset_estimation_stats.diff/PHY_vars_UE->dlsch_freq_offset_estimation_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->dlsch_freq_offset_estimation_stats.trials*2/3);
        printf("DLSCH rx pdcch                                       :%f us (%d trials)\n",(double)PHY_vars_UE->dlsch_rx_pdcch_stats.diff/PHY_vars_UE->dlsch_rx_pdcch_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->dlsch_rx_pdcch_stats.trials);
        std_phy_proc_rx_demod = sqrt((double)PHY_vars_UE->dlsch_llr_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                     2)/PHY_vars_UE->dlsch_llr_stats.trials - pow((double)PHY_vars_UE->dlsch_llr_stats.diff/PHY_vars_UE->dlsch_llr_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH Channel Compensation and LLR generation time  :%f us (%d trials)\n",(3)*(double)PHY_vars_UE->dlsch_llr_stats.diff/PHY_vars_UE->dlsch_llr_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->dlsch_llr_stats.trials/3);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_demod, rx_demod_median, rx_demod_q1, rx_demod_q3);
        printf("DLSCH unscrambling time                             :%f us (%d trials)\n",(double)PHY_vars_UE->dlsch_unscrambling_stats.diff/PHY_vars_UE->dlsch_unscrambling_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_UE->dlsch_unscrambling_stats.trials);
        std_phy_proc_rx_dec = sqrt((double)PHY_vars_UE->dlsch_decoding_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_UE->dlsch_decoding_stats.trials - pow((double)PHY_vars_UE->dlsch_decoding_stats.diff/PHY_vars_UE->dlsch_decoding_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH Decoding time (%02.2f Mbit/s, avg iter %1.2f)    :%f us (%d trials, max %f)\n",
               PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS/1000.0,(double)avg_iter[0]/iter_trials[0],
               (double)PHY_vars_UE->dlsch_decoding_stats.diff/PHY_vars_UE->dlsch_decoding_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_UE->dlsch_decoding_stats.trials,
               (double)PHY_vars_UE->dlsch_decoding_stats.max/cpu_freq_GHz/1000.0);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_dec, rx_dec_median, rx_dec_q1, rx_dec_q3);
        printf("|__ DLSCH Rate Unmatching                               :%f us (%d trials)\n",
               (double)PHY_vars_UE->dlsch_rate_unmatching_stats.diff/PHY_vars_UE->dlsch_rate_unmatching_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_UE->dlsch_rate_unmatching_stats.trials);
        printf("|__ DLSCH Turbo Decoding(%d bits)                       :%f us (%d trials)\n",
               PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->Cminus ? PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->Kminus : PHY_vars_UE->dlsch_ue[0][0]->harq_processes[0]->Kplus,
               (double)PHY_vars_UE->dlsch_turbo_decoding_stats.diff/PHY_vars_UE->dlsch_turbo_decoding_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_UE->dlsch_turbo_decoding_stats.trials);
        printf("    |__ init                                            %f us (cycles/iter %f, %d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_init_stats.diff/PHY_vars_UE->dlsch_tc_init_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_init_stats.diff/PHY_vars_UE->dlsch_tc_init_stats.trials/((double)avg_iter[0]/iter_trials[0]),
               PHY_vars_UE->dlsch_tc_init_stats.trials);
        printf("    |__ alpha                                           %f us (cycles/iter %f, %d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_alpha_stats.diff/PHY_vars_UE->dlsch_tc_alpha_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_alpha_stats.diff/PHY_vars_UE->dlsch_tc_alpha_stats.trials*2,
               PHY_vars_UE->dlsch_tc_alpha_stats.trials);
        printf("    |__ beta                                            %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_beta_stats.diff/PHY_vars_UE->dlsch_tc_beta_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_beta_stats.diff/PHY_vars_UE->dlsch_tc_beta_stats.trials*2,
               PHY_vars_UE->dlsch_tc_beta_stats.trials);
        printf("    |__ gamma                                           %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_gamma_stats.diff/PHY_vars_UE->dlsch_tc_gamma_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_gamma_stats.diff/PHY_vars_UE->dlsch_tc_gamma_stats.trials*2,
               PHY_vars_UE->dlsch_tc_gamma_stats.trials);
        printf("    |__ ext                                             %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_ext_stats.diff/PHY_vars_UE->dlsch_tc_ext_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_ext_stats.diff/PHY_vars_UE->dlsch_tc_ext_stats.trials*2,
               PHY_vars_UE->dlsch_tc_ext_stats.trials);
        printf("    |__ intl1                                           %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_intl1_stats.diff/PHY_vars_UE->dlsch_tc_intl1_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_intl1_stats.diff/PHY_vars_UE->dlsch_tc_intl1_stats.trials,
               PHY_vars_UE->dlsch_tc_intl1_stats.trials);
        printf("    |__ intl2+HD+CRC                                    %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_UE->dlsch_tc_intl2_stats.diff/PHY_vars_UE->dlsch_tc_intl2_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_UE->dlsch_tc_intl2_stats.diff/PHY_vars_UE->dlsch_tc_intl2_stats.trials,
               PHY_vars_UE->dlsch_tc_intl2_stats.trials);
      }

      if ((transmission_mode != 3) && (transmission_mode != 4)) {
        fprintf(bler_fd,"%f;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d\n",
                SNR,
                mcs1,
                PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
                rate[0],
                errs[0][0],
                round_trials[0][0],
                errs[0][1],
                round_trials[0][1],
                errs[0][2],
                round_trials[0][2],
                errs[0][3],
                round_trials[0][3],
                dci_errors);
      }
      else {
        fprintf(bler_fd,"%f;%d;%d;%d;%d;%f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f\n",
                SNR,
                mcs1,
                mcs2,
                tbs0_init,
                tbs1_init,
                rate0_init,
                rate1_init,
                errs[0][0],
                errs[1][0],
                round_trials[0][0],
                round_trials[1][0],
                errs[0][1],
                errs[1][1],
                round_trials[0][1],
                round_trials[1][1],
                errs[0][2],
                errs[1][2],
                round_trials[0][2],
                round_trials[1][2],
                errs[0][3],
                errs[1][3],
                round_trials[0][3],
                round_trials[1][3],
                thr_cw0[0],
                thr_cw1[0],
                thr_cw0[0]+thr_cw1[0],
                thr_cw0[1],
                thr_cw1[1],
                thr_cw0[1]+thr_cw1[1],
                thr_cw0[2],
                thr_cw1[2],
                thr_cw0[2]+thr_cw1[2],
                thr_cw0[3],
                thr_cw1[3],
                thr_cw0[3]+thr_cw1[3],
                thr_cw0[0]+thr_cw0[1]+thr_cw0[2]+thr_cw0[3]+thr_cw1[0]+thr_cw1[1]+thr_cw1[2]+ thr_cw1[3]);
            }


      if(abstx){ //ABSTRACTION
  if ((transmission_mode != 3)&& (transmission_mode != 4)) {
    blerr[0][0] = (double)errs[0][0]/(round_trials[0][0]);

    if(num_rounds>1){
      blerr[0][1] = (double)errs[0][1]/(round_trials[0][1]);

      blerr[0][2] = (double)errs[0][2]/(round_trials[0][2]);

      blerr[0][3] = (double)errs[0][3]/(round_trials[0][3]);

      fprintf(csv_fd,"%e;%e;%e;%e;\n",blerr[0][0],blerr[0][1],blerr[0][2],blerr[0][3]);
    }
    else {
      fprintf(csv_fd,"%e;\n",blerr[0][0]);
    }
  }
  else {
    blerr[0][0] = (double)errs[0][0]/(round_trials[0][0]);
    blerr[1][0] = (double)errs[1][0]/(round_trials[0][0]);
    if(num_rounds>1){
      blerr[0][1] = (double)errs[0][1]/(round_trials[0][1]);
      blerr[1][1] = (double)errs[1][1]/(round_trials[1][1]);
      blerr[0][2] = (double)errs[0][2]/(round_trials[0][2]);
      blerr[1][2] = (double)errs[1][2]/(round_trials[1][2]);
      blerr[0][3] = (double)errs[0][3]/(round_trials[0][3]);
      blerr[1][3] = (double)errs[1][3]/(round_trials[1][3]);
      fprintf(csv_fd,"%e;%e;%e;%e;%e;%e;%e;%e;\n",blerr[0][0],blerr[1][0],blerr[0][1],blerr[1][1],blerr[0][2],blerr[1][2], blerr[0][3], blerr[1][3]);
    }
    else {
      fprintf(csv_fd,"%e,%e;\n",blerr[0][0], blerr[1][0]);
    }
  }
     }

      if ( (test_perf != 0) && (100 * effective_rate > test_perf )) {
  //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; err0; trials0; err1; trials1; err2; trials2; err3; trials3; dci_err\n");
  if ((transmission_mode != 3) && (transmission_mode != 4)) {
    fprintf(time_meas_fd,"%f;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
      SNR,
      mcs1,
      PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      rate[0],
      errs[0][0],
      round_trials[0][0],
      errs[0][1],
      round_trials[0][1],
      errs[0][2],
      round_trials[0][2],
      errs[0][3],
      round_trials[0][3],
      dci_errors);

    //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; DL_DECOD_ITER; err0; trials0; err1; trials1; err2; trials2; err3; trials3; PE; dci_err;PE;ND;\n");
    fprintf(time_meas_fd,"%f;%d;%d;%f; %2.1f%%;%f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%e;%e;%e;%e;%d;%d;%e;%f;%f;",
      SNR,
      mcs1,
      PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      rate[0]*effective_rate,
      100*effective_rate,
      rate[0],
      (double)avg_iter[0]/iter_trials[0],
      errs[0][0],
      round_trials[0],
      errs[0][1],
      round_trials[1],
      errs[0][2],
      round_trials[2],
      errs[0][3],
      round_trials[3],
      (double)errs[0][0]/(round_trials[0][0]),
      (double)errs[0][1]/(round_trials[0][0]),
      (double)errs[0][2]/(round_trials[0][0]),
      (double)errs[0][3]/(round_trials[0][0]),
      dci_errors,
      round_trials[0],
      (double)dci_errors/(round_trials[0][0]),
      (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])+4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0])/(double)PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])+4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0]));
  }
  else {
    fprintf(time_meas_fd,"%f;%d;%d;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
      SNR,
      mcs1,mcs2,
      PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->TBS,
      rate[0],
      errs[0][0],
      round_trials[0][0],
      errs[0][1],
      round_trials[0][1],
      errs[0][2],
      round_trials[0][2],
      errs[0][3],
      round_trials[0][3],
      dci_errors);

    //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; DL_DECOD_ITER; err0; trials0; err1; trials1; err2; trials2; err3; trials3; PE; dci_err;PE;ND;\n");
    fprintf(time_meas_fd,"%f;%d;%d;%d;%d;%f;%2.1f;%f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%e;%e;%e;%e;%d;%d;%e;%f;%f;",
      SNR,
      mcs1,mcs2,
      PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      PHY_vars_eNB->dlsch_eNB[0][1]->harq_processes[0]->TBS,
      rate[0]*effective_rate,
      100*effective_rate,
      rate[0],
      (double)avg_iter[0]/iter_trials[0],
      errs[0][0],
      round_trials[0][0],
      errs[0][1],
      round_trials[0][1],
      errs[0][2],
      round_trials[0][2],
      errs[0][3],
      round_trials[3],
      (double)errs[0][0]/(round_trials[0][0]),
      (double)errs[0][1]/(round_trials[0][0]),
      (double)errs[0][2]/(round_trials[0][0]),
      (double)errs[0][3]/(round_trials[0][0]),
      dci_errors,
      round_trials[0][0],
      (double)dci_errors/(round_trials[0][0]),
      (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])+4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0])/(double)PHY_vars_eNB->dlsch_eNB[0][0]->harq_processes[0]->TBS,
      (1.0*(round_trials[0][0]-errs[0][0])+2.0*(round_trials[0][1]-errs[0][1])+3.0*(round_trials[0][2]-errs[0][2])+4.0*(round_trials[0][3]-errs[0][3]))/((double)round_trials[0][0]));
  }
  //fprintf(time_meas_fd,"eNB_PROC_TX(%d); OFDM_MOD(%d); DL_MOD(%d); DL_SCR(%d); DL_ENC(%d); UE_PROC_RX(%d); OFDM_DEMOD_CH_EST(%d); RX_PDCCH(%d); CH_COMP_LLR(%d); DL_USCR(%d); DL_DECOD(%d);\n",
  fprintf(time_meas_fd,"%d; %d; %d; %d; %d; %d; %d; %d; %d; %d; %d;",
    PHY_vars_eNB->phy_proc_tx.trials,
    PHY_vars_eNB->ofdm_mod_stats.trials,
    PHY_vars_eNB->dlsch_modulation_stats.trials,
    PHY_vars_eNB->dlsch_scrambling_stats.trials,
    PHY_vars_eNB->dlsch_encoding_stats.trials,
    PHY_vars_UE->phy_proc_rx.trials,
    PHY_vars_UE->ofdm_demod_stats.trials,
    PHY_vars_UE->dlsch_rx_pdcch_stats.trials,
    PHY_vars_UE->dlsch_llr_stats.trials,
    PHY_vars_UE->dlsch_unscrambling_stats.trials,
    PHY_vars_UE->dlsch_decoding_stats.trials
    );
  fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;",
    get_time_meas_us(&PHY_vars_eNB->phy_proc_tx),
    get_time_meas_us(&PHY_vars_eNB->ofdm_mod_stats),
    get_time_meas_us(&PHY_vars_eNB->dlsch_modulation_stats),
    get_time_meas_us(&PHY_vars_eNB->dlsch_scrambling_stats),
    get_time_meas_us(&PHY_vars_eNB->dlsch_encoding_stats),
    get_time_meas_us(&PHY_vars_UE->phy_proc_rx),
    nsymb*get_time_meas_us(&PHY_vars_UE->ofdm_demod_stats),
    get_time_meas_us(&PHY_vars_UE->dlsch_rx_pdcch_stats),
    3*get_time_meas_us(&PHY_vars_UE->dlsch_llr_stats),
    get_time_meas_us(&PHY_vars_UE->dlsch_unscrambling_stats),
    get_time_meas_us(&PHY_vars_UE->dlsch_decoding_stats)
    );
  //fprintf(time_meas_fd,"eNB_PROC_TX_STD;eNB_PROC_TX_MAX;eNB_PROC_TX_MIN;eNB_PROC_TX_MED;eNB_PROC_TX_Q1;eNB_PROC_TX_Q3;eNB_PROC_TX_DROPPED;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%d;", std_phy_proc_tx, t_tx_max, t_tx_min, tx_median, tx_q1, tx_q3, n_tx_dropped);

  //fprintf(time_meas_fd,"IFFT;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_ifft, tx_ifft_median, tx_ifft_q1, tx_ifft_q3);

  //fprintf(time_meas_fd,"MOD;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_mod, tx_mod_median, tx_mod_q1, tx_mod_q3);

  //fprintf(time_meas_fd,"ENC;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_enc, tx_enc_median, tx_enc_q1, tx_enc_q3);


  //fprintf(time_meas_fd,"UE_PROC_RX_STD;UE_PROC_RX_MAX;UE_PROC_RX_MIN;UE_PROC_RX_MED;UE_PROC_RX_Q1;UE_PROC_RX_Q3;UE_PROC_RX_DROPPED;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%d;", std_phy_proc_rx, t_rx_max, t_rx_min, rx_median, rx_q1, rx_q3, n_rx_dropped);

  //fprintf(time_meas_fd,"FFT;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_rx_fft, rx_fft_median, rx_fft_q1, rx_fft_q3);

  //fprintf(time_meas_fd,"DEMOD;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_rx_demod,rx_demod_median, rx_demod_q1, rx_demod_q3);

  //fprintf(time_meas_fd,"DEC;\n");
  fprintf(time_meas_fd,"%f;%f;%f;%f\n", std_phy_proc_rx_dec, rx_dec_median, rx_dec_q1, rx_dec_q3);


  /*
    fprintf(time_meas_fd,"%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
    PHY_vars_eNB->phy_proc_tx.trials,
    PHY_vars_eNB->ofdm_mod_stats.trials,
    PHY_vars_eNB->dlsch_modulation_stats.trials,
    PHY_vars_eNB->dlsch_scrambling_stats.trials,
    PHY_vars_eNB->dlsch_encoding_stats.trials,
    PHY_vars_UE->phy_proc_rx.trials,
    PHY_vars_UE->ofdm_demod_stats.trials,
    PHY_vars_UE->dlsch_rx_pdcch_stats.trials,
    PHY_vars_UE->dlsch_llr_stats.trials,
    PHY_vars_UE->dlsch_unscrambling_stats.trials,
    PHY_vars_UE->dlsch_decoding_stats.trials);
    */
    printf("[passed] effective rate : %f  (%2.1f%%,%f)): log and break \n",rate[0]*effective_rate, 100*effective_rate, rate[0]);
    break;
      } else if (test_perf !=0 ){
  printf("[continue] effective rate : %f  (%2.1f%%,%f)): increase snr \n",rate[0]*effective_rate, 100*effective_rate, rate[0]);
      }
  if (abstx == 1) {
    if ((rx_type==rx_IC_dual_stream) || (rx_type==rx_standard)) {
      if (((double)errs[0][0]/(round_trials[0][0]))<1e-2 && ((double)errs[1][0]/(round_trials[1][0]))<1e-2)
      if (((double)errs[0][0]/(round_trials[0][0]))<1e-2 && ((double)errs[1][0]/(round_trials[1][0]))<1e-2)
      break;
    }
    else{
      if (((double)errs[0][0]/(round_trials[0][0]))<1e-2)
      break;

      }
  }
  else {

     if ((rx_type==rx_IC_dual_stream) || (rx_type==rx_standard) || (rx_type==rx_SIC_dual_stream)) {
      if (((double)errs[0][0]/(round_trials[0][0]))<1e-3 && ((double)errs[1][0]/(round_trials[1][0]))<1e-3)
      break;
    }
    else{
      if (((double)errs[0][0]/(round_trials[0][0]))<1e-3)
      break;

      }

  }


      if (n_frames==2)
  break;

      }// SNR

  } //ch_realization

  fclose(bler_fd);

  if (test_perf !=0)
    fclose (time_meas_fd);

  //fprintf(tikz_fd,"};\n");
  //fclose(tikz_fd);

  if (input_trch_file==1)
    fclose(input_trch_fd);

  if (input_file==1)
    fclose(input_fd);

  if(abstx) { // ABSTRACTION
    fprintf(csv_fd,"];");
    fclose(csv_fd);
  }

  if (uncoded_ber_bit)
    free(uncoded_ber_bit);

  uncoded_ber_bit = NULL;

  for (k=0; k<n_users; k++) {
    free(input_buffer0[k]);
    free(input_buffer1[k]);
    input_buffer0[k]=NULL;
    input_buffer1[k]=NULL;
  }

  printf("Freeing dlsch structures\n");

  for (i=0; i<2; i++) {
    printf("eNB %d\n",i);
    free_eNB_dlsch(PHY_vars_eNB->dlsch_eNB[0][i]);
    printf("UE %d\n",i);
    free_ue_dlsch(PHY_vars_UE->dlsch_ue[0][i]);
  }


  printf("Freeing channel I/O\n");

  for (i=0; i<2; i++) {
    free(s_re[i]);
    free(s_im[i]);
    free(r_re[i]);
    free(r_im[i]);
  }

  free(s_re);
  free(s_im);
  free(r_re);
  free(r_im);

  //  lte_sync_time_free();

  //  printf("[MUMIMO] mcs %d, mcsi %d, offset %d, bler %f\n",mcs,mcs_i,offset_mumimo_llr_drange_fix,((double)errs[0])/((double)round_trials[0]));

  return(0);
}


