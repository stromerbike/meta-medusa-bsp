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

SRC_URI += " file://config_modification.cfg"
SRC_URI += " file://imx6ul-medusa.dts"
SRC_URI += " file://lsm6dsl.patch"
SRC_URI += " file://clk_fix.patch"
SRC_URI += " file://ili2116_st7789v.patch"

COMPATIBLE_MACHINE .= "|imx6ul-medusa"
