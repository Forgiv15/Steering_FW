/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include <stdint.h>
#include <string.h>
#include "definitions.h"                // SYS function prototypes

#if 0
/* Previous bring-up test application retained for reference. */

/*
 * Quick steering wheel hardware test firmware.
 *
 * Update the pin tables below to your real hardware mapping before test.
 * PORT_PIN_NONE entries are ignored.
 */

#define TEST_SWITCH_COUNT                8U
#define TEST_ENCODER_BITS                4U
#define TEST_LED_COUNT                   9U
#define TEST_CAN_TX_ID                   0x321U
#define TEST_LOOP_DELAY_MS               10U
#define TEST_HEARTBEAT_HALF_PERIOD_MS    500U
#define TEST_INPUT_ACTIVE_LOW            true

/* WS2812 timings are approximate for 48 MHz CPU clock. */
#define WS2812_T0H_NOP                   7U
#define WS2812_T0L_NOP                   18U
#define WS2812_T1H_NOP                   16U
#define WS2812_T1L_NOP                   9U
#define WS2812_RESET_US                  80U

static const PORT_PIN g_switchPins[TEST_SWITCH_COUNT] =
{
  GPIO_S1_PIN,
  GPIO_S2_PIN,
  GPIO_S3_PIN,
  GPIO_SW1_PIN,
  GPIO_SW2_PIN,
  GPIO_S4_PIN,
  GPIO_S5_PIN,
  GPIO_S6_PIN
};

/* Encoder 1 nibble: bit0..bit3 */
static const PORT_PIN g_encoder1Pins[TEST_ENCODER_BITS] =
{
  GPIO_ENC1_1_LSB_PIN,
  GPIO_ENC2_PIN,
  GPIO_ENC3_PIN,
  GPIO_ENC1_4_MSB_PIN
};

/* Encoder 2 nibble: bit0..bit3 */
static const PORT_PIN g_encoder2Pins[TEST_ENCODER_BITS] =
{
  GPIO_ENC2_1_LSB_PIN,
  GPIO_ENC2_2_PIN,
  GPIO_ENC2_3_PIN,
  GPIO_ENC2_4_MSB_PIN
};

/* Single WS2812 data output pin for the 9-LED chain. */
static const PORT_PIN g_ws2812DataPin = GPIO_WS2812B_PIN;

static void BusyNopDelay(uint32_t cycles)
{
  while (cycles > 0U)
  {
    __asm volatile("nop");
    cycles--;
  }
}

static void DelayUs(uint32_t us)
{
  SYSTICK_DelayUs(us);
}

static void DelayMs(uint32_t ms)
{
  SYSTICK_DelayMs(ms);
}

static bool ReadInputPin(PORT_PIN pin)
{
  bool level;

  if (pin == PORT_PIN_NONE)
  {
    return false;
  }

  level = PORT_PinRead(pin);
  return (TEST_INPUT_ACTIVE_LOW == true) ? (!level) : level;
}

static uint16_t ReadSwitchBitfield(void)
{
  uint16_t states = 0U;
  uint32_t i;

  for (i = 0U; i < TEST_SWITCH_COUNT; i++)
  {
    if (ReadInputPin(g_switchPins[i]))
    {
      states |= (uint16_t)(1U << i);
    }
  }

  return states;
}

static uint8_t ReadEncoderNibble(const PORT_PIN encoderPins[TEST_ENCODER_BITS])
{
  uint8_t nibble = 0U;
  uint32_t i;

  for (i = 0U; i < TEST_ENCODER_BITS; i++)
  {
    if (ReadInputPin(encoderPins[i]))
    {
      nibble |= (uint8_t)(1U << i);
    }
  }

  return nibble;
}

static inline void WsPinHigh(void)
{
  GPIO_WS2812B_Set();
}

static inline void WsPinLow(void)
{
  GPIO_WS2812B_Clear();
}

static void WS2812_SendBit(bool one)
{
  if (one)
  {
    WsPinHigh();
    BusyNopDelay(WS2812_T1H_NOP);
    WsPinLow();
    BusyNopDelay(WS2812_T1L_NOP);
  }
  else
  {
    WsPinHigh();
    BusyNopDelay(WS2812_T0H_NOP);
    WsPinLow();
    BusyNopDelay(WS2812_T0L_NOP);
  }
}

static void WS2812_SendByte(uint8_t value)
{
  uint8_t bitMask = 0x80U;

  while (bitMask > 0U)
  {
    WS2812_SendBit((value & bitMask) != 0U);
    bitMask >>= 1U;
  }
}

static void WS2812_WriteFrame(const uint8_t colors[TEST_LED_COUNT][3])
{
  uint32_t i;

  if (g_ws2812DataPin == PORT_PIN_NONE)
  {
    return;
  }

  __disable_irq();

  for (i = 0U; i < TEST_LED_COUNT; i++)
  {
    /* WS2812 uses GRB order. */
    WS2812_SendByte(colors[i][1]);
    WS2812_SendByte(colors[i][0]);
    WS2812_SendByte(colors[i][2]);
  }

  __enable_irq();
  DelayUs(WS2812_RESET_US);
}

static void BuildLedPattern(uint16_t switchBits, uint8_t encoder1Nibble, uint8_t encoder2Nibble, uint8_t phase, uint8_t colors[TEST_LED_COUNT][3])
{
  uint32_t i;
  uint16_t combinedBits;

  /* LED0/LED1 are always animated so feedback appears right after boot. */
  colors[0][0] = (uint8_t)(8U + ((phase * 5U) & 0x1FU));
  colors[0][1] = (uint8_t)(4U + ((phase * 3U) & 0x1FU));
  colors[0][2] = (uint8_t)(2U + ((phase * 7U) & 0x1FU));

  colors[1][0] = (uint8_t)(6U + (((phase + 13U) * 4U) & 0x1FU));
  colors[1][1] = (uint8_t)(5U + (((phase + 13U) * 6U) & 0x1FU));
  colors[1][2] = (uint8_t)(3U + (((phase + 13U) * 2U) & 0x1FU));

  combinedBits = (uint16_t)(switchBits | ((uint16_t)(encoder1Nibble & 0x0FU) << 8U) | ((uint16_t)(encoder2Nibble & 0x0FU) << 12U));

  for (i = 2U; i < TEST_LED_COUNT; i++)
  {
    uint32_t bitIndex = i - 2U;
    bool on = ((combinedBits >> bitIndex) & 0x1U) != 0U;

    if (on)
    {
      colors[i][0] = (uint8_t)(0x04U + (i * 3U));
      colors[i][1] = (uint8_t)(0x10U + (i * 2U));
      colors[i][2] = (uint8_t)(0x03U + i);
    }
    else
    {
      colors[i][0] = 0x00U;
      colors[i][1] = 0x02U;
      colors[i][2] = 0x00U;
    }
  }
}

static void ConfigureTestPins(void)
{
  uint32_t i;

  for (i = 0U; i < TEST_SWITCH_COUNT; i++)
  {
    if (g_switchPins[i] != PORT_PIN_NONE)
    {
      PORT_PinGPIOConfig(g_switchPins[i]);
      PORT_PinInputEnable(g_switchPins[i]);
    }
  }

  for (i = 0U; i < TEST_ENCODER_BITS; i++)
  {
    if (g_encoder1Pins[i] != PORT_PIN_NONE)
    {
      PORT_PinGPIOConfig(g_encoder1Pins[i]);
      PORT_PinInputEnable(g_encoder1Pins[i]);
    }

    if (g_encoder2Pins[i] != PORT_PIN_NONE)
    {
      PORT_PinGPIOConfig(g_encoder2Pins[i]);
      PORT_PinInputEnable(g_encoder2Pins[i]);
    }
  }

  if (g_ws2812DataPin != PORT_PIN_NONE)
  {
    PORT_PinGPIOConfig(g_ws2812DataPin);
    GPIO_WS2812B_Clear();
    GPIO_WS2812B_OutputEnable();
    WsPinLow();
    DelayUs(WS2812_RESET_US);
  }

  PORT_PinGPIOConfig(GPIO_HEARTBEAT_PIN);
  GPIO_HEARTBEAT_Clear();
  GPIO_HEARTBEAT_OutputEnable();
}

static void SendCanTestFrame(uint16_t switchBits, uint8_t encoder1Nibble, uint8_t encoder2Nibble, uint8_t aliveCounter)
{
  CAN_TX_BUFFER txMessage;
  bool txQueued;

  (void)memset(&txMessage, 0, sizeof(txMessage));

  txMessage.id = TEST_CAN_TX_ID;
  txMessage.xtd = 0U;
  txMessage.rtr = 0U;
  txMessage.esi = 0U;
  txMessage.fdf = 0U;
  txMessage.brs = 0U;
  txMessage.efc = 0U;
  txMessage.mm = aliveCounter;
  txMessage.dlc = 8U;

  txMessage.data[0] = (uint8_t)(0xA0U | (aliveCounter & 0x0FU));
  txMessage.data[1] = (uint8_t)(switchBits & 0xFFU);
  txMessage.data[2] = (uint8_t)((switchBits >> 8U) & 0xFFU);
  txMessage.data[3] = (uint8_t)(encoder1Nibble & 0x0FU);
  txMessage.data[4] = (uint8_t)(encoder2Nibble & 0x0FU);
  {
    uint32_t canErr = CAN0_ErrorGet();
    txMessage.data[5] = (uint8_t)(canErr & 0xFFU);
    txMessage.data[6] = (uint8_t)((canErr >> 8U) & 0xFFU);
  }
  txMessage.data[7] = 0x5AU;

  txQueued = CAN0_MessageTransmitFifo(1U, &txMessage);
  (void)txQueued;
}


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

 int main ( void )
 {
   uint32_t now;
   uint32_t nextInputScanMs = 0U;
   uint32_t nextLedFrameMs = 0U;
   uint32_t nextCanFrameMs = 0U;
   uint32_t nextHeartbeatMs = APP_HEARTBEAT_HALF_PERIOD_MS;
 
   /* Initialize all modules */
   SYS_Initialize ( NULL );
 
   APP_ConfigurePins();
 
   SERCOM1_SPI_CallbackRegister(APP_WS2812TransferCallback, (uintptr_t)NULL);
   TC0_TimerCallbackRegister(APP_TimerCallback, (uintptr_t)NULL);
   TC0_TimerStart();
 
   APP_UpdateInputs(&g_appInputs);
   APP_BuildLedPattern(&g_appInputs, g_ws2812Colors);
   (void)APP_WS2812StartTransfer(g_ws2812Colors);
 
   while ( true )
   {
     now = APP_TimeMsGet();
 
     if (APP_TimeReached(now, nextInputScanMs))
     {
       APP_UpdateInputs(&g_appInputs);
       nextInputScanMs = now + APP_INPUT_SCAN_PERIOD_MS;
     }
 
     if (APP_TimeReached(now, nextHeartbeatMs))
     {
       GPIO_HEARTBEAT_Toggle();
       nextHeartbeatMs = now + APP_HEARTBEAT_HALF_PERIOD_MS;
     }
 
     if (APP_TimeReached(now, nextCanFrameMs))
     {
       if (APP_SendCanFrame(&g_appInputs))
       {
         g_appInputs.aliveCounter++;
         nextCanFrameMs = now + APP_CAN_PERIOD_MS;
       }
     }
 
     if (APP_TimeReached(now, nextLedFrameMs))
     {
       APP_BuildLedPattern(&g_appInputs, g_ws2812Colors);
 
       if (APP_WS2812StartTransfer(g_ws2812Colors))
       {
         g_appInputs.ledPhase++;
         nextLedFrameMs = now + APP_LED_FRAME_PERIOD_MS;
       }
     }
 
     /* Maintain state machines of all polled MPLAB Harmony modules. */
     SYS_Tasks ( );
   }
 
   /* Execution should not come here during normal operation */
 
   return ( EXIT_FAILURE );
 }
#endif

#define APP_SWITCH_COUNT                8U
#define APP_ENCODER_BITS                4U
#define APP_LED_COUNT                   9U

#define APP_INPUT_SCAN_PERIOD_MS        2U
#define APP_LED_FRAME_PERIOD_MS         15U
#define APP_CAN_PERIOD_MS               10U
#define APP_HEARTBEAT_HALF_PERIOD_MS    500U

#define APP_CAN_TX_ID                   0x321U
#define APP_INPUT_ACTIVE_LOW            true

#define WS2812_SPI_PATTERN_0            0x4U
#define WS2812_SPI_PATTERN_1            0x6U
#define WS2812_SPI_BITS_PER_LED_BIT     3U
#define WS2812_RESET_BYTES              24U
#define WS2812_DATA_BYTES_PER_LED       9U
#define WS2812_FRAME_BYTES              ((2U * WS2812_RESET_BYTES) + (APP_LED_COUNT * WS2812_DATA_BYTES_PER_LED))

typedef struct
{
  uint16_t switchBits;
  uint8_t encoder1Nibble;
  uint8_t encoder2Nibble;
  uint8_t aliveCounter;
  uint8_t ledPhase;
} APP_INPUT_STATE;

static const PORT_PIN g_appSwitchPins[APP_SWITCH_COUNT] =
{
  GPIO_S1_PIN,
  GPIO_S2_PIN,
  GPIO_S3_PIN,
  GPIO_SW1_PIN,
  GPIO_SW2_PIN,
  GPIO_S4_PIN,
  GPIO_S5_PIN,
  GPIO_S6_PIN
};

static const PORT_PIN g_appEncoder1Pins[APP_ENCODER_BITS] =
{
  GPIO_ENC1_1_LSB_PIN,
  GPIO_ENC2_PIN,
  GPIO_ENC3_PIN,
  GPIO_ENC1_4_MSB_PIN
};

static const PORT_PIN g_appEncoder2Pins[APP_ENCODER_BITS] =
{
  GPIO_ENC2_1_LSB_PIN,
  GPIO_ENC2_2_PIN,
  GPIO_ENC2_3_PIN,
  GPIO_ENC2_4_MSB_PIN
};

static volatile uint32_t g_appMsTicks = 0U;
static volatile bool g_ws2812TransferDone = true;

static APP_INPUT_STATE g_appInputs;
static uint8_t g_ws2812Colors[APP_LED_COUNT][3];
static uint8_t g_ws2812TxBuffer[WS2812_FRAME_BYTES];

static void APP_TimerCallback(TC_TIMER_STATUS status, uintptr_t context)
{
  (void)context;

  if ((status & TC_TIMER_STATUS_OVERFLOW) != 0U)
  {
    g_appMsTicks++;
  }
}

static void APP_WS2812TransferCallback(uintptr_t context)
{
  (void)context;
  g_ws2812TransferDone = true;
}

static uint32_t APP_TimeMsGet(void)
{
  uint32_t firstRead;
  uint32_t secondRead;

  do
  {
    firstRead = g_appMsTicks;
    secondRead = g_appMsTicks;
  } while (firstRead != secondRead);

  return firstRead;
}

static bool APP_TimeReached(uint32_t now, uint32_t deadline)
{
  return ((int32_t)(now - deadline) >= 0);
}

static bool APP_ReadInputPin(PORT_PIN pin)
{
  bool level = PORT_PinRead(pin);

  return (APP_INPUT_ACTIVE_LOW == true) ? (!level) : level;
}

static uint16_t APP_ReadSwitchBitfield(void)
{
  uint16_t states = 0U;
  uint32_t index;

  for (index = 0U; index < APP_SWITCH_COUNT; index++)
  {
    if (APP_ReadInputPin(g_appSwitchPins[index]))
    {
      states |= (uint16_t)(1U << index);
    }
  }

  return states;
}

static uint8_t APP_ReadEncoderNibble(const PORT_PIN encoderPins[APP_ENCODER_BITS])
{
  uint8_t nibble = 0U;
  uint32_t index;

  for (index = 0U; index < APP_ENCODER_BITS; index++)
  {
    if (APP_ReadInputPin(encoderPins[index]))
    {
      nibble |= (uint8_t)(1U << index);
    }
  }

  return nibble;
}

static void APP_ConfigurePins(void)
{
  uint32_t index;

  for (index = 0U; index < APP_SWITCH_COUNT; index++)
  {
    PORT_PinGPIOConfig(g_appSwitchPins[index]);
    PORT_PinInputEnable(g_appSwitchPins[index]);
  }

  for (index = 0U; index < APP_ENCODER_BITS; index++)
  {
    PORT_PinGPIOConfig(g_appEncoder1Pins[index]);
    PORT_PinInputEnable(g_appEncoder1Pins[index]);

    PORT_PinGPIOConfig(g_appEncoder2Pins[index]);
    PORT_PinInputEnable(g_appEncoder2Pins[index]);
  }

  PORT_PinGPIOConfig(GPIO_HEARTBEAT_PIN);
  GPIO_HEARTBEAT_Clear();
  GPIO_HEARTBEAT_OutputEnable();
}

static void APP_UpdateInputs(APP_INPUT_STATE *state)
{
  state->switchBits = APP_ReadSwitchBitfield();
  state->encoder1Nibble = APP_ReadEncoderNibble(g_appEncoder1Pins);
  state->encoder2Nibble = APP_ReadEncoderNibble(g_appEncoder2Pins);
}

static void APP_BuildLedPattern(const APP_INPUT_STATE *state, uint8_t colors[APP_LED_COUNT][3])
{
  uint8_t animatedBase0 = (uint8_t)((state->ledPhase * 5U) & 0x1FU);
  uint8_t animatedBase1 = (uint8_t)(((state->ledPhase + 11U) * 3U) & 0x1FU);

  memset(colors, 0, (size_t)(APP_LED_COUNT * 3U));

  colors[0][0] = (uint8_t)(8U + animatedBase0 + ((state->encoder1Nibble & 0x03U) << 3));
  colors[0][1] = (uint8_t)(4U + animatedBase1);
  colors[0][2] = (uint8_t)(2U + ((state->encoder1Nibble & 0x0CU) << 2));

  colors[1][0] = (uint8_t)(4U + animatedBase1);
  colors[1][1] = (uint8_t)(5U + animatedBase0);
  colors[1][2] = (uint8_t)(2U + (state->encoder2Nibble << 4));

  colors[2][0] = ((state->switchBits & (1U << 0)) != 0U) ? 0x18U : 0x00U;
  colors[2][1] = ((state->switchBits & (1U << 0)) != 0U) ? 0x00U : 0x02U;

  colors[3][0] = ((state->switchBits & (1U << 1)) != 0U) ? 0x18U : 0x00U;
  colors[3][1] = ((state->switchBits & (1U << 1)) != 0U) ? 0x00U : 0x02U;

  colors[4][0] = ((state->switchBits & (1U << 2)) != 0U) ? 0x18U : 0x00U;
  colors[4][1] = ((state->switchBits & (1U << 2)) != 0U) ? 0x00U : 0x02U;

  colors[5][2] = ((state->switchBits & (1U << 3)) != 0U) ? 0x18U : 0x00U;
  colors[5][1] = ((state->switchBits & (1U << 3)) != 0U) ? 0x00U : 0x02U;

  colors[6][2] = ((state->switchBits & (1U << 4)) != 0U) ? 0x18U : 0x00U;
  colors[6][1] = ((state->switchBits & (1U << 4)) != 0U) ? 0x00U : 0x02U;

  colors[7][0] = ((state->switchBits & (1U << 5)) != 0U) ? 0x18U : 0x00U;
  colors[7][1] = ((state->switchBits & (1U << 6)) != 0U) ? 0x18U : 0x00U;
  colors[7][2] = ((state->switchBits & (1U << 7)) != 0U) ? 0x18U : 0x00U;

  colors[8][0] = (uint8_t)(state->encoder1Nibble << 4);
  colors[8][1] = (uint8_t)((state->switchBits != 0U) ? 0x08U : 0x00U);
  colors[8][2] = (uint8_t)(state->encoder2Nibble << 4);
}

static void APP_WS2812SetEncodedBit(uint8_t *buffer, uint16_t bitIndex, bool bitValue)
{
  if (bitValue)
  {
    buffer[bitIndex >> 3U] |= (uint8_t)(1U << (7U - (bitIndex & 0x7U)));
  }
}

static uint16_t APP_WS2812AppendPattern(uint8_t *buffer, uint16_t bitIndex, uint8_t pattern)
{
  uint8_t shift;

  for (shift = 0U; shift < WS2812_SPI_BITS_PER_LED_BIT; shift++)
  {
    bool bitValue = ((pattern << shift) & 0x4U) != 0U;
    APP_WS2812SetEncodedBit(buffer, bitIndex, bitValue);
    bitIndex++;
  }

  return bitIndex;
}

static void APP_WS2812BuildTransferBuffer(const uint8_t colors[APP_LED_COUNT][3])
{
  uint16_t bitIndex = (uint16_t)(WS2812_RESET_BYTES * 8U);
  uint32_t ledIndex;
  uint8_t componentOrder[3];
  uint8_t componentIndex;
  uint8_t mask;

  memset(g_ws2812TxBuffer, 0, sizeof(g_ws2812TxBuffer));

  for (ledIndex = 0U; ledIndex < APP_LED_COUNT; ledIndex++)
  {
    componentOrder[0] = colors[ledIndex][1];
    componentOrder[1] = colors[ledIndex][0];
    componentOrder[2] = colors[ledIndex][2];

    for (componentIndex = 0U; componentIndex < 3U; componentIndex++)
    {
      mask = 0x80U;
      while (mask > 0U)
      {
        bitIndex = APP_WS2812AppendPattern(
          g_ws2812TxBuffer,
          bitIndex,
          ((componentOrder[componentIndex] & mask) != 0U) ? WS2812_SPI_PATTERN_1 : WS2812_SPI_PATTERN_0
        );
        mask >>= 1U;
      }
    }
  }
}

static bool APP_WS2812StartTransfer(const uint8_t colors[APP_LED_COUNT][3])
{
  bool queued;

  if ((g_ws2812TransferDone == false) || (SERCOM1_SPI_IsBusy() == true))
  {
    return false;
  }

  APP_WS2812BuildTransferBuffer(colors);

  g_ws2812TransferDone = false;
  queued = SERCOM1_SPI_Write(g_ws2812TxBuffer, sizeof(g_ws2812TxBuffer));
  if (queued == false)
  {
    g_ws2812TransferDone = true;
  }

  return queued;
}

static bool APP_SendCanFrame(const APP_INPUT_STATE *state)
{
  CAN_TX_BUFFER txMessage;
  uint32_t canError;

  if (CAN0_TxFifoFreeLevelGet() == 0U)
  {
    return false;
  }

  memset(&txMessage, 0, sizeof(txMessage));

  txMessage.id = APP_CAN_TX_ID;
  txMessage.xtd = 0U;
  txMessage.rtr = 0U;
  txMessage.esi = 0U;
  txMessage.fdf = 0U;
  txMessage.brs = 0U;
  txMessage.efc = 0U;
  txMessage.mm = state->aliveCounter;
  txMessage.dlc = 8U;

  canError = CAN0_ErrorGet();

  txMessage.data[0] = (uint8_t)(0xA0U | (state->aliveCounter & 0x0FU));
  txMessage.data[1] = (uint8_t)(state->switchBits & 0xFFU);
  txMessage.data[2] = (uint8_t)((GPIO_HEARTBEAT_Get() != 0U ? 0x01U : 0x00U) |
                  (g_ws2812TransferDone == false ? 0x02U : 0x00U) |
                  (SERCOM1_SPI_IsBusy() == true ? 0x04U : 0x00U));
  txMessage.data[3] = (uint8_t)(state->encoder1Nibble & 0x0FU);
  txMessage.data[4] = (uint8_t)(state->encoder2Nibble & 0x0FU);
  txMessage.data[5] = (uint8_t)(canError & 0xFFU);
  txMessage.data[6] = (uint8_t)((canError >> 8U) & 0xFFU);
  txMessage.data[7] = 0x5AU;

  return CAN0_MessageTransmitFifo(1U, &txMessage);
}

int main ( void )
{
  uint32_t now;
  uint32_t nextInputScanMs = 0U;
  uint32_t nextLedFrameMs = 0U;
  uint32_t nextCanFrameMs = 0U;
  uint32_t nextHeartbeatMs = APP_HEARTBEAT_HALF_PERIOD_MS;

  SYS_Initialize ( NULL );

  APP_ConfigurePins();

  SERCOM1_SPI_CallbackRegister(APP_WS2812TransferCallback, (uintptr_t)NULL);
  TC0_TimerCallbackRegister(APP_TimerCallback, (uintptr_t)NULL);
  TC0_TimerStart();

  APP_UpdateInputs(&g_appInputs);
  APP_BuildLedPattern(&g_appInputs, g_ws2812Colors);
  (void)APP_WS2812StartTransfer(g_ws2812Colors);

  while ( true )
  {
    now = APP_TimeMsGet();

    if (APP_TimeReached(now, nextInputScanMs))
    {
      APP_UpdateInputs(&g_appInputs);
      nextInputScanMs = now + APP_INPUT_SCAN_PERIOD_MS;
    }

    if (APP_TimeReached(now, nextHeartbeatMs))
    {
      GPIO_HEARTBEAT_Toggle();
      nextHeartbeatMs = now + APP_HEARTBEAT_HALF_PERIOD_MS;
    }

    if (APP_TimeReached(now, nextCanFrameMs))
    {
      if (APP_SendCanFrame(&g_appInputs))
      {
        g_appInputs.aliveCounter++;
        nextCanFrameMs = now + APP_CAN_PERIOD_MS;
      }
    }

    if (APP_TimeReached(now, nextLedFrameMs))
    {
      APP_BuildLedPattern(&g_appInputs, g_ws2812Colors);

      if (APP_WS2812StartTransfer(g_ws2812Colors))
      {
        g_appInputs.ledPhase++;
        nextLedFrameMs = now + APP_LED_FRAME_PERIOD_MS;
      }
    }

    SYS_Tasks ( );
  }

  return ( EXIT_FAILURE );
}

#if 0
int main(void)
{
  SYS_Initialize(NULL);
  SYSTICK_TimerStart();

  PORT_PinGPIOConfig(GPIO_HEARTBEAT_PIN);
  GPIO_HEARTBEAT_Clear();
  GPIO_HEARTBEAT_OutputEnable();

  while (true)
  {
    GPIO_HEARTBEAT_Toggle();
    SYSTICK_DelayMs(500);
  }

  return (EXIT_FAILURE);
}
#endif

/*******************************************************************************
 End of File
*/

