#include "mprintf.h"
#include "greatest.h"
#include "test_config.h"

#include "stm32wlxx_ll_bus.h"
#include "stm32wlxx_ll_rcc.h"
#include "stm32wlxx_ll_system.h"
#include "stm32wlxx_ll_pwr.h"
#include "stm32wlxx_ll_gpio.h"
#include "stm32wlxx_ll_lpuart.h"
#include "stm32wlxx_ll_ipcc.h"
#include "stm32wlxx_ll_utils.h"
#include "stm32wlxx.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#define CPU2_INITIALISED 0xAA
#define CPU2_NOT_INITIALISED 0xBE
volatile uint8_t *cpu2InitDone = (uint8_t *)0x2000FFFF;

volatile uint32_t *cycle_count = (uint32_t *)0x2000FFF0;

#if defined(CORE_CM0PLUS)
static uint32_t min_cycles;
#endif

#if defined(CORE_CM4)
static inline void enable_cycle_count(void);
static inline uint32_t get_LSU_count(void);
#endif

static void UART_init(void);
static void sysclk_init(void);
static void init_IPCC(void);

static inline uint32_t get_cycle_count(void);

static void run_test(void);

TEST memcpy_test(uint32_t data_len, uint32_t src_offset, uint32_t dest_offset, bool print_performance);
TEST memcpy_iterate(uint32_t data_len_limit);
TEST memcpy_slide_dest(uint32_t data_len, uint32_t src_offset);

#define BUFFER_SIZE 0x1000

extern void* memcpy_(void *destination, const void *source, size_t num);

// Add definitions that need to be in the test runner's main file.
GREATEST_MAIN_DEFS();
extern uint32_t _vector_table_offset;

int main(void)
{
  SCB->VTOR = (uint32_t)(&_vector_table_offset);  // set the vector table offset
  sysclk_init();
#if defined(CORE_CM0PLUS)
  *cpu2InitDone = CPU2_INITIALISED;
#endif
#if defined(CORE_CM4)
  LL_PWR_DisableBootC2();
  enable_cycle_count();
#endif

#if (TEST_CM0PLUS)
  init_IPCC();
#if defined(CORE_CM0PLUS)
  UART_init();
  print_newline();
  println_("***Cortex-M0+ Test***");
  uint32_t curr_cycle = get_cycle_count();
  uint32_t new_cycle = get_cycle_count();
  min_cycles = new_cycle-curr_cycle;
  printfln_("min cycles is %u", new_cycle-curr_cycle);
  run_test();
#endif

#if defined(CORE_CM4)
  *cpu2InitDone = CPU2_NOT_INITIALISED;
  LL_PWR_EnableBootC2();
  while (*cpu2InitDone != CPU2_INITIALISED);
#endif

#else

#if defined(CORE_CM4)
  UART_init();
  print_newline();
  println_("Cortex-M4 Test");
  run_test();
#endif

#endif



  while (1)
  {
#if (TEST_CM0PLUS) && defined(CORE_CM4)
    // wait for IPCC trigger
    if(LL_C2_IPCC_IsActiveFlag_CHx(IPCC, LL_IPCC_CHANNEL_1) == LL_IPCC_CHANNEL_1){
      *cycle_count = get_cycle_count();
      LL_C1_IPCC_ClearFlag_CHx(IPCC, LL_IPCC_CHANNEL_1);
    }
#else
    LL_mDelay(1000);
#endif
  }
}

static void run_test(void)
{
  printfln_("%-6s %-10s %-10s %-8s %-8s %-8s", "d_len", "src_off", "dest_off", "o_cycle", "n_cycle", "c_diff");

  GREATEST_MAIN_BEGIN();  // command-line options, initialization.

  RUN_TESTp(memcpy_test, 1, 0x2, 0x7E, true);
  RUN_TESTp(memcpy_test, 10, 0x2, 0x7E, true);
  RUN_TESTp(memcpy_test, 20, 0x2, 0x7E, true);
  RUN_TESTp(memcpy_test, 50, 0x2, 0x7E, true);

  RUN_TESTp(memcpy_test, 1, 0x2, 0x82, true);
  RUN_TESTp(memcpy_test, 10, 0x2, 0x82, true);
  RUN_TESTp(memcpy_test, 20, 0x2, 0x82, true);
  RUN_TESTp(memcpy_test, 50, 0x2, 0x82, true);

  RUN_TESTp(memcpy_test, 1, 0x1, 0x102, true);
  RUN_TESTp(memcpy_test, 10, 0x1, 0x102, true);
  RUN_TESTp(memcpy_test, 20, 0x1, 0x102, true);
  RUN_TESTp(memcpy_test, 50, 0x1, 0x102, true);
  RUN_TESTp(memcpy_test, 200, 0x1, 0x102, true);

  RUN_TESTp(memcpy_slide_dest, 0x0f, 0x81);
  RUN_TESTp(memcpy_slide_dest, 0x10, 0x80);
  RUN_TESTp(memcpy_slide_dest, 0x22, 0x17f);
  RUN_TESTp(memcpy_slide_dest, 0x200, 0x400);

  // RUN_TEST1(memcpy_iterate, 20);

  GREATEST_MAIN_END();    // display results
}

TEST memcpy_test(uint32_t data_len, uint32_t src_offset, uint32_t dest_offset, bool print_performance)
{

  uint8_t expected[BUFFER_SIZE] = {0};
  uint8_t actual[BUFFER_SIZE] = {0};

  if(((data_len + src_offset) > BUFFER_SIZE) || ((data_len + dest_offset) > BUFFER_SIZE)){
    FAIL();
  }


  uint32_t i;

  for(i = 0; i < data_len; i++){
    expected[src_offset + i] = i;
    actual[src_offset + i] = i;
  }

  uint32_t memmove_orig_start = get_cycle_count();
  memcpy(&(expected[dest_offset]), &(expected[src_offset]), data_len);
  uint32_t memmove_orig_stop = get_cycle_count();

  uint32_t memmove_new_start = get_cycle_count();
  memcpy_(&(actual[dest_offset]), &(actual[src_offset]), data_len);
  uint32_t memmove_new_stop = get_cycle_count();



  if(print_performance){
    uint32_t orig_cycle = memmove_orig_stop-memmove_orig_start;
    uint32_t new_cycle = memmove_new_stop-memmove_new_start;
    int32_t cycle_diff = orig_cycle - new_cycle;
    printfln_("%-6u %-#10x %-#10x %-8u %-8u %-8d", data_len, src_offset, dest_offset, orig_cycle, new_cycle, cycle_diff);
  }

  ASSERT_MEM_EQ(expected, actual, BUFFER_SIZE);

  PASS();
}

TEST memcpy_iterate(uint32_t data_len_limit)
{
  uint32_t data_len;
  uint32_t src_offset;
  uint32_t dest_offset;

  for(data_len = 0; data_len < data_len_limit; data_len++){
    for(src_offset = 0; src_offset < 5; src_offset++){
      for(dest_offset = data_len; dest_offset < (data_len + 8); dest_offset++){
        CHECK_CALL(memcpy_test(data_len, src_offset, dest_offset, false));
      }
    }
  }

  PASS();
}

TEST memcpy_slide_dest(uint32_t data_len, uint32_t src_offset)
{
  uint32_t dest_offset;

  if((src_offset < data_len) || (src_offset + (2 * data_len)) > BUFFER_SIZE){
    FAIL();
  }

  CHECK_CALL(memcpy_test(data_len, src_offset, 0, true));
  CHECK_CALL(memcpy_test(data_len, src_offset, src_offset-data_len, true));

  CHECK_CALL(memcpy_test(data_len, src_offset, src_offset, true));

  CHECK_CALL(memcpy_test(data_len, src_offset, src_offset+data_len, true));
  CHECK_CALL(memcpy_test(data_len, src_offset, BUFFER_SIZE-data_len, true));

  PASS();
}

#if defined(CORE_CM4)
static inline void enable_cycle_count(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->LSUCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk | DWT_CTRL_LSUEVTENA_Msk;
}

static inline uint32_t get_LSU_count(void)
{
  return (DWT->LSUCNT) & 0xFF;
}

#endif

static void init_IPCC(void)
{
#if defined(CORE_CM0PLUS)
  LL_C2_AHB3_GRP1_EnableClock(LL_C2_AHB3_GRP1_PERIPH_IPCC);
#endif
#if defined(CORE_CM4)
  LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_IPCC);
#endif
}

static inline uint32_t get_cycle_count(void)
{
#if defined(CORE_CM0PLUS)
  LL_C2_IPCC_SetFlag_CHx(IPCC, LL_IPCC_CHANNEL_1);
  while(LL_C2_IPCC_IsActiveFlag_CHx(IPCC, LL_IPCC_CHANNEL_1)){}
  uint32_t current_cycle = *cycle_count;
  return current_cycle;
#endif
#if defined(CORE_CM4)
  return DWT->CYCCNT;
#endif
}

/******* Communication Functions *******/

int32_t putchar_(char c)
{
  // loop while the LPUART_TDR register is full
  while(LL_LPUART_IsActiveFlag_TXE_TXFNF(LPUART1) != 1);
  // once the LPUART_TDR register is empty, fill it with char c
  LL_LPUART_TransmitData8(LPUART1, (uint8_t)c);
  return (c);
}

static void sysclk_init(void)
{
#if defined(CORE_CM4)
  //set up to run off the 48MHz MSI clock
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
  {
  }

  // Configure the main internal regulator output voltage
  // set voltage scale to range 1, the high performance mode
  // this sets the internal main regulator to 1.2V and SYSCLK can be up to 64MHz
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while(LL_PWR_IsActiveFlag_VOS() == 1); // delay until VOS flag is 0

  // enable a wider range of MSI clock frequencies
  LL_RCC_MSI_EnableRangeSelection();

  /* Insure MSIRDY bit is set before writing MSIRANGE value */
  while (LL_RCC_MSI_IsReady() == 0U)
  {}

  /* Set MSIRANGE default value */
  LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_11);


  // delay until MSI is ready
  while (LL_RCC_MSI_IsReady() == 0U)
  {}

  // delay until MSI is system clock
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_MSI)
  {}

#endif

  // update the global variable SystemCoreClock
  SystemCoreClockUpdate();

  // configure 1ms systick for easy delays
  LL_RCC_ClocksTypeDef clk_struct;
  LL_RCC_GetSystemClocksFreq(&clk_struct);
  LL_Init1msTick(clk_struct.HCLK1_Frequency);
}

static void UART_init(void)
{
  // enable the UART GPIO port clock
#if defined(CORE_CM0PLUS)
  LL_C2_AHB2_GRP1_EnableClock(LL_C2_AHB2_GRP1_PERIPH_GPIOA);
#endif
#if defined(CORE_CM4)
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);
#endif

  // set the LPUART clock source to the peripheral clock
  LL_RCC_SetLPUARTClockSource(LL_RCC_LPUART1_CLKSOURCE_PCLK1);

  // enable clock for LPUART
#if defined(CORE_CM0PLUS)
  LL_C2_APB1_GRP2_EnableClock(LL_C2_APB1_GRP2_PERIPH_LPUART1);
#endif
#if defined(CORE_CM4)
  LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_LPUART1);
#endif

  // configure GPIO pins for LPUART1 communication
  // TX Pin is PA2, RX Pin is PA3
  LL_GPIO_InitTypeDef GPIO_InitStruct = {
  .Pin = LL_GPIO_PIN_2 | LL_GPIO_PIN_3,
  .Mode = LL_GPIO_MODE_ALTERNATE,
  .Pull = LL_GPIO_PULL_NO,
  .Speed = LL_GPIO_SPEED_FREQ_MEDIUM,
  .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
  .Alternate = LL_GPIO_AF_8
};
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // configure the LPUART to transmit with the following settings:
  // baud = 115200, data bits = 8, stop bits = 1, parity bits = 0
  LL_LPUART_InitTypeDef LPUART_InitStruct = {
      .PrescalerValue = LL_LPUART_PRESCALER_DIV1,
      .BaudRate = 115200,
      .DataWidth = LL_LPUART_DATAWIDTH_8B,
      .StopBits = LL_LPUART_STOPBITS_1,
      .Parity = LL_LPUART_PARITY_NONE,
      .TransferDirection = LL_LPUART_DIRECTION_TX_RX,
      .HardwareFlowControl = LL_LPUART_HWCONTROL_NONE
  };
  LL_LPUART_Init(LPUART1, &LPUART_InitStruct);
  LL_LPUART_Enable(LPUART1);

  // wait for the LPUART module to send an idle frame and finish initialization
  while(!(LL_LPUART_IsActiveFlag_TEACK(LPUART1)) || !(LL_LPUART_IsActiveFlag_REACK(LPUART1)));
}


