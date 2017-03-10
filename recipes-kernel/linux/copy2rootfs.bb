SUMMARY = "Copy file to Root FS"
DESCRIPTION = "Recipe for copying files from meta-omni layer into root file system"
LICENSE = "CLOSED"

FILESEXTRAPATHS_append := "${THISDIR}/linux-mainline:"

SRC_URI = "file://chip-init \
		   file://power-lock"

do_install () {
	install -d ${D}${sysconfdir}/init.d

	# Copy chip-init and create a symbol link
	install -m 0755 ${WORKDIR}/chip-init ${D}${sysconfdir}/init.d/chip-init
	install -d ${D}${sysconfdir}/rc5.d
	ln -sf ../init.d/chip-init ${D}${sysconfdir}/rc5.d/S90chip-init

	# Copy power-lock and create a symbol link
	install -m 0755 ${WORKDIR}/power-lock ${D}${sysconfdir}/init.d/power-lock
	install -d ${D}${sysconfdir}/rc0.d
	ln -sf ../init.d/power-lock ${D}${sysconfdir}/rc0.d/K90power-lock
}
