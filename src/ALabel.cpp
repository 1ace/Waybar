#include "ALabel.hpp"
#include <util/command.hpp>

#include <iostream>

waybar::ALabel::ALabel(const Json::Value& config, const std::string& format, uint16_t interval)
    : config_(config),
      format_(config_["format"].isString() ? config_["format"].asString() : format),
      interval_(config_["interval"] == "once"
                    ? std::chrono::seconds(100000000)
                    : std::chrono::seconds(
                          config_["interval"].isUInt() ? config_["interval"].asUInt() : interval)),
      default_format_(format_) {
  event_box_.add(label_);
  if (config_["max-length"].isUInt()) {
    label_.set_max_width_chars(config_["max-length"].asUInt());
    label_.set_ellipsize(Pango::EllipsizeMode::ELLIPSIZE_END);
  }

  if (config_["rotate"].isUInt()) {
    label_.set_angle(config["rotate"].asUInt());
  }

  if (config_["format-alt"].isString()) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &ALabel::handleToggle));
  }

  // configure events' user commands
  if (config_["on-click"].isString() || config_["on-click-right"].isString()) {
    event_box_.add_events(Gdk::BUTTON_PRESS_MASK);
    event_box_.signal_button_press_event().connect(sigc::mem_fun(*this, &ALabel::handleToggle));
  }
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &ALabel::handleScroll));
  }
}

waybar::ALabel::~ALabel() {
  for (const auto& pid : pid_) {
    if (pid != -1) {
      kill(-pid, 9);
    }
  }
}

auto waybar::ALabel::update() -> void {
  // Nothing here
}

bool waybar::ALabel::handleToggle(GdkEventButton* const& e) {
  if (config_["on-click"].isString() && e->button == 1) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-click"].asString()));
  } else if (config_["on-click-middle"].isString() && e->button == 2) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-click-middle"].asString()));
  } else if (config_["on-click-right"].isString() && e->button == 3) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-click-right"].asString()));
  } else if (config_["on-click-forward"].isString() && e->button == 8) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-click-backward"].asString()));
  } else if (config_["on-click-backward"].isString() && e->button == 9) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-click-forward"].asString()));
  }
  if (config_["format-alt-click"].isUInt() && e->button == config_["format-alt-click"].asUInt()) {
    alt_ = !alt_;
    if (alt_ && config_["format-alt"].isString()) {
      format_ = config_["format-alt"].asString();
    } else {
      format_ = default_format_;
    }
  }

  dp.emit();
  return true;
}

bool waybar::ALabel::handleScroll(GdkEventScroll* e) {
  // Avoid concurrent scroll event
  std::lock_guard<std::mutex> lock(mutex_);
  bool                        direction_up = false;

  if (e->direction == GDK_SCROLL_UP) {
    direction_up = true;
  }
  if (e->direction == GDK_SCROLL_DOWN) {
    direction_up = false;
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent*>(e), &delta_x, &delta_y);
    if (delta_y < 0) {
      direction_up = true;
    } else if (delta_y > 0) {
      direction_up = false;
    }
  }
  if (direction_up && config_["on-scroll-up"].isString()) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-scroll-up"].asString()));
  } else if (config_["on-scroll-down"].isString()) {
    pid_.push_back(waybar::util::command::forkExec(config_["on-scroll-down"].asString()));
  }
  dp.emit();
  return true;
}

std::string waybar::ALabel::getIcon(uint16_t percentage, const std::string& alt) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    if (!alt.empty() && (format_icons[alt].isString() || format_icons[alt].isArray())) {
      format_icons = format_icons[alt];
    } else {
      format_icons = format_icons["default"];
    }
  }
  if (format_icons.isArray()) {
    auto size = format_icons.size();
    auto idx = std::clamp(percentage / (100 / size), 0U, size - 1);
    format_icons = format_icons[idx];
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

bool waybar::ALabel::tooltipEnabled() {
  return config_["tooltip"].isBool() ? config_["tooltip"].asBool() : true;
}

waybar::ALabel::operator Gtk::Widget&() { return event_box_; }
