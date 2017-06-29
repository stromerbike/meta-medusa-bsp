# Original recipe core-image-full-cmdline
DESCRIPTION = "A console-only image with more full-featured Linux system \
functionality installed."

IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL = "\
	packagegroup-core-boot \
	packagegroup-core-full-cmdline \
	${CORE_IMAGE_EXTRA_INSTALL} \
	"

inherit core-image

# Recipe Medusa-image for customisation
SUMMARY = ""
DESCRIPTION = "Custom Stromer Medusa image"
LICENCE = "MIT"
IMAGE_INSTALL_append = " barebox dt-utils-barebox-state kernel-image kernel-devicetree \
						 minicom lrzsz systemd-analyze bluez-hcidump \
						 usbutils copy2rootfs zile i2c-tools \
						 fbtest evtest protobuf sqlite3 gtest can-utils \
						 htop nano util-linux vim \
						 wvdial  \
						 openssh"

# Added packets:
# barebox - bootloader
# kernel-image - copy kernel into rootfs (boot directory)
# kernel-devicetree - copy dtb into rootfs (boot directory)
# dt-utils-barebox-state - linux packet for set/get shared barebox variables
# minicom - communication program
# lrzsz - Transfer files via minicom
# systemd-analyze - Debug information collection

IMAGE_FSTYPES = "tar.gz ubifs"
