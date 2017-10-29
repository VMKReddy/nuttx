/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdbool.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/lcd/pcf8574_lcd_backpack.h>
#include <nuttx/i2c/i2c_master.h>


#include "up_arch.h"
#include "up_internal.h"

#include "stm32_gpio.h"
#include "stm32f103_minimum.h"

#ifdef CONFIG_SLCD

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LCD_I2C_PORT 2


/****************************************************************************
 * Private Data
 ****************************************************************************/
FAR struct i2c_master_s* g_i2c_lcd = NULL;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_slcd_initialize
 ****************************************************************************/

int stm32_slcd_initialize(FAR const char *devpath)
{
  int ret = 0;
  struct pcf8574_lcd_backpack_config_s cfg = LCD_I2C_BACKPACK_CFG_SAINSMART;
  cfg.addr = 0x20;
  cfg.rows = 4;
  cfg.cols = 20;

  g_i2c_lcd = stm32_i2cbus_initialize(LCD_I2C_PORT);
  if (!g_i2c_lcd) {
    lcderr("ERROR: Failed to initialize I2C port %d\n", LCD_I2C_PORT);
    return -ENODEV;
  }

  // register slcd device
  ret = pcf8574_lcd_backpack_register(devpath, g_i2c_lcd, &cfg);
  if (ret < 0) {
    stm32_i2cbus_uninitialize(g_i2c_lcd);
  }

  return ret;
}

#endif /* CONFIG_NX_LCDDRIVER */
