SUMMARY = "ZCCM DMA IOC — asynPortDriver for direct DMA-to-EPICS waveform readout"
DESCRIPTION = "C++ asynPortDriver that reads DMA frames from /dev/axi_stream_dma_0 \
in a background thread and posts sample data directly into EPICS waveform records, \
bypassing the dmaFrameStream subprocess and CA caput overhead."

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://ZccmDmaDriver.cpp \
    file://DmaDriver.h \
    file://AxisDriver.h \
    file://zccm_dma.db \
    file://st.cmd \
    file://zccm-dma-ioc.service \
"

S = "${WORKDIR}"

DEPENDS  += "epics-base epics-asyn epics-pvxs procserv"
RDEPENDS:${PN} += "epics-base epics-asyn epics-pvxs procserv"

EPICS_BASE = "/opt/epics/epics-base"
EPICS_ASYN = "/opt/epics/epics-asyn"
TARGET_ARCH_EPICS = "linux-aarch64"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} \
        -I${S} \
        -I${RECIPE_SYSROOT}/opt/epics/epics-base/include \
        -I${RECIPE_SYSROOT}/opt/epics/epics-base/include/os/Linux \
        -I${RECIPE_SYSROOT}/opt/epics/epics-base/include/compiler/gcc \
        -I${RECIPE_SYSROOT}/opt/epics/epics-asyn/include \
        -L${RECIPE_SYSROOT}/opt/epics/epics-base/lib/${TARGET_ARCH_EPICS} \
        -L${RECIPE_SYSROOT}/opt/epics/epics-asyn/lib/${TARGET_ARCH_EPICS} \
        -shared -fPIC \
        ${S}/ZccmDmaDriver.cpp \
        -o ${S}/libZccmDmaDriver.so \
        -lasyn \
        -lca -lCom \
        -Wl,-rpath,${EPICS_BASE}/lib/${TARGET_ARCH_EPICS} \
        -Wl,-rpath,${EPICS_ASYN}/lib/${TARGET_ARCH_EPICS}
}

do_install() {
    # IOC files
    install -d ${D}/opt/zccm-dma-ioc/db
    install -m 0644 ${WORKDIR}/zccm_dma.db  ${D}/opt/zccm-dma-ioc/db/zccm_dma.db
    install -m 0644 ${WORKDIR}/st.cmd       ${D}/opt/zccm-dma-ioc/st.cmd

    # Driver shared library — loaded by softIocPVX at runtime
    install -d ${D}/opt/zccm-dma-ioc/lib
    install -m 0755 ${S}/libZccmDmaDriver.so ${D}/opt/zccm-dma-ioc/lib/libZccmDmaDriver.so

    # systemd service
    install -d ${D}/etc/systemd/system/multi-user.target.wants
    install -m 0644 ${WORKDIR}/zccm-dma-ioc.service \
        ${D}/etc/systemd/system/zccm-dma-ioc.service
    ln -s /etc/systemd/system/zccm-dma-ioc.service \
        ${D}/etc/systemd/system/multi-user.target.wants/zccm-dma-ioc.service
}

FILES:${PN} += " \
    /opt/zccm-dma-ioc \
    /etc/systemd/system/zccm-dma-ioc.service \
    /etc/systemd/system/multi-user.target.wants/zccm-dma-ioc.service \
"
