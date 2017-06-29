SUMMARY = "Copy file to Root FS"
DESCRIPTION = "Recipe for copying files from meta-omni layer into root file system"
LICENSE = "CLOSED"
FILESEXTRAPATHS_append := "${THISDIR}/script:"
FILESEXTRAPATHS_append := "${THISDIR}/service:"


SRC_URI = "file://adc.service \
		   file://bmp280.service \
		   file://lsm6dsl.service \
		   file://gnss.sh \
		   file://gnss.service \
		   file://gsm.sh \
		   file://gsm.service \
		   file://led.sh \
		   file://led.service"

inherit systemd

SYSTEMD_SERVICE_${PN} = "adc.service bmp280.service gnss.service \
					  	 gsm.service led.service lsm6dsl.service"

SYSTEMD_AUTO_ENABLE = "disable"

REQUIRED_DISTRO_FEATURES = "systemd"

RDEPENDS_copy2rootfs = "bash"

do_install () {
	install -d ${D}${sysconfdir}/scripts
	install -d ${D}${systemd_system_unitdir}

	install -m 0644 ${WORKDIR}/adc.service ${D}${systemd_system_unitdir}/adc.service

	install -m 0644 ${WORKDIR}/bmp280.service ${D}${systemd_system_unitdir}/bmp280.service

	install -m 0644 ${WORKDIR}/lsm6dsl.service ${D}${systemd_system_unitdir}/lsm6dsl.service

	install -m 0755 ${WORKDIR}/gnss.sh ${D}${sysconfdir}/scripts/gnss.sh
	install -m 0644 ${WORKDIR}/gnss.service ${D}${systemd_system_unitdir}/gnss.service

	install -m 0755 ${WORKDIR}/gsm.sh ${D}${sysconfdir}/scripts/gsm.sh
	install -m 0644 ${WORKDIR}/gsm.service ${D}${systemd_system_unitdir}/gsm.service

	install -m 0755 ${WORKDIR}/led.sh ${D}${sysconfdir}/scripts/led.sh
	install -m 0644 ${WORKDIR}/led.service ${D}${systemd_system_unitdir}/led.service
}

FILES_${PN} += "${systemd_system_unitdir}"