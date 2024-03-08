/* auto generated: Monday, August 15th, 2016 12:26:47pm */
/*
 * Copyright (c) 2016, Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Intel nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __MNH_HWIO_PCIE_SS_
#define __MNH_HWIO_PCIE_SS_

#define HWIO_PCIE_SS_PCIE_SW_INTR_TRIGG_REGOFF 0x0
#define HWIO_PCIE_SS_PCIE_SW_INTR_TRIGG_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_SW_INTR_TRIGG_REGOFF)
#define HWIO_PCIE_SS_PCIE_SW_INTR_TRIGG_PCIE_SW_INTR_TRIGG_FLDMASK (0xffffffff)
#define HWIO_PCIE_SS_PCIE_SW_INTR_TRIGG_PCIE_SW_INTR_TRIGG_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_REGOFF 0x4
#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_SW_INTR_EN_REGOFF)
#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_RSVD0_FLDMASK (0x80000000)
#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_RSVD0_FLDSHFT (31)
#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_PCIE_SW_INTR_EN_FLDMASK (0x7fffffff)
#define HWIO_PCIE_SS_PCIE_SW_INTR_EN_PCIE_SW_INTR_EN_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_REGOFF 0x8
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_SS_INTR_STS_REGOFF)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD0_FLDMASK (0xff000000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD0_FLDSHFT (24)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_TRGT_CPL_TIMEOUT_FLDMASK (0x800000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_TRGT_CPL_TIMEOUT_FLDSHFT (23)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_RADM_CPL_TIMEOUT_FLDMASK (0x400000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_RADM_CPL_TIMEOUT_FLDSHFT (22)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_PM_TURNOFF_FLDMASK (0x200000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_PM_TURNOFF_FLDSHFT (21)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_RADM_MSG_UNLOCK_FLDMASK (0x100000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_RADM_MSG_UNLOCK_FLDSHFT (20)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD1_FLDMASK (0x80000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD1_FLDSHFT (19)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_FATAL_ERR_FLDMASK (0x40000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_FATAL_ERR_FLDSHFT (18)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_NONFATAL_ERR_FLDMASK (0x20000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_NONFATAL_ERR_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_COR_ERR_FLDMASK (0x10000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_COR_ERR_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD2_FLDMASK (0xfe00)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_RSVD2_FLDSHFT (9)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_FRS_SENT_FLDMASK (0x100)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_FRS_SENT_FLDSHFT (8)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_DRS_SENT_FLDMASK (0x80)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_DRS_SENT_FLDSHFT (7)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LTR_SENT_FLDMASK (0x40)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LTR_SENT_FLDSHFT (6)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG0_RXD_FLDMASK (0x20)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG0_RXD_FLDSHFT (5)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG1_RXD_FLDMASK (0x10)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG1_RXD_FLDSHFT (4)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG_SENT_FLDMASK (0x8)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_VMSG_SENT_FLDSHFT (3)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_MSI_SENT_FLDMASK (0x4)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_MSI_SENT_FLDSHFT (2)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LINK_REQ_RST_NOT_FLDMASK (0x2)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LINK_REQ_RST_NOT_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LINK_EQ_REQ_INT_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_SS_INTR_STS_PCIE_LINK_EQ_REQ_INT_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_REGOFF 0x0C
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_SS_INTR_EN_REGOFF)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD0_FLDMASK (0xff000000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD0_FLDSHFT (24)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_TRGT_CPL_TIMEOUT_IE_FLDMASK (0x800000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_TRGT_CPL_TIMEOUT_IE_FLDSHFT (23)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_RADM_CPL_TIMEOUT_IE_FLDMASK (0x400000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_RADM_CPL_TIMEOUT_IE_FLDSHFT (22)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_PM_TURNOFF_IE_FLDMASK (0x200000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_PM_TURNOFF_IE_FLDSHFT (21)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_RADM_MSG_UNLOCK_IE_FLDMASK (0x100000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_RADM_MSG_UNLOCK_IE_FLDSHFT (20)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD1_FLDMASK (0x80000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD1_FLDSHFT (19)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_FATAL_ERR_IE_FLDMASK (0x40000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_FATAL_ERR_IE_FLDSHFT (18)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_NONFATAL_ERR_IE_FLDMASK (0x20000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_NONFATAL_ERR_IE_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_COR_ERR_IE_FLDMASK (0x10000)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_COR_ERR_IE_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD2_FLDMASK (0xfe00)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_RSVD2_FLDSHFT (9)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_FRS_SENT_IE_FLDMASK (0x100)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_FRS_SENT_IE_FLDSHFT (8)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_DRS_SENT_IE_FLDMASK (0x80)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_DRS_SENT_IE_FLDSHFT (7)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LTR_SENT_IE_FLDMASK (0x40)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LTR_SENT_IE_FLDSHFT (6)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG0_RXD_IE_FLDMASK (0x20)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG0_RXD_IE_FLDSHFT (5)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG1_RXD_IE_FLDMASK (0x10)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG1_RXD_IE_FLDSHFT (4)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG_SENT_IE_FLDMASK (0x8)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_VMSG_SENT_IE_FLDSHFT (3)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_MSI_SENT_IE_FLDMASK (0x4)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_MSI_SENT_IE_FLDSHFT (2)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LINK_REQ_RST_NOT_IE_FLDMASK (0x2)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LINK_REQ_RST_NOT_IE_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LINK_EQ_REQ_IE_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_SS_INTR_EN_PCIE_LINK_EQ_REQ_IE_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_APP_CTRL_REGOFF 0x10
#define HWIO_PCIE_SS_PCIE_APP_CTRL_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_APP_CTRL_REGOFF)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_RSVD0_FLDMASK (0xffc00000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_RSVD0_FLDSHFT (22)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_CMNREF_CLK_MODE_FLDMASK (0x200000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_CMNREF_CLK_MODE_FLDSHFT (21)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_SRAM_LD_DONE_FLDMASK (0x100000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_SRAM_LD_DONE_FLDSHFT (20)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_SRAM_BYPASS_FLDMASK (0x80000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PHY_SRAM_BYPASS_FLDSHFT (19)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_TEST_BYPASS_LP_FLDMASK (0x40000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_TEST_BYPASS_LP_FLDSHFT (18)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_TX_LANE_FLIP_EN_FLDMASK (0x20000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_TX_LANE_FLIP_EN_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_RX_LANE_FLIP_EN_FLDMASK (0x10000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_RX_LANE_FLIP_EN_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_RSVD1_FLDMASK (0xc000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_RSVD1_FLDSHFT (14)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_FRS_READY_FLDMASK (0x2000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_FRS_READY_FLDSHFT (13)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_DRS_READY_FLDMASK (0x1000)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_DRS_READY_FLDSHFT (12)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_CLK_PM_EN_FLDMASK (0x800)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_CLK_PM_EN_FLDSHFT (11)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_XFER_PEND_FLDMASK (0x400)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_XFER_PEND_FLDSHFT (10)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_EXIT_L1_FLDMASK (0x200)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_EXIT_L1_FLDSHFT (9)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_READY_ENTRY_L23_FLDMASK (0x100)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_READY_ENTRY_L23_FLDSHFT (8)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_ENTRY_L1_FLDMASK (0x80)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_ENTRY_L1_FLDSHFT (7)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_SYS_AUX_PWR_DET_FLDMASK (0x40)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_SYS_AUX_PWR_DET_FLDSHFT (6)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_PM_XMT_PME_FLDMASK (0x20)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_PM_XMT_PME_FLDSHFT (5)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PERST_N_FLDMASK (0x10)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_PERST_N_FLDSHFT (4)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_CLK_REQ_N_EN_FLDMASK (0x8)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_CLK_REQ_N_EN_FLDSHFT (3)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_INIT_RST_EN_FLDMASK (0x4)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_INIT_RST_EN_FLDSHFT (2)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_RETRY_EN_FLDMASK (0x2)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_REQ_RETRY_EN_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_LTSSM_EN_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_APP_CTRL_PCIE_APP_LTSSM_EN_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_APP_STS_REGOFF 0x14
#define HWIO_PCIE_SS_PCIE_APP_STS_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_APP_STS_REGOFF)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PHY_SRAM_INIT_DONE_FLDMASK (0x80000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PHY_SRAM_INIT_DONE_FLDSHFT (31)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD0_FLDMASK (0x40000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD0_FLDSHFT (30)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_BRDG_SLV_XFER_PEND_FLDMASK (0x20000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_BRDG_SLV_XFER_PEND_FLDSHFT (29)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_BRDG_DBI_XFER_PEND_FLDMASK (0x10000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_BRDG_DBI_XFER_PEND_FLDSHFT (28)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_EDMA_XFER_PEND_FLDMASK (0x8000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_EDMA_XFER_PEND_FLDSHFT (27)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_XFER_PEND_FLDMASK (0x4000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_XFER_PEND_FLDSHFT (26)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_Q_NOTEMPTY_FLDMASK (0x2000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_Q_NOTEMPTY_FLDSHFT (25)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_QOVRFLOW_FLDMASK (0x1000000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_QOVRFLOW_FLDSHFT (24)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_STATE_FLDMASK (0xe00000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_STATE_FLDSHFT (21)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_SMLH_LTSSM_STATE_FLDMASK (0x1f8000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_SMLH_LTSSM_STATE_FLDSHFT (15)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD1_FLDMASK (0x4000)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD1_FLDSHFT (14)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_WAKE_EVENT_FLDMASK (0x2000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_WAKE_EVENT_FLDSHFT (13)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_L2_EXIT_FLDMASK (0x1000)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_L2_EXIT_FLDSHFT (12)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L2_FLDMASK (0x800)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L2_FLDSHFT (11)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L1sub_FLDMASK (0x400)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L1sub_FLDSHFT (10)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L1_FLDMASK (0x200)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L1_FLDSHFT (9)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L0s_FLDMASK (0x100)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_LINKST_IN_L0s_FLDSHFT (8)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD2_FLDMASK (0xc0)
#define HWIO_PCIE_SS_PCIE_APP_STS_RSVD2_FLDSHFT (6)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RDLH_LINK_UP_FLDMASK (0x20)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RDLH_LINK_UP_FLDSHFT (5)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_CURR_ST_FLDMASK (0x1c)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_PM_CURR_ST_FLDSHFT (2)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_SMLH_LINK_UP_FLDMASK (0x2)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_SMLH_LINK_UP_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_IDLE_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_APP_STS_PCIE_RADM_IDLE_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_MSI_TRIG_REGOFF 0x18
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_MSI_TRIG_REGOFF)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_RSVD0_FLDMASK (0xffff0000)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_RSVD0_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_TC_FLDMASK (0xe000)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_TC_FLDSHFT (13)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_VEC_FLDMASK (0x1f00)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_VEC_FLDSHFT (8)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_RSVD1_FLDMASK (0xfe)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_RSVD1_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_REQ_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_MSI_TRIG_PCIE_VEN_MSI_REQ_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_MSI_PEND_REGOFF 0x1C
#define HWIO_PCIE_SS_PCIE_MSI_PEND_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_MSI_PEND_REGOFF)
#define HWIO_PCIE_SS_PCIE_MSI_PEND_PCIE_CFG_MSI_PEND_FLDMASK (0xffffffff)
#define HWIO_PCIE_SS_PCIE_MSI_PEND_PCIE_CFG_MSI_PEND_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_CURR_LTR_STS_REGOFF 0x20
#define HWIO_PCIE_SS_PCIE_CURR_LTR_STS_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_CURR_LTR_STS_REGOFF)
#define HWIO_PCIE_SS_PCIE_CURR_LTR_STS_PCIE_CURR_LTR_STS_FLDMASK (0xffffffff)
#define HWIO_PCIE_SS_PCIE_CURR_LTR_STS_PCIE_CURR_LTR_STS_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_LTR_MSG_LATENCY_REGOFF 0x24
#define HWIO_PCIE_SS_PCIE_LTR_MSG_LATENCY_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_LTR_MSG_LATENCY_REGOFF)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_LATENCY_PCIE_LTR_MSG_LATENCY_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_LATENCY_PCIE_LTR_MSG_LATENCY_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_REGOFF 0x28
#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_REGOFF)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_RSVD0_FLDMASK (0xfffffffe)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_RSVD0_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_PCIE_LTR_MSG_REQ_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_LTR_MSG_CTRL_PCIE_LTR_MSG_REQ_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_REGOFF 0x30
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_RSVD0_FLDMASK (0xfffe0000)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_RSVD0_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_PCIE_RADM_MSG0_VALID_FLDMASK (0x10000)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_PCIE_RADM_MSG0_VALID_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_PCIE_RADM_MSG0_REQ_ID_FLDMASK (0xffff)
#define HWIO_PCIE_SS_PCIE_RX_VMSG0_ID_PCIE_RADM_MSG0_REQ_ID_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG1_REGOFF 0x34
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG1_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG1_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG1_PCIE_RX_MSG0_PYLD_REG1_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG1_PCIE_RX_MSG0_PYLD_REG1_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG0_REGOFF 0x38
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG0_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG0_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG0_PCIE_RX_MSG0_PYLD_REG0_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_RX_MSG0_PYLD_REG0_PCIE_RX_MSG0_PYLD_REG0_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_REGOFF 0x40
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_RSVD0_FLDMASK (0xfffe0000)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_RSVD0_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_PCIE_RADM_MSG1_VALID_FLDMASK (0x10000)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_PCIE_RADM_MSG1_VALID_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_PCIE_RADM_MSG1_REQ_ID_FLDMASK (0xffff)
#define HWIO_PCIE_SS_PCIE_RX_VMSG1_ID_PCIE_RADM_MSG1_REQ_ID_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG1_REGOFF 0x44
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG1_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG1_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG1_PCIE_RX_MSG1_PYLD_REG1_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG1_PCIE_RX_MSG1_PYLD_REG1_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG0_REGOFF 0x48
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG0_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG0_REGOFF)
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG0_PCIE_RX_MSG1_PYLD_REG0_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_RX_MSG1_PYLD_REG0_PCIE_RX_MSG1_PYLD_REG0_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG1_REGOFF 0x50
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG1_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG1_REGOFF)
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG1_PCIE_TX_MSG_PYLD_REG1_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG1_PCIE_TX_MSG_PYLD_REG1_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG0_REGOFF 0x54
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG0_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG0_REGOFF)
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG0_PCIE_TX_MSG_PYLD_REG0_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_TX_MSG_PYLD_REG0_PCIE_TX_MSG_PYLD_REG0_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_REGOFF 0x58
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_REGOFF)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_FMT_FLDMASK (0xc0000000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_FMT_FLDSHFT (30)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TYPE_FLDMASK (0x3e000000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TYPE_FLDSHFT (25)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TC_FLDMASK (0x1c00000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TC_FLDSHFT (22)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_RSVD0_FLDMASK (0x200000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_RSVD0_FLDSHFT (21)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_ATTR_FLDMASK (0x180000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_ATTR_FLDSHFT (19)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TD_FLDMASK (0x40000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_TD_FLDSHFT (18)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_EP_FLDMASK (0x20000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_EP_FLDSHFT (17)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_RSVD1_FLDMASK (0x1fc00)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_RSVD1_FLDSHFT (10)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_LEN_FLDMASK (0x3ff)
#define HWIO_PCIE_SS_PCIE_TX_MSG_FIELD_PCIE_TX_MSG_LEN_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_REGOFF 0x5C
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_TX_MSG_REQ_REGOFF)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_TAG_FLDMASK (0xff000000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_TAG_FLDSHFT (24)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_CODE_FLDMASK (0xff0000)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_CODE_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_RSVD0_FLDMASK (0xfffe)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_RSVD0_FLDSHFT (1)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_REQ_FLDMASK (0x1)
#define HWIO_PCIE_SS_PCIE_TX_MSG_REQ_PCIE_TX_MSG_REQ_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG1_REGOFF 0x60
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG1_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG1_REGOFF)
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG1_PCIE_CORE_DEBUG_REG1_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG1_PCIE_CORE_DEBUG_REG1_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG0_REGOFF 0x64
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG0_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG0_REGOFF)
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG0_PCIE_CORE_DEBUG_REG0_FLDMASK \
	(0xffffffff)
#define HWIO_PCIE_SS_PCIE_CORE_DEBUG_REG0_PCIE_CORE_DEBUG_REG0_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_REGOFF 0x6C
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_ADDR(bAddr, regX) \
	(bAddr + HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_REGOFF)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_RSVD0_FLDMASK (0xfffc0000)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_RSVD0_FLDSHFT (18)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_PCIE_DEBUG_MUX_SEL_FLDMASK (0x30000)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_PCIE_DEBUG_MUX_SEL_FLDSHFT (16)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_PCIE_CORE_EIDEBUG_REG0_FLDMASK \
	(0xffff)
#define HWIO_PCIE_SS_PCIE_CORE_EIDEBUG_REG0_PCIE_CORE_EIDEBUG_REG0_FLDSHFT (0)

#define HWIO_PCIE_SS_PCIE_GP_REGNUM 8
#define HWIO_PCIE_SS_PCIE_GP_REGOFF 0x080
#define HWIO_PCIE_SS_PCIE_GP_ADDR(bAddr, regX) \
	(((regX >= 0) && (regX < HWIO_PCIE_SS_PCIE_GP_REGNUM)) ? \
	(bAddr + HWIO_PCIE_SS_PCIE_GP_REGOFF + (regX * 4)) : MNH_BAD_ADDR)
#define HWIO_PCIE_SS_PCIE_GP_PCIE_GP_FLDMASK (0xffffffff)
#define HWIO_PCIE_SS_PCIE_GP_PCIE_GP_FLDSHFT (0)

#endif /* __MNH_HWIO_PCIE_SS_ */
