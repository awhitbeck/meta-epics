#
# Helper class containing functions that may be re-used in EPICS module/IOC
# recipes.
#

# Register the meta-epics python library path so the 'epics' module is
# available in all python task functions. This replaces the 'addpylib'
# directive which requires BitBake >= 2.4.0 (mickledore/scarthgap) and
# is not available in BitBake 2.2.0 (kirkstone/langdale, PetaLinux 2024.1).
python () {
    import sys, os, importlib, bb.utils
    # Locate the meta-epics layer root by searching BBPATH for this class file.
    # LAYERDIR is not available when a class is inherited by recipes in other
    # layers, and __file__ is not defined in BitBake anonymous python functions.
    bbpath = d.getVar('BBPATH') or ''
    cls_rel = 'classes/epics-functions.bbclass'
    cls_abs = bb.utils.which(bbpath, cls_rel)
    if not cls_abs:
        bb.fatal('epics-functions.bbclass: could not locate itself via BBPATH')
    pypath = os.path.normpath(os.path.join(os.path.dirname(cls_abs), '..', 'python'))
    if pypath not in sys.path:
        sys.path.insert(0, pypath)
    mod = importlib.import_module('epics')
    bb.utils._context['epics'] = mod
}

set_pcre () {
    # Unset PCRE "location" for host builds; this causes us issues if the build machine doesn't have libpcre1
    echo "PCRE=" >> "${S}/configure/CONFIG_SITE.Common.linux-${BUILD_ARCH}"

    # Point it at the right libraries for cross builds
    echo "PCRE_INCLUDE=${RECIPE_SYSROOT}/usr/include" >> "${S}/configure/CONFIG_SITE.Common.linux-${TARGET_ARCH}"
    echo "PCRE_LIB=${RECIPE_SYSROOT}/usr/lib" >> "${S}/configure/CONFIG_SITE.Common.linux-${TARGET_ARCH}"
    echo "PCRE=" >> "${S}/configure/CONFIG_SITE.Common.linux-${TARGET_ARCH}"
}

set_tirpc () {
    # Enable TIRPC, we need it on this glibc version!
    echo "TIRPC=YES" >> "${S}/configure/CONFIG_SITE.local"
}

