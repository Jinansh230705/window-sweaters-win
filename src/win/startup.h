// startup.h — "Run on Startup" toggle via HKCU...\Run (per-user, no admin).
#pragma once
int startup_is_enabled(void);
int startup_set(int on); // 1 ok, 0 failed
