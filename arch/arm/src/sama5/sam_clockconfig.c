/****************************************************************************
 * arch/arm/src/sama5/sam_clockconfig.c
 *
 *   Copyright (C) 2013 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <arch/board/board.h>

#include "up_arch.h"
#include "up_internal.h"

#include "sam_clockconfig.h"
#include "chip/sam_pmc.h"
#include "chip/sam_wdt.h"
#include "chip/sam_matrix.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PMC register settings based on the board configuration values defined
 * in board.h
 */

#define BOARD_CKGR_MOR      (PMC_CKGR_MOR_KEY | BOARD_CKGR_MOR_MOSCXTST | \
                             PMC_CKGR_MOR_MOSCRCEN | PMC_CKGR_MOR_MOSCXTEN)
#define BOARD_CKGR_PLLAR    (PMC_CKGR_PLLAR_ONE | BOARD_CKGR_PLLAR_MUL | \
                             BOARD_CKGR_PLLAR_COUNT | BOARD_CKGR_PLLAR_DIV)
#define BOARD_PMC_MCKR_FAST (BOARD_PMC_MCKR_PRES | PMC_MCKR_CSS_MAIN)
#define BOARD_PMC_MCKR      (BOARD_PMC_MCKR_PRES | BOARD_PMC_MCKR_CSS)
#define BOARD_CKGR_UCKR     (BOARD_CKGR_UCKR_UPLLCOUNT | PMC_CKGR_UCKR_UPLLEN)

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sam_wdtsetup
 *
 * Description:
 *   Disable the watchdog timer
 *
 ****************************************************************************/

static inline void sam_wdtsetup(void)
{
  putreg32(WDT_MR_WDDIS, SAM_WDT_MR);
}

/****************************************************************************
 * Name: sam_pmcwait
 *
 * Description:
 *   Wait for the specide PMC status bit to become "1"
 *
 ****************************************************************************/

static void sam_pmcwait(uint32_t bit)
{
  volatile uint32_t delay;

  for (delay = 0;
       (getreg32(SAM_PMC_SR) & bit) == 0 && delay < UINT32_MAX;
       delay++);
}

/****************************************************************************
 * Name: sam_pmcsetup
 *
 * Description:
 *   Initialize clocking
 *
 ****************************************************************************/

static inline void sam_pmcsetup(void)
{
  uint32_t regval;

  /* Enable main oscillator (if it has not already been selected) */

  if ((getreg32(SAM_PMC_CKGR_MOR) & PMC_CKGR_MOR_MOSCSEL) == 0)
    {
      /* "When the MOSCXTEN bit and the MOSCXTCNT are written in CKGR_MOR to
       *  enable the main oscillator, the MOSCXTS bit in the Power Management
       *  Controller Status Register (PMC_SR) is cleared and the counter starts
       *  counting down on the slow clock divided by 8 from the MOSCXTCNT
       *  value. ... When the counter reaches 0, the MOSCXTS bit is set,
       *  indicating that the main clock is valid."
       */

      putreg32(BOARD_CKGR_MOR, SAM_PMC_CKGR_MOR);
      sam_pmcwait(PMC_INT_MOSCXTS);
    }

  /* "Switch to the main oscillator.  The selection is made by writing the
   *  MOSCSEL bit in the Main Oscillator Register (CKGR_MOR). The switch of
   *  the Main Clock source is glitch free, so there is no need to run out
   *  of SLCK, PLLACK or UPLLCK in order to change the selection. The MOSCSELS
   *  bit of the power Management Controller Status Register (PMC_SR) allows
   *  knowing when the switch sequence is done."
   *
   *   MOSCSELS: Main Oscillator Selection Status
   *             0 = Selection is done
   *             1 = Selection is in progress
   */

  putreg32((BOARD_CKGR_MOR | PMC_CKGR_MOR_MOSCSEL), SAM_PMC_CKGR_MOR);
  sam_pmcwait(PMC_INT_MOSCSELS);

  /* "Select the master clock. "The Master Clock selection is made by writing
   *  the CSS field (Clock Source Selection) in PMC_MCKR (Master Clock Register).
   *  The prescaler supports the division by a power of 2 of the selected clock
   *  between 1 and 64, and the division by 3. The PRES field in PMC_MCKR programs
   *  the prescaler. Each time PMC_MCKR is written to define a new Master Clock,
   *  the MCKRDY bit is cleared in PMC_SR. It reads 0 until the Master Clock is
   *  established.
   */

  regval = getreg32(SAM_PMC_MCKR);
  regval &= ~PMC_MCKR_CSS_MASK;
  regval |= PMC_MCKR_CSS_MAIN;
  putreg32(regval, SAM_PMC_MCKR);
  sam_pmcwait(PMC_INT_MCKRDY);

  /* Setup PLLA and wait for LOCKA */

  putreg32(BOARD_CKGR_PLLAR, SAM_PMC_CKGR_PLLAR);
  sam_pmcwait(PMC_INT_LOCKA);

  /* Setup UTMI for USB and wait for LOCKU */

#ifdef CONFIG_USBDEV
  regval = getreg32(SAM_PMC_CKGR_UCKR);
  regval |= BOARD_CKGR_UCKR;
  putreg32(regval, SAM_PMC_CKGR_UCKR);
  sam_pmcwait(PMC_INT_LOCKU);
#endif

  /* Switch to the fast clock and wait for MCKRDY */

  putreg32(BOARD_PMC_MCKR_FAST, SAM_PMC_MCKR);
  sam_pmcwait(PMC_INT_MCKRDY);

  putreg32(BOARD_PMC_MCKR, SAM_PMC_MCKR);
  sam_pmcwait(PMC_INT_MCKRDY);
}

/****************************************************************************
 * Name: sam_enabledefaultmaster and sam_disabledefaultmaster
 *
 * Description:
 *   Enable/disable default master access
 *
 ****************************************************************************/

static inline void sam_enabledefaultmaster(void)
{
#warning Missing Logic
#if 0
  uint32_t regval;

  /* Set default master: SRAM0 -> Cortex-A5 System */

  regval  = getreg32(SAM_MATRIX_SCFG0);
  regval |= (MATRIX_SCFG0_FIXEDDEFMSTR_ARMS|MATRIX_SCFG_DEFMSTRTYPE_FIXED);
  putreg32(regval, SAM_MATRIX_SCFG0);

  /* Set default master: SRAM1 -> Cortex-A5 System */

  regval  = getreg32(SAM_MATRIX_SCFG1);
  regval |= (MATRIX_SCFG1_FIXEDDEFMSTR_ARMS|MATRIX_SCFG_DEFMSTRTYPE_FIXED);
  putreg32(regval, SAM_MATRIX_SCFG1);

  /* Set default master: Internal flash0 -> Cortex-A5 Instruction/Data */

  regval  = getreg32(SAM_MATRIX_SCFG3);
  regval |= (MATRIX_SCFG3_FIXEDDEFMSTR_ARMC|MATRIX_SCFG_DEFMSTRTYPE_FIXED);
  putreg32(regval, SAM_MATRIX_SCFG3);
#endif
}

#if 0 /* Not used */
static inline void sam_disabledefaultmaster(void)
{
  uint32_t regval;

  /* Clear default master: SRAM0 -> Cortex-A5 System */

  regval  = getreg32(SAM_MATRIX_SCFG0);
  regval &= ~MATRIX_SCFG_DEFMSTRTYPE_MASK;
  putreg32(regval, SAM_MATRIX_SCFG0);

  /* Clear default master: SRAM1 -> Cortex-A5 System */

  regval  = getreg32(SAM_MATRIX_SCFG1);
  regval &= ~MATRIX_SCFG_DEFMSTRTYPE_MASK;
  putreg32(regval, SAM_MATRIX_SCFG1);

  /* Clear default master: Internal flash0 -> Cortex-A5 Instruction/Data */

  regval  = getreg32(SAM_MATRIX_SCFG3);
  regval &= ~MATRIX_SCFG_DEFMSTRTYPE_MASK;
  putreg32(regval, SAM_MATRIX_SCFG3);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sam_clockconfig
 *
 * Description:
 *   Called to initialize the SAM3/4.  This does whatever setup is needed to
 *   put the SoC in a usable state.  This includes the initialization of
 *   clocking using the settings in board.h.  (After power-on reset, the SAM3/4
 *   is initially running on a 12MHz internal RC clock).  This function also
 *   performs other low-level chip initialization of the chip including master
 *   clock, IRQ & watchdog configuration.
 *
 ****************************************************************************/

void sam_clockconfig(void)
{
  /* Configure the watchdog timer */

  sam_wdtsetup();

  /* Initialize clocking */

  sam_pmcsetup();

  /* Optimize CPU setting for speed */

  sam_enabledefaultmaster();
}
