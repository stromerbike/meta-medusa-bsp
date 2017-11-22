#
# This file
#

FILESEXTRAPATHS_append := "${THISDIR}"

# Patch which modifies dts and some scripts
SRC_URI_append = " file://dts_script.patch"

do_configure_append() {
	# Enable barebox state
	kconfig_set STATE y
	kconfig_set STATE_DRV y
	kconfig_set CMD_STATE y
	kconfig_set STATE_CRYPTO n
	# Set configuration to enable backlight
	kconfig_set DRIVER_VIDEO_BACKLIGHT y
	kconfig_set DRIVER_VIDEO_BACKLIGHT_PWM y
}

do_install_append() {
	# Remove barebox from rootfs
	rm ${D}${base_bootdir}/${BAREBOX_IMAGE_BASE_NAME}.bin
	rm ${D}${base_bootdir}/${BAREBOX_BIN_SYMLINK}
}

python do_env_append_imx6ul-medusa() {
    env_add(d, "nv/autoboot_timeout", "0\n")
    env_add(d, "nv/vt.global_cursor_default", "0\n")

    env_rm(d, "expansions/imx6qdl-mira-enable-lvds")
    env_rm(d, "expansions/imx6qdl-mira-peb-eval-01")
    env_rm(d, "expansions/imx6qdl-phytec-lcd")
    env_rm(d, "expansions/imx6qdl-phytec-lcd-018-peb-av-02")
    env_rm(d, "expansions/imx6qdl-phytec-lcd-018-peb-av-02-res")
    env_rm(d, "expansions/imx6qdl-phytec-peb-wlbt-01")
    env_rm(d, "boot/nand")

    env_add(d, "boot/nand",
"""#!/bin/sh

[ -e /env/config-expansions ] && /env/config-expansions

[ ! -e /dev/nand0.root.ubi ] && ubiattach /dev/nand0.root

# Perform automatic update
if [ $state.update -eq 1 ] || [ $state.barebox -eq 1 ]; then
./env/update
fi

# Detect NAND and mount partition
detect -a
mkdir mnt/rootfs

# Remove UART8 from DTB
if [ $state.uart8 -eq 0 ]; then
of_fixup_status -d /soc/aips-bus@02000000/spba-bus@02000000/serial@02024000/
fi

if [ $state.partition -eq 0 ]; then
mount /dev/nand0.root.ubi.part0 mnt/rootfs
global.linux.bootargs.dyn.root="root=ubi0:part0 ubi.mtd=root rootfstype=ubifs rw vt.global_cursor_default=0 fsck.make=skip quiet"
else
mount /dev/nand0.root.ubi.part1 mnt/rootfs
global.linux.bootargs.dyn.root="root=ubi0:part1 ubi.mtd=root rootfstype=ubifs rw vt.global_cursor_default=0 fsck.make=skip quiet"
fi

global.bootm.image="/mnt/rootfs/boot/zImage"
global.bootm.oftree="/mnt/rootfs/boot/imx6ul-medusa.dtb"
""")

    env_add(d, "config-expansion",
"""#!/bin/sh
#
# LCD initialization
#
# Apply clean reset sequence
#gpio_direction_output 68 1
#msleep 1
#gpio_direction_output 68 0
#msleep 10
#gpio_direction_output 68 1
#msleep 120

# Turn off sleep mode
#spi -w 9 0x11 0x00
#msleep 120

# Memory data access contrl
#spi -w 9 0x36 0x00 0x00 0x01

# Porch settings
#spi -w 9 0xb2 0x00 0x0c 0x01 0x0c 0x01 0x00 0x01 0x33 0x01 0x33 0x01

# Gate control
# VGH = 13.26V
# VGL = -10.43V
#spi -w 9 0xb7 0x00 0x35 0x01

# VCOM = 1.175V
#spi -w 9 0xbb 0x00 0x2b 0x01

# LCMCTRL
#spi -w 9 0xc0 0x00 0x2c 0x01

# VDV and VRH register values come from command write (instead of NVM)
#spi -w 9 0xc2 0x00 0x01 0x01

# VAP =  4.1V + (VCOM + VCOM offset + 0.5 * VDV)
# VAN = -4.1V + (VCOM + VCOM offset + 0.5 * VDV)
#spi -w 9 0xc3 0x00 0x20 0x01

# VDV = 0V
#spi -w 9 0xc4 0x00 0x20 0x01

# Frame rate control
#spi -w 9 0xc6 0x00 0x0f 0x01

# AVDD = 6.8V
# AVCL = -4.8V
# VDS = 2.3V
#spi -w 9 0xd0 0x00 0xa4 0x01 0xa1 0x01

# Positive voltage gamma control
#spi -w 9 0xe0 0x00 0xd0 0x01 0xca 0x01 0xe0 0x01 0x08 0x01 0x09 0x01 0x07 0x01 0x2d 0x01 0x3b 0x01 0x3d 0x01 0x34 0x01 0x0a 0x01 0x0a 0x01 0x1b 0x01 0x28 0x01

# Negative voltage gamma control
#spi -w 9 0xe1 0x00 0xd0 0x01 0xca 0x01 0x0f 0x01 0x08 0x01 0x08 0x01 0x07 0x01 0x2e 0x01 0x5c 0x01 0X40 0x01 0x34 0x01 0x09 0x01 0x0b 0x01 0x1b 0x01 0x28 0x01

# RAM control
#spi -w 9 0xb0 0x00 0x11 0x01 0x00 0x01 0x00 0x01

# RGB Interface control
#spi -w 9 0xb1 0x00 0x40 0x01 0x04 0x01 0x0a 0x01

# Set pixel format to RGB-565
#spi -w 9 0x3a 0x00 0x55 0x01

# Frame rate control
#spi -w 9 0x2a 0x00 0x00 0x01 0x00 0x01 0x00 0x01 0xef 0x01

# Display function control
#spi -w 9 0x2b 0x00 0x00 0x01 0x00 0x01 0x01 0x01 0x3f 0x01

# Display on
#spi -w 9 0x29 0x00

# Memory write control
#spi -w 9 0x2c 0x00
""")

    env_add(d, "update",
"""#!/bin/sh
echo ========= Update process =============
i2c_write -b 1 -a 0x20 -r 6 -v 0xfe
sleep 0.3
i2c_write -b 1 -a 0x20 -r 2 -v 0xff
sleep 0.5
i2c_write -b 1 -a 0x20 -r 7 -v 0xfb
sleep 0.3
i2c_write -b 1 -a 0x20 -r 3 -v 0xff
sleep 1
otg.mode=host

# Wait is skipped in automatic update
if [ $state.update -eq 0 ] && [ $state.barebox -eq 0 ]; then
readline "Stick USB and press any key to continue..." var
fi

usb
sleep 2
mount /dev/disk0.0

echo ============ Barebox =================
barebox_update -y -t nand /mnt/disk0.0/barebox.bin

# Perform only Barebox update and boot
if [ $state.barebox -eq 1 ]; then
state.barebox=0
state -s
reset
fi

# Format and partition are skipped in automatic update
if [ $state.update -eq 0 ]; then
echo ============== Format ================
ubiformat /dev/nand0.root
sleep 2

echo ============ Partitions ==============
detect -a
sleep 2
ubimkvol -t dynamic /dev/nand0.root.ubi part0 192M
ubimkvol -t dynamic /dev/nand0.root.ubi part1 192M
ubimkvol -t dynamic /dev/nand0.root.ubi data0 48M
ubimkvol -t dynamic /dev/nand0.root.ubi data1 48M
fi

echo ============== Linux =================
sleep 2
detect -a
echo "Part0 (copy)..."
cp -v /mnt/disk0.0/medusa-image-imx6ul-medusa-escatec.ubifs /dev/nand0.root.ubi.part0
echo "Part1 (copy)..."
cp -v /mnt/disk0.0/medusa-image-imx6ul-medusa-stromer.ubifs /dev/nand0.root.ubi.part1

echo ======= Update end and boot ==========
state.partition=0
state.update=0
state -s

if [ $state.update -eq 1 ]; then
reset
else
boot nand
fi
""")
}

COMPATIBLE_MACHINE .= "|imx6ul-medusa"
