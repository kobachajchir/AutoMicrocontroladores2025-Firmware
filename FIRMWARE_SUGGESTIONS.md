# Embedded C Firmware Audit & Suggestions

This document presents a detailed audit of the C firmware codebase (focused on `Core/Src` and `Core/Inc`), categorizing suggestions from most important to least important: Security, Stability & Bugs, Performance, and Code Quality.

Each suggestion includes the file, lines involved, the identified issue, and a detailed plan to resolve it.

---

## 1. Security

### 1.1 Unbounded String Copies and Length Validations
- **File:** `Core/Src/uner_app.c`
- **Lines:** ~430, ~540 (Functions `UNER_App_SetOledSsidFromBytes` and `UNER_App_ParseFirmwareResponse`)
- **Issue:** Strings provided via UNER packets (such as the SSID or the ESP firmware version) are copied directly into fixed-size buffers. While there are some checks `if (copy_len > WIFI_SSID_MAX_LEN)`, the logic uses `memcpy` followed by manual null-termination, which can be prone to off-by-one errors or truncation issues if max bounds change.
- **Solution / Idea:**
  - Standardize string buffering by using `snprintf` or robust variants like `strlcpy` wherever dynamic data is copied into fixed global arrays (`espFirmwareVersion`, `ssid_buf`, `wifiNetworkSsids`).
  - Example: `snprintf(ssid_buf, sizeof(ssid_buf), "%.*s", copy_len, ssid);`. This prevents manual boundary miscalculations.

### 1.2 Authentication Validation Payload Length
- **File:** `Core/Src/uner_app.c`
- **Lines:** ~700 (`UNER_App_ParseAuthValidatePinResponse`)
- **Issue:** The PIN validation parses `ttl_ms` using hardcoded offsets (`offset + 1u`, `offset + 2u`, etc.) after only checking if there are 4 bytes left `if ((uint8_t)(p->len - offset) >= 4u)`. A malformed packet with bad lengths can easily cause OOB memory reads leading to data leaks or HardFaults.
- **Solution / Idea:**
  - Create a safer serialization/deserialization helper for extracting uint32_t from payloads, returning a status code rather than directly indexing arrays.
  - Assert the packet length is exactly as expected, rather than just `len < 4u`.

---

## 2. Stability & Bugs

### 2.1 UART1 DMA Sychronization and State Resets
- **File:** `Core/Src/utils.c`
- **Lines:** ~17 (`USART1_DMA_CheckHealth`)
- **Issue:** The health check function re-enables DMA RX if the state becomes `HAL_UART_STATE_READY` or `USART_CR3_DMAR` clears. However, resetting the DMA blindly clears any bytes already present in the hardware FIFO or partially transferred by DMA, leading to dropped UNER protocol packets or corrupted parsing states.
- **Solution / Idea:**
  - Instead of resetting `HAL_UART_Receive_DMA` abruptly, add a logic that cleanly flushes the parsing state in the UNER Core state machine when a UART error (like Overrun or Framing) is detected.
  - Check the UART error flags explicitly (`__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)`) and use the correct HAL abort functions before restarting the DMA.

### 2.2 I2C Manager State Machine Spurious Errors
- **File:** `Core/Src/i2c_manager.c`
- **Lines:** ~170 (`I2C_Manager_OnError`)
- **Issue:** If an I2C error occurs, the manager forcibly releases the bus (`hmgr->owner_id = NONE`) and grants it to the next pending device immediately, while ignoring the potential hardware state of the I2C peripheral (e.g. still outputting a STOP condition or stuck busy).
- **Solution / Idea:**
  - Before granting the bus to the next device, the `recover_cb` must ensure the I2C hardware is completely reset (e.g., toggling the SWRST bit in I2C CR1 or performing a GPIO toggle sequence).
  - Delay the next bus grant until `HAL_I2C_GetState()` reports `HAL_I2C_STATE_READY`.

### 2.3 OLED State Machine Non-Atomic Access
- **File:** `Core/Src/main.c`
- **Lines:** ~65 (`OLED_Is_Ready`), ~350 (`OLED_MainTask`)
- **Issue:** `menuSystem.renderFlag` and `oled_first_draw` are accessed and modified both in normal thread execution (`main()` loop) and inside 10ms Timer callbacks (`TIM3` -> `OLED_Task_10ms` setting flags). This lacks `volatile` atomicity and could cause race conditions where a render is lost or double-executed.
- **Solution / Idea:**
  - Standardize `menuSystem.renderFlag` type to `volatile bool` and implement critical sections (`__disable_irq(); ... __enable_irq();`) around accesses where the flag and the function pointers are modified together.

---

## 3. Performance

### 3.1 OLED Pixel Rendering Overheads
- **File:** `Core/Src/ssd1306.c`
- **Issue:** The SSD1306 rendering is heavily dependent on bitmap drawings and pixel-by-pixel iterations for geometries. Since the SSD1306 is page-oriented (8 vertical pixels per byte), drawing non-aligned pixels requires reading the buffer, bit-masking, and writing back, which is CPU intensive.
- **Solution / Idea:**
  - Ensure all heavily-used UI components (fonts, icons, menus) are optimized to align with 8-pixel horizontal/vertical boundaries where possible.
  - Implement a `ssd1306_DrawBitmap_MSB` wrapper that transposes the data upfront into vertical chunks. The project memory states there's an optimized version for QR codes, it should be applied to standard UI icon rendering as well to cut down CPU cycles spent before triggering the DMA I2C transmission.

### 3.2 UNER Core Polling Overhead
- **File:** `Core/Src/uner_core.c` and `Core/Src/uner_transport_uart1_dma.c`
- **Issue:** The function `UNER_TransportUart1Dma_PollRx` polls the DMA counter every cycle of the main `while(1)`. This wastes CPU cycles when no data is arriving on UART.
- **Solution / Idea:**
  - Take advantage of the `UART_IT_IDLE` interrupt or `uner_uart1_rx_hint`. The `UNER_App_Poll` function should skip `UNER_Handle_Poll` completely if the DMA counter hasn't changed since the last poll.
  - Track `__HAL_DMA_GET_COUNTER` at the start of `Poll()`. If the difference is zero compared to the previous tick, `return` early.

---

## 4. Code Quality & Maintenance

### 4.1 Global Variables Proliferation
- **File:** `Core/Inc/globals.h`
- **Lines:** 85-180
- **Issue:** The `globals.h` file acts as a massive sink for system flags (`systemFlags`, `systemFlags2`, `systemFlags3`), state variables, and handlers. This creates tight coupling and makes unit testing incredibly difficult, as any test requires mocking the entire global state.
- **Solution / Idea:**
  - Migrate towards a Singleton or Context-based architecture (e.g., `SystemContext_t` or `AppState_t`) managed by accessors.
  - Split `globals.h` into domain-specific contexts: `wifi_state.h`, `motor_state.h`, `sensor_state.h`.

### 4.2 Hardcoded Magic Numbers
- **File:** `Core/Src/mpu6050.c` (and others)
- **Lines:** ~210 (`hmpu->data.gyro_x_mdps = gx * 1000 / 131;`)
- **Issue:** There are many magic numbers like `131`, `16384`, `340`, and `3653` used for MPU conversions, or `3900` for motor frequencies.
- **Solution / Idea:**
  - Define macros with clear names in the header files (e.g., `#define MPU6050_GYRO_SENSITIVITY_250DPS 131.0f`). This makes the math self-documenting and much easier to adjust if the sensor configuration (e.g., Full Scale Range) changes.

### 4.3 Breakpoints in Production Code
- **File:** `Core/Src/main.c`, `Core/Src/i2c_manager.c`, etc.
- **Issue:** The codebase contains multiple instances of `__NOP(); // BREAKPOINT: ...`. This adds unnecessary instructions in production builds and clutters the code logic.
- **Solution / Idea:**
  - Use conditional debugging macros instead: `#ifdef DEBUG_BREAKPOINTS / __BKPT(0); / #endif`. Or simply remove them once development for those specific features is stabilized.
