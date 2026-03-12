#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE BEGIN 0 */
static void ssd1306_SwapInt16(int16_t *a, int16_t *b) {
    int16_t temp = *a;
    *a = *b;
    *b = temp;
}
/* USER CODE END 0 */

#if defined(SSD1306_USE_I2C)

void ssd1306_Reset(void) {
    /* for I2C - do nothing */
}

// Send a byte to the command register
void ssd1306_WriteCommand(uint8_t byte) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x80, 1, &byte, 1, HAL_MAX_DELAY);
}

// Send data
void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1, buffer, buff_size, HAL_MAX_DELAY);
}

#elif defined(SSD1306_USE_SPI)

void ssd1306_Reset(void) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(SSD1306_Reset_Port, SSD1306_Reset_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

void ssd1306_WriteCommand(uint8_t byte) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, (uint8_t *) &byte, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET);
}

void ssd1306_WriteData(uint8_t* buffer, size_t buff_size) {
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SSD1306_DC_Port, SSD1306_DC_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&SSD1306_SPI_PORT, buffer, buff_size, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SSD1306_CS_Port, SSD1306_CS_Pin, GPIO_PIN_SET);
}

#endif

// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

// Screen object
static SSD1306_t SSD1306;

/* Fills the Screenbuffer with values from a given buffer of a fixed length */
SSD1306_Error_t ssd1306_FillBuffer(uint8_t* buf, uint32_t len) {
    SSD1306_Error_t ret = SSD1306_ERR;
    if (len <= SSD1306_BUFFER_SIZE) {
        memcpy(SSD1306_Buffer, buf, len);
        ret = SSD1306_OK;
    }
    return ret;
}

/* Initialize the oled screen */
void ssd1306_Init(void) {
    HAL_Delay(100); // Wait for screen to boot

    ssd1306_SetDisplayOn(0); // display off

    ssd1306_WriteCommand(0x20); // Set Memory Addressing Mode
    ssd1306_WriteCommand(0x00); // Horizontal Addressing Mode

    ssd1306_WriteCommand(0xB0); // Set Page Start Address

#ifdef SSD1306_MIRROR_VERT
    ssd1306_WriteCommand(0xC0);
#else
    ssd1306_WriteCommand(0xC8);
#endif

    ssd1306_WriteCommand(0x00); // set low column address
    ssd1306_WriteCommand(0x10); // set high column address
    ssd1306_WriteCommand(0x40); // set start line address

    ssd1306_SetContrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_WriteCommand(0xA0);
#else
    ssd1306_WriteCommand(0xA1);
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_WriteCommand(0xA7);
#else
    ssd1306_WriteCommand(0xA6);
#endif

    ssd1306_WriteCommand(0xA8); // set multiplex ratio
#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x1F);
#else
    ssd1306_WriteCommand(0x3F);
#endif

    ssd1306_WriteCommand(0xA4); // Output follows RAM content
    ssd1306_WriteCommand(0xD3); // set display offset
    ssd1306_WriteCommand(0x00);
    ssd1306_WriteCommand(0xD5); // set display clock divide ratio
    ssd1306_WriteCommand(0xF0);
    ssd1306_WriteCommand(0xD9); // set pre-charge period
    ssd1306_WriteCommand(0x22);
    ssd1306_WriteCommand(0xDA); // set com pins hardware configuration

#if (SSD1306_HEIGHT == 32)
    ssd1306_WriteCommand(0x02);
#else
    ssd1306_WriteCommand(0x12);
#endif

    ssd1306_WriteCommand(0xDB); // set vcomh
    ssd1306_WriteCommand(0x20);
    ssd1306_WriteCommand(0x8D); // set DC-DC enable
    ssd1306_WriteCommand(0x14);
    ssd1306_SetDisplayOn(1);

    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    SSD1306.Initialized = 1;
}

void ssd1306_Fill(SSD1306_COLOR color) {
    memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

void ssd1306_UpdateScreen(void) {
    for(uint8_t i = 0; i < SSD1306_HEIGHT/8; i++) {
        ssd1306_WriteCommand(0xB0 + i);
        ssd1306_WriteCommand(0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_WriteCommand(0x10 + SSD1306_X_OFFSET_UPPER);
        ssd1306_WriteData(&SSD1306_Buffer[SSD1306_WIDTH*i], SSD1306_WIDTH);
    }
}

void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color) {
    if(x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return;

    if(color == White) {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    } else {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color) {
    uint32_t i, b, j;
    if (ch < 32 || ch > 126) return 0;
    if (SSD1306_WIDTH < (SSD1306.CurrentX + Font.FontWidth) || SSD1306_HEIGHT < (SSD1306.CurrentY + Font.FontHeight)) return 0;

    for(i = 0; i < Font.FontHeight; i++) {
        b = Font.data[(ch - 32) * Font.FontHeight + i];
        for(j = 0; j < Font.FontWidth; j++) {
            if((b << j) & 0x8000)  {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), color);
            } else {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), !color);
            }
        }
    }
    SSD1306.CurrentX += Font.FontWidth;
    return ch;
}

char ssd1306_WriteString(char* str, FontDef Font, SSD1306_COLOR color) {
    while (*str) {
        if (ssd1306_WriteChar(*str, Font, color) != *str) return *str;
        str++;
    }
    return *str;
}

void ssd1306_SetCursor(uint8_t x, uint8_t y) {
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX = ((x1 < x2) ? 1 : -1);
    int32_t signY = ((y1 < y2) ? 1 : -1);
    int32_t error = deltaX - deltaY;
    int32_t error2;

    ssd1306_DrawPixel(x2, y2, color);
    while((x1 != x2) || (y1 != y2)) {
        ssd1306_DrawPixel(x1, y1, color);
        error2 = error * 2;
        if(error2 > -deltaY) {
            error -= deltaY;
            x1 += signX;
        }
        if(error2 < deltaX) {
            error += deltaX;
            y1 += signY;
        }
    }
}

void ssd1306_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, SSD1306_COLOR color){
    ssd1306_Line(x1,y1,x2,y2, color);
    ssd1306_Line(x3,y3,x2,y2, color);
    ssd1306_Line(x1,y1,x3,y3, color);
}

void ssd1306_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, SSD1306_COLOR color) {
    int16_t a, b, y, last;
    int16_t _x1 = x1, _y1 = y1, _x2 = x2, _y2 = y2, _x3 = x3, _y3 = y3;

    if (_y1 > _y2) { ssd1306_SwapInt16(&_y1, &_y2); ssd1306_SwapInt16(&_x1, &_x2); }
    if (_y2 > _y3) { ssd1306_SwapInt16(&_y3, &_y2); ssd1306_SwapInt16(&_x3, &_x2); }
    if (_y1 > _y2) { ssd1306_SwapInt16(&_y1, &_y2); ssd1306_SwapInt16(&_x1, &_x2); }

    if (_y1 == _y3) {
        for (a = _x1; a <= _x3; a++) ssd1306_DrawPixel(a, _y1, color);
    } else {
        int16_t dx13 = _x3 - _x1, dy13 = _y3 - _y1;
        int16_t dx12 = _x2 - _x1, dy12 = _y2 - _y1;
        int16_t dx23 = _x3 - _x2, dy23 = _y3 - _y2;
        int32_t sa = 0, sb = 0;

        last = (_y2 == _y3) ? _y2 : _y2 - 1;

        for (y = _y1; y <= last; y++) {
            a = _x1 + sa / dy13; b = _x1 + sb / dy12;
            sa += dx13; sb += dx12;
            if (a > b) ssd1306_SwapInt16(&a, &b);
            for (int16_t i = a; i <= b; i++) ssd1306_DrawPixel(i, y, color);
        }

        for (y = _y2; y <= _y3; y++) {
            a = _x1 + sa / dy13; b = _x2 + sb / dy23;
            sa += dx13; sb += dx23;
            if (a > b) ssd1306_SwapInt16(&a, &b);
            for (int16_t i = a; i <= b; i++) ssd1306_DrawPixel(i, y, color);
        }
    }
}

void ssd1306_DrawCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR par_color) {
    int32_t x = -par_r, y = 0, err = 2 - 2 * par_r, e2;
    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) return;
    do {
        ssd1306_DrawPixel(par_x - x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y - y, par_color);
        ssd1306_DrawPixel(par_x - x, par_y - y, par_color);
        e2 = err;
        if (e2 <= y) { y++; err = err + (y * 2 + 1); if(-x == y && e2 <= x) e2 = 0; }
        if (e2 > x) { x++; err = err + (x * 2 + 1); }
    } while (x <= 0);
}

void ssd1306_FillCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR par_color) {
    int32_t x = -par_r, y = 0, err = 2 - 2 * par_r, e2;
    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT) return;
    do {
        for (uint8_t _y = (par_y + y); _y >= (par_y - y); _y--) {
            for (uint8_t _x = (par_x - x); _x >= (par_x + x); _x--) {
                ssd1306_DrawPixel(_x, _y, par_color);
            }
        }
        e2 = err;
        if (e2 <= y) { y++; err = err + (y * 2 + 1); if (-x == y && e2 <= x) e2 = 0; }
        if (e2 > x) { x++; err = err + (x * 2 + 1); }
    } while (x <= 0);
}

void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    ssd1306_Line(x1, y1, x2, y1, color);
    ssd1306_Line(x2, y1, x2, y2, color);
    ssd1306_Line(x2, y2, x1, y2, color);
    ssd1306_Line(x1, y2, x1, y1, color);
}

void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color) {
    uint8_t x_start = (x1 <= x2) ? x1 : x2;
    uint8_t x_end = (x1 <= x2) ? x2 : x1;
    uint8_t y_start = (y1 <= y2) ? y1 : y2;
    uint8_t y_end = (y1 <= y2) ? y2 : y1;

    for (uint8_t y = y_start; y <= y_end && y < SSD1306_HEIGHT; y++) {
        for (uint8_t x = x_start; x <= x_end && x < SSD1306_WIDTH; x++) {
            ssd1306_DrawPixel(x, y, color);
        }
    }
}

void ssd1306_SetContrast(const uint8_t value) {
    ssd1306_WriteCommand(0x81);
    ssd1306_WriteCommand(value);
}

void ssd1306_SetDisplayOn(const uint8_t on) {
    ssd1306_WriteCommand(on ? 0xAF : 0xAE);
    SSD1306.DisplayOn = on;
}
