// prefs.h — persistent preferences (ports NSUserDefaults persistence).
// Stored as INI at %APPDATA%\WindowSweaters\settings.ini. CLI args still win:
// load runs before CLI parsing, every menu action saves.
#pragma once
#include "../core/config.h"
void prefs_load(struct settings* st);
void prefs_save(void);
