#pragma once

#define DRIVER_TAG      'tcpK'
#define DRIVER_PREFIX   "KProtector: "

#define err(msg, status) KdPrint((DRIVER_PREFIX "Error %s : (0x%08X)\n", msg, status))
#define log(msg) KdPrint((DRIVER_PREFIX "%s\n", msg))

#define DEREF_PTR(exp) (*(exp))