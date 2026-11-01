/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

typedef enum
{
    WAVE_SINE = 1,
    WAVE_SAWTOOTH,
    WAVE_SQUARE
} WaveType_t;

static volatile uint8_t rx_byte;
static volatile uint8_t pending_command = 0;

static WaveType_t current_wave = WAVE_SINE;
static float wave_phase = 0.0f;
static uint32_t last_update_time = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static float Wave_GetPeriod(void)
{
    switch (current_wave)
    {
        case WAVE_SINE:
            return 1.0f;

        case WAVE_SAWTOOTH:
            return 0.5f;

        case WAVE_SQUARE:
            return 1.5f;

        default:
            return 1.0f;
    }
}

static float Wave_GetValue(void)
{
    switch (current_wave)
    {
        case WAVE_SINE:
            /* 正弦波：-2 ~ 2，周期 1 s */
            return 2.0f * sinf(6.2831853f * wave_phase);

        case WAVE_SAWTOOTH:
            /* 锯齿波：-1 ~ 1，周期 0.5 s */
            return -1.0f + 2.0f * wave_phase;

        case WAVE_SQUARE:
            /* 方波：-0.5 ~ 0.5，占空比 50% */
            if (wave_phase < 0.5f)
            {
                return 0.5f;
            }
            else
            {
                return -0.5f;
            }

        default:
            return 0.0f;
    }
}

static void OLED_ShowWaveName(void)
{
    OLED_operate_gram(PEN_CLEAR);

    switch (current_wave)
    {
        case WAVE_SINE:
            OLED_printf(0, 0, "Sine");
            OLED_printf(2, 0, "T: 1.0 s");
            OLED_printf(4, 0, "A: 2.0");
            break;

        case WAVE_SAWTOOTH:
            OLED_printf(0, 0, "Sawtooth");
            OLED_printf(2, 0, "T: 0.5 s");
            OLED_printf(4, 0, "A: 1.0");
            break;

        case WAVE_SQUARE:
            OLED_printf(0, 0, "Square");
            OLED_printf(2, 0, "T: 1.5 s");
            OLED_printf(4, 0, "A: 0.5");
            break;

        default:
            break;
    }

    OLED_refresh_gram();
}

static void VOFA_SendFloat(float value)
{
    union
    {
        float value;
        uint8_t byte[4];
    } data;

    uint8_t packet[8];

    data.value = value;

    packet[0] = data.byte[0];
    packet[1] = data.byte[1];
    packet[2] = data.byte[2];
    packet[3] = data.byte[3];

    /* JustFloat 帧尾 */
    packet[4] = 0x00;
    packet[5] = 0x00;
    packet[6] = 0x80;
    packet[7] = 0x7F;

    HAL_UART_Transmit(&huart1, packet, 8, 20);
}

static void Process_Command(void)
{
    uint8_t command;

    if (pending_command == 0)
    {
        return;
    }

    command = pending_command;
    pending_command = 0;

    if (command == '1')
    {
        current_wave = WAVE_SINE;
    }
    else if (command == '2')
    {
        current_wave = WAVE_SAWTOOTH;
    }
    else if (command == '3')
    {
        current_wave = WAVE_SQUARE;
    }
    else
    {
        return;
    }

    /* 切换波形后从周期起点开始 */
    wave_phase = 0.0f;

    OLED_ShowWaveName();//刷新oled显示
}

/* USER CODE END 0 */
/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
 /* USER CODE BEGIN 2 */

OLED_init();

/* 上电默认显示正弦波 */
OLED_ShowWaveName();

/* 开启 USART1 单字节中断接收 */
HAL_UART_Receive_IT(&huart1,(uint8_t *)&rx_byte,1);

last_update_time = HAL_GetTick();//上次更新时间

/* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

while (1)
{
    uint32_t now;
    float value;
    float period;

    /* 处理串口指令 */
    Process_Command();

    now = HAL_GetTick();

    /* 每 10 ms 生成并发送一个数据点 */
    if ((now - last_update_time) >= 10U)
    {
        last_update_time += 10U;

        value = Wave_GetValue();

        /* 发送给 VOFA+ */
        VOFA_SendFloat(value);

        /*
         * 相位增量：
         * 10 ms / 波形周期
         */
        period = Wave_GetPeriod();
        wave_phase += 0.010f / period;

        if (wave_phase >= 1.0f)
        {
            wave_phase -= 1.0f;
        }
    }
}
/* USER CODE END WHILE */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /*
         * 保存有效指令。
         * 回车和换行会被直接忽略。
         */
        if ((rx_byte == '1') ||
            (rx_byte == '2') ||
            (rx_byte == '3'))
        {
            pending_command = rx_byte;
        }

        /* 继续接收下一个字节 */
        HAL_UART_Receive_IT(&huart1,(uint8_t *)&rx_byte,1);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
