#pragma once

#include <stddef.h>

#include "theme.h"

size_t theme_registry_count();
const ThemePack* theme_registry_at(size_t index);
const ThemePack* theme_registry_find(const char* id);
const ThemePack* theme_registry_default();
const ThemePack* theme_registry_next(const char* current_id, int direction = 1);
bool theme_registry_validate();
