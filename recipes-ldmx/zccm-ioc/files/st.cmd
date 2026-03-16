epicsEnvSet("IOC", "zccm-ioc")
epicsEnvSet("EPICS_BASE", "/opt/epics/epics-base")

dbLoadRecords("/opt/zccm-ioc/db/zccm.db")
dbLoadRecords("/opt/zccm-ioc/db/counter_stream.db")

iocInit

dbl
