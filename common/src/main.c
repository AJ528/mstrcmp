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

TEST strcmp_test(const char *str1, const char *str2, bool print_performance);
TEST strcmp_variable_alignment(const char *str1, const char *str2, bool print_performance);

#define BUFFER_SIZE 0x100

extern int strcmp_(const char *str1, const char *str2);

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


  RUN_TESTp(strcmp_variable_alignment, "test", "test", true);
  RUN_TESTp(strcmp_variable_alignment, "test_string1", "test_string2", true);

  GREATEST_MAIN_END();    // display results
}

TEST strcmp_test(const char *str1, const char *str2, bool print_performance)
{
  int expected_retval = strcmp(str1, str2);
  uint32_t strcmp_orig_start = get_cycle_count();
  expected_retval = strcmp(str1, str2);
  uint32_t strcmp_orig_stop = get_cycle_count();

  int actual_retval = strcmp(str1, str2);
  uint32_t strcmp_new_start = get_cycle_count();
  actual_retval = strcmp(str1, str2);
  uint32_t strcmp_new_stop = get_cycle_count();

  if(print_performance){
    uint32_t orig_cycle = strcmp_orig_stop-strcmp_orig_start;
    uint32_t new_cycle = strcmp_new_stop-strcmp_new_start;
    int32_t cycle_diff = orig_cycle - new_cycle;
    printfln_("%-6u %-#10x %-#10x %-8u %-8u %-8d", 0, 0, 0, orig_cycle, new_cycle, cycle_diff);
  }

  ASSERT_EQ(expected_retval, actual_retval);
  PASS();
}

TEST strcmp_variable_alignment(const char *str1, const char *str2, bool print_performance)
{
  char s1[BUFFER_SIZE] __attribute__((aligned(4))) = {0};
  char s2[BUFFER_SIZE] __attribute__((aligned(4))) = {0};

  for(int s1_offset = 0; s1_offset < 4; s1_offset++){
    for(int s2_offset = 0; s2_offset < 4; s2_offset++){
      char *s1_addr = s1 + s1_offset;
      char *s2_addr = s2 + s2_offset;
      strcpy(s1_addr, str1);
      strcpy(s2_addr, str2);
      CHECK_CALL(strcmp_test(s1_addr, s2_addr, print_performance));
    }
  }
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

static inline __attribute__((always_inline)) uint32_t get_cycle_count(void)
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


