SUMMARY = "ZCCM EPICS soft IOC"
DESCRIPTION = "softIocPVX database, startup script, Python register driver, \
and systemd services for the LDMX zCCM EPICS IOC."

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch systemd

SRC_URI = " \
    file://zccm.db \
    file://st.cmd \
    file://zccm_driver.py \
    file://zccm-ioc.service \
    file://zccm-driver.service \
"

DEPENDS += "epics-base epics-pvxs procserv"
RDEPENDS:${PN} += "epics-base epics-pvxs procserv python3-pyepics"

SYSTEMD_SERVICE:${PN} = "zccm-ioc.service zccm-driver.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    # IOC files
    install -d ${D}/opt/zccm-ioc/db
    install -m 0644 ${WORKDIR}/zccm.db  ${D}/opt/zccm-ioc/db/zccm.db
    install -m 0644 ${WORKDIR}/st.cmd   ${D}/opt/zccm-ioc/st.cmd

    # Python driver
    install -m 0755 ${WORKDIR}/zccm_driver.py ${D}/opt/zccm-ioc/zccm_driver.py

    # systemd service units
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/zccm-ioc.service    ${D}${systemd_system_unitdir}/zccm-ioc.service
    install -m 0644 ${WORKDIR}/zccm-driver.service ${D}${systemd_system_unitdir}/zccm-driver.service
}

FILES:${PN} += " \
    /opt/zccm-ioc \
    ${systemd_system_unitdir}/zccm-ioc.service \
    ${systemd_system_unitdir}/zccm-driver.service \
"
