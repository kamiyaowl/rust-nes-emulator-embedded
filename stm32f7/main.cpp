#include "mbed.h"

// for peripheral
#include "stm32f769i_discovery.h"
#include "stm32f769i_discovery_lcd.h"
#include "stm32f769i_discovery_sdram.h"
#include "stm32f769i_discovery_ts.h"
#include "stm32f769i_discovery_sd.h"
#include "stm32f769i_discovery_audio.h"

// for system
#include "stm32f7xx_hal_flash.h"
#include "core_cm7.h"

// for emulator
#include "rust_nes_emulator.h"

// EMU_WORK on DTCM 64K
#define EMU_WORK_SIZE (64*1024) // 64K
MBED_ALIGN(32) static uint8_t emuWorkBuffer[EMU_WORK_SIZE]; // MBED_SECTION(".emu_work");

// user specified values
#define PRINT_MESSAGE_HEIGHT (20)
#define SDRAM_BASE_ADDR      (SDRAM_DEVICE_ADDR)
#define SDRAM_SIZE           (SDRAM_DEVICE_SIZE)
#define DISPLAY_WIDTH        (OTM8009A_800X480_WIDTH)  // 800
#define DISPLAY_HEIGHT       (OTM8009A_800X480_HEIGHT) // 480

void initSystem(void) {
    // Enabled I/D Cache, Prefetch Buffer
    __HAL_FLASH_ART_ENABLE();
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    SCB_EnableDCache();
    SCB_EnableICache();
    __DMB();
}

void initLcd(void) {
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, SDRAM_BASE_ADDR);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 40);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetFont(&Font24);
}

bool initSdram(void) {
    return (BSP_SDRAM_Init() == SDRAM_OK);
}

bool initTouchscreen(void) {
    return BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) == TS_OK;
}

bool initSd(void) {
    return (BSP_SD_Init() == MSD_OK);
}
int main(void) {
    char msg[64];
    uint32_t messageLine = 0;

    // Init core peripheral
    initSystem();
    initLcd();

    // Print startup info
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"kamiyaowl/rust-nes-emulator-embedded", LEFT_MODE);
    sprintf(msg, "[DEBUG] SystemCoreClock: %ld MHz", SystemCoreClock / 1000000ul);
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)msg, LEFT_MODE);
    sprintf(msg, "[DEBUG] emuWorkbufferAddr: %08x", emuWorkBuffer);
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)msg, LEFT_MODE);

    // Init other peripherals
    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init SDRAM", LEFT_MODE);
    if(!initSdram()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }

    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init Touchscreen", LEFT_MODE);
    if (!initTouchscreen()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }   

    BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[INFO ] Init SD", LEFT_MODE);
    if (!initSd()) {
        BSP_LCD_DisplayStringAt(0, (messageLine++ * PRINT_MESSAGE_HEIGHT), (uint8_t *)"[ERROR] FAILED", LEFT_MODE);
        while(1);
    }   

    // Init emulator
    while(1);
    /* Emulator Test */
    // EmbeddedEmulator_init();
    // if (EmbeddedEmulator_load()) {
    //     BSP_LCD_DisplayStringAt(20, 140, (uint8_t *)"Emulator ROM Load  : OK", LEFT_MODE);
    // } else {
    //     BSP_LCD_DisplayStringAt(20, 140, (uint8_t *)"Emulator ROM Load  : FAILED", LEFT_MODE);
    // }


    // wait_ms(3000);
    // BSP_LCD_Clear(LCD_COLOR_BLACK);

    // for (uint32_t counter = 0 ; ; ++counter ) {
    //     EmbeddedEmulator_update_screen(&fb);

    //     sprintf(msg, "%d", counter);
    //     BSP_LCD_DisplayStringAt(5, 5, (uint8_t *)msg, LEFT_MODE);
    // }
}