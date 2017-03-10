# Original recipe core-image-full-cmdline
DESCRIPTION = "A console-only image with more full-featured Linux system \
functionality installed."

IMAGE_FEATURES += "splash ssh-server-openssh"

IMAGE_INSTALL = "\
	packagegroup-core-boot \
	packagegroup-core-full-cmdline \
	${CORE_IMAGE_EXTRA_INSTALL} \
	"

inherit core-image

# Recipe omni-image for customisation
SUMMARY = ""
DESCRIPTION = "Custom OMNI image"
LICENCE = "MIT"
IMAGE_INSTALL_append = " barebox usbutils bash copy2rootfs zile i2c-tools tree \
						 minicom fbtest protobuf sqlite3 gtest can-utils gawk \
						 grep htop nano sed tar tcf-agent util-linux vim ppp wvdial"

IMAGE_FSTYPES = "tar.gz ubifs"