#include "wifi_qr_large.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace esphome::wifi_qr_large {

using tessera::Font;
namespace ui = tessera::ui;

static std::string wifi_escape(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (char ch : value) {
    if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"') escaped += '\\';
    escaped += ch;
  }
  return escaped;
}

static uint64_t fingerprint(const std::string &value) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : value) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

class WifiTile : public tessera::Tile {
 public:
  explicit WifiTile(WifiQr *plugin) : plugin_(plugin) {}

  void create(const tessera::TileContext &c) override {
    width_ = c.width;
    height_ = c.height;
    const int line = ui::line_height(Font::BODY);
    const int gap = ui::px(6);
    // "Scan QR code to connect" above the code, in the largest of two fonts it fits in (shorter words on a narrow
    // tile), where the tile is high enough for a heading, a code and the network's name.
    // The tile's own name from the editor ("Name on the screen") is the heading where one was given.
    const bool named = c.name != nullptr && c.name[0];
    const std::string scan = named ? std::string(c.name) : std::string(plugin_->text("scan"));
    const std::string scan_short = named ? scan : std::string(plugin_->text("scan_short"));
    Font heading_font = Font::BODY;
    std::string heading = scan_short;
    if (ui::text_width(scan, Font::BODY_LARGE) <= width_) {
      heading_font = Font::BODY_LARGE;
      heading = scan;
    } else if (ui::text_width(scan, Font::BODY) <= width_) {
      heading = scan;
    }
    const int heading_h = ui::line_height(heading_font);
    const bool headed = height_ >= heading_h + line + 2 * gap + ui::mm(14) && ui::text_width(heading, heading_font) <= width_;
    const int top = headed ? heading_h + gap : 0;
    const int available = std::max(1, height_ - top - line - gap);
    const int side = std::max(1, std::min(width_, available));
    const int zone = std::max(4, side / 12);
    const int code = std::max(21, side - 2 * zone);

#if LV_USE_QRCODE
    qr_ = lv_qrcode_create(c.parent);
    lv_qrcode_set_dark_color(qr_, theme::color(theme::QR_DARK));
    lv_qrcode_set_light_color(qr_, theme::color(theme::QR_LIGHT));
    lv_obj_set_style_border_color(qr_, theme::color(theme::QR_LIGHT), 0);
    lv_obj_set_style_border_width(qr_, zone, 0);
    lv_qrcode_set_size(qr_, code);
    lv_obj_set_pos(qr_, (width_ - code) / 2, top);
    lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
#endif

    if (headed) {
      title_ = ui::label(c.parent, heading_font, theme::INK);
      lv_obj_set_width(title_, width_);
      lv_obj_set_height(title_, heading_h);
      lv_obj_set_pos(title_, 0, 0);
      lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
      ui::set_text(title_, heading);
    }
    status_ = ui::label(c.parent, Font::BODY, theme::MUTED);
    lv_obj_set_width(status_, width_);
    lv_obj_set_height(status_, line);
    lv_obj_set_pos(status_, 0, height_ - line);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, 0);
  }

  void on_tick(uint32_t) override {
    const std::string &ssid = plugin_->ssid();
    const std::string &password = plugin_->password();
    if (ssid.empty() || password.empty()) {
      ui::set_text(status_, plugin_->text("helpers_empty"));
#if LV_USE_QRCODE
      lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
#endif
      shown_ = false;
      return;
    }

    const std::string payload = "WIFI:T:WPA;S:" + wifi_escape(ssid) + ";P:" + wifi_escape(password) + ";;";
    const uint64_t next = fingerprint(payload);
#if LV_USE_QRCODE
    if (!shown_ || next != fingerprint_) {
      lv_qrcode_update(qr_, payload.data(), payload.size());
      fingerprint_ = next;
      lv_obj_remove_flag(qr_, LV_OBJ_FLAG_HIDDEN);
      shown_ = true;
    }
#endif
    ui::set_text(status_, ssid);
  }

  void on_theme() override {
    ui::set_color(status_, theme::MUTED);
    if (title_) ui::set_color(title_, theme::INK);
#if LV_USE_QRCODE
    lv_qrcode_set_dark_color(qr_, theme::color(theme::QR_DARK));
    lv_qrcode_set_light_color(qr_, theme::color(theme::QR_LIGHT));
    lv_obj_set_style_border_color(qr_, theme::color(theme::QR_LIGHT), 0);
#endif
  }

 private:
  WifiQr *plugin_;
  int width_ = 0;
  int height_ = 0;
  bool shown_ = false;
  uint64_t fingerprint_ = 0;
  lv_obj_t *qr_{};
  lv_obj_t *status_{};
  lv_obj_t *title_{};
};

void WifiQr::setup() { add_tile("connect", [this]() { return new WifiTile(this); }); }

}  // namespace esphome::wifi_qr_large
