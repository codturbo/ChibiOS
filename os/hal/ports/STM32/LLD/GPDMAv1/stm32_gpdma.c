/*
    ChibiOS - Copyright (C) 2006..2023 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    GPDMAv1/stm32_gpdma.c
 * @brief   GPDMA helper driver code.
 *
 * @addtogroup STM32_GPDMA
 * @details GPDMA sharing helper driver. In the STM32 the DMA channels are a
 *          shared resource, this driver allows to allocate and free DMA
 *          channels at runtime in order to allow all the other device
 *          drivers to coordinate the access to the resource.
 * @note    The DMA ISR handlers are all declared into this module because
 *          sharing, the various device drivers can associate a callback to
 *          ISRs when allocating channels.
 * @{
 */

#include "hal.h"

/* The following macro is only defined if some driver requiring GPDMA services
   has been enabled.*/
#if defined(STM32_GPDMA_REQUIRED) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   GPDMA channels descriptors.
 * @details This table keeps the association between an unique channel
 *          identifier and the involved physical registers.
 * @note    Don't use this array directly, use the appropriate wrapper macros
 *          instead: @p STM32_GPDMA1_CHANNEL1, @p STM32_GPDMA1_CHANNEL2 etc.
 */
const stm32_gpdma_channel_t __stm32_gpdma_channels[STM32_GPDMA_CHANNELS] = {
#if STM32_GPDMA1_NUM_CHANNELS > 0
  {GPDMA1_Channel0, STM32_GPDMA1_CH0_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 1
  {GPDMA1_Channel1, STM32_GPDMA1_CH1_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 2
  {GPDMA1_Channel2, STM32_GPDMA1_CH2_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 3
  {GPDMA1_Channel3, STM32_GPDMA1_CH3_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 4
  {GPDMA1_Channel4, STM32_GPDMA1_CH4_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 5
  {GPDMA1_Channel5, STM32_GPDMA1_CH5_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 6
  {GPDMA1_Channel6, STM32_GPDMA1_CH6_NUMBER},
#endif
#if STM32_GPDMA1_NUM_CHANNELS > 7
  {GPDMA1_Channel7, STM32_GPDMA1_CH7_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 0
  {GPDMA2_Channel0, STM32_GPDMA2_CH0_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 1
  {GPDMA2_Channel1, STM32_GPDMA2_CH1_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 2
  {GPDMA2_Channel2, STM32_GPDMA2_CH2_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 3
  {GPDMA2_Channel3, STM32_GPDMA2_CH3_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 4
  {GPDMA2_Channel4, STM32_GPDMA2_CH4_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 5
  {GPDMA2_Channel5, STM32_GPDMA2_CH5_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 6
  {GPDMA2_Channel6, STM32_GPDMA2_CH6_NUMBER},
#endif
#if STM32_GPDMA2_NUM_CHANNELS > 7
  {GPDMA2_Channel7, STM32_GPDMA2_CH7_NUMBER},
#endif
};

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   Global DMA-related data structures.
 */
static struct {
  /**
   * @brief   Mask of the allocated channels.
   */
  uint32_t              allocated_mask;
  /**
   * @brief   DMA IRQ redirectors.
   */
  struct {
    /**
     * @brief   DMA callback function.
     */
    stm32_gpdmaisr_t    func;
    /**
     * @brief   DMA callback parameter.
     */
    void                *param;
  } channels[STM32_GPDMA_CHANNELS];
} gpdma;

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   STM32 DMA helper initialization.
 *
 * @init
 */
void dmaInit(void) {
  unsigned i;

  gpdma.allocated_mask = 0U;
  for (i = 0; i < STM32_GPDMA_CHANNELS; i++) {
    __stm32_gpdma_channels[i].channel->CCR = 0U;
    gpdma.channels[i].func = NULL;
  }
#if STM32_GPDMA2_NUM_CHANNELS > 0
#endif
}

/**
 * @brief   Allocates a DMA channel.
 * @details The channel is allocated and, if required, the DMA clock enabled.
 *          The function also enables the IRQ vector associated to the channel
 *          and initializes its priority.
 *
 * @param[in] cmask     channels mask where to search for an available chennel
 * @param[in] irqprio   IRQ priority for the DMA channel
 * @param[in] func      handling function pointer, can be @p NULL
 * @param[in] param     a parameter to be passed to the handling function
 * @return              Pointer to the allocated @p stm32_dma_channel_t
 *                      structure.
 * @retval NULL         if a/the channel is not available.
 *
 * @iclass
 */
const stm32_gpdma_channel_t *gpdmaChannelAllocI(uint32_t cmask,
                                                uint32_t irqprio,
                                                stm32_gpdmaisr_t func,
                                                void *param) {
  unsigned i;
  uint32_t available;

  osalDbgCheckClassI();

  /* Mask of the available channels within the specified channels.*/
  available = gpdma.allocated_mask & cmask;

  /* Searching for a free channel.*/
  for (i = 0U; i <= STM32_GPDMA_CHANNELS; i++) {
    uint32_t mask = (uint32_t)(1U << i);
    if ((available & mask) == 0U) {
      /* Channel found.*/
      const stm32_gpdma_channel_t *dmachp = STM32_GPDMA_CHANNEL(i);

      /* Installs the DMA handler.*/
      gpdma.channels[i].func  = func;
      gpdma.channels[i].param = param;
      gpdma.allocated_mask  |= mask;

      /* Enabling DMA clocks required by the current channels set.*/
      if ((STM32_GPDMA1_MASK_ANY & mask) != 0U) {
        rccEnableGPDMA1(true);
      }
#if STM32_GPDMA2_NUM_CHANNELS > 0
      if ((STM32_GPDMA2_MASK_ANY & mask) != 0U) {
        rccEnableGPDMA2(true);
      }
#endif

      /* Enables the associated IRQ vector if not already enabled and if a
         callback is defined.*/
      if (func != NULL) {
        /* Could be already enabled but no problem.*/
        nvicEnableVector(dmachp->vector, irqprio);
      }

      /* Putting the channel in a known state.*/
      gpdmaStreamDisable(dmachp);
      dmachp->channel->CCR = 0U;

      return dmachp;
    }
  }

  return NULL;
}

/**
 * @brief   Allocates a DMA channel.
 * @details The channel is allocated and, if required, the DMA clock enabled.
 *          The function also enables the IRQ vector associated to the channel
 *          and initializes its priority.
 *
 * @param[in] cmask     channels mask where to search for an available chennel
 * @param[in] irqprio   IRQ priority for the DMA channel
 * @param[in] func      handling function pointer, can be @p NULL
 * @param[in] param     a parameter to be passed to the handling function
 * @return              Pointer to the allocated @p stm32_dma_channel_t
 *                      structure.
 * @retval NULL         if a/the channel is not available.
 *
 * @api
 */
const stm32_gpdma_channel_t *gpdmaChannelAlloc(uint32_t cmask,
                                               uint32_t irqprio,
                                               stm32_gpdmaisr_t func,
                                               void *param) {
  const stm32_gpdma_channel_t *dmachp;

  osalSysLock();
  dmachp = gpdmaChannelAllocI(cmask, irqprio, func, param);
  osalSysUnlock();

  return dmachp;
}

/**
 * @brief   Releases a DMA channel.
 * @details The channel is freed and, if required, the DMA clock disabled.
 *          Trying to release a unallocated channel is an illegal operation
 *          and is trapped if assertions are enabled.
 *
 * @param[in] dmachp    pointer to a @p stm32_dma_channel_t structure
 *
 * @iclass
 */
void gpdmaChannelFreeI(const stm32_gpdma_channel_t *dmachp) {
  uint32_t selfindex = (uint32_t)(dmachp - __stm32_gpdma_channels);

  osalDbgCheck(dmachp != NULL);

  /* Check if the channels is not taken.*/
  osalDbgAssert((gpdma.allocated_mask & (1U << selfindex)) != 0U,
                "not allocated");

  /* Marks the channel as not allocated.*/
  gpdma.allocated_mask &= ~(1U << selfindex);

  /* Disables the associated IRQ vector if it is no more in use.*/
  nvicDisableVector(dmachp->vector);

  /* Removes the DMA handler.*/
  gpdma.channels[selfindex].func  = NULL;
  gpdma.channels[selfindex].param = NULL;

  /* Shutting down clocks that are no more required, if any.*/
  if ((gpdma.allocated_mask & STM32_GPDMA1_MASK_ANY) == 0U) {
    rccDisableGPDMA1();
  }
#if STM32_GPDMA2_NUM_CHANNELS > 0
  if ((gpdma.allocated_mask & STM32_GPDMA2_MASK_ANY) == 0U) {
    rccDisableGPDMA2();
  }
#endif
}

/**
 * @brief   Releases a DMA channel.
 * @details The channel is freed and, if required, the DMA clock disabled.
 *          Trying to release a unallocated channel is an illegal operation
 *          and is trapped if assertions are enabled.
 *
 * @param[in] dmachp    pointer to a @p stm32_dma_channel_t structure
 *
 * @api
 */
void gpdmaChannelFree(const stm32_gpdma_channel_t *dmachp) {

  osalSysLock();
  gpdmaStreamFreeI(dmachp);
  osalSysUnlock();
}

/**
 * @brief   Serves a DMA IRQ.
 *
 * @param[in] dmachp    pointer to a @p stm32_gpdma_channel_t structure
 *
 * @special
 */
void gpdmaServeInterrupt(const stm32_gpdma_channel_t *dmachp) {
  uint32_t csr;
  uint32_t selfindex = (uint32_t)(dmachp - __stm32_gpdma_channels);

  csr = dmachp->channel->CSR;
  dmachp->channel->CFCR = csr;
  if (csr & dmachp->channel->CCR) {
    if (gpdma.channels[selfindex].func) {
      gpdma.channels[selfindex].func(gpdma.channels[selfindex].param, csr);
    }
  }
}

#endif /* defined(STM32_GPDMA_REQUIRED) */

/** @} */
