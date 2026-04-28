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
  uint8_t alive = 0U;
  uint8_t phase = 0U;
  uint16_t heartbeatElapsedMs = 0U;
  uint8_t ledColors[TEST_LED_COUNT][3];

    /* Initialize all modules */
    SYS_Initialize ( NULL );

  SYSTICK_TimerStart();

  ConfigureTestPins();

  /* Immediate visual boot feedback on first two LEDs. */
  (void)memset(ledColors, 0, sizeof(ledColors));
  BuildLedPattern(0U, 0U, 0U, phase, ledColors);
  WS2812_WriteFrame(ledColors);

    while ( true )
    {
    uint16_t switchBits = ReadSwitchBitfield();
    uint8_t encoder1Nibble = ReadEncoderNibble(g_encoder1Pins);
    uint8_t encoder2Nibble = ReadEncoderNibble(g_encoder2Pins);

    BuildLedPattern(switchBits, encoder1Nibble, encoder2Nibble, phase, ledColors);
    WS2812_WriteFrame(ledColors);
    SendCanTestFrame(switchBits, encoder1Nibble, encoder2Nibble, alive);
    alive++;
    phase++;

    heartbeatElapsedMs = (uint16_t)(heartbeatElapsedMs + TEST_LOOP_DELAY_MS);
    if (heartbeatElapsedMs >= TEST_HEARTBEAT_HALF_PERIOD_MS)
    {
      GPIO_HEARTBEAT_Toggle();
      heartbeatElapsedMs = 0U;
    }

    /* Maintain state machines of all polled MPLAB Harmony modules. */
        SYS_Tasks ( );

    DelayMs(TEST_LOOP_DELAY_MS);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}
#endif


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );
    SYSTICK_TimerStart();

    PORT_PinGPIOConfig(GPIO_HEARTBEAT_PIN);
    GPIO_HEARTBEAT_Clear();
    GPIO_HEARTBEAT_OutputEnable();

    while ( true )
    {
        GPIO_HEARTBEAT_Toggle();
        SYSTICK_DelayMs(500);
    }

    /* Execution should not come here during normal operation */

    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

