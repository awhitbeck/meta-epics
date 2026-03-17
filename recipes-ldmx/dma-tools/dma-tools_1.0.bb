SUMMARY = "AES stream DMA test utilities"
DESCRIPTION = "dmaRead utility from aes-stream-drivers for testing the axistreamdma kernel driver."

LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = " \
    file://dmaRead.cpp \
    file://PrbsData.cpp \
    file://PrbsData.h \
    file://DmaDriver.h \
    file://AxisDriver.h \
"

S = "${WORKDIR}"

do_compile() {
    ${CXX} ${CXXFLAGS} ${LDFLAGS} \
        -I${S} \
        ${S}/PrbsData.cpp \
        ${S}/dmaRead.cpp \
        -o ${S}/dmaRead \
        -lpthread
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/dmaRead ${D}${bindir}/dmaRead
}

FILES:${PN} = "${bindir}/dmaRead"
