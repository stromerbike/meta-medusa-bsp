#
# This file
#

#FILESEXTRAPATHS_append := "${THISDIR}:${THISDIR}/linux-mainline:"
FILESEXTRAPATHS_append := "${THISDIR}/linux-mainline:"

# Due to append, nothing needed
# SUMMARY = "Custom kernel modifications"
# SECTION = "kernel"
# LICENSE = "MIT"
# LIC_FILES_CHKSUM = "file://LICENSE;md5=96af5705d6f64a88e035781ef00e98a8"

KERNEL_DEVICETREE_append = " imx6ull-medusa.dtb"

SRC_URI += " file://defconfig"
SRC_URI += " file://imx6ul-medusa.dts"
SRC_URI += " file://imx6ull-medusa.dts"
SRC_URI += " file://clk_fix.patch"
SRC_URI += " file://st1633i_st7789v.patch"
SRC_URI += " file://bootlogo.patch"
SRC_URI += " file://ext_gpio.patch"
SRC_URI += " file://gsm_uart.patch"
SRC_URI += " file://spi-imx.patch"
SRC_URI += " file://0001-Revert-UPSTREAM-ARM-dts-imx6ul-Remove-leading-zeroes.patch"

# Disable autoboot for specified modules
# led driver
KERNEL_MODULE_PROBECONF += " leds-lp5523"
module_conf_leds-lp5523 = "blacklist leds-lp5523"
KERNEL_MODULE_PROBECONF += " leds-lp55xx-common"
module_conf_leds-lp55xx-common = "blacklist leds-lp55xx-common"

# iio drivers utilized by adc, pressure and accelerometer
KERNEL_MODULE_PROBECONF += " kfifo_buf"
module_conf_kfifo_buf = "blacklist kfifo_buf"
KERNEL_MODULE_PROBECONF += " industrialio"
module_conf_industrialio = "blacklist industrialio"
KERNEL_MODULE_PROBECONF += " industrialio-configfs"
module_conf_industrialio-configfs = "blacklist industrialio-configfs"
KERNEL_MODULE_PROBECONF += " industrialio-buffer-cb"
module_conf_industrialio-buffer-cb = "blacklist industrialio-configfs"
KERNEL_MODULE_PROBECONF += " industrialio-triggered-buffer"
module_conf_industrialio-triggered-buffer = "blacklist industrialio-triggered-buffer"
KERNEL_MODULE_PROBECONF += " industrialio-sw-trigger"
module_conf_industrialio-sw-trigger = "blacklist industrialio-sw-trigger"
KERNEL_MODULE_PROBECONF += " industrialio-sw-device"
module_conf_industrialio-sw-device = "blacklist industrialio-sw-device"

# adc driver
KERNEL_MODULE_PROBECONF += " vf610_adc"
module_conf_vf610_adc = "blacklist vf610_adc"

# accelerometer driver
KERNEL_MODULE_PROBECONF += " st_lsm6dsx"
module_conf_st_lsm6dsx = "blacklist st_lsm6dsx"
KERNEL_MODULE_PROBECONF += " st_lsm6dsx_i2c"
module_conf_st_lsm6dsx_i2c = "blacklist st_lsm6dsx_i2c"
KERNEL_MODULE_PROBECONF += " st_lsm6dsx_spi"
module_conf_st_lsm6dsx_spi = "blacklist st_lsm6dsx_spi"

# pressure driver
KERNEL_MODULE_PROBECONF += " bmp280"
module_conf_bmp280 = "blacklist bmp280"
KERNEL_MODULE_PROBECONF += " bmp280-i2c"
module_conf_bmp280-i2c = "blacklist bmp280-i2c"
KERNEL_MODULE_PROBECONF += " bmp280-spi"
module_conf_bmp280-spi = "blacklist bmp280-spi"

# external gpio expander driver
KERNEL_MODULE_PROBECONF += " gpio-pca953x-external"
module_conf_gpio-pca953x-external = "blacklist gpio-pca953x-external"

# gsm uart driver
KERNEL_MODULE_PROBECONF += " imx6ul_mod_uart"
module_conf_imx6ul_mod_uart = "blacklist imx6ul_mod_uart"

COMPATIBLE_MACHINE .= "|imx6ul-medusa"
COMPATIBLE_MACHINE .= "|imx6ull-medusa"
