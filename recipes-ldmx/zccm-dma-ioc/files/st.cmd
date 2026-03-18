epicsEnvSet("IOC",        "zccm-dma-ioc")
epicsEnvSet("EPICS_BASE", "/opt/epics/epics-base")
epicsEnvSet("ASYN",       "/opt/epics/epics-asyn")

# Load the DMA driver shared library
dlload /opt/zccm-dma-ioc/lib/libZccmDmaDriver.so

# Instantiate the driver: portName, devPath
ZccmDmaDriverConfigure("ZCCM_DMA", "/dev/axi_stream_dma_0")

dbLoadRecords("/opt/zccm-dma-ioc/db/zccm_dma.db", "PORT=ZCCM_DMA")

iocInit

dbl
