SUMMARY = "Copy file to Root FS"
DESCRIPTION = "Recipe for copying files from meta-omni layer into root file system"
LICENSE = "CLOSED"

FILESEXTRAPATHS_append := "${THISDIR}/services:"

SRC_URI = "file://gnss.sh \
		   file://gnss.service \
		   file://gsm.sh \
		   file://gsm.service \
		   file://led.sh \
		   file://led.service"

inherit systemd

SYSTEMD_SERVICE_${PN} = "gnss.service gsm.service led.service"

REQUIRED_DISTRO_FEATURES = "systemd"

RDEPENDS_copy2rootfs = "bash"

do_install () {
	install -d ${D}${sysconfdir}/scripts
	install -d ${D}${systemd_system_unitdir}

	install -m 0755 ${WORKDIR}/gnss.sh ${D}${sysconfdir}/scripts/gnss.sh
	install -m 0644 ${WORKDIR}/gnss.service ${D}${systemd_system_unitdir}/gnss.service

	install -m 0755 ${WORKDIR}/gsm.sh ${D}${sysconfdir}/scripts/gsm.sh
	install -m 0644 ${WORKDIR}/gsm.service ${D}${systemd_system_unitdir}/gsm.service

	install -m 0755 ${WORKDIR}/led.sh ${D}${sysconfdir}/scripts/led.sh
	install -m 0644 ${WORKDIR}/led.service ${D}${systemd_system_unitdir}/led.service
}

FILES_${PN} += "${systemd_system_unitdir}"