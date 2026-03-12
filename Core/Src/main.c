/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
#include "stm32h5xx_hal.h"
#include "stdarg.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CZEKAM_NA_START 1
#define ZBIERAM_DANE    2

#define USART_TXBUF_LEN 300
#define USART_RXBUF_LEN 300

#define MAX_FRAME_LEN 270
#define MOJ_ADRES "STM"
/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart2;
extern I2C_HandleTypeDef hi2c1; // Dodany uchwyt I2C dla ekranu

/* USER CODE BEGIN PV */
// Bufor nadawczy (TX)
__IO uint8_t USART_TxBuf[USART_TXBUF_LEN];
__IO int USART_TX_Empty = 0; // Indeks zapisu do bufora
__IO int USART_TX_Busy = 0;  // Indeks wysyłania przez UART

// Bufor odbiorczy (RX) - Bufor kołowy
__IO uint8_t USART_RxBuf[USART_RXBUF_LEN];
__IO int USART_RX_Head = 0; // Indeks zapisu (Interrupt)
__IO int USART_RX_Tail = 0; // Indeks odczytu (Main Loop)

// Zmienne protokołu
char bufor_ramki[MAX_FRAME_LEN];
uint16_t indeks_ramki = 0;
uint8_t stan_programu = CZEKAM_NA_START;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_USART2_UART_Init(void);
void MX_I2C1_Init(void); // Dodany prototyp I2C
/* USER CODE BEGIN PFP */

// Sprawdza czy w buforze kołowym są nowe dane
uint8_t USART_kbhit(void)
{
    return (USART_RX_Head != USART_RX_Tail);
}

// Pobiera jeden znak z bufora kołowego
int16_t USART_getchar(void)
{
    if (USART_RX_Head != USART_RX_Tail)
    {
        uint8_t tmp = USART_RxBuf[USART_RX_Tail];
        USART_RX_Tail = (USART_RX_Tail + 1) % USART_RXBUF_LEN;
        return tmp;
    }
    return -1;
}



// Wysyłanie sformatowane (printf przez UART)
void USART_fsend(char *format, ...)
{
    char tmp_rs[300];
    va_list arglist;
    va_start(arglist, format);
    vsprintf(tmp_rs, format, arglist);
    va_end(arglist);

    int idx = USART_TX_Empty;
    for (int i = 0; i < strlen(tmp_rs); i++)
    {
        USART_TxBuf[idx] = tmp_rs[i];
        idx = (idx + 1) % USART_TXBUF_LEN;
    }

    __disable_irq();
    // Jeśli UART nie wysyła teraz niczego, zacznij wysyłanie pierwszego znaku
    if ((USART_TX_Empty == USART_TX_Busy) && (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) == SET))
    {
        USART_TX_Empty = idx;
        uint8_t tmp = USART_TxBuf[USART_TX_Busy];
        USART_TX_Busy = (USART_TX_Busy + 1) % USART_TXBUF_LEN;
        HAL_UART_Transmit_IT(&huart2, &tmp, 1);
    }
    else
    {
        USART_TX_Empty = idx;
    }
    __enable_irq();
}

uint16_t ObliczCRC(const char *dane, int dlugosc) {
    uint16_t crc = 0x0000;
    for (int i = 0; i < dlugosc; i++) {
        crc ^= ((uint16_t)dane[i] << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x8005;
            else crc <<= 1;
        }
    }
    return crc;
}

void WyslijBlad(const char *adresat, const char *kod_bledu) {
    uint16_t crc = ObliczCRC(kod_bledu, strlen(kod_bledu));
    USART_fsend(">%s%s%03d%s%04X<", MOJ_ADRES, adresat, (int)strlen(kod_bledu), kod_bledu, crc);
}

/* --- Funkcja pomocnicza do animacji --- */
void AnimacjaPrzewijania(int x1, int y1, int x2, int y2, int predkosc, char *tekst) {
    FontDef font = Font_7x10;

    int szerokosc_tekstu = strlen(tekst) * font.FontWidth;
    int delay_ms = 100 - (predkosc * 10);
    if (delay_ms < 5) delay_ms = 5;

    int start_x = x2;
    int end_x = x1 - szerokosc_tekstu;

    for (int curr_x = start_x; curr_x >= end_x; curr_x--) {
        // Czyścimy okno
        ssd1306_FillRectangle(x1, y1, x2, y2, Black);

        // Rysujemy tekst w nowej pozycji
        ssd1306_SetCursor(curr_x, y1);
        ssd1306_WriteString(tekst, font, White);

        ssd1306_UpdateScreen();
        HAL_Delay(delay_ms);

        // Przerwanie animacji
        if (USART_kbhit()) break;
    }
    // Sprzątanie po animacji
    ssd1306_FillRectangle(x1, y1, x2, y2, Black);
    ssd1306_UpdateScreen();
}

void WykonajKomende(char *dane, char *nadawca) {
    if (strncmp(dane, "CLEAR", 5) == 0) {
        ssd1306_Fill(Black);
        ssd1306_UpdateScreen();
        return;
    }

    if (strlen(dane) < 1) {
        WyslijBlad(nadawca, "WCMMD");
        return;
    }

    char naglowek = dane[0];

    // --- FIGURY ---
    if (naglowek == 'F') {
        char figura = dane[1];
        char tryb = dane[2];
        char bX[4] = {0}, bY[3] = {0}, bR[3] = {0};

        if (figura == 'C') { // Koło
            strncpy(bX, dane + 3, 3); int x = atoi(bX);
            strncpy(bY, dane + 6, 2); int y = atoi(bY);
            strncpy(bR, dane + 8, 2); int r = atoi(bR);
            if (tryb == 'P') ssd1306_FillCircle(x, y, r, White);
            else ssd1306_DrawCircle(x, y, r, White);
        }
        else if (figura == 'R') { // Prostokąt
            char bX2[4] = {0}, bY2[3] = {0};
            strncpy(bX, dane + 3, 3); int x1 = atoi(bX);
            strncpy(bY, dane + 6, 2); int y1 = atoi(bY);
            strncpy(bX2, dane + 8, 3); int x2 = atoi(bX2);
            strncpy(bY2, dane + 11, 2); int y2 = atoi(bY2);
            if (tryb == 'P') ssd1306_FillRectangle(x1, y1, x2, y2, White);
            else ssd1306_DrawRectangle(x1, y1, x2, y2, White);
        }
        else if (figura == 'T') { // Trójkąt
            char bX2[4]={0}, bY2[3]={0}, bX3[4]={0}, bY3[3]={0};
            strncpy(bX, dane + 3, 3); int x1 = atoi(bX);
            strncpy(bY, dane + 6, 2); int y1 = atoi(bY);
            strncpy(bX2, dane + 8, 3); int x2 = atoi(bX2);
            strncpy(bY2, dane + 11, 2); int y2 = atoi(bY2);
            strncpy(bX3, dane + 13, 3); int x3 = atoi(bX3);
            strncpy(bY3, dane + 16, 2); int y3 = atoi(bY3);
            if (tryb == 'P') ssd1306_DrawFilledTriangle(x1, y1, x2, y2, x3, y3, White);
            else ssd1306_DrawTriangle(x1, y1, x2, y2, x3, y3, White);
        }
        ssd1306_UpdateScreen();
    }
    // --- TEKST ---
    else if (naglowek == 'T') {
        char numer_czcionki = dane[1];
        char kod_wielkosci[4] = {0};
        strncpy(kod_wielkosci, dane + 2, 3);

        char bX[4] = {0}, bY[3] = {0};
        strncpy(bX, dane + 5, 3); int x = atoi(bX);
        strncpy(bY, dane + 8, 2); int y = atoi(bY);
        char *txt = dane + 10;

        ssd1306_SetCursor(x, y);

        // wskaznik
        FontDef *wybrany_font;

        // --- LOGIKA MAPOWANIA ---

        if (numer_czcionki == '1') {
            if (strncmp(kod_wielkosci, "SMA", 3) == 0)      wybrany_font = &Font_6x8;
            else if (strncmp(kod_wielkosci, "MED", 3) == 0) wybrany_font = &Font_7x10;
            else                                            wybrany_font = &Font_11x18;
        }

        else if (numer_czcionki == '2') {
            if (strncmp(kod_wielkosci, "SMA", 3) == 0)      wybrany_font = &Font_7x10;
            else if (strncmp(kod_wielkosci, "MED", 3) == 0) wybrany_font = &Font_11x18;
            else                                            wybrany_font = &Font_16x26;
        }

        else if (numer_czcionki == '3') {
            if (strncmp(kod_wielkosci, "SMA", 3) == 0)      wybrany_font = &Font_11x18;
            else if (strncmp(kod_wielkosci, "MED", 3) == 0) wybrany_font = &Font_16x24;
            else                                            wybrany_font = &Font_16x26;
        }

        else if (numer_czcionki == '4') {
            if (strncmp(kod_wielkosci, "SMA", 3) == 0)      wybrany_font = &Font_6x8;
            else if (strncmp(kod_wielkosci, "MED", 3) == 0) wybrany_font = &Font_16x24;
            else                                            wybrany_font = &Font_16x26;
        }
        else {
            //  adres (&)
            wybrany_font = &Font_7x10;
        }

        ssd1306_WriteString(txt, *wybrany_font, White);
        ssd1306_UpdateScreen();
    }
    // --- PRZEWIJANIE ---
    else if (naglowek == 'S') {
        if (strlen(dane) < 12) { WyslijBlad(nadawca, "WRLEN"); return; }

        int predkosc = dane[1] - '0';
        if (predkosc < 0 || predkosc > 9) predkosc = 5;

        char bX1[4] = {0}, bY1[3] = {0}, bX2[4] = {0}, bY2[3] = {0};

        strncpy(bX1, dane + 2, 3);
        strncpy(bY1, dane + 5, 2);
        strncpy(bX2, dane + 7, 3);
        strncpy(bY2, dane + 10, 2);

        int x1 = atoi(bX1);
        int y1 = atoi(bY1);
        int x2 = atoi(bX2);
        int y2 = atoi(bY2);
        char *tekst_scroll = dane + 12;

        AnimacjaPrzewijania(x1, y1, x2, y2, predkosc, tekst_scroll);
    }

    else {
        WyslijBlad(nadawca, "WCMMD");
    }
}

void AnalizujRamke(char *ramka, int len) {
    if (len < 13) return;
    char nadawca[4] = {0}, odbiorca[4] = {0}, len_str[4] = {0}, crc_str[5] = {0};

    strncpy(nadawca, ramka, 3);
    strncpy(odbiorca, ramka + 3, 3);
    strncpy(len_str, ramka + 6, 3);

    if (strcmp(odbiorca, MOJ_ADRES) != 0) return;
    if ((len_str[0] < '0' || len_str[0] > '9') || (len_str[1] < '0' || len_str[1] > '9') || (len_str[2] < '0' || len_str[2] > '9')) return;
    int d_len = atoi(len_str);
    if (d_len != (len - 13)) { WyslijBlad(nadawca, "WRLEN"); return; }

    char *dane = ramka + 9;
    strncpy(crc_str, ramka + 9 + d_len, 4);
    if (((crc_str[0] < '0' || crc_str[0] > '9') && (crc_str[0] < 'A' || crc_str[0] > 'F'))
    		|| ((crc_str[1] < '0' || crc_str[1] > '9') && (crc_str[1] < 'A' || crc_str[1] > 'F'))
			|| ((crc_str[2] < '0' || crc_str[2] > '9') && (crc_str[2] < 'A' || crc_str[2] > 'F'))
			|| ((crc_str[3] < '0' || crc_str[3] > '9') && (crc_str[3] < 'A' || crc_str[3] > 'F'))) return;
    uint16_t odebrane_crc = (uint16_t)strtol(crc_str, NULL, 16);

    if (ObliczCRC(dane, d_len) != odebrane_crc) {
        WyslijBlad(nadawca, "WRSUM");
        return;
    }

    dane[d_len] = '\0';
    WykonajKomende(dane, nadawca);
}

// Callback przerwania po wysłaniu znaku
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2 && USART_TX_Empty != USART_TX_Busy)
    {
        uint8_t tmp = USART_TxBuf[USART_TX_Busy];
        USART_TX_Busy = (USART_TX_Busy + 1) % USART_TXBUF_LEN;
        HAL_UART_Transmit_IT(&huart2, &tmp, 1);
    }
}

// Callback przerwania po odebraniu znaku
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2)
    {
        USART_RX_Head = (USART_RX_Head + 1) % USART_RXBUF_LEN;
        HAL_UART_Receive_IT(&huart2, (uint8_t*)&USART_RxBuf[USART_RX_Head], 1);
    }
}
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init(); // Inicjalizacja I2C

  /* USER CODE BEGIN 2 */
  // Start odbioru UART
  HAL_UART_Receive_IT(&huart2, (uint8_t*)&USART_RxBuf[USART_RX_Head], 1);

  ssd1306_Init();
  ssd1306_Fill(Black);
  ssd1306_UpdateScreen();
  USART_fsend("STM GOTOWY\n");
  /* USER CODE END 2 */

  uint8_t znak;
  while (1)
  {
      while (USART_kbhit())
      {
          znak = USART_getchar();
          if (znak == 0) {
              stan_programu = CZEKAM_NA_START;
          }
          if (znak == '>') {
              indeks_ramki = 0;
              stan_programu = ZBIERAM_DANE;
          }
          else if (stan_programu == ZBIERAM_DANE) {
              if (znak == '<') {
                  bufor_ramki[indeks_ramki] = '\0';
                  AnalizujRamke(bufor_ramki, indeks_ramki);
                  stan_programu = CZEKAM_NA_START;
              }
              else  {
            	  bufor_ramki[indeks_ramki++] = znak;
            	  if  (indeks_ramki < MAX_FRAME_LEN - 1) {
                      stan_programu = CZEKAM_NA_START;
            	  }
              }
          }
      }
  }
}

/* --- KONFIGURACJA SPRZĘTOWA --- */




void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC->APB3ENR |= (1UL << 28); // Bezpośrednie włączenie zegara PWR w rejestrze APB3
  __IO uint32_t tmpreg = RCC->APB3ENR; // Krótkie opóźnienie dla stabilizacji
  (void)tmpreg;
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) { Error_Handler(); }
}


void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}
