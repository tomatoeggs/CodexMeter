#include "theme_runtime.h"

#include <string.h>

#include "device_log.h"
#include "theme_registry.h"

bool ThemeRuntime::begin(
    lv_obj_t* host, const ThemeResources& resources) {
  host_ = host;
  resources_ = resources;
  return host_ != nullptr;
}

bool ThemeRuntime::switch_to(const char* id) {
  const ThemePack* requested = theme_registry_find(id);
  bool exact_match = requested != nullptr;
  if (!requested) {
    device_logf("WARN", "theme unknown id=%s; fallback=classic", id ? id : "-");
    requested = theme_registry_default();
  }
  if (active_ == requested) return exact_match;

  if (active_) {
    active_->dashboard.unmount(state_);
    active_ = nullptr;
  }
  if (host_) lv_obj_clean(host_);
  memset(state_, 0, sizeof(state_));

  if (mount_pack(requested)) return exact_match;
  const ThemePack* fallback = theme_registry_default();
  if (requested != fallback) {
    if (host_) lv_obj_clean(host_);
    memset(state_, 0, sizeof(state_));
    mount_pack(fallback);
    return false;
  }
  return false;
}

bool ThemeRuntime::next(int direction) {
  const ThemePack* next_pack =
      theme_registry_next(active_ ? active_->id : nullptr, direction);
  return next_pack && switch_to(next_pack->id);
}

void ThemeRuntime::update(const DashboardViewModel& model) {
  model_ = model;
  has_model_ = true;
  if (active_) active_->dashboard.update(state_, model_);
}

void ThemeRuntime::tick(uint32_t now_ms) {
  if (active_) active_->dashboard.tick(state_, now_ms);
}

void ThemeRuntime::shutdown() {
  if (active_) active_->dashboard.unmount(state_);
  active_ = nullptr;
  if (host_) lv_obj_clean(host_);
  memset(state_, 0, sizeof(state_));
}

const char* ThemeRuntime::current_id() const {
  return active_ ? active_->id : "";
}

const char* ThemeRuntime::current_name() const {
  return active_ ? active_->display_name : "";
}

uint8_t ThemeRuntime::drift_margin_px() const {
  return active_ ? active_->drift_margin_px : 0;
}

bool ThemeRuntime::mount_pack(const ThemePack* pack) {
  if (!pack || !host_ || !pack->dashboard.mount ||
      !pack->dashboard.update || !pack->dashboard.tick ||
      !pack->dashboard.unmount ||
      pack->manifest_version != 1 ||
      pack->dashboard.state_size == 0 ||
      pack->dashboard.state_size > sizeof(state_)) {
    device_logf(
        "ERROR", "theme mount rejected id=%s size=%u",
        pack ? pack->id : "-", pack ? (unsigned int)pack->dashboard.state_size : 0);
    return false;
  }

  if (!pack->dashboard.mount(state_, host_, resources_)) {
    pack->dashboard.unmount(state_);
    if (host_) lv_obj_clean(host_);
    memset(state_, 0, sizeof(state_));
    device_logf("ERROR", "theme mount failed id=%s", pack->id);
    return false;
  }
  active_ = pack;
  if (has_model_) active_->dashboard.update(state_, model_);
  device_logf("INFO", "theme active id=%s", active_->id);
  return true;
}
