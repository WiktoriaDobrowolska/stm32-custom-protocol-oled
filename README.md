# 🕹️ STM32 Custom Protocol & Advanced OLED Controller

## 📌 Project Overview
An embedded system project completed as part of the **Microprocessors** university course. The main goal was to implement fully asynchronous communication with a PC using a custom protocol and to create an advanced graphical driver for an OLED display, operating at the bit-manipulation level.

The project demonstrates proficiency in programming STM32 microcontrollers (ARM Cortex-M33 core), handling interrupts, memory management (ring buffers), and communication via UART and I2C buses.

## 🛠️ Tech Stack
* **Microcontroller:** STM32H533RE (Nucleo-H533RE)
* **Peripherals:** 0.96" OLED Display (SSD1306 Controller)
* **Interfaces:** UART (Asynchronous), I2C (Fast Mode)
* **Language & Environment:** C (C99 Standard), STM32CubeIDE, HAL libraries

---

## 📜 Implemented Specification & Key Features

### 1. Custom Communication Protocol (UART + Interrupts)
A protocol was built from scratch to allow the secure transmission of arbitrary data while maintaining its integrity:
* **Frame Structure:** `[>] [Sender] [Receiver] [Data Length] [Data] [Checksum] [<]`.
* **Asynchrony & Ring Buffers:** Communication (receiving and transmitting) is executed entirely in the background via interrupt vectors (`USART2_IRQHandler`), utilizing ring buffers to eliminate the risk of data loss.
* **Transmission Security:** Node addressing, packet order checking, and checksum verification were implemented. Invalid frames are automatically rejected by the state machine.

### 2. Advanced OLED Graphical Driver (I2C)
Instead of using simple, pre-made solutions, custom algorithms for rendering graphics in the microcontroller's RAM buffer were implemented (optimizing bitwise operations in `ssd1306_DrawPixel`):
* **Geometric Primitives:** Hardware-level drawing of rectangles, triangles, and circles (both outlined and filled).
* **Typography Support:** Designed and integrated **four different font sizes** (e.g., `Font_7x10`, `Font_11x18`), allowing for dynamic text formatting.
* **Text Scrolling:** Developed a dedicated procedure that enables smooth text scrolling within a strictly defined screen window at a user-specified speed.

---

## 📂 Project Structure
* `/Core/Inc` and `/Core/Src` – C source code files.
  * `main.c` – Main loop, protocol state machine.
  * `stm32h5xx_it.c` – Hardware interrupt handling.
  * `ssd1306.c` / `ssd1306_fonts.c` – Graphical algorithms, font rendering, and buffer transmission (`ssd1306_UpdateScreen` function).
* `/docs` – Technical project documentation including the description of the transmission frame structure.

---

## ⚙️ Hardware Setup

**OLED Display (SSD1306) <-> STM32 Nucleo**
* `VCC`  -> `3V3`
* `GND`  -> `GND`
* `SCL`  -> `PB6` (I2C1 Clock)
* `SDA`  -> `PB7` (I2C1 Data)

**PC Communication**
Connect the board to the computer via the USB port (the built-in ST-Link will handle the virtual COM port). Configure the serial terminal: `Baudrate: 115200, 8N1`. Data must be sent according to the frame format defined in the specification (starting with the `>` character, ending with `<`).
