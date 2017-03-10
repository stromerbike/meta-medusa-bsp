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

SRC_URI += " file://imx6ul-omni.dts"
SRC_URI += " file://lsm6dsl.patch"
SRC_URI += " file://omni_driver.patch"
SRC_URI += " file://remove_unneeded.cfg"

COMPATIBLE_MACHINE .= "|imx6ul-omni"
