
FILESEXTRAPATHS_append := "${THISDIR}/files:"

SRC_URI += " file://psplash-start.service"
SRC_URI += " file://psplash-quit.service"

inherit systemd

SYSTEMD_SERVICE_${PN} = "psplash-start.service psplash-quit.service"

do_install_append() {
	install -d ${D}${systemd_system_unitdir}

	install -m 0644 ${WORKDIR}/psplash-start.service ${D}${systemd_system_unitdir}/psplash-start.service
	install -m 0644 ${WORKDIR}/psplash-quit.service ${D}${systemd_system_unitdir}/psplash-quit.service
}