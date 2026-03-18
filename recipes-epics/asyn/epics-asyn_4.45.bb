inherit epics-module

SUMMARY = "Asyn recipe"
DESCRIPTION = "Recipe for building asyn for the EPICS control system."

LICENSE = "synApps"
LIC_FILES_CHKSUM = "file://LICENSE;md5=9f42f43716fb1d5e8498617125cb3c21"
LICENSE_PATH += "${S}"
NO_GENERIC_LICENSE[synApps] = "LICENSE"

SRCREV = "d55786e0508b1f8244cfae943ebc5fffccfb7590"
SRC_URI = "git://github.com/epics-modules/asyn;protocol=https;branch=master;rev=${SRCREV}"

DEPENDS += "libtirpc"

S = "${WORKDIR}/git"

python do_configure() {
    # Generate a RELEASE.local handling all dependencies
    epics.generate_release_local(d)

    # Do NOT set TIRPC=YES — that causes asyn's RULES_BUILD to append
    # -I/usr/include/tirpc (host path) which fails cross-compilation with
    # -Werror=poison-system-directories.  Instead supply the sysroot-relative
    # tirpc include and lib paths directly.
    sysroot = d.getVar('RECIPE_SYSROOT')
    epics.generate_config_site(d, {
        "USR_CPPFLAGS": f"-I{sysroot}/usr/include/tirpc",
        "USR_LDFLAGS":  f"--sysroot={sysroot} -L{sysroot}/usr/lib -ltirpc",
    })
}

# Build only the asyn library, skip test apps which are not needed on target
do_compile() {
    make -j${BB_NUMBER_THREADS} -C asyn install
}

# Same install method as in epics-component.bbclass but we don't copy iocBoot dirs
do_install() {
    make -j${BB_NUMBER_THREADS} -C asyn install

    # Remove unnecessary test libraries from this package
    rm -f ${D}/opt/epics/${MODNAME}/lib/linux-${TARGET_ARCH}/libtest*.a
    rm -f ${D}/opt/epics/${MODNAME}/lib/linux-${TARGET_ARCH}/libdevTestGpib.a
}
