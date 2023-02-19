#ifndef BOOSTER_CLIENT_H
#define BOOSTER_CLIENT_H

#define perr(x) printf("%s (error=%u)\n", x, GetLastError())
BOOL change_priority(int tid, int priority, HANDLE device);

#endif
