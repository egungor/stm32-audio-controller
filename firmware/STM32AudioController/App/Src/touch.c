/**
 * @file    touch.c
 * @brief   FT5336 capacitive touch controller on I2C3 (polling).
 */
#include "touch.h"

#include "i2c.h"
#include "ltdc.h"

#define FT5336_I2C_ADDR      0x70U /* 8-bit form, as HAL expects (0x38 7-bit) */
#define FT5336_REG_TD_STATUS 0x02U
#define FT5336_REG_G_MODE    0xA4U
#define FT5336_REG_CHIP_ID   0xA8U
#define FT5336_G_MODE_POLL   0x00U
#define FT5336_MAX_TOUCHES   5U
#define FT5336_POWERUP_MS    200U  /* first access after power-up, per ST driver */
#define I2C_TIMEOUT_MS       10U

void Touch_Decode(const uint8_t regs[TOUCH_REG_BLOCK_LEN], uint16_t width,
                  uint16_t height, TouchPoint *point)
{
    const uint8_t touches = regs[0] & 0x0FU;
    uint16_t raw_x;
    uint16_t raw_y;

    point->pressed = false;
    point->x = 0U;
    point->y = 0U;
    if ((touches == 0U) || (touches > FT5336_MAX_TOUCHES) || (width == 0U) || (height == 0U))
    {
        return;
    }

    raw_x = (uint16_t)(((uint16_t)(regs[1] & 0x0FU) << 8) | regs[2]);
    raw_y = (uint16_t)(((uint16_t)(regs[3] & 0x0FU) << 8) | regs[4]);

    /* Board orientation (BSP TS_SWAP_XY): LCD x = raw y, LCD y = raw x. */
    point->x = (raw_y < width) ? raw_y : (uint16_t)(width - 1U);
    point->y = (raw_x < height) ? raw_x : (uint16_t)(height - 1U);
    point->pressed = true;
}

static bool read_regs(uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c3, FT5336_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, data, len,
                            I2C_TIMEOUT_MS) == HAL_OK;
}

bool Touch_Init(uint8_t *chip_id)
{
    uint8_t mode = FT5336_G_MODE_POLL;

    *chip_id = 0U;
    if (HAL_GetTick() < FT5336_POWERUP_MS)
    {
        HAL_Delay(FT5336_POWERUP_MS - HAL_GetTick());
    }
    if (!read_regs(FT5336_REG_CHIP_ID, chip_id, 1U) || (*chip_id != TOUCH_FT5336_ID))
    {
        return false;
    }
    return HAL_I2C_Mem_Write(&hi2c3, FT5336_I2C_ADDR, FT5336_REG_G_MODE, I2C_MEMADD_SIZE_8BIT,
                             &mode, 1U, I2C_TIMEOUT_MS) == HAL_OK;
}

bool Touch_Read(TouchPoint *point)
{
    uint8_t regs[TOUCH_REG_BLOCK_LEN];
    const LTDC_LayerCfgTypeDef *layer = &hltdc.LayerCfg[0];

    if (!read_regs(FT5336_REG_TD_STATUS, regs, TOUCH_REG_BLOCK_LEN))
    {
        point->pressed = false;
        return false;
    }
    Touch_Decode(regs, (uint16_t)layer->ImageWidth, (uint16_t)layer->ImageHeight, point);
    return true;
}
