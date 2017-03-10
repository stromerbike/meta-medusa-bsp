#
# This file
#

FILESEXTRAPATHS_append := "${THISDIR}"

# Patch for adding and starting backlight
SRC_URI_append = " file://barebox.patch"

do_configure_append() {
	# Set configuration to enable backlight
	kconfig_set DRIVER_VIDEO_BACKLIGHT y
	kconfig_set	DRIVER_VIDEO_BACKLIGHT_PWM y
}

python do_env_append_imx6ul-omni() {
    env_add(d, "boot/mmc",
"""#!/bin/sh

[ -e /env/config-expansions ] && /env/config-expansions

global.bootm.image="/mnt/mmc/zImage"
global.bootm.oftree="/mnt/mmc/oftree"
global.linux.bootargs.dyn.root="root=/dev/mmcblk0p2 rootflags='data=journal'"
""")
    env_add(d, "boot/nand",
"""#!/bin/sh

[ -e /env/config-expansions ] && /env/config-expansions

[ ! -e /dev/nand0.root.ubi ] && ubiattach /dev/nand0.root

global.bootm.image="/dev/nand0.root.ubi.kernel"
global.bootm.oftree="/dev/nand0.root.ubi.oftree"

global.linux.bootargs.dyn.root="root=ubi0:root ubi.mtd=root rootfstype=ubifs rw"
""")
    env_add(d, "config-expansions",
"""#!/bin/sh

. /env/expansions/imx6ul-phytec-segin-peb-eval-01
#use this expansion when a capacitive touchscreen is connected
#. /env/expansions/imx6ul-phytec-segin-peb-av-02
#use this expansion when a resisitive touchscreen is connected
#. /env/expansions/imx6ul-phytec-segin-peb-av-02-res

# imx6ul-phytec-lcd: 7" display
#of_display_timings -S /soc/aips-bus@02100000/lcdif@021c8000/display@di0/display-timings/ETM0700G0EDH6

# imx6ul-phytec-lcd: 5.7" display
#of_display_timings -S /soc/aips-bus@02100000/lcdif@021c8000/display@di0/display-timings/ETMV570G2DHU

# imx6ul-phytec-lcd: 4.3" display
#of_display_timings -S /soc/aips-bus@02100000/lcdif@021c8000/display@di0/display-timings/ETM0430G0DH6

# imx6ul-phytec-lcd: 3.5" display
#of_display_timings -S /soc/aips-bus@02100000/lcdif@021c8000/display@di0/display-timings/ETM0350G0DH6
""")
    env_add(d, "expansions/imx6ul-phytec-segin-peb-eval-01",
"""
of_fixup_status /gpio-keys
of_fixup_status /user_leds
""")
    env_add(d, "expansions/imx6ul-phytec-segin-peb-av-02",
"""
of_fixup_status /soc/aips-bus@02100000/lcdif@021c8000/
of_fixup_status /soc/aips-bus@02100000/lcdif@021c8000/display@di0
of_fixup_status /backlight
of_fixup_status /soc/aips-bus@02100000/i2c@021a0000/edt-ft5x06@38
of_fixup_status /soc/aips-bus@02000000/pwm@02088000/
""")
    env_add(d, "expansions/imx6ul-phytec-segin-peb-av-02-res",
"""
of_fixup_status /soc/aips-bus@02100000/lcdif@021c8000/
of_fixup_status /soc/aips-bus@02100000/lcdif@021c8000/display@di0
of_fixup_status /backlight
of_fixup_status /soc/aips-bus@02100000/i2c@021a0000/stmpe@44
of_fixup_status /soc/aips-bus@02000000/pwm@02088000/
""")
}

COMPATIBLE_MACHINE .= "|imx6ul-omni"
