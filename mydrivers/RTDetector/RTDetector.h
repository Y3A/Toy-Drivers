#pragma once

struct RemoteThreadInfo;

void DisplayData(RemoteThreadInfo *rt, DWORD count);
void DisplayTime(const LARGE_INTEGER &time);