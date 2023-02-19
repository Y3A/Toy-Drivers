#ifndef BOOSTER_DRV_H
#define BOOSTER_DRV_H

void BoosterUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS BoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS BoosterWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#endif