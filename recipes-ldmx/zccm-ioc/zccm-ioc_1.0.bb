SUMMARY = "ZCCM EPICS soft IOC"
DESCRIPTION = "softIocPVX database, startup script, Python register driver, \
and systemd services for the LDMX zCCM EPICS IOC."

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit allarch

SRC_URI = " \
    file://zccm.db \
    file://st.cmd \
    file://zccm_driver.py \
    file://zccm-ioc.service \
    file://zccm-driver.service \
"

DEPENDS += "epics-base epics-pvxs procserv"
RDEPENDS:${PN} += "epics-base epics-pvxs procserv python3-pyepics"

do_install() {
    # IOC files
    install -d ${D}/opt/zccm-ioc/db
    install -m 0644 ${WORKDIR}/zccm.db  ${D}/opt/zccm-ioc/db/zccm.db
    install -m 0644 ${WORKDIR}/st.cmd   ${D}/opt/zccm-ioc/st.cmd

    # Python driver
    install -m 0755 ${WORKDIR}/zccm_driver.py ${D}/opt/zccm-ioc/zccm_driver.py

    # systemd service units + wants symlinks (same pattern as epics-base caRepeater)
    install -d ${D}/etc/systemd/system/multi-user.target.wants
    install -m 0644 ${WORKDIR}/zccm-ioc.service    ${D}/etc/systemd/system/zccm-ioc.service
    install -m 0644 ${WORKDIR}/zccm-driver.service ${D}/etc/systemd/system/zccm-driver.service
    ln -s /etc/systemd/system/zccm-ioc.service    ${D}/etc/systemd/system/multi-user.target.wants/zccm-ioc.service
    ln -s /etc/systemd/system/zccm-driver.service ${D}/etc/systemd/system/multi-user.target.wants/zccm-driver.service
}

FILES:${PN} += " \
    /opt/zccm-ioc \
    /etc/systemd/system/zccm-ioc.service \
    /etc/systemd/system/zccm-driver.service \
    /etc/systemd/system/multi-user.target.wants/zccm-ioc.service \
    /etc/systemd/system/multi-user.target.wants/zccm-driver.service \
"
