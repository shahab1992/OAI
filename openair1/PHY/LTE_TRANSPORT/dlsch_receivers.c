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

/*! \file PHY/LTE_TRANSPORT/dlsch_receivers.c
 * \brief Top-level routines for receive algorithms of the PDSCH physical channel from 36-211, V8.6 2009-03
 * \author R. Knopp, F. Kaltenberger,A. Bhamri, S. Wagner, X Jiang
 * \date 2011
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr,florian.kaltenberger@eurecom.fr,ankit.bhamri@eurecom.fr,sebastien.aubert@eurecom.fr, sebastian.wagner@eurecom.fr
 * \note
 * \warning
 */

#include "PHY/extern.h"
#include "defs.h"
#include "extern.h"
#include "PHY/sse_intrin.h"
#include "T.h"

#define ant_idx(tx, rx) ((tx<<1) + rx)

/**
   @brief Receiver for single-antenna port transmission scheme
   @param
 */
int32_t rx_pdsch_sap (LTE_UE_COMMON  *common_vars,
                      LTE_UE_PDSCH   **pdsch_vars,
                      LTE_UE_DLSCH_t **dlsch,
                      LTE_DL_FRAME_PARMS *frame_parms,
                      PHY_MEASUREMENTS *measurements,
                      uint32_t *rballoc,
                      uint8_t eNB_id,
                      uint8_t subframe,
                      uint8_t symbol,
                      uint8_t first_symbol_flag,
                      uint8_t harq_pid,
                      uint8_t high_speed_flag)
{
    LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch[0]->harq_processes[harq_pid];
    uint16_t nb_rb;
    uint8_t num_rx_ant = frame_parms->nb_antennas_rx;
    int32_t avg_channel_power[num_rx_ant];
    int32_t avg_channel_power_max = 0;
    uint32_t aarx;

    // Extract symbols and channel estimates on the PDSCH resource elements
    nb_rb = dlsch_extract_rbs_single(common_vars->common_vars_rx_data_per_thread[subframe&0x1].rxdataF,
                                     common_vars->common_vars_rx_data_per_thread[subframe&0x1].dl_ch_estimates[eNB_id],
                                     pdsch_vars[eNB_id]->rxdataF_ext,
                                     pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                                     dlsch0_harq->pmi_alloc,
                                     pdsch_vars[eNB_id]->pmi_ext,
                                     rballoc,
                                     symbol,
                                     subframe,
                                     high_speed_flag,
                                     frame_parms);

    // Scale channel estimates according to configured power ratio rho_a/rho_b
    dlsch_scale_channel(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                        frame_parms,
                        dlsch,
                        symbol,
                        nb_rb);

    // Compute shift parameter for optimal dynamic range
    // based on the first OFDM symbol received
    if (first_symbol_flag==1)
    {
        dlsch_channel_level(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                            frame_parms,
                            avg_channel_power,
                            symbol,
                            nb_rb);

        // Calculate mas of channel power over links
        for (aarx=0; aarx < num_rx_ant; aarx++)
        {
            avg_channel_power_max = cmax(avg_channel_power_max, avg_channel_power[ant_idx(0,aarx)]);
        }

        // Compute optimal shift
        pdsch_vars[eNB_id]->log2_maxh = (log2_approx(avg_channel_power_max)/2) + 1;
    }

    // Compute matched filter output
    dlsch_channel_compensation(pdsch_vars[eNB_id]->rxdataF_ext,
                               pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                               pdsch_vars[eNB_id]->dl_ch_mag0,
                               pdsch_vars[eNB_id]->dl_ch_magb0,
                               pdsch_vars[eNB_id]->rxdataF_comp0,
                               NULL,
                               frame_parms,
                               symbol,
                               first_symbol_flag,
                               dlsch0_harq->Qm,
                               nb_rb,
                               pdsch_vars[eNB_id]->log2_maxh,
                               measurements); // log2_maxh+I0_shift


    // Combine different links from single tx antenna to receive antennas
    if (num_rx_ant > 1)
    {
        dlsch_detection_mrc(frame_parms,
                            pdsch_vars[eNB_id]->rxdataF_comp0,
                            NULL,
                            NULL,
                            NULL,
                            pdsch_vars[eNB_id]->dl_ch_mag0,
                            pdsch_vars[eNB_id]->dl_ch_magb0,
                            NULL,
                            NULL,
                            symbol,
                            nb_rb,
                            0);
    }

    // LLR computation
    switch (dlsch0_harq->Qm)
    {
    case 2:
    {
        dlsch_qpsk_llr(frame_parms,
                       pdsch_vars[eNB_id]->rxdataF_comp0,
                       pdsch_vars[eNB_id]->llr[0],
                       symbol,
                       first_symbol_flag,
                       nb_rb,
                       adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,2,subframe,symbol),
                       pdsch_vars[eNB_id]->llr128,
                       0);
        break;
    }
    case 4:
    {
        dlsch_16qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,4,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
        break;
    }
    case 6:
    {
        dlsch_64qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        pdsch_vars[eNB_id]->dl_ch_magb0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,6,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
    }
    default:
    {
        LOG_W(PHY, "rx_pdsch_sap : Unknown modulation order!!!!\n");
        return(-1);
    }
    }

    // Everything went well...
    return(0);
}

/**
   @brief Receiver for single-antenna port transmission scheme
   @param
 */
int32_t rx_pdsch_txdiv (LTE_UE_COMMON  *common_vars,
                        LTE_UE_PDSCH   **pdsch_vars,
                        LTE_UE_DLSCH_t **dlsch,
                        LTE_DL_FRAME_PARMS *frame_parms,
                        PHY_MEASUREMENTS *measurements,
                        uint32_t *rballoc,
                        uint8_t eNB_id,
                        uint8_t subframe,
                        uint8_t symbol,
                        uint8_t first_symbol_flag,
                        uint8_t harq_pid,
                        uint8_t high_speed_flag)
{
    LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch[0]->harq_processes[harq_pid];
    uint16_t nb_rb;
    uint8_t num_rx_ant = frame_parms->nb_antennas_rx;
    uint8_t num_tx_ant = frame_parms->nb_antenna_ports_eNB;
    int32_t avg_channel_power[num_rx_ant];
    int32_t avg_channel_power_max = 0;
    uint32_t aarx;
    uint32_t aatx;
    
    // Extract symbols and channel estimates on the PDSCH resource elements
    nb_rb = dlsch_extract_rbs_dual(common_vars->common_vars_rx_data_per_thread[subframe&0x1].rxdataF,
                                   common_vars->common_vars_rx_data_per_thread[subframe&0x1].dl_ch_estimates[eNB_id],
                                   pdsch_vars[eNB_id]->rxdataF_ext,
                                   pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                                   dlsch0_harq->pmi_alloc,
                                   pdsch_vars[eNB_id]->pmi_ext,
                                   rballoc,
                                   symbol,
                                   subframe,
                                   high_speed_flag,
                                   frame_parms,
                                   dlsch0_harq->mimo_mode);

    // Scale channel estimates according to configured power ratio rho_a/rho_b
    dlsch_scale_channel(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                        frame_parms,
                        dlsch,
                        symbol,
                        nb_rb);

    // Compute shift parameter for optimal dynamic range
    // based on the first OFDM symbol received
    if (first_symbol_flag==1)
    {
        dlsch_channel_level(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                            frame_parms,
                            avg_channel_power,
                            symbol,
                            nb_rb);

        // Calculate mas of channel power over links
        for (aatx=0; aatx < num_tx_ant; aatx++)
        {
            for (aarx=0; aarx < num_rx_ant; aarx++)
            {
                avg_channel_power_max = cmax(avg_channel_power_max, avg_channel_power[ant_idx(aatx,aarx)]);
            }
        }

        // Compute optimal shift
        pdsch_vars[eNB_id]->log2_maxh = (log2_approx(avg_channel_power_max)/2) + 1;
    }

    // Compute matched filter output
    dlsch_channel_compensation(pdsch_vars[eNB_id]->rxdataF_ext,
                               pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                               pdsch_vars[eNB_id]->dl_ch_mag0,
                               pdsch_vars[eNB_id]->dl_ch_magb0,
                               pdsch_vars[eNB_id]->rxdataF_comp0,
                               NULL,
                               frame_parms,
                               symbol,
                               first_symbol_flag,
                               dlsch0_harq->Qm,
                               nb_rb,
                               pdsch_vars[eNB_id]->log2_maxh,
                               measurements); // log2_maxh+I0_shift


    // Combine different links from single tx antenna to receive antennas
    if (num_rx_ant > 1)
    {
        dlsch_detection_mrc(frame_parms,
                            pdsch_vars[eNB_id]->rxdataF_comp0,
                            NULL,
                            NULL,
                            NULL,
                            pdsch_vars[eNB_id]->dl_ch_mag0,
                            pdsch_vars[eNB_id]->dl_ch_magb0,
                            NULL,
                            NULL,
                            symbol,
                            nb_rb,
                            0);
    }

    // Combine for space-frequency block coding
    dlsch_alamouti(frame_parms,
                   pdsch_vars[eNB_id]->rxdataF_comp0,
                   pdsch_vars[eNB_id]->dl_ch_mag0,
                   pdsch_vars[eNB_id]->dl_ch_magb0,
                   symbol,
                   nb_rb);

    // LLR computation
    switch (dlsch0_harq->Qm)
    {
    case 2:
    {
        dlsch_qpsk_llr(frame_parms,
                       pdsch_vars[eNB_id]->rxdataF_comp0,
                       pdsch_vars[eNB_id]->llr[0],
                       symbol,
                       first_symbol_flag,
                       nb_rb,
                       adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,2,subframe,symbol),
                       pdsch_vars[eNB_id]->llr128,
                       0);
        break;
    }
    case 4:
    {
        dlsch_16qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,4,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
        break;
    }
    case 6:
    {
        dlsch_64qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        pdsch_vars[eNB_id]->dl_ch_magb0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,6,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
    }
    default:
    {
        LOG_W(PHY, "rx_pdsch_sap : Unknown modulation order!!!!\n");
        return(-1);
    }
    }

    // Everything went well...
    return(0);
}

/**
   @brief Receiver for SU-MIMO spatial mutliplexing
   @param
 */
int32_t rx_pdsch_sumimo (LTE_UE_COMMON  *common_vars,
                         LTE_UE_PDSCH   **pdsch_vars,
                         LTE_UE_DLSCH_t **dlsch,
                         LTE_DL_FRAME_PARMS *frame_parms,
                         PHY_MEASUREMENTS *measurements,
                         uint32_t *rballoc,
                         uint8_t eNB_id,
                         uint8_t subframe,
                         uint8_t symbol,
                         uint8_t first_symbol_flag,
                         uint8_t harq_pid,
                         uint8_t high_speed_flag,
                         RX_type_t rx_type)
{
    LTE_DL_UE_HARQ_t *dlsch0_harq = dlsch[0]->harq_processes[harq_pid];
    uint16_t nb_rb;
    uint8_t num_rx_ant = frame_parms->nb_antennas_rx;
    uint8_t num_tx_ant = frame_parms->nb_antenna_ports_eNB;
    int32_t avg_channel_power[num_rx_ant];
    int32_t avg_channel_power_max = 0;
    uint32_t aarx;
    uint32_t aatx;
    
    // Extract symbols and channel estimates on the PDSCH resource elements
    nb_rb = dlsch_extract_rbs_dual(common_vars->common_vars_rx_data_per_thread[subframe&0x1].rxdataF,
                                   common_vars->common_vars_rx_data_per_thread[subframe&0x1].dl_ch_estimates[eNB_id],
                                   pdsch_vars[eNB_id]->rxdataF_ext,
                                   pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                                   dlsch0_harq->pmi_alloc,
                                   pdsch_vars[eNB_id]->pmi_ext,
                                   rballoc,
                                   symbol,
                                   subframe,
                                   high_speed_flag,
                                   frame_parms,
                                   dlsch0_harq->mimo_mode);

    // Scale channel estimates according to configured power ratio rho_a/rho_b
    dlsch_scale_channel(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                        frame_parms,
                        dlsch,
                        symbol,
                        nb_rb);

    // Compute shift parameter for optimal dynamic range
    // based on the first OFDM symbol received
    if (first_symbol_flag==1)
    {
        dlsch_channel_level(pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                            frame_parms,
                            avg_channel_power,
                            symbol,
                            nb_rb);

        // Calculate mas of channel power over links
        for (aatx=0; aatx < num_tx_ant; aatx++)
        {
            for (aarx=0; aarx < num_rx_ant; aarx++)
            {
                avg_channel_power_max = cmax(avg_channel_power_max, avg_channel_power[ant_idx(aatx,aarx)]);
            }
        }

        // Compute optimal shift
        pdsch_vars[eNB_id]->log2_maxh = (log2_approx(avg_channel_power_max)/2) + 1;
    }

    // Compute matched filter output
    dlsch_channel_compensation(pdsch_vars[eNB_id]->rxdataF_ext,
                               pdsch_vars[eNB_id]->dl_ch_estimates_ext,
                               pdsch_vars[eNB_id]->dl_ch_mag0,
                               pdsch_vars[eNB_id]->dl_ch_magb0,
                               pdsch_vars[eNB_id]->rxdataF_comp0,
                               NULL,
                               frame_parms,
                               symbol,
                               first_symbol_flag,
                               dlsch0_harq->Qm,
                               nb_rb,
                               pdsch_vars[eNB_id]->log2_maxh,
                               measurements); // log2_maxh+I0_shift


    // Combine different links from single tx antenna to receive antennas
    if (num_rx_ant > 1)
    {
        dlsch_detection_mrc(frame_parms,
                            pdsch_vars[eNB_id]->rxdataF_comp0,
                            NULL,
                            NULL,
                            NULL,
                            pdsch_vars[eNB_id]->dl_ch_mag0,
                            pdsch_vars[eNB_id]->dl_ch_magb0,
                            NULL,
                            NULL,
                            symbol,
                            nb_rb,
                            0);
    }

    // Combine for space-frequency block coding
    dlsch_alamouti(frame_parms,
                   pdsch_vars[eNB_id]->rxdataF_comp0,
                   pdsch_vars[eNB_id]->dl_ch_mag0,
                   pdsch_vars[eNB_id]->dl_ch_magb0,
                   symbol,
                   nb_rb);

    // LLR computation
    switch (dlsch0_harq->Qm)
    {
    case 2:
    {
        dlsch_qpsk_llr(frame_parms,
                       pdsch_vars[eNB_id]->rxdataF_comp0,
                       pdsch_vars[eNB_id]->llr[0],
                       symbol,
                       first_symbol_flag,
                       nb_rb,
                       adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,2,subframe,symbol),
                       pdsch_vars[eNB_id]->llr128,
                       0);
        break;
    }
    case 4:
    {
        dlsch_16qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,4,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
        break;
    }
    case 6:
    {
        dlsch_64qam_llr(frame_parms,
                        pdsch_vars[eNB_id]->rxdataF_comp0,
                        pdsch_vars[eNB_id]->llr[0],
                        pdsch_vars[eNB_id]->dl_ch_mag0,
                        pdsch_vars[eNB_id]->dl_ch_magb0,
                        symbol,
                        first_symbol_flag,
                        nb_rb,
                        adjust_G2(frame_parms,dlsch0_harq->rb_alloc_even,6,subframe,symbol),
                        pdsch_vars[eNB_id]->llr128,
                        0);
    }
    default:
    {
        LOG_W(PHY, "rx_pdsch_sap : Unknown modulation order!!!!\n");
        return(-1);
    }
    }

    // Everything went well...
    return(0);
}

void dlsch_alamouti(LTE_DL_FRAME_PARMS *frame_parms,
                    int32_t **rxdataF_comp,
                    int32_t **dl_ch_mag,
                    int32_t **dl_ch_magb,
                    uint8_t symbol,
                    uint16_t nb_rb)
{

#if defined(__x86_64__)||defined(__i386__)

    short *rxF0,*rxF1;
    __m128i *ch_mag0,*ch_mag1,*ch_mag0b,*ch_mag1b, *rxF0_128;
    unsigned char rb,re;
    int jj = (symbol*frame_parms->N_RB_DL*12);
    uint8_t symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;
    uint8_t pilots = ((symbol_mod==0)||(symbol_mod==(4-frame_parms->Ncp))) ? 1 : 0;
    rxF0_128 = (__m128i*) &rxdataF_comp[0][jj];

    // printf("Doing alamouti!\n");
    rxF0     = (short*)&rxdataF_comp[0][jj];  //tx antenna 0  h0*y
    rxF1     = (short*)&rxdataF_comp[2][jj];  //tx antenna 1  h1*y
    ch_mag0 = (__m128i *)&dl_ch_mag[0][jj];
    ch_mag1 = (__m128i *)&dl_ch_mag[2][jj];
    ch_mag0b = (__m128i *)&dl_ch_magb[0][jj];
    ch_mag1b = (__m128i *)&dl_ch_magb[2][jj];

    for (rb=0; rb < nb_rb; rb++)
    {
        for (re=0; re < ((pilots==0)?12:8); re+=2)
        {
            // Alamouti RX combining

            // printf("Alamouti: symbol %d, rb %d, re %d: rxF0 (%d,%d,%d,%d), rxF1 (%d,%d,%d,%d)\n",symbol,rb,re,rxF0[0],rxF0[1],rxF0[2],rxF0[3],rxF1[0],rxF1[1],rxF1[2],rxF1[3]);
            rxF0[0] = rxF0[0] + rxF1[2];
            rxF0[1] = rxF0[1] - rxF1[3];

            rxF0[2] = rxF0[2] - rxF1[0];
            rxF0[3] = rxF0[3] + rxF1[1];

            // printf("Alamouti: rxF0 after (%d,%d,%d,%d)\n",rxF0[0],rxF0[1],rxF0[2],rxF0[3]);
            rxF0+=4;
            rxF1+=4;
        }

        // compute levels for 16QAM or 64 QAM llr unit
        ch_mag0[0] = _mm_adds_epi16(ch_mag0[0],ch_mag1[0]);
        ch_mag0[1] = _mm_adds_epi16(ch_mag0[1],ch_mag1[1]);

        ch_mag0b[0] = _mm_adds_epi16(ch_mag0b[0],ch_mag1b[0]);
        ch_mag0b[1] = _mm_adds_epi16(ch_mag0b[1],ch_mag1b[1]);

        if (pilots == 0)
        {
            ch_mag0[2] = _mm_adds_epi16(ch_mag0[2],ch_mag1[2]);
            ch_mag0b[2] = _mm_adds_epi16(ch_mag0b[2],ch_mag1b[2]);

            ch_mag0  += 3;
            ch_mag1  += 3;
            ch_mag0b += 3;
            ch_mag1b += 3;
            rxF0_128 += 3;
        }
        else
        {
            ch_mag0  += 2;
            ch_mag1  += 2;
            ch_mag0b += 2;
            ch_mag1b += 2;
            rxF0_128 += 2;
        }
    }

    _mm_empty();
    _m_empty();

#elif defined(__arm__)

#endif
}

void dlsch_detection_mrc(LTE_DL_FRAME_PARMS *frame_parms,
                         int **rxdataF_comp,
                         int **rxdataF_comp_i,
                         int **rho,
                         int **rho_i,
                         int **dl_ch_mag,
                         int **dl_ch_magb,
                         int **dl_ch_mag_i,
                         int **dl_ch_magb_i,
                         unsigned char symbol,
                         unsigned short nb_rb,
                         unsigned char dual_stream_UE)
{

#if defined(__x86_64__)||defined(__i386__)

    unsigned char aatx;
    int i;
    __m128i *rxdataF_comp128_0,*rxdataF_comp128_1,*rxdataF_comp128_i0,*rxdataF_comp128_i1,*dl_ch_mag128_0,*dl_ch_mag128_1,*dl_ch_mag128_0b,*dl_ch_mag128_1b,*rho128_0,*rho128_1,*rho128_i0,*rho128_i1,
        *dl_ch_mag128_i0,*dl_ch_mag128_i1,*dl_ch_mag128_i0b,*dl_ch_mag128_i1b;

    if (frame_parms->nb_antennas_rx>1)
    {
        for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
        {
            rxdataF_comp128_0 = (__m128i *)&rxdataF_comp[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_1 = (__m128i *)&rxdataF_comp[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_0    = (__m128i *)&dl_ch_mag[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_1    = (__m128i *)&dl_ch_mag[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_0b   = (__m128i *)&dl_ch_magb[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_1b   = (__m128i *)&dl_ch_magb[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];

            // MRC on each re of rb, both on MF output and magnitude (for 16QAM/64QAM llr computation)
            for (i=0; i<nb_rb*3; i++)
            {
                rxdataF_comp128_0[i] = _mm_adds_epi16(_mm_srai_epi16(rxdataF_comp128_0[i],1),_mm_srai_epi16(rxdataF_comp128_1[i],1));
                dl_ch_mag128_0[i]    = _mm_adds_epi16(_mm_srai_epi16(dl_ch_mag128_0[i],1),_mm_srai_epi16(dl_ch_mag128_1[i],1));
                dl_ch_mag128_0b[i]   = _mm_adds_epi16(_mm_srai_epi16(dl_ch_mag128_0b[i],1),_mm_srai_epi16(dl_ch_mag128_1b[i],1));
                // print_shorts("mrc comp0:",&rxdataF_comp128_0[i]);
                // print_shorts("mrc mag0:",&dl_ch_mag128_0[i]);
                // print_shorts("mrc mag0b:",&dl_ch_mag128_0b[i]);
                // print_shorts("mrc rho1:",&rho128_1[i]);
            }
        }

        if (rho)
        {
            rho128_0 = (__m128i *) &rho[0][symbol*frame_parms->N_RB_DL*12];
            rho128_1 = (__m128i *) &rho[1][symbol*frame_parms->N_RB_DL*12];
            for (i=0;i<nb_rb*3;i++)
            {
                // print_shorts("mrc rho0:",&rho128_0[i]);
                // print_shorts("mrc rho1:",&rho128_1[i]);
                rho128_0[i] = _mm_adds_epi16(_mm_srai_epi16(rho128_0[i],1),_mm_srai_epi16(rho128_1[i],1));
            }
        }

        if (dual_stream_UE == 1)
        {
            rho128_i0 = (__m128i *) &rho_i[0][symbol*frame_parms->N_RB_DL*12];
            rho128_i1 = (__m128i *) &rho_i[1][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_i0   = (__m128i *)&rxdataF_comp_i[0][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_i1   = (__m128i *)&rxdataF_comp_i[1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i0      = (__m128i *)&dl_ch_mag_i[0][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i1      = (__m128i *)&dl_ch_mag_i[1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i0b     = (__m128i *)&dl_ch_magb_i[0][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i1b     = (__m128i *)&dl_ch_magb_i[1][symbol*frame_parms->N_RB_DL*12];

            for (i=0; i<nb_rb*3; i++)
            {
                rxdataF_comp128_i0[i] = _mm_adds_epi16(_mm_srai_epi16(rxdataF_comp128_i0[i],1),_mm_srai_epi16(rxdataF_comp128_i1[i],1));
                rho128_i0[i]          = _mm_adds_epi16(_mm_srai_epi16(rho128_i0[i],1),_mm_srai_epi16(rho128_i1[i],1));

                dl_ch_mag128_i0[i]  = _mm_adds_epi16(_mm_srai_epi16(dl_ch_mag128_i0[i],1),_mm_srai_epi16(dl_ch_mag128_i1[i],1));
                dl_ch_mag128_i0b[i] = _mm_adds_epi16(_mm_srai_epi16(dl_ch_mag128_i0b[i],1),_mm_srai_epi16(dl_ch_mag128_i1b[i],1));
            }
        }
    }

    _mm_empty();
    _m_empty();

#elif defined(__arm__)

    unsigned char aatx;
    int i;
    int16x8_t *rxdataF_comp128_0,*rxdataF_comp128_1,*rxdataF_comp128_i0,*rxdataF_comp128_i1,*dl_ch_mag128_0,*dl_ch_mag128_1,*dl_ch_mag128_0b,*dl_ch_mag128_1b,*rho128_0,*rho128_1,*rho128_i0,*rho128_i1,*dl_ch_mag128_i0,*dl_ch_mag128_i1,*dl_ch_mag128_i0b,*dl_ch_mag128_i1b;

    if (frame_parms->nb_antennas_rx>1)
    {
        for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
        {
            rxdataF_comp128_0 = (int16x8_t *)&rxdataF_comp[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_1 = (int16x8_t *)&rxdataF_comp[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_0    = (int16x8_t *)&dl_ch_mag[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_1    = (int16x8_t *)&dl_ch_mag[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_0b   = (int16x8_t *)&dl_ch_magb[(aatx<<1)][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_1b   = (int16x8_t *)&dl_ch_magb[(aatx<<1)+1][symbol*frame_parms->N_RB_DL*12];

            // MRC on each re of rb, both on MF output and magnitude (for 16QAM/64QAM llr computation)
            for (i=0; i<nb_rb*3; i++)
            {
                rxdataF_comp128_0[i] = vhaddq_s16(rxdataF_comp128_0[i],rxdataF_comp128_1[i]);
                dl_ch_mag128_0[i]    = vhaddq_s16(dl_ch_mag128_0[i],dl_ch_mag128_1[i]);
                dl_ch_mag128_0b[i]   = vhaddq_s16(dl_ch_mag128_0b[i],dl_ch_mag128_1b[i]);
            }
        }

        if (rho)
        {
            rho128_0 = (int16x8_t *) &rho[0][symbol*frame_parms->N_RB_DL*12];
            rho128_1 = (int16x8_t *) &rho[1][symbol*frame_parms->N_RB_DL*12];

            for (i=0; i<nb_rb*3; i++)
            {
                //  print_shorts("mrc rho0:",&rho128_0[i]);
                //  print_shorts("mrc rho1:",&rho128_1[i]);
                rho128_0[i] = vhaddq_s16(rho128_0[i],rho128_1[i]);
            }
        }

        if (dual_stream_UE == 1)
        {
            rho128_i0 = (int16x8_t *) &rho_i[0][symbol*frame_parms->N_RB_DL*12];
            rho128_i1 = (int16x8_t *) &rho_i[1][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_i0 = (int16x8_t *)&rxdataF_comp_i[0][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128_i1 = (int16x8_t *)&rxdataF_comp_i[1][symbol*frame_parms->N_RB_DL*12];

            dl_ch_mag128_i0  = (int16x8_t *)&dl_ch_mag_i[0][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i1  = (int16x8_t *)&dl_ch_mag_i[1][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i0b = (int16x8_t *)&dl_ch_magb_i[0][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128_i1b = (int16x8_t *)&dl_ch_magb_i[1][symbol*frame_parms->N_RB_DL*12];

            for (i=0; i<nb_rb*3; i++)
            {
                rxdataF_comp128_i0[i] = vhaddq_s16(rxdataF_comp128_i0[i],rxdataF_comp128_i1[i]);
                rho128_i0[i]          = vhaddq_s16(rho128_i0[i],rho128_i1[i]);

                dl_ch_mag128_i0[i]    = vhaddq_s16(dl_ch_mag128_i0[i],dl_ch_mag128_i1[i]);
                dl_ch_mag128_i0b[i]   = vhaddq_s16(dl_ch_mag128_i0b[i],dl_ch_mag128_i1b[i]);
            }
        }
    }

#endif
}

void dlsch_channel_compensation(int32_t **rxdataF_ext,
                                int32_t **dl_ch_estimates_ext,
                                int32_t **dl_ch_mag,
                                int32_t **dl_ch_magb,
                                int32_t **rxdataF_comp,
                                int32_t **rho,
                                LTE_DL_FRAME_PARMS *frame_parms,
                                uint8_t symbol,
                                uint8_t first_symbol_flag,
                                uint8_t mod_order,
                                uint16_t nb_rb,
                                uint8_t output_shift,
                                PHY_MEASUREMENTS *measurements)
{

#if defined(__i386) || defined(__x86_64)

    uint16_t rb;
    uint8_t aatx,aarx,symbol_mod,pilots=0;
    __m128i *dl_ch128,*dl_ch128_2,*dl_ch_mag128,*dl_ch_mag128b,*rxdataF128,*rxdataF_comp128,*rho128;
    __m128i mmtmpD0,mmtmpD1,mmtmpD2,mmtmpD3,QAM_amp128,QAM_amp128b;

    symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;

    if ((symbol_mod == 0) || (symbol_mod == (4-frame_parms->Ncp)))
    {
        if (frame_parms->mode1_flag==1)
        {
            // 10 out of 12 so don't reduce size
            nb_rb=1+(5*nb_rb/6);
        }
        else
        {
            pilots=1;
        }
    }

    for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
    {
        if (mod_order == 4)
        {
            QAM_amp128 = _mm_set1_epi16(QAM16_n1);  // 2/sqrt(10)
            QAM_amp128b = _mm_setzero_si128();
        }
        else if (mod_order == 6)
        {
            QAM_amp128  = _mm_set1_epi16(QAM64_n1); //
            QAM_amp128b = _mm_set1_epi16(QAM64_n2);
        }

        // printf("comp: rxdataF_comp %p, symbol %d\n",rxdataF_comp[0],symbol);

        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {

            dl_ch128        = (__m128i *)&dl_ch_estimates_ext[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128    = (__m128i *)&dl_ch_mag[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128b   = (__m128i *)&dl_ch_magb[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            rxdataF128      = (__m128i *)&rxdataF_ext[aarx][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128 = (__m128i *)&rxdataF_comp[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];


            for (rb=0; rb<nb_rb; rb++)
            {
                if (mod_order>2)
                {
                    // get channel amplitude if not QPSK
                    mmtmpD0 = _mm_madd_epi16(dl_ch128[0],dl_ch128[0]);
                    mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);

                    mmtmpD1 = _mm_madd_epi16(dl_ch128[1],dl_ch128[1]);
                    mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);

                    mmtmpD0 = _mm_packs_epi32(mmtmpD0,mmtmpD1);

                    // store channel magnitude here in a new field of dlsch

                    dl_ch_mag128[0] = _mm_unpacklo_epi16(mmtmpD0,mmtmpD0);
                    dl_ch_mag128b[0] = dl_ch_mag128[0];
                    dl_ch_mag128[0] = _mm_mulhi_epi16(dl_ch_mag128[0],QAM_amp128);
                    dl_ch_mag128[0] = _mm_slli_epi16(dl_ch_mag128[0],1);
                    //print_ints("Re(ch):",(int16_t*)&mmtmpD0);
                    //print_shorts("QAM_amp:",(int16_t*)&QAM_amp128);
                    //print_shorts("mag:",(int16_t*)&dl_ch_mag128[0]);
                    dl_ch_mag128[1] = _mm_unpackhi_epi16(mmtmpD0,mmtmpD0);
                    dl_ch_mag128b[1] = dl_ch_mag128[1];
                    dl_ch_mag128[1] = _mm_mulhi_epi16(dl_ch_mag128[1],QAM_amp128);
                    dl_ch_mag128[1] = _mm_slli_epi16(dl_ch_mag128[1],1);

                    if (pilots==0)
                    {
                        mmtmpD0 = _mm_madd_epi16(dl_ch128[2],dl_ch128[2]);
                        mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                        mmtmpD1 = _mm_packs_epi32(mmtmpD0,mmtmpD0);

                        dl_ch_mag128[2] = _mm_unpacklo_epi16(mmtmpD1,mmtmpD1);
                        dl_ch_mag128b[2] = dl_ch_mag128[2];

                        dl_ch_mag128[2] = _mm_mulhi_epi16(dl_ch_mag128[2],QAM_amp128);
                        dl_ch_mag128[2] = _mm_slli_epi16(dl_ch_mag128[2],1);
                    }

                    dl_ch_mag128b[0] = _mm_mulhi_epi16(dl_ch_mag128b[0],QAM_amp128b);
                    dl_ch_mag128b[0] = _mm_slli_epi16(dl_ch_mag128b[0],1);

                    dl_ch_mag128b[1] = _mm_mulhi_epi16(dl_ch_mag128b[1],QAM_amp128b);
                    dl_ch_mag128b[1] = _mm_slli_epi16(dl_ch_mag128b[1],1);

                    if (pilots==0)
                    {
                        dl_ch_mag128b[2] = _mm_mulhi_epi16(dl_ch_mag128b[2],QAM_amp128b);
                        dl_ch_mag128b[2] = _mm_slli_epi16(dl_ch_mag128b[2],1);
                    }
                }

                // multiply by conjugated channel
                mmtmpD0 = _mm_madd_epi16(dl_ch128[0],rxdataF128[0]);
                //  print_ints("re",&mmtmpD0);

                // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[0],_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&conjugate[0]);
                //  print_ints("im",&mmtmpD1);
                mmtmpD1 = _mm_madd_epi16(mmtmpD1,rxdataF128[0]);
                // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                //  print_ints("re(shift)",&mmtmpD0);
                mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                //  print_ints("im(shift)",&mmtmpD1);
                mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
                //        print_ints("c0",&mmtmpD2);
                //  print_ints("c1",&mmtmpD3);
                rxdataF_comp128[0] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
                //  print_shorts("rx:",rxdataF128);
                //  print_shorts("ch:",dl_ch128);
                //  print_shorts("pack:",rxdataF_comp128);

                // multiply by conjugated channel
                mmtmpD0 = _mm_madd_epi16(dl_ch128[1],rxdataF128[1]);
                // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[1],_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)conjugate);
                mmtmpD1 = _mm_madd_epi16(mmtmpD1,rxdataF128[1]);
                // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);

                rxdataF_comp128[1] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
                //  print_shorts("rx:",rxdataF128+1);
                //  print_shorts("ch:",dl_ch128+1);
                //  print_shorts("pack:",rxdataF_comp128+1);

                if (pilots==0)
                {
                    // multiply by conjugated channel
                    mmtmpD0 = _mm_madd_epi16(dl_ch128[2],rxdataF128[2]);
                    // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                    mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[2],_MM_SHUFFLE(2,3,0,1));
                    mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                    mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)conjugate);
                    mmtmpD1 = _mm_madd_epi16(mmtmpD1,rxdataF128[2]);
                    // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                    mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                    mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                    mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                    mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);

                    rxdataF_comp128[2] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
                    //  print_shorts("rx:",rxdataF128+2);
                    //  print_shorts("ch:",dl_ch128+2);
                    //        print_shorts("pack:",rxdataF_comp128+2);

                    dl_ch128+=3;
                    dl_ch_mag128+=3;
                    dl_ch_mag128b+=3;
                    rxdataF128+=3;
                    rxdataF_comp128+=3;
                }
                else
                {
                    // we have a smaller PDSCH in symbols with pilots so skip last group of 4 REs and increment less
                    dl_ch128+=2;
                    dl_ch_mag128+=2;
                    dl_ch_mag128b+=2;
                    rxdataF128+=2;
                    rxdataF_comp128+=2;
                }

            }
        }
    }

    if (rho)
    {
        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            rho128     = (__m128i *)&rho[aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch128   = (__m128i *)&dl_ch_estimates_ext[aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch128_2 = (__m128i *)&dl_ch_estimates_ext[2+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0; rb<nb_rb; rb++)
            {
                // multiply by conjugated channel
                mmtmpD0 = _mm_madd_epi16(dl_ch128[0],dl_ch128_2[0]);
                //  print_ints("re",&mmtmpD0);

                // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[0],_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)&conjugate[0]);
                //  print_ints("im",&mmtmpD1);
                mmtmpD1 = _mm_madd_epi16(mmtmpD1,dl_ch128_2[0]);
                // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                //  print_ints("re(shift)",&mmtmpD0);
                mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                //  print_ints("im(shift)",&mmtmpD1);
                mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);
                //        print_ints("c0",&mmtmpD2);
                //  print_ints("c1",&mmtmpD3);
                rho128[0] = _mm_packs_epi32(mmtmpD2,mmtmpD3);

                //print_shorts("rx:",dl_ch128_2);
                //print_shorts("ch:",dl_ch128);
                //print_shorts("pack:",rho128);

                // multiply by conjugated channel
                mmtmpD0 = _mm_madd_epi16(dl_ch128[1],dl_ch128_2[1]);
                // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[1],_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)conjugate);
                mmtmpD1 = _mm_madd_epi16(mmtmpD1,dl_ch128_2[1]);
                // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);

                rho128[1] =_mm_packs_epi32(mmtmpD2,mmtmpD3);
                //print_shorts("rx:",dl_ch128_2+1);
                //print_shorts("ch:",dl_ch128+1);
                //print_shorts("pack:",rho128+1);
                // multiply by conjugated channel
                mmtmpD0 = _mm_madd_epi16(dl_ch128[2],dl_ch128_2[2]);
                // mmtmpD0 contains real part of 4 consecutive outputs (32-bit)
                mmtmpD1 = _mm_shufflelo_epi16(dl_ch128[2],_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_shufflehi_epi16(mmtmpD1,_MM_SHUFFLE(2,3,0,1));
                mmtmpD1 = _mm_sign_epi16(mmtmpD1,*(__m128i*)conjugate);
                mmtmpD1 = _mm_madd_epi16(mmtmpD1,dl_ch128_2[2]);
                // mmtmpD1 contains imag part of 4 consecutive outputs (32-bit)
                mmtmpD0 = _mm_srai_epi32(mmtmpD0,output_shift);
                mmtmpD1 = _mm_srai_epi32(mmtmpD1,output_shift);
                mmtmpD2 = _mm_unpacklo_epi32(mmtmpD0,mmtmpD1);
                mmtmpD3 = _mm_unpackhi_epi32(mmtmpD0,mmtmpD1);

                rho128[2] = _mm_packs_epi32(mmtmpD2,mmtmpD3);
                //print_shorts("rx:",dl_ch128_2+2);
                //print_shorts("ch:",dl_ch128+2);
                //print_shorts("pack:",rho128+2);

                dl_ch128+=3;
                dl_ch128_2+=3;
                rho128+=3;
            }

            if (first_symbol_flag==1)
            {
                measurements->rx_correlation[0][aarx] = signal_energy(&rho[aarx][symbol*frame_parms->N_RB_DL*12],rb*12);
            }
        }
    }

    _mm_empty();
    _m_empty();

#elif defined(__arm__)

    unsigned short rb;
    unsigned char aatx,aarx,symbol_mod,pilots=0;

    int16x4_t *dl_ch128,*dl_ch128_2,*rxdataF128;
    int32x4_t mmtmpD0,mmtmpD1,mmtmpD0b,mmtmpD1b;
    int16x8_t *dl_ch_mag128,*dl_ch_mag128b,mmtmpD2,mmtmpD3,mmtmpD4;
    int16x8_t QAM_amp128,QAM_amp128b;
    int16x4x2_t *rxdataF_comp128,*rho128;

    int16_t conj[4]__attribute__((aligned(16))) = {1,-1,1,-1};
    int32x4_t output_shift128 = vmovq_n_s32(-(int32_t)output_shift);

    symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;

    if ((symbol_mod == 0) || (symbol_mod == (4-frame_parms->Ncp)))
    {
        if (frame_parms->mode1_flag==1)
        {   // 10 out of 12 so don't reduce size
            nb_rb = 1 + (5*nb_rb/6);
        }
        else {
            pilots=1;
        }
    }

    for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
    {
        if (mod_order == 4)
        {
            QAM_amp128  = vmovq_n_s16(QAM16_n1);  // 2/sqrt(10)
            QAM_amp128b = vmovq_n_s16(0);
        }
        else if (mod_order == 6)
        {
            QAM_amp128  = vmovq_n_s16(QAM64_n1); //
            QAM_amp128b = vmovq_n_s16(QAM64_n2);
        }
        // printf("comp: rxdataF_comp %p, symbol %d\n",rxdataF_comp[0],symbol);

        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            dl_ch128        = (int16x4_t*)&dl_ch_estimates_ext[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128    = (int16x8_t*)&dl_ch_mag[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch_mag128b   = (int16x8_t*)&dl_ch_magb[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];
            rxdataF128      = (int16x4_t*)&rxdataF_ext[aarx][symbol*frame_parms->N_RB_DL*12];
            rxdataF_comp128 = (int16x4x2_t*)&rxdataF_comp[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0; rb<nb_rb; rb++)
            {
                if (mod_order>2)
                {
                    // get channel amplitude if not QPSK
                    mmtmpD0 = vmull_s16(dl_ch128[0], dl_ch128[0]);
                    // mmtmpD0 = [ch0*ch0,ch1*ch1,ch2*ch2,ch3*ch3];
                    mmtmpD0 = vqshlq_s32(vqaddq_s32(mmtmpD0,vrev64q_s32(mmtmpD0)),output_shift128);
                    // mmtmpD0 = [ch0*ch0 + ch1*ch1,ch0*ch0 + ch1*ch1,ch2*ch2 + ch3*ch3,ch2*ch2 + ch3*ch3]>>output_shift128 on 32-bits
                    mmtmpD1 = vmull_s16(dl_ch128[1], dl_ch128[1]);
                    mmtmpD1 = vqshlq_s32(vqaddq_s32(mmtmpD1,vrev64q_s32(mmtmpD1)),output_shift128);
                    mmtmpD2 = vcombine_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));
                    // mmtmpD2 = [ch0*ch0 + ch1*ch1,ch0*ch0 + ch1*ch1,ch2*ch2 + ch3*ch3,ch2*ch2 + ch3*ch3,ch4*ch4 + ch5*ch5,ch4*ch4 + ch5*ch5,ch6*ch6 + ch7*ch7,ch6*ch6 + ch7*ch7]>>output_shift128 on 16-bits
                    mmtmpD0 = vmull_s16(dl_ch128[2], dl_ch128[2]);
                    mmtmpD0 = vqshlq_s32(vqaddq_s32(mmtmpD0,vrev64q_s32(mmtmpD0)),output_shift128);
                    mmtmpD1 = vmull_s16(dl_ch128[3], dl_ch128[3]);
                    mmtmpD1 = vqshlq_s32(vqaddq_s32(mmtmpD1,vrev64q_s32(mmtmpD1)),output_shift128);
                    mmtmpD3 = vcombine_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                    if (pilots==0)
                    {
                        mmtmpD0 = vmull_s16(dl_ch128[4], dl_ch128[4]);
                        mmtmpD0 = vqshlq_s32(vqaddq_s32(mmtmpD0,vrev64q_s32(mmtmpD0)),output_shift128);
                        mmtmpD1 = vmull_s16(dl_ch128[5], dl_ch128[5]);
                        mmtmpD1 = vqshlq_s32(vqaddq_s32(mmtmpD1,vrev64q_s32(mmtmpD1)),output_shift128);
                        mmtmpD4 = vcombine_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));
                    }

                    dl_ch_mag128b[0] = vqdmulhq_s16(mmtmpD2,QAM_amp128b);
                    dl_ch_mag128b[1] = vqdmulhq_s16(mmtmpD3,QAM_amp128b);
                    dl_ch_mag128[0] = vqdmulhq_s16(mmtmpD2,QAM_amp128);
                    dl_ch_mag128[1] = vqdmulhq_s16(mmtmpD3,QAM_amp128);

                    if (pilots==0)
                    {
                        dl_ch_mag128b[2] = vqdmulhq_s16(mmtmpD4,QAM_amp128b);
                        dl_ch_mag128[2]  = vqdmulhq_s16(mmtmpD4,QAM_amp128);
                    }
                }

                mmtmpD0 = vmull_s16(dl_ch128[0], rxdataF128[0]);
                //mmtmpD0 = [Re(ch[0])Re(rx[0]) Im(ch[0])Im(ch[0]) Re(ch[1])Re(rx[1]) Im(ch[1])Im(ch[1])]
                mmtmpD1 = vmull_s16(dl_ch128[1], rxdataF128[1]);
                //mmtmpD1 = [Re(ch[2])Re(rx[2]) Im(ch[2])Im(ch[2]) Re(ch[3])Re(rx[3]) Im(ch[3])Im(ch[3])]
                mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                       vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));
                //mmtmpD0 = [Re(ch[0])Re(rx[0])+Im(ch[0])Im(ch[0]) Re(ch[1])Re(rx[1])+Im(ch[1])Im(ch[1]) Re(ch[2])Re(rx[2])+Im(ch[2])Im(ch[2]) Re(ch[3])Re(rx[3])+Im(ch[3])Im(ch[3])]

                mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[0],*(int16x4_t*)conj)), rxdataF128[0]);
                //mmtmpD0 = [-Im(ch[0])Re(rx[0]) Re(ch[0])Im(rx[0]) -Im(ch[1])Re(rx[1]) Re(ch[1])Im(rx[1])]
                mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[1],*(int16x4_t*)conj)), rxdataF128[1]);
                //mmtmpD0 = [-Im(ch[2])Re(rx[2]) Re(ch[2])Im(rx[2]) -Im(ch[3])Re(rx[3]) Re(ch[3])Im(rx[3])]
                mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                       vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));
                //mmtmpD1 = [-Im(ch[0])Re(rx[0])+Re(ch[0])Im(rx[0]) -Im(ch[1])Re(rx[1])+Re(ch[1])Im(rx[1]) -Im(ch[2])Re(rx[2])+Re(ch[2])Im(rx[2]) -Im(ch[3])Re(rx[3])+Re(ch[3])Im(rx[3])]

                mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                rxdataF_comp128[0] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));
                mmtmpD0 = vmull_s16(dl_ch128[2], rxdataF128[2]);
                mmtmpD1 = vmull_s16(dl_ch128[3], rxdataF128[3]);
                mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                       vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));
                mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[2],*(int16x4_t*)conj)), rxdataF128[2]);
                mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[3],*(int16x4_t*)conj)), rxdataF128[3]);
                mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                       vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));
                mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                rxdataF_comp128[1] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                if (pilots==0)
                {
                    mmtmpD0 = vmull_s16(dl_ch128[4], rxdataF128[4]);
                    mmtmpD1 = vmull_s16(dl_ch128[5], rxdataF128[5]);
                    mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                           vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));

                    mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[4],*(int16x4_t*)conj)), rxdataF128[4]);
                    mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[5],*(int16x4_t*)conj)), rxdataF128[5]);
                    mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                           vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));

                    mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                    mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                    rxdataF_comp128[2] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                    dl_ch128+=6;
                    dl_ch_mag128+=3;
                    dl_ch_mag128b+=3;
                    rxdataF128+=6;
                    rxdataF_comp128+=3;
                }
                else
                {
                    // we have a smaller PDSCH in symbols with pilots so skip last group of 4 REs and increment less
                    dl_ch128+=4;
                    dl_ch_mag128+=2;
                    dl_ch_mag128b+=2;
                    rxdataF128+=4;
                    rxdataF_comp128+=2;
                }
            }
        }
    }

    if (rho)
    {
        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            rho128     = (int16x4x2_t*)&rho[aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch128   = (int16x4_t*)&dl_ch_estimates_ext[aarx][symbol*frame_parms->N_RB_DL*12];
            dl_ch128_2 = (int16x4_t*)&dl_ch_estimates_ext[2+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0; rb<nb_rb; rb++)
            {
                mmtmpD0 = vmull_s16(dl_ch128[0], dl_ch128_2[0]);
                mmtmpD1 = vmull_s16(dl_ch128[1], dl_ch128_2[1]);
                mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                       vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));
                mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[0],*(int16x4_t*)conj)), dl_ch128_2[0]);
                mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[1],*(int16x4_t*)conj)), dl_ch128_2[1]);
                mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                       vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));

                mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                rho128[0] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                mmtmpD0 = vmull_s16(dl_ch128[2], dl_ch128_2[2]);
                mmtmpD1 = vmull_s16(dl_ch128[3], dl_ch128_2[3]);
                mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                       vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));
                mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[2],*(int16x4_t*)conj)), dl_ch128_2[2]);
                mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[3],*(int16x4_t*)conj)), dl_ch128_2[3]);
                mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                       vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));

                mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                rho128[1] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                mmtmpD0 = vmull_s16(dl_ch128[0], dl_ch128_2[0]);
                mmtmpD1 = vmull_s16(dl_ch128[1], dl_ch128_2[1]);
                mmtmpD0 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0),vget_high_s32(mmtmpD0)),
                                       vpadd_s32(vget_low_s32(mmtmpD1),vget_high_s32(mmtmpD1)));
                mmtmpD0b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[4],*(int16x4_t*)conj)), dl_ch128_2[4]);
                mmtmpD1b = vmull_s16(vrev32_s16(vmul_s16(dl_ch128[5],*(int16x4_t*)conj)), dl_ch128_2[5]);
                mmtmpD1 = vcombine_s32(vpadd_s32(vget_low_s32(mmtmpD0b),vget_high_s32(mmtmpD0b)),
                                       vpadd_s32(vget_low_s32(mmtmpD1b),vget_high_s32(mmtmpD1b)));

                mmtmpD0 = vqshlq_s32(mmtmpD0,output_shift128);
                mmtmpD1 = vqshlq_s32(mmtmpD1,output_shift128);
                rho128[2] = vzip_s16(vmovn_s32(mmtmpD0),vmovn_s32(mmtmpD1));

                dl_ch128+=6;
                dl_ch128_2+=6;
                rho128+=3;
            }

            if (first_symbol_flag==1)
            {
                measurements->rx_correlation[0][aarx] = signal_energy(&rho[aarx][symbol*frame_parms->N_RB_DL*12],rb*12);
            }
        }
    }
#endif
}

//compute average channel_level on each (TX,RX) antenna pair
void dlsch_channel_level(int **dl_ch_estimates_ext,
                         LTE_DL_FRAME_PARMS *frame_parms,
                         int *avg,
                         uint8_t symbol,
                         unsigned short nb_rb)
{

#if defined(__x86_64__)||defined(__i386__)

    short rb;
    unsigned char aatx,aarx,nre=12,symbol_mod;
    __m128i *dl_ch128, avg128D;

    symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;

    for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
    {
        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            //clear average level
            avg128D = _mm_setzero_si128();
            // 5 is always a symbol with no pilots for both normal and extended prefix

            dl_ch128=(__m128i *)&dl_ch_estimates_ext[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0;rb<nb_rb;rb++)
            {
                // printf("rb %d : ",rb);
                // print_shorts("ch",&dl_ch128[0]);
                avg128D = _mm_add_epi32(avg128D,_mm_madd_epi16(dl_ch128[0],dl_ch128[0]));
                avg128D = _mm_add_epi32(avg128D,_mm_madd_epi16(dl_ch128[1],dl_ch128[1]));
                if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==0))
                {
                    dl_ch128+=2;
                }
                else
                {
                    avg128D = _mm_add_epi32(avg128D,_mm_madd_epi16(dl_ch128[2],dl_ch128[2]));
                    dl_ch128+=3;
                }
                /*
                  if (rb==0) {
                  print_shorts("dl_ch128",&dl_ch128[0]);
                  print_shorts("dl_ch128",&dl_ch128[1]);
                  print_shorts("dl_ch128",&dl_ch128[2]);
                  }
                */
            }

            if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==0))
            {
                nre = 8;
            }
            else if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==1))
            {
                nre = 10;
            }
            else
            {
                nre=12;
            }

            avg[(aatx<<1)+aarx] = (((int*)&avg128D)[0] +
                                   ((int*)&avg128D)[1] +
                                   ((int*)&avg128D)[2] +
                                   ((int*)&avg128D)[3])/(nb_rb*nre);

            // printf("Channel level : %d\n",avg[(aatx<<1)+aarx]);
        } // loop over rx
    } // loop over tx

    _mm_empty();
    _m_empty();

#elif defined(__arm__)

    short rb;
    unsigned char aatx,aarx,nre=12,symbol_mod;
    int32x4_t avg128D;
    int16x4_t *dl_ch128;

    symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;

    for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
    {
        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            //clear average level
            avg128D = vdupq_n_s32(0);
            // 5 is always a symbol with no pilots for both normal and extended prefix

            dl_ch128=(int16x4_t *)&dl_ch_estimates_ext[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0; rb<nb_rb; rb++)
            {
                // printf("rb %d : ",rb);
                // print_shorts("ch",&dl_ch128[0]);
                avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[0],dl_ch128[0]));
                avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[1],dl_ch128[1]));
                avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[2],dl_ch128[2]));
                avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[3],dl_ch128[3]));

                if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==0))
                {
                    dl_ch128 += 4;
                }
                else
                {
                    avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[4],dl_ch128[4]));
                    avg128D = vqaddq_s32(avg128D,vmull_s16(dl_ch128[5],dl_ch128[5]));
                    dl_ch128 += 6;
                }

                /*
                  if (rb==0) {
                  print_shorts("dl_ch128",&dl_ch128[0]);
                  print_shorts("dl_ch128",&dl_ch128[1]);
                  print_shorts("dl_ch128",&dl_ch128[2]);
                  }
                */
            }

            if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==0))
            {
                nre = 8;
            }
            else if (((symbol_mod == 0) || (symbol_mod == (frame_parms->Ncp-1)))&&(frame_parms->mode1_flag==1))
            {
                nre = 10;
            }
            else
            {
                nre = 12;
            }

            avg[(aatx<<1)+aarx] = (((int32_t*)&avg128D)[0] +
                                   ((int32_t*)&avg128D)[1] +
                                   ((int32_t*)&avg128D)[2] +
                                   ((int32_t*)&avg128D)[3])/(nb_rb*nre);

            // printf("Channel level : %d\n",avg[(aatx<<1)+aarx]);
        } // loop over rx
    } // loop over tx

#endif
}

void dlsch_scale_channel(int32_t **dl_ch_estimates_ext,
                         LTE_DL_FRAME_PARMS *frame_parms,
                         LTE_UE_DLSCH_t **dlsch_ue,
                         uint8_t symbol,
                         uint16_t nb_rb)
{

#if defined(__x86_64__)||defined(__i386__)

    short rb, ch_amp;
    unsigned char aatx,aarx,pilots=0,symbol_mod;
    __m128i *dl_ch128, ch_amp128;

    symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;

    if ((symbol_mod == 0) || (symbol_mod == (4-frame_parms->Ncp)))
    {
        if (frame_parms->mode1_flag==1)
        {   // 10 out of 12 so don't reduce size
            nb_rb = 1 + (5*nb_rb/6);
        }
        else
        {
            pilots=1;
        }
    }

    // Determine scaling amplitude based the symbol

    ch_amp = ((pilots) ? (dlsch_ue[0]->sqrt_rho_b) : (dlsch_ue[0]->sqrt_rho_a));

    LOG_D(PHY,"Scaling PDSCH Chest in OFDM symbol %d by %d\n",symbol_mod,ch_amp);
    // printf("Scaling PDSCH Chest in OFDM symbol %d by %d\n",symbol_mod,ch_amp);

    ch_amp128 = _mm_set1_epi16(ch_amp); // Q3.13

    for (aatx=0; aatx<frame_parms->nb_antenna_ports_eNB; aatx++)
    {
        for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++)
        {
            dl_ch128=(__m128i *)&dl_ch_estimates_ext[(aatx<<1)+aarx][symbol*frame_parms->N_RB_DL*12];

            for (rb=0;rb<nb_rb;rb++)
            {
                dl_ch128[0] = _mm_mulhi_epi16(dl_ch128[0],ch_amp128);
                dl_ch128[0] = _mm_slli_epi16(dl_ch128[0],3);

                dl_ch128[1] = _mm_mulhi_epi16(dl_ch128[1],ch_amp128);
                dl_ch128[1] = _mm_slli_epi16(dl_ch128[1],3);

                if (pilots)
                {
                    dl_ch128 += 2;
                }
                else
                {
                    dl_ch128[2] = _mm_mulhi_epi16(dl_ch128[2],ch_amp128);
                    dl_ch128[2] = _mm_slli_epi16(dl_ch128[2],3);
                    dl_ch128+=3;
                }
            }
        }
    }

#elif defined(__arm__)

#endif
}

//==============================================================================================
// EXTRACTION
//==============================================================================================

unsigned short dlsch_extract_rbs_single(int **rxdataF,
                                        int **dl_ch_estimates,
                                        int **rxdataF_ext,
                                        int **dl_ch_estimates_ext,
                                        unsigned short pmi,
                                        unsigned char *pmi_ext,
                                        unsigned int *rb_alloc,
                                        unsigned char symbol,
                                        unsigned char subframe,
                                        uint8_t high_speed_flag,
                                        LTE_DL_FRAME_PARMS *frame_parms) {



  unsigned short rb,nb_rb=0;
  unsigned char rb_alloc_ind;
  unsigned char i,aarx,l,nsymb,skip_half=0,sss_symb,pss_symb=0;
  int *dl_ch0,*dl_ch0_ext,*rxF,*rxF_ext;
  unsigned char symbol_mod,pilots=0,j=0,poffset=0;

  symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;
  pilots = ((symbol_mod==0)||(symbol_mod==(4-frame_parms->Ncp))) ? 1 : 0;
  l = symbol;
  nsymb = (frame_parms->Ncp==NORMAL) ? 14:12;

  if (frame_parms->frame_type == TDD) {  // TDD
    sss_symb = nsymb-1;
    pss_symb = 2;
  } else {
    sss_symb = (nsymb>>1)-2;
    pss_symb = (nsymb>>1)-1;
  }

  if (symbol_mod==(4-frame_parms->Ncp))
    poffset=3;

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {

    if (high_speed_flag == 1)
      dl_ch0     = &dl_ch_estimates[aarx][5+(symbol*(frame_parms->ofdm_symbol_size))];
    else
      dl_ch0     = &dl_ch_estimates[aarx][5];

    dl_ch0_ext = &dl_ch_estimates_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];

    rxF_ext   = &rxdataF_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];
    rxF       = &rxdataF[aarx][(frame_parms->first_carrier_offset + (symbol*(frame_parms->ofdm_symbol_size)))];

    if ((frame_parms->N_RB_DL&1) == 0)  // even number of RBs

      for (rb=0;rb<frame_parms->N_RB_DL;rb++) {

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

        if (rb_alloc_ind == 1)
          nb_rb++;

        // For second half of RBs skip DC carrier
        if (rb==(frame_parms->N_RB_DL>>1)) {
          rxF       = &rxdataF[aarx][(1 + (symbol*(frame_parms->ofdm_symbol_size)))];
          //dl_ch0++;
        }

        // PBCH
        if ((subframe==0) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=nsymb>>1) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }

        //SSS
        if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }


        if (frame_parms->frame_type == FDD) {
          //PSS
          if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if ((frame_parms->frame_type == TDD) &&
            (subframe==6)) { //TDD Subframe 6
          if ((rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if (rb_alloc_ind==1) {
          *pmi_ext = (pmi>>((rb>>2)<<1))&3;
          memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));

          /*
            printf("rb %d\n",rb);
            for (i=0;i<12;i++)
            printf("(%d %d)",((short *)dl_ch0)[i<<1],((short*)dl_ch0)[1+(i<<1)]);
            printf("\n");
          */
          if (pilots==0) {
            for (i=0; i<12; i++) {
              rxF_ext[i]=rxF[i];
              /*
                printf("%d : (%d,%d)\n",(rxF+i-&rxdataF[aarx][( (symbol*(frame_parms->ofdm_symbol_size)))]),
                ((short*)&rxF[i])[0],((short*)&rxF[i])[1]);*/
            }

            dl_ch0_ext+=12;
            rxF_ext+=12;
          } else {
            j=0;

            for (i=0; i<12; i++) {
              if ((i!=(frame_parms->nushift+poffset)) &&
                  (i!=((frame_parms->nushift+poffset+6)%12))) {
                rxF_ext[j]=rxF[i];
                //            printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
                dl_ch0_ext[j++]=dl_ch0[i];

              }
            }

            dl_ch0_ext+=10;
            rxF_ext+=10;
          }


        }

        dl_ch0+=12;
        rxF+=12;

      }
    else {  // Odd number of RBs
      for (rb=0; rb<frame_parms->N_RB_DL>>1; rb++) {
#ifdef DEBUG_DLSCH_DEMOD
        printf("dlch_ext %d\n",dl_ch0_ext-&dl_ch_estimates_ext[aarx][0]);
#endif
        skip_half=0;

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

        if (rb_alloc_ind == 1)
          nb_rb++;


        // PBCH
        if ((subframe==0) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }

        //PBCH subframe 0, symbols nsymb>>1 ... nsymb>>1 + 3
        if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=1;
        else if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=2;

        //SSS

        if (((subframe==0)||(subframe==5)) &&
            (rb>((frame_parms->N_RB_DL>>1)-3)) &&
            (rb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }
        //SSS
        if (((subframe==0)||(subframe==5)) &&
            (rb==((frame_parms->N_RB_DL>>1)-3)) &&
            (l==sss_symb))
          skip_half=1;
        else if (((subframe==0)||(subframe==5)) &&
                 (rb==((frame_parms->N_RB_DL>>1)+3)) &&
                 (l==sss_symb))
          skip_half=2;

        //PSS in subframe 0/5 if FDD
        if (frame_parms->frame_type == FDD) {  //FDD

          if (((subframe==0)||(subframe==5)) &&
              (rb>((frame_parms->N_RB_DL>>1)-3)) &&
              (rb<((frame_parms->N_RB_DL>>1)+3)) &&
              (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if ((frame_parms->frame_type == TDD) &&
            (subframe==6)){  //TDD Subframe 6
          if ((rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
          if ((rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if ((rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }


        if (rb_alloc_ind==1) {

#ifdef DEBUG_DLSCH_DEMOD
          printf("rb %d/symbol %d (skip_half %d)\n",rb,l,skip_half);
#endif
          if (pilots==0) {
            //      printf("Extracting w/o pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            if (skip_half==1) {
              memcpy(dl_ch0_ext,dl_ch0,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=6;
              rxF_ext+=6;
            } else if (skip_half==2) {
              memcpy(dl_ch0_ext,dl_ch0+6,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=6;
              rxF_ext+=6;
            } else {
              memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));

              for (i=0; i<12; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=12;
              rxF_ext+=12;
            }
          } else {
            //      printf("Extracting with pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            j=0;

            if (skip_half==1) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              }
              rxF_ext+=5;
              dl_ch0_ext+=5;
            } else if (skip_half==2) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i+6];
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else {
              for (i=0; i<12; i++) {
                if ((i!=(frame_parms->nushift+poffset)) &&
                    (i!=((frame_parms->nushift+poffset+6)%12))) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];

                }
              }

              dl_ch0_ext+=10;
              rxF_ext+=10;
            }
          }
        }
        dl_ch0+=12;
        rxF+=12;
      } // first half loop


      // Do middle RB (around DC)
      if (rb < 32)
        rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
      else if (rb < 64)
        rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
      else if (rb < 96)
        rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
      else if (rb < 100)
        rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
      else
        rb_alloc_ind = 0;


      if (rb_alloc_ind == 1)
        nb_rb++;

      // PBCH

      if ((subframe==0) &&
          (l>=(nsymb>>1)) &&
          (l<((nsymb>>1) + 4))) {
        rb_alloc_ind = 0;
      }

      //SSS
      if (((subframe==0)||(subframe==5)) && (l==sss_symb) ) {
        rb_alloc_ind = 0;
      }

      if (frame_parms->frame_type == FDD) {
        //PSS
        if (((subframe==0)||(subframe==5)) && (l==pss_symb) ) {
          rb_alloc_ind = 0;
        }
      }

      //PSS
      if ((frame_parms->frame_type == TDD) &&
          (subframe==6) &&
          (l==pss_symb) ) {
        rb_alloc_ind = 0;
      }


      //  printf("dlch_ext %d\n",dl_ch0_ext-&dl_ch_estimates_ext[aarx][0]);
      //      printf("DC rb %d (%p)\n",rb,rxF);
      if (rb_alloc_ind==1) {
#ifdef DEBUG_DLSCH_DEMOD
        printf("rb %d/symbol %d (skip_half %d)\n",rb,l,skip_half);
#endif
        if (pilots==0) {
          for (i=0; i<6; i++) {
            dl_ch0_ext[i]=dl_ch0[i];
            rxF_ext[i]=rxF[i];
          }

          rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];

          for (; i<12; i++) {
            dl_ch0_ext[i]=dl_ch0[i];
            rxF_ext[i]=rxF[(1+i-6)];
          }

          dl_ch0_ext+=12;
          rxF_ext+=12;
        } else { // pilots==1
          j=0;

          for (i=0; i<6; i++) {
            if (i!=((frame_parms->nushift+poffset)%6)) {
              dl_ch0_ext[j]=dl_ch0[i];
              rxF_ext[j++]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("**extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j-1],*(1+(short*)&rxF_ext[j-1]));
#endif
            }
          }

          rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];

          for (; i<12; i++) {
            if (i!=((frame_parms->nushift+6+poffset)%12)) {
              dl_ch0_ext[j]=dl_ch0[i];
              rxF_ext[j++]=rxF[(1+i-6)];
#ifdef DEBUG_DLSCH_DEMOD
              printf("**extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j-1],*(1+(short*)&rxF_ext[j-1]));
#endif
            }
          }

          dl_ch0_ext+=10;
          rxF_ext+=10;
        } // symbol_mod==0
      } // rballoc==1
      else {
        rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];
      }

      dl_ch0+=12;
      rxF+=7;
      rb++;

      for (;rb<frame_parms->N_RB_DL;rb++) {
        //      printf("dlch_ext %d\n",dl_ch0_ext-&dl_ch_estimates_ext[aarx][0]);
        //      printf("rb %d (%p)\n",rb,rxF);
        skip_half=0;

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

        if (rb_alloc_ind == 1)
          nb_rb++;

        // PBCH
        if ((subframe==0) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=nsymb>>1) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }
        //PBCH subframe 0, symbols nsymb>>1 ... nsymb>>1 + 3
        if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=1;
        else if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=2;

        //SSS
        if (((subframe==0)||(subframe==5)) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }
        //SSS
        if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==sss_symb))
          skip_half=1;
        else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb))
          skip_half=2;
        if (frame_parms->frame_type == FDD) {
          //PSS
          if (((subframe==0)||(subframe==5)) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          //PSS

          if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if ((frame_parms->frame_type == TDD) &&

            (subframe==6)) { //TDD Subframe 6
          if ((rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if ((rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if ((rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if (rb_alloc_ind==1) {
#ifdef DEBUG_DLSCH_DEMOD
          printf("rb %d/symbol %d (skip_half %d)\n",rb,l,skip_half);
#endif
          /*
            printf("rb %d\n",rb);
            for (i=0;i<12;i++)
            printf("(%d %d)",((short *)dl_ch0)[i<<1],((short*)dl_ch0)[1+(i<<1)]);
            printf("\n");
          */
          if (pilots==0) {
            //      printf("Extracting w/o pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            if (skip_half==1) {
              memcpy(dl_ch0_ext,dl_ch0,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=6;
              rxF_ext+=6;

            } else if (skip_half==2) {
              memcpy(dl_ch0_ext,dl_ch0+6,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=6;
              rxF_ext+=6;

            } else {
              memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));

              for (i=0; i<12; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
              dl_ch0_ext+=12;
              rxF_ext+=12;
            }
          } else {
            //      printf("Extracting with pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            j=0;

            if (skip_half==1) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else if (skip_half==2) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i+6];
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else {
              for (i=0; i<12; i++) {
                if ((i!=(frame_parms->nushift+poffset)) &&
                    (i!=((frame_parms->nushift+poffset+6)%12))) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              }
              dl_ch0_ext+=10;
              rxF_ext+=10;
            }
          } // pilots=0
        }

        dl_ch0+=12;
        rxF+=12;
      }
    }
  }


  return(nb_rb/frame_parms->nb_antennas_rx);
}

unsigned short dlsch_extract_rbs_dual(int **rxdataF,
                                      int **dl_ch_estimates,
                                      int **rxdataF_ext,
                                      int **dl_ch_estimates_ext,
                                      unsigned short pmi,
                                      unsigned char *pmi_ext,
                                      unsigned int *rb_alloc,
                                      unsigned char symbol,
                                      unsigned char subframe,
                                      uint32_t high_speed_flag,
                                      LTE_DL_FRAME_PARMS *frame_parms,
                                      MIMO_mode_t mimo_mode)
{
  int prb,nb_rb=0;
  int prb_off,prb_off2;
  int rb_alloc_ind,skip_half=0,sss_symb,pss_symb=0,nsymb,l;
  int i,aarx;
  int32_t *dl_ch0,*dl_ch0p,*dl_ch0_ext,*dl_ch1,*dl_ch1p,*dl_ch1_ext,*rxF,*rxF_ext;
  int symbol_mod,pilots=0,j=0;
  unsigned char *pmi_loc;

  symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;
  //  printf("extract_rbs: symbol_mod %d\n",symbol_mod);

  if ((symbol_mod == 0) || (symbol_mod == (4-frame_parms->Ncp)))
    pilots=1;

  nsymb = (frame_parms->Ncp==NORMAL) ? 14:12;
  l=symbol;

  if (frame_parms->frame_type == TDD) {  // TDD
    sss_symb = nsymb-1;
    pss_symb = 2;
  } else {
    sss_symb = (nsymb>>1)-2;
    pss_symb = (nsymb>>1)-1;
  }

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {

    if (high_speed_flag==1) {
      dl_ch0     = &dl_ch_estimates[aarx][5+(symbol*(frame_parms->ofdm_symbol_size))];
      dl_ch1     = &dl_ch_estimates[2+aarx][5+(symbol*(frame_parms->ofdm_symbol_size))];
    } else {
      dl_ch0     = &dl_ch_estimates[aarx][5];
      dl_ch1     = &dl_ch_estimates[2+aarx][5];
    }

    pmi_loc = pmi_ext;

    // pointers to extracted RX signals and channel estimates
    rxF_ext    = &rxdataF_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];
    dl_ch0_ext = &dl_ch_estimates_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];
    dl_ch1_ext = &dl_ch_estimates_ext[2+aarx][symbol*(frame_parms->N_RB_DL*12)];

    for (prb=0; prb<frame_parms->N_RB_DL; prb++) {
      skip_half=0;

      if (prb < 32)
        rb_alloc_ind = (rb_alloc[0]>>prb) & 1;
      else if (prb < 64)
        rb_alloc_ind = (rb_alloc[1]>>(prb-32)) & 1;
      else if (prb < 96)
        rb_alloc_ind = (rb_alloc[2]>>(prb-64)) & 1;
      else if (prb < 100)
        rb_alloc_ind = (rb_alloc[3]>>(prb-96)) & 1;
      else
        rb_alloc_ind = 0;

      if (rb_alloc_ind == 1)
          nb_rb++;


      if ((frame_parms->N_RB_DL&1) == 0) {  // even number of RBs

        // PBCH
        if ((subframe==0) &&
            (prb>=((frame_parms->N_RB_DL>>1)-3)) &&
            (prb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l>=(nsymb>>1)) &&
            (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
          //    printf("symbol %d / rb %d: skipping PBCH REs\n",symbol,prb);
        }

        //SSS

        if (((subframe==0)||(subframe==5)) &&
            (prb>=((frame_parms->N_RB_DL>>1)-3)) &&
            (prb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l==sss_symb) ) {
          rb_alloc_ind = 0;
          //    printf("symbol %d / rb %d: skipping SSS REs\n",symbol,prb);
        }



        //PSS in subframe 0/5 if FDD
        if (frame_parms->frame_type == FDD) {  //FDD
          if (((subframe==0)||(subframe==5)) &&
              (prb>=((frame_parms->N_RB_DL>>1)-3)) &&
              (prb<((frame_parms->N_RB_DL>>1)+3)) &&
              (l==pss_symb) ) {
            rb_alloc_ind = 0;
            //    printf("symbol %d / rb %d: skipping PSS REs\n",symbol,prb);
          }
        }

        if ((frame_parms->frame_type == TDD) &&
            (subframe==6)) { //TDD Subframe 6
          if ((prb>=((frame_parms->N_RB_DL>>1)-3)) &&
              (prb<((frame_parms->N_RB_DL>>1)+3)) &&
              (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if (rb_alloc_ind==1) {              // PRB is allocated



          prb_off      = 12*prb;
          prb_off2     = 1+(12*(prb-(frame_parms->N_RB_DL>>1)));
          dl_ch0p    = dl_ch0+(12*prb);
          dl_ch1p    = dl_ch1+(12*prb);
          if (prb<(frame_parms->N_RB_DL>>1)){
            rxF      = &rxdataF[aarx][prb_off+
                                      frame_parms->first_carrier_offset +
                                      (symbol*(frame_parms->ofdm_symbol_size))];
          }
          else {
            rxF      = &rxdataF[aarx][prb_off2+
                                      (symbol*(frame_parms->ofdm_symbol_size))];
          }

         /*
         if (mimo_mode <= PUSCH_PRECODING1)
          *pmi_loc = (pmi>>((prb>>2)<<1))&3;
         else
          *pmi_loc=(pmi>>prb)&1;*/

         *pmi_loc = get_pmi(frame_parms->N_RB_DL,mimo_mode,pmi,prb);
          pmi_loc++;


          if (pilots == 0) {

            memcpy(dl_ch0_ext,dl_ch0p,12*sizeof(int));
            memcpy(dl_ch1_ext,dl_ch1p,12*sizeof(int));
            memcpy(rxF_ext,rxF,12*sizeof(int));
            dl_ch0_ext +=12;
            dl_ch1_ext +=12;
            rxF_ext    +=12;
          } else { // pilots==1
            j=0;
            for (i=0; i<12; i++) {
              if ((i!=frame_parms->nushift) &&
                  (i!=frame_parms->nushift+3) &&
                  (i!=frame_parms->nushift+6) &&
                  (i!=((frame_parms->nushift+9)%12))) {
                rxF_ext[j]=rxF[i];
                //        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
                dl_ch0_ext[j]=dl_ch0p[i];
                dl_ch1_ext[j++]=dl_ch1p[i];
              }
            }
            dl_ch0_ext+=8;
            dl_ch1_ext+=8;
            rxF_ext+=8;
          } // pilots==1

        }
      } else {  // Odd number of RBs


      // PBCH
        if ((subframe==0) &&
            (prb>((frame_parms->N_RB_DL>>1)-3)) &&
            (prb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l>=(nsymb>>1)) &&
            (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
          //    printf("symbol %d / rb %d: skipping PBCH REs\n",symbol,prb);
        }

        //SSS

        if (((subframe==0)||(subframe==5)) &&
            (prb>((frame_parms->N_RB_DL>>1)-3)) &&
            (prb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l==sss_symb) ) {
          rb_alloc_ind = 0;
          //    printf("symbol %d / rb %d: skipping SSS REs\n",symbol,prb);
        }



        //PSS in subframe 0/5 if FDD
        if (frame_parms->frame_type == FDD) {  //FDD
          if (((subframe==0)||(subframe==5)) &&
              (prb>((frame_parms->N_RB_DL>>1)-3)) &&
              (prb<((frame_parms->N_RB_DL>>1)+3)) &&
              (l==pss_symb) ) {
            rb_alloc_ind = 0;
            //    printf("symbol %d / rb %d: skipping PSS REs\n",symbol,prb);
          }
        }

        if ((frame_parms->frame_type == TDD) &&
            ((subframe==1) || (subframe==6))) { //TDD Subframe 1-6
          if ((prb>((frame_parms->N_RB_DL>>1)-3)) &&
              (prb<((frame_parms->N_RB_DL>>1)+3)) &&
              (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if (rb_alloc_ind == 1) {
          skip_half=0;

          //Check if we have to drop half a PRB due to PSS/SSS/PBCH
          // skip_half == 0 means full PRB
          // skip_half == 1 means first half is used (leftmost half-PRB from PSS/SSS/PBCH)
          // skip_half == 2 means second half is used (rightmost half-PRB from PSS/SSS/PBCH)
          //PBCH subframe 0, symbols nsymb>>1 ... nsymb>>1 + 3
          if ((subframe==0) &&
              (prb==((frame_parms->N_RB_DL>>1)-3)) &&
              (l>=(nsymb>>1)) &&
              (l<((nsymb>>1) + 4)))
            skip_half=1;
          else if ((subframe==0) &&
                   (prb==((frame_parms->N_RB_DL>>1)+3)) &&
                   (l>=(nsymb>>1)) &&
                   (l<((nsymb>>1) + 4)))
            skip_half=2;

          //SSS
          if (((subframe==0)||(subframe==5)) &&
              (prb==((frame_parms->N_RB_DL>>1)-3)) &&
              (l==sss_symb))
            skip_half=1;
          else if (((subframe==0)||(subframe==5)) &&
                   (prb==((frame_parms->N_RB_DL>>1)+3)) &&
                   (l==sss_symb))
            skip_half=2;

          //PSS Subframe 0,5
          if (((frame_parms->frame_type == FDD) &&
               (((subframe==0)||(subframe==5)))) ||  //FDD Subframes 0,5
              ((frame_parms->frame_type == TDD) &&
               (((subframe==1) || (subframe==6))))) { //TDD Subframes 1,6

            if ((prb==((frame_parms->N_RB_DL>>1)-3)) &&
                (l==pss_symb))
              skip_half=1;
            else if ((prb==((frame_parms->N_RB_DL>>1)+3)) &&
                     (l==pss_symb))
              skip_half=2;
          }


          prb_off      = 12*prb;
          prb_off2     = 7+(12*(prb-(frame_parms->N_RB_DL>>1)-1));
          dl_ch0p      = dl_ch0+(12*prb);
          dl_ch1p      = dl_ch1+(12*prb);

          if (prb<=(frame_parms->N_RB_DL>>1)){
            rxF      = &rxdataF[aarx][prb_off+
                                      frame_parms->first_carrier_offset +
                                      (symbol*(frame_parms->ofdm_symbol_size))];
          }
          else {
            rxF      = &rxdataF[aarx][prb_off2+
                                      (symbol*(frame_parms->ofdm_symbol_size))];
          }
#ifdef DEBUG_DLSCH_DEMOD
          printf("symbol %d / rb %d: alloc %d skip_half %d (rxF %p, rxF_ext %p) prb_off (%d,%d)\n",symbol,prb,rb_alloc_ind,skip_half,rxF,rxF_ext,prb_off,prb_off2);
#endif
         /* if (mimo_mode <= PUSCH_PRECODING1)
           *pmi_loc = (pmi>>((prb>>2)<<1))&3;
          else
           *pmi_loc=(pmi>>prb)&1;
         // printf("symbol_mod %d (pilots %d) rb %d, sb %d, pmi %d (pmi_loc %p,rxF %p, ch00 %p, ch01 %p, rxF_ext %p dl_ch0_ext %p dl_ch1_ext %p)\n",symbol_mod,pilots,prb,prb>>2,*pmi_loc,pmi_loc,rxF,dl_ch0, dl_ch1, rxF_ext,dl_ch0_ext,dl_ch1_ext);
*/
         *pmi_loc = get_pmi(frame_parms->N_RB_DL,mimo_mode,pmi,prb);
          pmi_loc++;

          if (prb != (frame_parms->N_RB_DL>>1)) { // This PRB is not around DC
            if (pilots==0) {
              if (skip_half==1) {
                memcpy(dl_ch0_ext,dl_ch0p,6*sizeof(int32_t));
                memcpy(dl_ch1_ext,dl_ch1p,6*sizeof(int32_t));
                memcpy(rxF_ext,rxF,6*sizeof(int32_t));
#ifdef DEBUG_DLSCH_DEMOD
                for (i=0;i<6;i++)
                  printf("extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                dl_ch0_ext+=6;
                dl_ch1_ext+=6;
                rxF_ext+=6;
              } else if (skip_half==2) {
                memcpy(dl_ch0_ext,dl_ch0p+6,6*sizeof(int32_t));
                memcpy(dl_ch1_ext,dl_ch1p+6,6*sizeof(int32_t));
                memcpy(rxF_ext,rxF+6,6*sizeof(int32_t));
#ifdef DEBUG_DLSCH_DEMOD
                for (i=0;i<6;i++)
                  printf("extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                dl_ch0_ext+=6;
                dl_ch1_ext+=6;
                rxF_ext+=6;
              } else {  // skip_half==0
                memcpy(dl_ch0_ext,dl_ch0p,12*sizeof(int32_t));
                memcpy(dl_ch1_ext,dl_ch1p,12*sizeof(int32_t));
                memcpy(rxF_ext,rxF,12*sizeof(int32_t));
#ifdef DEBUG_DLSCH_DEMOD
                for (i=0;i<12;i++)
                  printf("extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                dl_ch0_ext+=12;
                dl_ch1_ext+=12;
                rxF_ext+=12;
              }
            } else { // pilots=1
              j=0;

              if (skip_half==1) {
                for (i=0; i<6; i++) {
                  if ((i!=frame_parms->nushift) &&
                      (i!=((frame_parms->nushift+3)%6))) {
                    rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                    printf("(pilots,skip1)extract rb %d, re %d (%d)=> (%d,%d)\n",prb,i,j,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                    dl_ch0_ext[j]=dl_ch0p[i];
                    dl_ch1_ext[j++]=dl_ch1p[i];
                  }
                }
                dl_ch0_ext+=4;
                dl_ch1_ext+=4;
                rxF_ext+=4;
              } else if (skip_half==2) {
                for (i=0; i<6; i++) {
                  if ((i!=frame_parms->nushift) &&
                      (i!=((frame_parms->nushift+3)%6))) {
                    rxF_ext[j]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
                    printf("(pilots,skip2)extract rb %d, re %d (%d) => (%d,%d)\n",prb,i,j,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                    dl_ch0_ext[j]=dl_ch0p[i+6];
                    dl_ch1_ext[j++]=dl_ch1p[i+6];
                  }
                }
                dl_ch0_ext+=4;
                dl_ch1_ext+=4;
                rxF_ext+=4;

              } else { //skip_half==0
                for (i=0; i<12; i++) {
                  if ((i!=frame_parms->nushift) &&
                      (i!=frame_parms->nushift+3) &&
                      (i!=frame_parms->nushift+6) &&
                      (i!=((frame_parms->nushift+9)%12))) {
                    rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                    printf("(pilots)extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                    dl_ch0_ext[j]  =dl_ch0p[i];
                    dl_ch1_ext[j++]=dl_ch1p[i];
                  }
                }
                dl_ch0_ext+=8;
                dl_ch1_ext+=8;
                rxF_ext+=8;
              } //skip_half==0
            } //pilots==1
          } else {       // Do middle RB (around DC)

            if (pilots==0) {
              memcpy(dl_ch0_ext,dl_ch0p,6*sizeof(int32_t));
              memcpy(dl_ch1_ext,dl_ch1p,6*sizeof(int32_t));
              memcpy(rxF_ext,rxF,6*sizeof(int32_t));
#ifdef DEBUG_DLSCH_DEMOD
              for (i=0; i<6; i++) {
                printf("extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
              }
#endif
              rxF_ext+=6;
              dl_ch0_ext+=6;
              dl_ch1_ext+=6;
              dl_ch0p+=6;
              dl_ch1p+=6;

              rxF       = &rxdataF[aarx][1+((symbol*(frame_parms->ofdm_symbol_size)))];

              memcpy(dl_ch0_ext,dl_ch0p,6*sizeof(int32_t));
              memcpy(dl_ch1_ext,dl_ch1p,6*sizeof(int32_t));
              memcpy(rxF_ext,rxF,6*sizeof(int32_t));
#ifdef DEBUG_DLSCH_DEMOD
              for (i=0; i<6; i++) {
                printf("extract rb %d, re %d => (%d,%d)\n",prb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
              }
#endif
              rxF_ext+=6;
              dl_ch0_ext+=6;
              dl_ch1_ext+=6;
            } else { // pilots==1
              j=0;

              for (i=0; i<6; i++) {
                if ((i!=frame_parms->nushift) &&
                    (i!=((frame_parms->nushift+3)%6))) {
                  dl_ch0_ext[j]=dl_ch0p[i];
                  dl_ch1_ext[j]=dl_ch1p[i];
                  rxF_ext[j++]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("(pilots)extract rb %d, re %d (%d) => (%d,%d)\n",prb,i,j,*(short *)&rxF[i],*(1+(short*)&rxF[i]));
#endif
                }
              }
              rxF       = &rxdataF[aarx][1+symbol*(frame_parms->ofdm_symbol_size)];

              for (; i<12; i++) {
                if ((i!=((frame_parms->nushift+6)%12)) &&
                    (i!=((frame_parms->nushift+9)%12))) {
                  dl_ch0_ext[j]=dl_ch0p[i];
                  dl_ch1_ext[j]=dl_ch1p[i];
                  rxF_ext[j++]=rxF[i-6];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("(pilots)extract rb %d, re %d (%d) => (%d,%d)\n",prb,i,j,*(short *)&rxF[1+i-6],*(1+(short*)&rxF[1+i-6]));
#endif
                }
              }

              dl_ch0_ext+=8;
              dl_ch1_ext+=8;
              rxF_ext+=8;
            } //pilots==1
          }  // if Middle PRB
        } // if odd PRB
      } // if rballoc==1
    } // for prb
  } // for aarx
  return(nb_rb/frame_parms->nb_antennas_rx);
}

unsigned short dlsch_extract_rbs_TM7(int **rxdataF,
                                     int **dl_bf_ch_estimates,
                                     int **rxdataF_ext,
                                     int **dl_bf_ch_estimates_ext,
                                     unsigned int *rb_alloc,
                                     unsigned char symbol,
                                     unsigned char subframe,
                                     uint32_t high_speed_flag,
                                     LTE_DL_FRAME_PARMS *frame_parms)
{

  unsigned short rb,nb_rb=0;
  unsigned char rb_alloc_ind;
  unsigned char i,aarx,l,nsymb,skip_half=0,sss_symb,pss_symb=0;
  int *dl_ch0,*dl_ch0_ext,*rxF,*rxF_ext;

  unsigned char symbol_mod,pilots=0,uespec_pilots=0,j=0,poffset=0,uespec_poffset=0;
  int8_t uespec_nushift = frame_parms->Nid_cell%3;

  symbol_mod = (symbol>=(7-frame_parms->Ncp)) ? symbol-(7-frame_parms->Ncp) : symbol;
  pilots = ((symbol_mod==0)||(symbol_mod==(4-frame_parms->Ncp))) ? 1 : 0;
  l=symbol;
  nsymb = (frame_parms->Ncp==NORMAL) ? 14:12;

  if (frame_parms->Ncp==0){
    if (symbol==3 || symbol==6 || symbol==9 || symbol==12)
      uespec_pilots = 1;
  } else{
    if (symbol==4 || symbol==7 || symbol==10)
      uespec_pilots = 1;
  }

  if (frame_parms->frame_type == TDD) {// TDD
    sss_symb = nsymb-1;
    pss_symb = 2;
  } else {
    sss_symb = (nsymb>>1)-2;
    pss_symb = (nsymb>>1)-1;
  }

  if (symbol_mod==(4-frame_parms->Ncp))
    poffset=3;

  if ((frame_parms->Ncp==0 && (symbol==6 ||symbol ==12)) || (frame_parms->Ncp==1 && symbol==7))
    uespec_poffset=2;

  for (aarx=0; aarx<frame_parms->nb_antennas_rx; aarx++) {

    if (high_speed_flag == 1)
      dl_ch0     = &dl_bf_ch_estimates[aarx][symbol*(frame_parms->ofdm_symbol_size)];
    else
      dl_ch0     = &dl_bf_ch_estimates[aarx][0];

    dl_ch0_ext = &dl_bf_ch_estimates_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];

    rxF_ext    = &rxdataF_ext[aarx][symbol*(frame_parms->N_RB_DL*12)];
    rxF        = &rxdataF[aarx][(frame_parms->first_carrier_offset + (symbol*(frame_parms->ofdm_symbol_size)))];

    if ((frame_parms->N_RB_DL&1) == 0)  // even number of RBs
      for (rb=0; rb<frame_parms->N_RB_DL; rb++) {

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

  if (rb_alloc_ind == 1)
          nb_rb++;

        // For second half of RBs skip DC carrier
        if (rb==(frame_parms->N_RB_DL>>1)) {
          rxF       = &rxdataF[aarx][(1 + (symbol*(frame_parms->ofdm_symbol_size)))];
          //dl_ch0++;
        }

        // PBCH
        if ((subframe==0) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=nsymb>>1) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }

        //SSS
        if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }


        if (frame_parms->frame_type == FDD) {
          //PSS
          if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if ((frame_parms->frame_type == TDD) &&
            (subframe==6)) { //TDD Subframe 6
          if ((rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }
        }

        if (rb_alloc_ind==1) {

          /*
              printf("rb %d\n",rb);
              for (i=0;i<12;i++)
              printf("(%d %d)",((short *)dl_ch0)[i<<1],((short*)dl_ch0)[1+(i<<1)]);
              printf("\n");
          */
          if (pilots==0 && uespec_pilots==0) {
            memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));

            for (i=0; i<12; i++) {
              rxF_ext[i]=rxF[i];
            }

            dl_ch0_ext+=12;
            rxF_ext+=12;
          } else if(pilots==1 && uespec_pilots==0) {
            j=0;

            for (i=0; i<12; i++) {
              if ((i!=(frame_parms->nushift+poffset)) &&
                  (i!=((frame_parms->nushift+poffset+6)%12))) {
                rxF_ext[j]=rxF[i];
                dl_ch0_ext[j++]=dl_ch0[i];
              }
            }

            dl_ch0_ext+=10;
            rxF_ext+=10;

          } else if (pilots==0 && uespec_pilots==1) {
            j=0;


      for (i=0; i<12; i++){
              if (frame_parms->Ncp==0){
                if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
      rxF_ext[j] = rxF[i];
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              } else{
                if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
      rxF_ext[j] = rxF[i];
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              }

      }

            dl_ch0_ext+=9-frame_parms->Ncp;
            rxF_ext+=9-frame_parms->Ncp;

          } else {
            msg("dlsch_extract_rbs_TM7(dl_demodulation.c):pilot or ue spec pilot detection error\n");
            exit(-1);
          }

        }

        dl_ch0+=12;
        rxF+=12;

      }
    else {  // Odd number of RBs
      for (rb=0; rb<frame_parms->N_RB_DL>>1; rb++) {
        skip_half=0;

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

        if (rb_alloc_ind == 1)
          nb_rb++;

        // PBCH
        if ((subframe==0) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }

        //PBCH subframe 0, symbols nsymb>>1 ... nsymb>>1 + 3
        if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=1;
        else if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=2;

        //SSS

        if (((subframe==0)||(subframe==5)) &&
            (rb>((frame_parms->N_RB_DL>>1)-3)) &&
            (rb<((frame_parms->N_RB_DL>>1)+3)) &&
            (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }

        //SSS
        if (((subframe==0)||(subframe==5)) &&
            (rb==((frame_parms->N_RB_DL>>1)-3)) &&
            (l==sss_symb))
          skip_half=1;
        else if (((subframe==0)||(subframe==5)) &&
                 (rb==((frame_parms->N_RB_DL>>1)+3)) &&
                 (l==sss_symb))
          skip_half=2;

        //PSS in subframe 0/5 if FDD
        if (frame_parms->frame_type == FDD) {  //FDD
          if (((subframe==0)||(subframe==5)) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if ((frame_parms->frame_type == TDD) && ((subframe==1)||(subframe==6))) { //TDD Subframe 1 and 6
          if ((rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if ((rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if ((rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }


        if (rb_alloc_ind==1) {
#ifdef DEBUG_DLSCH_DEMOD
          printf("rb %d/symbol %d pilots %d, uespec_pilots %d, (skip_half %d)\n",rb,l,pilots,uespec_pilots,skip_half);
#endif

          if (pilots==0 && uespec_pilots==0) {
            //printf("Extracting w/o pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);

            if (skip_half==1) {
              memcpy(dl_ch0_ext,dl_ch0,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
    printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }

              dl_ch0_ext+=6;
              rxF_ext+=6;
            } else if (skip_half==2) {
              memcpy(dl_ch0_ext,dl_ch0+6,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[(i+6)];
#ifdef DEBUG_DLSCH_DEMOD
    printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }

              dl_ch0_ext+=6;
              rxF_ext+=6;
            } else {
              memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));

              for (i=0; i<12; i++){
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                printf("extract rb %d, re %d => (%d,%d)\n",symbol,rb,i,*(short *)&rxF[i],*(1+(short*)&rxF[i]));
#endif
              }
              dl_ch0_ext+=12;
              rxF_ext+=12;
            }
          } else if (pilots==1 && uespec_pilots==0) {
            // printf("Extracting with pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            j=0;

            if (skip_half==1) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[i];
                  dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
    printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else if (skip_half==2) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[(i+6)];
                  dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
    printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else {
              for (i=0; i<12; i++) {
                if ((i!=(frame_parms->nushift+poffset)) &&
                    (i!=((frame_parms->nushift+poffset+6)%12))) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];

                }
              }

              dl_ch0_ext+=10;
              rxF_ext+=10;
            }
          } else if(pilots==0 && uespec_pilots==1){
            //printf("Extracting with uespec pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            j=0;

            if (skip_half==1) {
              if (frame_parms->Ncp==0){
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                    rxF_ext[j]=rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=6-(uespec_nushift+uespec_poffset<6)-(uespec_nushift+uespec_poffset+4<6)-((uespec_nushift+uespec_poffset+8)%12<6);
                rxF_ext+=6-(uespec_nushift+uespec_poffset<6)-(uespec_nushift+uespec_poffset+4<6)-((uespec_nushift+uespec_poffset+8)%12<6);

              } else{
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                    rxF_ext[j]=rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=4;
                rxF_ext+=4;
              }

            } else if (skip_half==2) {
              if(frame_parms->Ncp==0){
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                    rxF_ext[j]=rxF[(i+6)];
                    dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=6-(uespec_nushift+uespec_poffset>6)-(uespec_nushift+uespec_poffset+4>6)-((uespec_nushift+uespec_poffset+8)%12>6);
                rxF_ext+=6-(uespec_nushift+uespec_poffset>6)-(uespec_nushift+uespec_poffset+4>6)-((uespec_nushift+uespec_poffset+8)%12>6);

              } else {
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                    rxF_ext[j]=rxF[(i+6)];
                    dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=4;
                rxF_ext+=4;
              }

            } else {

        for (i=0; i<12; i++){
                if (frame_parms->Ncp==0){
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
              rxF_ext[j] = rxF[i];
                    dl_ch0_ext[j++] = dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
                    printf("extract rb %d, re %d, j %d => (%d,%d)\n",symbol,rb,i,j-1,*(short *)&dl_ch0[j],*(1+(short*)&dl_ch0[i]));
#endif
                  }
                } else{
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
              rxF_ext[j] = rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }

        }

              dl_ch0_ext+=9-frame_parms->Ncp;
              rxF_ext+=9-frame_parms->Ncp;
      }

          } else {
            msg("dlsch_extract_rbs_TM7(dl_demodulation.c):pilot or ue spec pilot detection error\n");
            exit(-1);

          }
        }

        dl_ch0+=12;
        rxF+=12;
      } // first half loop


      // Do middle RB (around DC)
      if (rb < 32)
        rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
      else if (rb < 64)
        rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
      else if (rb < 96)
        rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
      else if (rb < 100)
        rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
      else
        rb_alloc_ind = 0;

      if (rb_alloc_ind == 1)
        nb_rb++;

      // PBCH
      if ((subframe==0) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4))) {
        rb_alloc_ind = 0;
      }

      //SSS
      if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb) ) {
        rb_alloc_ind = 0;
      }

      if (frame_parms->frame_type == FDD) {
        //PSS
        if (((subframe==0)||(subframe==5)) && (rb>=((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
          rb_alloc_ind = 0;
        }
      }

      if ((frame_parms->frame_type == TDD) && ((subframe==1)||(subframe==6))) {
        //PSS
        if ((rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
          rb_alloc_ind = 0;
        }
      }

      //printf("dlch_ext %d\n",dl_ch0_ext-&dl_ch_estimates_ext[aarx][0]);
      //printf("DC rb %d (%p)\n",rb,rxF);
      if (rb_alloc_ind==1) {
        //printf("rb %d/symbol %d (skip_half %d)\n",rb,l,skip_half);
        if (pilots==0 && uespec_pilots==0) {
          for (i=0; i<6; i++) {
            dl_ch0_ext[i]=dl_ch0[i];
            rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
      printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
          }

          rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];

          for (; i<12; i++) {
            dl_ch0_ext[i]=dl_ch0[i];
            rxF_ext[i]=rxF[(1+i-6)];
#ifdef DEBUG_DLSCH_DEMOD
      printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
          }

          dl_ch0_ext+=12;
          rxF_ext+=12;
        } else if(pilots==1 && uespec_pilots==0){ // pilots==1
          j=0;

          for (i=0; i<6; i++) {
            if (i!=((frame_parms->nushift+poffset)%6)) {
              dl_ch0_ext[j]=dl_ch0[i];
              rxF_ext[j++]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
            }
          }

          rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];

          for (; i<12; i++) {
            if (i!=((frame_parms->nushift+6+poffset)%12)) {
              dl_ch0_ext[j]=dl_ch0[i];
              rxF_ext[j++]=rxF[(1+i-6)];
#ifdef DEBUG_DLSCH_DEMOD
        printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
            }
          }

          dl_ch0_ext+=10;
          rxF_ext+=10;
        } else if(pilots==0 && uespec_pilots==1) {
          j=0;

    for (i=0; i<6; i++) {
            if (frame_parms->Ncp==0){
              if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                dl_ch0_ext[j]=dl_ch0[i];
          rxF_ext[j++] = rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
            } else {
              if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                dl_ch0_ext[j]=dl_ch0[i];
          rxF_ext[j++] = rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
            }
    }

          rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];

          for (; i<12; i++) {
            if (frame_parms->Ncp==0){
              if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                dl_ch0_ext[j]=dl_ch0[i];
                rxF_ext[j++]=rxF[(1+i-6)];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
            } else {
              if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                dl_ch0_ext[j]=dl_ch0[i];
          rxF_ext[j++] = rxF[(1+i-6)];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }
            }
          }

          dl_ch0_ext+=9-frame_parms->Ncp;
          rxF_ext+=9-frame_parms->Ncp;

  }// symbol_mod==0

      } // rballoc==1
      else {
        rxF       = &rxdataF[aarx][((symbol*(frame_parms->ofdm_symbol_size)))];
      }

      dl_ch0+=12;
      rxF+=7;
      rb++;

      for (; rb<frame_parms->N_RB_DL; rb++) {
        //  printf("dlch_ext %d\n",dl_ch0_ext-&dl_ch_estimates_ext[aarx][0]);
        //  printf("rb %d (%p)\n",rb,rxF);
        skip_half=0;

        if (rb < 32)
          rb_alloc_ind = (rb_alloc[0]>>rb) & 1;
        else if (rb < 64)
          rb_alloc_ind = (rb_alloc[1]>>(rb-32)) & 1;
        else if (rb < 96)
          rb_alloc_ind = (rb_alloc[2]>>(rb-64)) & 1;
        else if (rb < 100)
          rb_alloc_ind = (rb_alloc[3]>>(rb-96)) & 1;
        else
          rb_alloc_ind = 0;

        if (rb_alloc_ind==1)
          nb_rb++;

        // PBCH
        if ((subframe==0) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l>=nsymb>>1) && (l<((nsymb>>1) + 4))) {
          rb_alloc_ind = 0;
        }

        //PBCH subframe 0, symbols nsymb>>1 ... nsymb>>1 + 3
        if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=1;
        else if ((subframe==0) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l>=(nsymb>>1)) && (l<((nsymb>>1) + 4)))
          skip_half=2;

        //SSS
        if (((subframe==0)||(subframe==5)) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb) ) {
          rb_alloc_ind = 0;
        }

        //SSS
        if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==sss_symb))
          skip_half=1;
        else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==sss_symb))
          skip_half=2;

        //PSS
        if (frame_parms->frame_type == FDD) {
          if (((subframe==0)||(subframe==5)) && (rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if (((subframe==0)||(subframe==5)) && (rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if ((frame_parms->frame_type == TDD) && ((subframe==1)||(subframe==6))) { //TDD Subframe 1 and 6
          if ((rb>((frame_parms->N_RB_DL>>1)-3)) && (rb<((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb) ) {
            rb_alloc_ind = 0;
          }

          if ((rb==((frame_parms->N_RB_DL>>1)-3)) && (l==pss_symb))
            skip_half=1;
          else if ((rb==((frame_parms->N_RB_DL>>1)+3)) && (l==pss_symb))
            skip_half=2;
        }

        if (rb_alloc_ind==1) {
#ifdef DEBUG_DLSCH_DEMOD
           printf("rb %d/symbol %d (skip_half %d)\n",rb,l,skip_half);
#endif
          /*
              printf("rb %d\n",rb);
            for (i=0;i<12;i++)
            printf("(%d %d)",((short *)dl_ch0)[i<<1],((short*)dl_ch0)[1+(i<<1)]);
            printf("\n");
          */
          if (pilots==0 && uespec_pilots==0) {
            //printf("Extracting w/o pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            if (skip_half==1) {
              memcpy(dl_ch0_ext,dl_ch0,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }

              dl_ch0_ext+=6;
              rxF_ext+=6;

            } else if (skip_half==2) {
              memcpy(dl_ch0_ext,dl_ch0+6,6*sizeof(int));

              for (i=0; i<6; i++) {
                rxF_ext[i]=rxF[i+6];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }

              dl_ch0_ext+=6;
              rxF_ext+=6;

            } else {
              memcpy(dl_ch0_ext,dl_ch0,12*sizeof(int));
              //printf("symbol %d, extract rb %d, => (%d,%d)\n",symbol,rb,*(short *)&dl_ch0[j],*(1+(short*)&dl_ch0[i]));

              for (i=0; i<12; i++) {
                rxF_ext[i]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
          printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
              }

              dl_ch0_ext+=12;
              rxF_ext+=12;
            }
          } else if (pilots==1 && uespec_pilots==0){
            //printf("Extracting with pilots (symbol %d, rb %d, skip_half %d)\n",l,rb,skip_half);
            j=0;

            if (skip_half==1) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[i];
                  dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
            printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else if (skip_half==2) {
              for (i=0; i<6; i++) {
                if (i!=((frame_parms->nushift+poffset)%6)) {
                  rxF_ext[j]=rxF[(i+6)];
                  dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
            printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                }
              }

              dl_ch0_ext+=5;
              rxF_ext+=5;
            } else {
              for (i=0; i<12; i++) {
                if ((i!=(frame_parms->nushift+poffset)) &&
                    (i!=((frame_parms->nushift+poffset+6)%12))) {
                  rxF_ext[j]=rxF[i];
#ifdef DEBUG_DLSCH_DEMOD
                  printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[j],*(1+(short*)&rxF_ext[j]));
#endif
                  dl_ch0_ext[j++]=dl_ch0[i];
                }
              }

              dl_ch0_ext+=10;
              rxF_ext+=10;
            }
          } else if(pilots==0 && uespec_pilots==1) {
            j=0;

            if (skip_half==1) {
              if (frame_parms->Ncp==0){
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                    rxF_ext[j]=rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=6-(uespec_nushift+uespec_poffset<6)-(uespec_nushift+uespec_poffset+4<6)-((uespec_nushift+uespec_poffset+8)%12<6);
                rxF_ext+=6-(uespec_nushift+uespec_poffset<6)-(uespec_nushift+uespec_poffset+4<6)-((uespec_nushift+uespec_poffset+8)%12<6);

              } else{
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                    rxF_ext[j]=rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=4;
                rxF_ext+=4;
              }

            } else if (skip_half==2) {
              if(frame_parms->Ncp==0){
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
                    rxF_ext[j]=rxF[i+6];
                    dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=6-(uespec_nushift+uespec_poffset>6)-(uespec_nushift+uespec_poffset+4>6)-((uespec_nushift+uespec_poffset+8)%12>6);
                rxF_ext+=6-(uespec_nushift+uespec_poffset>6)-(uespec_nushift+uespec_poffset+4>6)-((uespec_nushift+uespec_poffset+8)%12>6);

              } else {
                for (i=0; i<6; i++) {
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
                    rxF_ext[j]=rxF[(i+6)];
                    dl_ch0_ext[j++]=dl_ch0[i+6];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
                dl_ch0_ext+=4;
                rxF_ext+=4;
              }

            } else {
        for (i=0; i<12; i++){
                if (frame_parms->Ncp==0){
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+4 && i!=(uespec_nushift+uespec_poffset+8)%12){
              rxF_ext[j] = rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                } else{
                  if (i!=uespec_nushift+uespec_poffset && i!=uespec_nushift+uespec_poffset+3 && i!=uespec_nushift+uespec_poffset+6 && i!=(uespec_nushift+uespec_poffset+9)%12){
              rxF_ext[j] = rxF[i];
                    dl_ch0_ext[j++]=dl_ch0[i];
#ifdef DEBUG_DLSCH_DEMOD
              printf("extract rb %d, re %d => (%d,%d)\n",rb,i,*(short *)&rxF_ext[i],*(1+(short*)&rxF_ext[i]));
#endif
                  }
                }
        }

              dl_ch0_ext+=9-frame_parms->Ncp;
              rxF_ext+=9-frame_parms->Ncp;

            }

          }// pilots=0
        }

        dl_ch0+=12;
        rxF+=12;
      }
    }
  }

  _mm_empty();
  _m_empty();

  return(nb_rb/frame_parms->nb_antennas_rx);
}
