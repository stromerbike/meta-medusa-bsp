
PACKAGECONFIG_append = " networkd"
PACKAGECONFIG[backlight] = "--disable-backlight"
PACKAGECONFIG[timedated] = "--disable-timedated"
PACKAGECONFIG[timesyncd] = "--disable-timesyncd"

do_install_append() {
	# systemd-journald disable storing data
	sed -i -e 's/.*Storage.*/Storage=none/' ${D}${sysconfdir}/systemd/journald.conf
	sed -i -e 's/.*ForwardToSyslog.*/#ForwardToSyslog/' ${D}${sysconfdir}/systemd/journald.conf
	# disable virtual console
	rm ${D}${sysconfdir}/systemd/system/getty.target.wants/getty@tty1.service
}