#
# Copyright (C) 2019 Escatec Switzerland AG
#

FILESEXTRAPATHS_append := "${THISDIR}"

# Patch which modifies dts and some scripts
SRC_URI_append = " \
    file://backend-stridesize-legacy.patch \
    file://cp-verbose.patch \
    file://digest-verbose.patch \
    file://dts_script.patch \
    file://global.boot.default_nand.patch \
    file://powerlock.patch \
    file://process_escape_sequence_use_hostname.patch \
    file://set_hostname_imx6ul-imx6ull-medusa.patch \
"

BAREBOX_IMAGE_BASE_NAME = "barebox-${PV}-#${DISTRO_VERSION}-imx6ul\(l\)-${DISTRO}"

do_configure_append() {
    PHY_TAG=`echo ${PV} | sed "s/.*-//"`
    kconfig_set LOCALVERSION \"-${PHY_TAG}\"

    # Enable colored console output during boot
    kconfig_set CONSOLE_ALLOW_COLOR y

    # Enable IO expander
    kconfig_set GPIO_PCA953X y

    # Enable barebox state
    kconfig_set STATE y
    kconfig_set STATE_DRV y
    kconfig_set STATE_BACKWARD_COMPATIBLE y
    kconfig_set CMD_STATE y
    kconfig_set STATE_CRYPTO n

    # Enable backlight driver
    kconfig_set DRIVER_VIDEO_BACKLIGHT y
    kconfig_set DRIVER_VIDEO_BACKLIGHT_PWM y

    # Set malloc size to be large enough to fit two images with the size of a system partition (2x192x1024x1024)
    kconfig_set MALLOC_SIZE 0x18000000

    # Enable HMAC SH256 digest
    kconfig_set DIGEST_HMAC_GENERIC y
    kconfig_set DIGEST_SHA256_GENERIC y
    kconfig_set DIGEST_SHA256_ARM y

    # Enable 2048 for fun
    kconfig_set CMD_2048 y
}

do_compile_prepend() {
    export KBUILD_BUILD_VERSION="${DISTRO_VERSION}"
    # DISTRO_VERSION already reflects the timestamp and can be used otherwise
    export KBUILD_BUILD_TIMESTAMP="imx6ul(l)-${DISTRO}"
}

do_install_append() {
    # Remove barebox from rootfs
    rm ${D}${base_bootdir}/${BAREBOX_IMAGE_BASE_NAME}.bin
    rm ${D}${base_bootdir}/${BAREBOX_BIN_SYMLINK}
}

python do_env_append() {
    env_add(d, "nv/allow_color", "true\n")
    env_add(d, "nv/autoboot_timeout", "0\n")
    env_add(d, "nv/vt.global_cursor_default", "0\n")

    env_rm(d, "boot/emmc")
    env_rm(d, "boot/mmc")
    env_rm(d, "boot/net")
    env_rm(d, "boot/system0")
    env_rm(d, "boot/system1")
    env_rm(d, "boot/spi")

    env_rm(d, "nv/bootchooser.targets")
    env_rm(d, "nv/bootchooser.system0.boot")
    env_rm(d, "nv/bootchooser.system1.boot")
    env_rm(d, "nv/bootchooser.state_prefix")

    # Left LED:
    # - Off:     Kernel image present
    # - White:   No Kernel image present
    #
    # Right LED:
    # - Off:     Correct oftree present
    # - White:   No oftree present
    # - Magenta: Correct oftree not present
    env_rm(d, "boot/nand")
    env_add(d, "boot/nand",
"""#!/bin/sh

[ -e /env/config-expansions ] && /env/config-expansions

[ ! -e /dev/nand0.root.ubi ] && ubiattach /dev/nand0.root

# Invoke update if main button is pressed (held after power-on)
gpio_direction_input 130
gpio_get_value 130
if [ $? -eq 1 ]; then
    echo ON_SWITCH: pressed
    ./env/update
fi

# Mount active system partition
mkdir /mnt/rootfs
if [ $state.partition -eq 0 ]; then
    mount /dev/nand0.root.ubi.part0 /mnt/rootfs
    global.linux.bootargs.dyn.root="root=ubi0:part0 ubi.mtd=root rootfstype=ubifs rw vt.global_cursor_default=0 fsck.make=skip quiet"
else
    mount /dev/nand0.root.ubi.part1 /mnt/rootfs
    global.linux.bootargs.dyn.root="root=ubi0:part1 ubi.mtd=root rootfstype=ubifs rw vt.global_cursor_default=0 fsck.make=skip quiet"
fi

# Fall back to imx6ul-medusa.dtb because booting with potential errors is better than being stuck in bootloader
if [ -e /mnt/rootfs/boot/zImage ]; then
    global.bootm.image="/mnt/rootfs/boot/zImage"
    if [ -e "/mnt/rootfs/boot/${global.hostname}.dtb" ]; then
        global.bootm.oftree="/mnt/rootfs/boot/${global.hostname}.dtb"
    else
        # RGB_ON activate and init LED chip
        gpio_direction_output 164 1
        i2c_write -b 1 -a 0x35 -r 0x36 0x53
        i2c_write -b 1 -a 0x35 -r 0x00 0x40
        if [ ! -e /mnt/rootfs/boot/imx6ul-medusa.dtb ]; then
            # Activate right LED as white
            i2c_write -b 1 -a 0x35 -r 0x19 0xff
            i2c_write -b 1 -a 0x35 -r 0x1a 0xff
            i2c_write -b 1 -a 0x35 -r 0x1b 0xff
        else
            # Activate right LED as magenta
            i2c_write -b 1 -a 0x35 -r 0x19 0xff
            i2c_write -b 1 -a 0x35 -r 0x1b 0xff
            global.bootm.oftree="/mnt/rootfs/boot/imx6ul-medusa.dtb"
        fi
    fi
else
    # RGB_ON activate, init LED chip and activate left LEDs as white
    gpio_direction_output 164 1
    i2c_write -b 1 -a 0x35 -r 0x36 0x53
    i2c_write -b 1 -a 0x35 -r 0x00 0x40
    i2c_write -b 1 -a 0x35 -r 0x16 0xff
    i2c_write -b 1 -a 0x35 -r 0x17 0xff
    i2c_write -b 1 -a 0x35 -r 0x18 0xff
    if [ ! -e /mnt/rootfs/boot/*-medusa.dtb ]; then
        # Activate right LED as white
        i2c_write -b 1 -a 0x35 -r 0x19 0xff
        i2c_write -b 1 -a 0x35 -r 0x1a 0xff
        i2c_write -b 1 -a 0x35 -r 0x1b 0xff
    fi
    echo DISCONNECT USB FROM PC NOW and connect USB drive
    echo ON_SWITCH: waiting to be pressed for invoking ./env/update
    echo CTRL+C to abort
    while true; do
        echo -n .
        gpio_get_value 130
        if [ $? -eq 1 ]; then
            echo
            echo ON_SWITCH: pressed
            ./env/update
            echo ON_SWITCH: waiting to be pressed for invoking ./env/update again
            echo CTRL+C to abort
        fi
        msleep 100
    done
fi
""")

    env_add(d, "musb",
"""#!/bin/sh
gpio_direction_output 23 1
gpio_direction_output 160 1
gpio_direction_output 170 1
otg.mode=host
detect -a
mount /dev/disk0.0
""")
}

COMPATIBLE_MACHINE .= "|imx6ul-medusa"
COMPATIBLE_MACHINE .= "|imx6ull-medusa"
