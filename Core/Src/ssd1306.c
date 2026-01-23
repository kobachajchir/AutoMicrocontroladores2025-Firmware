/*
 * ssd1306.c
 *
 *  Created on: 14/04/2018
 *  Update on: 10/04/2019
 *      Author: Andriy Honcharenko
 *      version: 2
 *
 *  Modify on: 06/11/2021
 *      Author: Roberto Benjami
 *  Added features in DMA mode:
 *  - ssd1306_UpdateScreen works without blocking
 *  - you can query that UpdateScreen is complete (if ssd1306_UpdateScreenCompleted() == 1)
 *  - callback function if UpdateScreen is complete (ssd1306_UpdateCompletedCallback)
 *  Added features in DMA mode with continuous display update:
 *  - enable continuous display update
 *  - disable continuous display update
 *  - enable raster interrupt(s) of PAGEx (you can set which PAGE(s) )
 *
 *  Modified for I2C Manager arbitration (Koba):
 *  - When SSD1306_USE_I2C_MANAGER==1, the OLED never owns the bus permanently.
 *  - Update is event-driven: UpdateScreen() requests the bus; on grant, it runs a TX DMA state-machine.
 *  - No HAL_I2C_MemTxCpltCallback implementation here; completion must be routed via I2C_Manager_OnTxCplt/OnRxCplt().
 */

#include <math.h>
#include "ssd1306.h"

#if SSD1306_USE_I2C_MANAGER == 1 && SSD1306_CONTUPDATE == 1
#error "SSD1306_CONTUPDATE must be 0 when using I2C Manager (bus arbitration)."
#endif

#if SSD1306_USE_DMA == 0 && SSD1306_CONTUPDATE == 1
#error SSD1306_CONTUPDATE only in DMA MODE !
#endif

// Screen object
static SSD1306_t SSD1306;
// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];
// SSD1306 display geometry
SSD1306_Geometry display_geometry = SSD1306_GEOMETRY;

//
//  Get a width and height screen size
//
static const uint16_t width(void)  { return SSD1306_WIDTH; }
static const uint16_t height(void) { return SSD1306_HEIGHT; }

uint16_t ssd1306_GetWidth(void)  { return SSD1306_WIDTH; }
uint16_t ssd1306_GetHeight(void) { return SSD1306_HEIGHT; }

SSD1306_COLOR ssd1306_GetColor(void) { return SSD1306.Color; }

void ssd1306_SetColor(SSD1306_COLOR color) { SSD1306.Color = color; }

/* -------------------------------------------------------------------------- */
/* I2C MANAGER INTEGRATION (event-driven DMA update)                           */
/* -------------------------------------------------------------------------- */
#if SSD1306_USE_I2C_MANAGER == 1

static I2C_DeviceID g_oled_dev_id = DEVICE_ID_OLED;
static I2C_ManagerHandle *g_i2c_mgr = NULL;

typedef struct {
  uint8_t used;
  uint8_t page;
  I2C_RequestPrio prio;
  uint8_t keep_bus;
  uint32_t seq;
  OLED_SM_State state;
} OLED_PageReq;

static OLED_PageReq g_page_q[8];
static uint32_t g_seq = 0;
static uint8_t oled_inflight_page = 0xFFu;

// simple state machine for page update
typedef enum {
  OLED_SM_IDLE = 0,
  OLED_SM_CMD_PAGE,
  OLED_SM_CMD_LOWCOL,
  OLED_SM_CMD_HIGHCOL,
  OLED_SM_DATA,
  OLED_SM_PAGE_DONE
} OLED_SM_State;

static volatile uint8_t oled_page = 0;

// we reuse a single byte buffer for commands (DMA needs RAM address)
static uint8_t oled_cmd_byte = 0;

// manager callbacks
static void OLED_I2C_RequestApproved(void *ctx);
static void OLED_I2C_TxComplete(void *ctx);
static void OLED_I2C_TransferComplete(void);
static void OLED_I2C_Error(void *ctx, I2C_ManagerError err);
static uint8_t OLED_QueueHasPending(void);
static uint8_t OLED_QueueMaxPrio(void);
static uint8_t OLED_QueueSelectNext(uint8_t *slot_idx);
static uint8_t OLED_QueueFindByPage(uint8_t page, uint8_t *slot_idx);
static HAL_StatusTypeDef OLED_QueueUpsert(uint8_t page, I2C_RequestPrio prio, uint8_t keep_bus);
static void OLED_SendForState(uint8_t page, OLED_SM_State state);

// helper: start first step of a frame
static void OLED_StartFrame(uint8_t page)
{
  oled_page = page;

  __NOP(); // BREAKPOINT: inicio de frame OLED (page 0)
  oled_cmd_byte = (uint8_t)(0xB0u + oled_page);
  (void)HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &oled_cmd_byte, 1);
}

static uint8_t OLED_QueueHasPending(void)
{
  for (uint8_t i = 0; i < 8; i++) {
    if (g_page_q[i].used) {
      return 1;
    }
  }
  return 0;
}

static uint8_t OLED_QueueMaxPrio(void)
{
  I2C_RequestPrio max_prio = I2C_REQ_PRIO_LOW;
  for (uint8_t i = 0; i < 8; i++) {
    if (g_page_q[i].used && g_page_q[i].prio > max_prio) {
      max_prio = g_page_q[i].prio;
    }
  }
  return (uint8_t)max_prio;
}

static uint8_t OLED_QueueSelectNext(uint8_t *slot_idx)
{
  uint8_t best = 0xFFu;
  I2C_RequestPrio best_prio = I2C_REQ_PRIO_LOW;
  uint32_t best_seq = 0xFFFFFFFFu;

  for (uint8_t i = 0; i < 8; i++) {
    if (!g_page_q[i].used) {
      continue;
    }
    if (best == 0xFFu ||
        g_page_q[i].prio > best_prio ||
        (g_page_q[i].prio == best_prio && g_page_q[i].seq < best_seq)) {
      best = i;
      best_prio = g_page_q[i].prio;
      best_seq = g_page_q[i].seq;
    }
  }

  if (best == 0xFFu) {
    return 0;
  }

  *slot_idx = best;
  return 1;
}

static uint8_t OLED_QueueFindByPage(uint8_t page, uint8_t *slot_idx)
{
  for (uint8_t i = 0; i < 8; i++) {
    if (g_page_q[i].used && g_page_q[i].page == page) {
      *slot_idx = i;
      return 1;
    }
  }
  return 0;
}

static HAL_StatusTypeDef OLED_QueueUpsert(uint8_t page, I2C_RequestPrio prio, uint8_t keep_bus)
{
  uint8_t free_idx = 0xFFu;
  uint8_t victim = 0xFFu;
  I2C_RequestPrio victim_prio = I2C_REQ_PRIO_HIGH;
  uint32_t victim_seq = 0;

  for (uint8_t i = 0; i < 8; i++) {
    if (g_page_q[i].used && g_page_q[i].page == page) {
      g_page_q[i].prio = (g_page_q[i].prio < prio) ? prio : g_page_q[i].prio;
      g_page_q[i].keep_bus = keep_bus;
      g_page_q[i].state = OLED_SM_IDLE;
      g_page_q[i].seq = g_seq++;
      return HAL_OK;
    }
    if (!g_page_q[i].used && free_idx == 0xFFu) {
      free_idx = i;
    }
  }

  if (free_idx != 0xFFu) {
    g_page_q[free_idx].used = 1;
    g_page_q[free_idx].page = page;
    g_page_q[free_idx].prio = prio;
    g_page_q[free_idx].keep_bus = keep_bus;
    g_page_q[free_idx].state = OLED_SM_IDLE;
    g_page_q[free_idx].seq = g_seq++;
    return HAL_OK;
  }

  for (uint8_t i = 0; i < 8; i++) {
    if (!g_page_q[i].used) {
      continue;
    }
    if (victim == 0xFFu ||
        g_page_q[i].prio < victim_prio ||
        (g_page_q[i].prio == victim_prio && g_page_q[i].seq < victim_seq)) {
      victim = i;
      victim_prio = g_page_q[i].prio;
      victim_seq = g_page_q[i].seq;
    }
  }

  if (victim != 0xFFu) {
    g_page_q[victim].used = 1;
    g_page_q[victim].page = page;
    g_page_q[victim].prio = prio;
    g_page_q[victim].keep_bus = keep_bus;
    g_page_q[victim].state = OLED_SM_IDLE;
    g_page_q[victim].seq = g_seq++;
    return HAL_OK;
  }

  return HAL_ERROR;
}

static void OLED_I2C_RequestApproved(void *ctx)
{
  (void)ctx;
  if (oled_inflight_page != 0xFFu) {
    return;
  }
  uint8_t slot_idx = 0xFFu;
  if (!OLED_QueueSelectNext(&slot_idx)) {
    if (g_i2c_mgr) {
      (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
    }
    return;
  }
  oled_inflight_page = g_page_q[slot_idx].page;
  g_page_q[slot_idx].state = OLED_SM_CMD_PAGE;
  __NOP(); // BREAKPOINT: bus I2C otorgado al OLED (update pendiente)
  OLED_StartFrame(oled_inflight_page);

  // If we got granted while already mid-transfer, do nothing.
  // Transfers advance on DMA complete callback.
}

static void OLED_I2C_TxComplete(void *ctx)
{
  (void)ctx;
  if (g_i2c_mgr && I2C_Manager_GetOwner(g_i2c_mgr) != g_oled_dev_id) {
    return;
  }
  if (oled_inflight_page == 0xFFu) {
    return;
  }
  OLED_I2C_TransferComplete();
}

static void OLED_I2C_TransferComplete(void)
{
  // OLED only does TX in this driver. If manager passes RX, ignore safely.
  if (oled_inflight_page == 0xFFu) {
    // spurious callback
    if (g_i2c_mgr) {
      (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
    }
    return;
  }

  uint8_t slot_idx = 0xFFu;
  if (!OLED_QueueFindByPage(oled_inflight_page, &slot_idx)) {
    oled_inflight_page = 0xFFu;
    if (g_i2c_mgr) {
      (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
    }
    return;
  }

  switch (g_page_q[slot_idx].state)
  {
    case OLED_SM_CMD_PAGE:
      g_page_q[slot_idx].state = OLED_SM_CMD_LOWCOL;
      __NOP(); // BREAKPOINT: comando de low column
      OLED_SendForState(oled_inflight_page, OLED_SM_CMD_LOWCOL);
      break;

    case OLED_SM_CMD_LOWCOL:
      g_page_q[slot_idx].state = OLED_SM_CMD_HIGHCOL;
      __NOP(); // BREAKPOINT: comando de high column
      OLED_SendForState(oled_inflight_page, OLED_SM_CMD_HIGHCOL);
      break;

    case OLED_SM_CMD_HIGHCOL:
      g_page_q[slot_idx].state = OLED_SM_DATA;
      __NOP(); // BREAKPOINT: envío de datos de página
      OLED_SendForState(oled_inflight_page, OLED_SM_DATA);
      break;

    case OLED_SM_DATA:
      g_page_q[slot_idx].state = OLED_SM_PAGE_DONE;
      __NOP(); // BREAKPOINT: frame OLED completo

      ssd1306_PageSentCallback(oled_inflight_page);
      g_page_q[slot_idx].used = 0;

      uint8_t keep_bus = g_page_q[slot_idx].keep_bus;
      oled_inflight_page = 0xFFu;

      if (g_i2c_mgr) {
        (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
      }

      if (keep_bus && OLED_QueueHasPending()) {
        if (g_i2c_mgr) {
          (void)I2C_Manager_RequestBusPrio(
              g_i2c_mgr,
              g_oled_dev_id,
              (I2C_RequestPrio)OLED_QueueMaxPrio());
        }
      } else if (!OLED_QueueHasPending()) {
        ssd1306_UpdateCompletedCallback();
        ssd1306_QueueEmptyCallback();
      }
      break;

    default:
      // Fail-safe: never deadlock the bus
      oled_inflight_page = 0xFFu;
      if (g_i2c_mgr) {
        (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
      }
      break;
  }
}

static void OLED_I2C_Error(void *ctx, I2C_ManagerError err)
{
  (void)ctx;
  (void)err;
  oled_inflight_page = 0xFFu;
  if (g_i2c_mgr) {
    (void)I2C_Manager_ReleaseBus(g_i2c_mgr, g_oled_dev_id);
  }
}

static void OLED_SendForState(uint8_t page, OLED_SM_State state)
{
  switch (state)
  {
    case OLED_SM_CMD_LOWCOL:
      oled_cmd_byte = (uint8_t)SETLOWCOLUMN;
      (void)HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &oled_cmd_byte, 1);
      break;

    case OLED_SM_CMD_HIGHCOL:
      oled_cmd_byte = (uint8_t)SETHIGHCOLUMN;
      (void)HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &oled_cmd_byte, 1);
      break;

    case OLED_SM_DATA:
      (void)HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1,
                                  &SSD1306_Buffer[SSD1306_WIDTH * page],
                                  width());
      break;

    default:
      break;
  }
}

#endif /* SSD1306_USE_I2C_MANAGER */

HAL_StatusTypeDef ssd1306_BindI2CManager(I2C_ManagerHandle *hmgr, I2C_DeviceID dev_id)
{
#if SSD1306_USE_I2C_MANAGER == 1
  if (!hmgr) return HAL_ERROR;
  g_i2c_mgr = hmgr;
  g_oled_dev_id = dev_id;

  __NOP(); // BREAKPOINT: bind OLED al I2C Manager
  return I2C_Manager_RegisterDevice(
      hmgr,
      dev_id,
      SSD1306_ADDRESS,
      1,
      NULL,
      OLED_I2C_RequestApproved,
      OLED_I2C_TxComplete,
      NULL,
      OLED_I2C_Error
  );
#else
  (void)hmgr;
  (void)dev_id;
  return HAL_ERROR;
#endif
}

HAL_StatusTypeDef ssd1306_RequestPageUpdate(uint8_t page,
                                            I2C_RequestPrio prio,
                                            uint8_t keep_bus)
{
#if SSD1306_USE_I2C_MANAGER == 1
  uint8_t max_pages = (uint8_t)(SSD1306_HEIGHT / 8);
  if (page >= max_pages) {
    return HAL_ERROR;
  }
  if (OLED_QueueUpsert(page, prio, keep_bus) != HAL_OK) {
    return HAL_ERROR;
  }
  if (oled_inflight_page == 0xFFu && g_i2c_mgr) {
    (void)I2C_Manager_RequestBusPrio(g_i2c_mgr, g_oled_dev_id, prio);
  }
  return HAL_OK;
#else
  (void)page;
  (void)prio;
  (void)keep_bus;
  return HAL_ERROR;
#endif
}


/* -------------------------------------------------------------------------- */
/* INIT                                                                        */
/* -------------------------------------------------------------------------- */

// A blocking command write used ONLY during init (safe and simple).
static void ssd1306_WriteCommand_Blocking(uint8_t command)
{
  (void)HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &command, 1, 10);
}

//  Initialize the oled screen
uint8_t ssd1306_Init(void)
{
  __NOP(); // BREAKPOINT: inicio de ssd1306_Init
  /* Check if LCD connected to I2C */
#if SSD1306_USE_I2C_MANAGER == 1
  // Manager must be initialized+registered BEFORE init:
  // I2C_Manager_RegisterDevice(hmgr, DEVICE_ID_OLED, SSD1306_ADDRESS, prio, ctx, OLED_I2C_RequestApproved, OLED_I2C_TxComplete, NULL, NULL)
  // But we still check readiness via HAL here to keep init robust:
  if (HAL_I2C_IsDeviceReady(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 5, 1000) != HAL_OK)
  {
    SSD1306.Initialized = 0;
    return 0;
  }
#endif
#if SSD1306_USE_I2C_MANAGER == 1
  __NOP(); // BREAKPOINT: OLED listo en I2C (init)
#else
  if (HAL_I2C_IsDeviceReady(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 5, 1000) != HAL_OK)
  {
    SSD1306.Initialized = 0;
    /* Return false */
    return 0;
  }
#endif

  // Wait for the screen to boot
  HAL_Delay(100);

  /* Init LCD (BLOCKING ONLY HERE) */
  ssd1306_WriteCommand_Blocking(DISPLAYOFF);
  ssd1306_WriteCommand_Blocking(SETDISPLAYCLOCKDIV);
  ssd1306_WriteCommand_Blocking(0xF0); // Increase speed of the display max ~96Hz
  ssd1306_WriteCommand_Blocking(SETMULTIPLEX);
  ssd1306_WriteCommand_Blocking(height() - 1);
  ssd1306_WriteCommand_Blocking(SETDISPLAYOFFSET);
  ssd1306_WriteCommand_Blocking(0x00);
  ssd1306_WriteCommand_Blocking(SETSTARTLINE);
  ssd1306_WriteCommand_Blocking(CHARGEPUMP);
  ssd1306_WriteCommand_Blocking(0x14);
  ssd1306_WriteCommand_Blocking(MEMORYMODE);
  ssd1306_WriteCommand_Blocking(0x00);
  ssd1306_WriteCommand_Blocking(SEGREMAP | 0x01);
  ssd1306_WriteCommand_Blocking(COMSCANDEC);
  ssd1306_WriteCommand_Blocking(SETCOMPINS);

  if (display_geometry == GEOMETRY_128_64)
  {
    ssd1306_WriteCommand_Blocking(0x12);
  }
  else if (display_geometry == GEOMETRY_128_32)
  {
    ssd1306_WriteCommand_Blocking(0x02);
  }

  ssd1306_WriteCommand_Blocking(SETCONTRAST);

  if (display_geometry == GEOMETRY_128_64)
  {
    ssd1306_WriteCommand_Blocking(0xCF);
  }
  else if (display_geometry == GEOMETRY_128_32)
  {
    ssd1306_WriteCommand_Blocking(0x8F);
  }

  ssd1306_WriteCommand_Blocking(SETPRECHARGE);
  ssd1306_WriteCommand_Blocking(0xF1);
  ssd1306_WriteCommand_Blocking(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  ssd1306_WriteCommand_Blocking(0x40);          //0x40 default, to lower the contrast, put 0
  ssd1306_WriteCommand_Blocking(DISPLAYALLON_RESUME);
  ssd1306_WriteCommand_Blocking(NORMALDISPLAY);
  ssd1306_WriteCommand_Blocking(0x2e);            // stop scroll
  ssd1306_WriteCommand_Blocking(DISPLAYON);
  __NOP(); // BREAKPOINT: comandos de init enviados

  // Set default values for screen object
  SSD1306.CurrentX = 0;
  SSD1306.CurrentY = 0;
  SSD1306.Color = Black;

  // Clear screen
  ssd1306_Clear();

#if SSD1306_CONTUPDATE == 1
  // Continuous Update on (ONLY when not using manager)
  ssd1306_ContUpdateEnable();
#endif

  // Flush buffer to screen (non-blocking in DMA mode)
  __NOP(); // BREAKPOINT: primer UpdateScreen del OLED
  ssd1306_UpdateScreen();

  SSD1306.Initialized = 1;

  /* Return OK */
  return 1;
}

/* -------------------------------------------------------------------------- */
/* DRAWING                                                                      */
/* -------------------------------------------------------------------------- */

void ssd1306_Fill(void)
{
  uint32_t i;
  for(i = 0; i < sizeof(SSD1306_Buffer); i++)
  {
    SSD1306_Buffer[i] = (SSD1306.Color == Black) ? 0x00 : 0xFF;
  }
}

void ssd1306_DrawPixel(uint8_t x, uint8_t y)
{
  SSD1306_COLOR color = SSD1306.Color;

  if (x >= ssd1306_GetWidth() || y >= ssd1306_GetHeight())
  {
    return;
  }

  if (SSD1306.Inverted)
  {
    color = (SSD1306_COLOR) !color;
  }

  if (color == White)
  {
    SSD1306_Buffer[x + (y / 8) * width()] |= 1 << (y % 8);
  }
  else
  {
    SSD1306_Buffer[x + (y / 8) * width()] &= ~(1 << (y % 8));
  }
}

/* --- Rest of drawing functions unchanged (verbatim from original) --- */

void ssd1306_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)
  {
    SWAP_INT16_T(x0, y0);
    SWAP_INT16_T(x1, y1);
  }

  if (x0 > x1)
  {
    SWAP_INT16_T(x0, x1);
    SWAP_INT16_T(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1)
  {
    ystep = 1;
  }
  else
  {
    ystep = -1;
  }

  for (; x0<=x1; x0++)
  {
    if (steep)
    {
      ssd1306_DrawPixel(y0, x0);
    }
    else
    {
      ssd1306_DrawPixel(x0, y0);
    }
    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}

void ssd1306_DrawHorizontalLine(int16_t x, int16_t y, int16_t length)
{
  if (y < 0 || y >= height()) { return; }

  if (x < 0)
  {
    length += x;
    x = 0;
  }

  if ( (x + length) > width())
  {
    length = (width() - x);
  }

  if (length <= 0) { return; }

  uint8_t * bufferPtr = SSD1306_Buffer;
  bufferPtr += (y >> 3) * width();
  bufferPtr += x;

  uint8_t drawBit = 1 << (y & 7);

  switch (SSD1306.Color)
  {
    case White:
      while (length--)
      {
        *bufferPtr++ |= drawBit;
      };
      break;
    case Black:
      drawBit = ~drawBit;
      while (length--)
      {
        *bufferPtr++ &= drawBit;
      };
      break;
    case Inverse:
      while (length--)
      {
        *bufferPtr++ ^= drawBit;
      }; break;
  }
}

void ssd1306_DrawVerticalLine(int16_t x, int16_t y, int16_t length)
{
  if (x < 0 || x >= width()) return;

  if (y < 0)
  {
    length += y;
    y = 0;
  }

  if ( (y + length) > height())
  {
    length = (height() - y);
  }

  if (length <= 0) return;

  uint8_t yOffset = y & 7;
  uint8_t drawBit;
  uint8_t *bufferPtr = SSD1306_Buffer;

  bufferPtr += (y >> 3) * width();
  bufferPtr += x;

  if (yOffset)
  {
    yOffset = 8 - yOffset;
    drawBit = ~(0xFF >> (yOffset));

    if (length < yOffset)
    {
      drawBit &= (0xFF >> (yOffset - length));
    }

    switch (SSD1306.Color)
    {
      case White:   *bufferPtr |=  drawBit; break;
      case Black:   *bufferPtr &= ~drawBit; break;
      case Inverse: *bufferPtr ^=  drawBit; break;
    }

    if (length < yOffset) return;

    length -= yOffset;
    bufferPtr += width();
  }

  if (length >= 8)
  {
    switch (SSD1306.Color)
    {
      case White:
      case Black:
        drawBit = (SSD1306.Color == White) ? 0xFF : 0x00;
        do {
          *bufferPtr = drawBit;
          bufferPtr += width();
          length -= 8;
        } while (length >= 8);
        break;
      case Inverse:
        do {
          *bufferPtr = ~(*bufferPtr);
          bufferPtr += width();
          length -= 8;
        } while (length >= 8);
        break;
    }
  }

  if (length > 0)
  {
    drawBit = (1 << (length & 7)) - 1;
    switch (SSD1306.Color)
    {
      case White:   *bufferPtr |=  drawBit; break;
      case Black:   *bufferPtr &= ~drawBit; break;
      case Inverse: *bufferPtr ^=  drawBit; break;
    }
  }
}

void ssd1306_DrawRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
  ssd1306_DrawHorizontalLine(x, y, width);
  ssd1306_DrawVerticalLine(x, y, height);
  ssd1306_DrawVerticalLine(x + width - 1, y, height);
  ssd1306_DrawHorizontalLine(x, y + height - 1, width);
}

void ssd1306_FillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height)
{
  for (int16_t x = xMove; x < xMove + width; x++)
  {
    ssd1306_DrawVerticalLine(x, yMove, height);
  }
}

void ssd1306_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
  ssd1306_DrawLine(x1, y1, x2, y2);
  ssd1306_DrawLine(x2, y2, x3, y3);
  ssd1306_DrawLine(x3, y3, x1, y1);
}

void ssd1306_DrawFillTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3)
{
  int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
  yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
  curpixel = 0;

  deltax = abs(x2 - x1);
  deltay = abs(y2 - y1);
  x = x1;
  y = y1;

  if (x2 >= x1)
  {
    xinc1 = 1;
    xinc2 = 1;
  }
  else
  {
    xinc1 = -1;
    xinc2 = -1;
  }

  if (y2 >= y1)
  {
    yinc1 = 1;
    yinc2 = 1;
  }
  else
  {
    yinc1 = -1;
    yinc2 = -1;
  }

  if (deltax >= deltay)
  {
    xinc1 = 0;
    yinc2 = 0;
    den = deltax;
    num = deltax / 2;
    numadd = deltay;
    numpixels = deltax;
  }
  else
  {
    xinc2 = 0;
    yinc1 = 0;
    den = deltay;
    num = deltay / 2;
    numadd = deltax;
    numpixels = deltay;
  }

  for (curpixel = 0; curpixel <= numpixels; curpixel++)
  {
    ssd1306_DrawLine(x, y, x3, y3);

    num += numadd;
    if (num >= den)
    {
      num -= den;
      x += xinc1;
      y += yinc1;
    }
    x += xinc2;
    y += yinc2;
  }
}

void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size)
{
  uint16_t i;
  if(par_vertex != 0)
  {
    for(i = 1; i < par_size; i++)
    {
      ssd1306_DrawLine(par_vertex[i - 1].x, par_vertex[i - 1].y, par_vertex[i].x, par_vertex[i].y);
    }
  }
  else
  {
    /*nothing to do*/
  }
  return;
}

static float ssd1306_DegToRad(float par_deg)
{
  return par_deg * 3.14f / 180.0f;
}

static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg)
{
  uint16_t loc_angle;
  if(par_deg <= 360)
  {
    loc_angle = par_deg;
  }
  else
  {
    loc_angle = par_deg % 360;
    loc_angle = ((par_deg != 0) ? par_deg : 360);
  }
  return loc_angle;
}

void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep)
{
  #define CIRCLE_APPROXIMATION_SEGMENTS 36
  float approx_degree;
  uint32_t approx_segments;
  uint8_t xp1, xp2;
  uint8_t yp1, yp2;
  uint32_t count = 0;
  uint32_t loc_sweep = 0;
  float rad;

  loc_sweep = ssd1306_NormalizeTo0_360(sweep);

  count = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
  approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
  approx_degree = loc_sweep / (float)approx_segments;
  while(count < approx_segments)
  {
    rad = ssd1306_DegToRad(count * approx_degree);
    xp1 = x + (int8_t)(sin(rad) * radius);
    yp1 = y + (int8_t)(cos(rad) * radius);
    count++;
    if(count != approx_segments)
    {
      rad = ssd1306_DegToRad(count * approx_degree);
    }
    else
    {
      rad = ssd1306_DegToRad(loc_sweep);
    }
    xp2 = x + (int8_t)(sin(rad) * radius);
    yp2 = y + (int8_t)(cos(rad) * radius);
    ssd1306_DrawLine(xp1, yp1, xp2, yp2);
  }

  return;
}

void ssd1306_DrawCircle(int16_t x0, int16_t y0, int16_t radius)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  do
  {
    if (dp < 0)
      dp = dp + 2 * (++x) + 3;
    else
      dp = dp + 2 * (++x) - 2 * (--y) + 5;

    ssd1306_DrawPixel(x0 + x, y0 + y);
    ssd1306_DrawPixel(x0 - x, y0 + y);
    ssd1306_DrawPixel(x0 + x, y0 - y);
    ssd1306_DrawPixel(x0 - x, y0 - y);
    ssd1306_DrawPixel(x0 + y, y0 + x);
    ssd1306_DrawPixel(x0 - y, y0 + x);
    ssd1306_DrawPixel(x0 + y, y0 - x);
    ssd1306_DrawPixel(x0 - y, y0 - x);

  } while (x < y);

  ssd1306_DrawPixel(x0 + radius, y0);
  ssd1306_DrawPixel(x0, y0 + radius);
  ssd1306_DrawPixel(x0 - radius, y0);
  ssd1306_DrawPixel(x0, y0 - radius);
}

void ssd1306_FillCircle(int16_t x0, int16_t y0, int16_t radius)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  do
  {
    if (dp < 0)
    {
      dp = dp + 2 * (++x) + 3;
    }
    else
    {
      dp = dp + 2 * (++x) - 2 * (--y) + 5;
    }

    ssd1306_DrawHorizontalLine(x0 - x, y0 - y, 2*x);
    ssd1306_DrawHorizontalLine(x0 - x, y0 + y, 2*x);
    ssd1306_DrawHorizontalLine(x0 - y, y0 - x, 2*y);
    ssd1306_DrawHorizontalLine(x0 - y, y0 + x, 2*y);

  } while (x < y);
  ssd1306_DrawHorizontalLine(x0 - radius, y0, 2 * radius);
}

void ssd1306_DrawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  while (x < y)
  {
    if (dp < 0)
      dp = dp + 2 * (++x) + 3;
    else
      dp = dp + 2 * (++x) - 2 * (--y) + 5;
    if (quads & 0x1)
    {
      ssd1306_DrawPixel(x0 + x, y0 - y);
      ssd1306_DrawPixel(x0 + y, y0 - x);
    }
    if (quads & 0x2)
    {
      ssd1306_DrawPixel(x0 - y, y0 - x);
      ssd1306_DrawPixel(x0 - x, y0 - y);
    }
    if (quads & 0x4)
    {
      ssd1306_DrawPixel(x0 - y, y0 + x);
      ssd1306_DrawPixel(x0 - x, y0 + y);
    }
    if (quads & 0x8)
    {
      ssd1306_DrawPixel(x0 + x, y0 + y);
      ssd1306_DrawPixel(x0 + y, y0 + x);
    }
  }
  if (quads & 0x1 && quads & 0x8)
  {
    ssd1306_DrawPixel(x0 + radius, y0);
  }
  if (quads & 0x4 && quads & 0x8)
  {
    ssd1306_DrawPixel(x0, y0 + radius);
  }
  if (quads & 0x2 && quads & 0x4)
  {
    ssd1306_DrawPixel(x0 - radius, y0);
  }
  if (quads & 0x1 && quads & 0x2)
  {
    ssd1306_DrawPixel(x0, y0 - radius);
  }
}

void ssd1306_DrawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress)
{
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = 2 * radius;
  uint16_t innerRadius = radius - 2;

  ssd1306_SetColor(White);
  ssd1306_DrawCircleQuads(xRadius, yRadius, radius, 0x06 /*0b00000110*/);
  ssd1306_DrawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  ssd1306_DrawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  ssd1306_DrawCircleQuads(x + width - radius, yRadius, radius, 0x09 /*0b00001001*/);

  uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 100;

  ssd1306_FillCircle(xRadius, yRadius, innerRadius);
  ssd1306_FillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
  ssd1306_FillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
}

void ssd1306_DrawBitmap(uint8_t X, uint8_t Y, uint8_t W, uint8_t H, const uint8_t* pBMP)
{
  uint8_t pX;
  uint8_t pY;
  uint8_t tmpCh;
  uint8_t bL;

  pY = Y;
  while (pY < Y + H)
  {
    pX = X;
    while (pX < X + W)
    {
      bL = 0;
      tmpCh = *pBMP++;
      if (tmpCh)
      {
        while (bL < 8)
        {
          if (tmpCh & 0x01) ssd1306_DrawPixel(pX,pY + bL);
          tmpCh >>= 1;
          if (tmpCh)
          {
            bL++;
          }
          else
          {
            pX++;
            break;
          }
        }
      }
      else
      {
        pX++;
      }
    }
    pY += 8;
  }
}

char ssd1306_WriteChar(char ch, FontDef Font)
{
  uint32_t i, b, j;

  if (width() <= (SSD1306.CurrentX + Font.FontWidth) ||
      height() <= (SSD1306.CurrentY + Font.FontHeight))
  {
    return 0;
  }

  for (i = 0; i < Font.FontHeight; i++)
  {
    b = Font.data[(ch - 32) * Font.FontHeight + i];
    for (j = 0; j < Font.FontWidth; j++)
    {
      if ((b << j) & 0x8000)
      {
        ssd1306_DrawPixel(SSD1306.CurrentX + j, SSD1306.CurrentY + i);
      }
      else
      {
        SSD1306.Color = !SSD1306.Color;
        ssd1306_DrawPixel(SSD1306.CurrentX + j, SSD1306.CurrentY + i);
        SSD1306.Color = !SSD1306.Color;
      }
    }
  }

  SSD1306.CurrentX += Font.FontWidth;
  return ch;
}

char ssd1306_WriteString(char* str, FontDef Font)
{
  while (*str)
  {
    if (ssd1306_WriteChar(*str, Font) != *str)
    {
      return *str;
    }
    str++;
  }
  return *str;
}

void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
  SSD1306.CurrentX = x;
  SSD1306.CurrentY = y;
}

void ssd1306_Clear(void)
{
  memset(SSD1306_Buffer, 0, SSD1306_BUFFER_SIZE);
}

/* -------------------------------------------------------------------------- */
/* TRANSFER IMPLEMENTATIONS                                                    */
/* -------------------------------------------------------------------------- */

#if SSD1306_USE_DMA == 0

void ssd1306_WriteCommand(uint8_t command)
{
  HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &command, 1, 10);
}

void ssd1306_WriteData(uint8_t* data, uint16_t size)
{
  HAL_I2C_Mem_Write(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x40, 1, data, size, 100);
}

void ssd1306_UpdateScreen(void)
{
  uint8_t i;
  for (i = 0; i < SSD1306_HEIGHT / 8; i++)
  {
    ssd1306_WriteCommand(0xB0 + i);
    ssd1306_WriteCommand(SETLOWCOLUMN);
    ssd1306_WriteCommand(SETHIGHCOLUMN);
    ssd1306_WriteData(&SSD1306_Buffer[SSD1306_WIDTH * i], width());
  }
}

#elif SSD1306_USE_DMA == 1

volatile uint8_t ssd1306_updatestatus = 0;
volatile uint8_t ssd1306_updateend;

#if SSD1306_CONTUPDATE == 0

void ssd1306_WriteCommand(uint8_t command)
{
#if SSD1306_USE_I2C_MANAGER == 1
  // For commands outside init, we just request an update.
  // (You can extend this later with a command queue if needed.)
  (void)command;
  ssd1306_UpdateScreen();
#else
  while(ssd1306_updatestatus);
  while(HAL_I2C_GetState(&SSD1306_I2C_PORT) != HAL_I2C_STATE_READY) { };
  HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &command, 1);
#endif
}

void ssd1306_UpdateScreen(void)
{
#if SSD1306_USE_I2C_MANAGER == 1
  __NOP(); // BREAKPOINT: solicitud de refresco OLED
  uint8_t max_pages = (uint8_t)(SSD1306_HEIGHT / 8);
  for (uint8_t page = 0; page < max_pages; page++) {
    uint8_t keep_bus = (page + 1u) < max_pages;
    (void)ssd1306_RequestPageUpdate(page, I2C_REQ_PRIO_REGULAR, keep_bus);
  }
#else
  uint8_t  command;
  if(ssd1306_updatestatus == 0)
  {
    ssd1306_updatestatus = SSD1306_HEIGHT;
    ssd1306_updateend = SSD1306_HEIGHT + (SSD1306_HEIGHT / 2);
    command = 0xB0;
    HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &command, 1);
  }
  else if(ssd1306_updatestatus >= SSD1306_HEIGHT)
  {
    ssd1306_updatestatus -= (SSD1306_HEIGHT / 2);
    ssd1306_updateend = (ssd1306_updatestatus + (SSD1306_HEIGHT / 2 + 1)) & 0xFC;
  }
#endif
}

char ssd1306_UpdateScreenCompleted(void)
{
#if SSD1306_USE_I2C_MANAGER == 1
  return (!OLED_QueueHasPending() && oled_inflight_page == 0xFFu) ? 1 : 0;
#else
  if(ssd1306_updatestatus)
    return 0;
  else
    return 1;
#endif
}

__weak void ssd1306_UpdateCompletedCallback(void) { };
__weak void ssd1306_PageSentCallback(uint8_t page) { (void)page; };
__weak void ssd1306_QueueEmptyCallback(void) { };

#elif SSD1306_CONTUPDATE == 1

/* When using I2C Manager, CONTUPDATE must be 0, so this block is not compiled. */

volatile uint8_t ssd1306_command = 0;
volatile uint8_t i2c_command = 0;
volatile uint8_t ssd1306_ContUpdate = 0;
volatile uint8_t ssd1306_RasterIntRegs = 0;

void ssd1306_WriteCommand(uint8_t command)
{
  if(ssd1306_updatestatus)
  {
    while(ssd1306_command);
    ssd1306_command = command;
  }
  else
  {
    while(HAL_I2C_GetState(&SSD1306_I2C_PORT) != HAL_I2C_STATE_READY) { };
    i2c_command = command;
    HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &i2c_command, 1);
  }
}

void ssd1306_ContUpdateEnable(void)
{
  if(!ssd1306_ContUpdate)
  {
    while(HAL_I2C_GetState(&SSD1306_I2C_PORT) != HAL_I2C_STATE_READY) { };
    ssd1306_updatestatus = SSD1306_HEIGHT;
    ssd1306_updateend = SSD1306_HEIGHT + (SSD1306_HEIGHT / 2);
    i2c_command = 0xB0;
    ssd1306_ContUpdate = 1;
    HAL_I2C_Mem_Write_DMA(&SSD1306_I2C_PORT, SSD1306_I2C_ADDR, 0x00, 1, &i2c_command, 1);
  }
}

void ssd1306_ContUpdateDisable(void)
{
  if(ssd1306_ContUpdate)
  {
    ssd1306_ContUpdate = 0;
    while(ssd1306_updatestatus) { };
  }
}

void ssd1306_SetRasterInt(uint8_t r)
{
  ssd1306_RasterIntRegs = r;
}

__weak void ssd1306_RasterIntCallback(uint8_t r)
{
}

#endif /* CONTUPDATE */

#endif /* USE_DMA */
