#include "theme_registry.h"

#include <string.h>

#include "animal_crossing_theme.h"
#include "classic_theme.h"
#include "cyberpunk_theme.h"
#include "famicom_theme.h"

namespace {

const ThemePack* const THEMES[] = {
    &classic_theme_pack(),
    &cyberpunk_theme_pack(),
    &famicom_theme_pack(),
    &animal_crossing_theme_pack(),
};

bool valid_theme_id(const char* id) {
  if (!id || !id[0]) return false;
  size_t length = strnlen(id, 24);
  if (length == 24) return false;
  for (size_t i = 0; i < length; ++i) {
    char ch = id[i];
    if (!((ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9') ||
          ch == '_' || ch == '-')) {
      return false;
    }
  }
  return true;
}

}  // namespace

size_t theme_registry_count() {
  return sizeof(THEMES) / sizeof(THEMES[0]);
}

const ThemePack* theme_registry_at(size_t index) {
  return index < theme_registry_count() ? THEMES[index] : nullptr;
}

const ThemePack* theme_registry_find(const char* id) {
  if (!id || !id[0]) return nullptr;
  for (size_t i = 0; i < theme_registry_count(); ++i) {
    if (strcmp(THEMES[i]->id, id) == 0) return THEMES[i];
  }
  return nullptr;
}

const ThemePack* theme_registry_default() {
  return THEMES[0];
}

const ThemePack* theme_registry_next(const char* current_id, int direction) {
  size_t count = theme_registry_count();
  if (count == 0) return nullptr;
  size_t current = 0;
  for (size_t i = 0; i < count; ++i) {
    if (current_id && strcmp(THEMES[i]->id, current_id) == 0) {
      current = i;
      break;
    }
  }
  int normalized = direction < 0 ? -1 : 1;
  size_t next =
      normalized > 0 ? (current + 1) % count : (current + count - 1) % count;
  return THEMES[next];
}

bool theme_registry_validate() {
  if (theme_registry_count() == 0 || !theme_registry_default()) {
    return false;
  }
  for (size_t i = 0; i < theme_registry_count(); ++i) {
    const ThemePack* theme = THEMES[i];
    if (!theme || !valid_theme_id(theme->id) ||
        !theme->display_name || !theme->display_name[0]) {
      return false;
    }
    for (size_t j = i + 1; j < theme_registry_count(); ++j) {
      if (THEMES[j] && THEMES[j]->id &&
          strcmp(theme->id, THEMES[j]->id) == 0) {
        return false;
      }
    }
  }
  return true;
}
