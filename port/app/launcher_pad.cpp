// The launcher with a controller (launcher_pad.h).
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "launcher_pad.h"
#include "launcher_egg.h"
#include "launcher_melee_assets.h"
#include "../runtime/gx/walter_egg.h"

#include <libusb.h>
#include <xinput.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

#pragma comment(lib, "xinput9_1_0.lib")

namespace launcher::pad {
namespace {

HWND g_main = nullptr, g_hand = nullptr;
std::atomic<bool> g_game{false}, g_quit{false};
std::atomic<ULONGLONG> g_last_used{0};
constexpr ULONGLONG kHandShownFor = 4000;   // the hand stays this long after the controller last moved it

// One controller's state this poll, in the GameCube's terms.
struct Pad { bool ok = false; uint16_t buttons = 0; float x = 0, y = 0, cx = 0, cy = 0; };   // sticks -1..1, y up

// ---- the official adapter (WinUSB / libusbK / libusb-win32 through libusb, as the game reads it)
struct Adapter {
  libusb_context* ctx = nullptr; libusb_device_handle* h = nullptr; bool started = false;
  ULONGLONG next_try = 0;
  void close() {
    if (h) { libusb_release_interface(h, 0); libusb_close(h); h = nullptr; }
    started = false;
  }
  Pad read() {
    Pad p;
    if (!ctx && libusb_init(&ctx) != 0) { ctx = nullptr; return p; }
    if (!h) {
      if (GetTickCount64() < next_try) return p;
      next_try = GetTickCount64() + 2000;   // no adapter: look again in 2 s
      h = libusb_open_device_with_vid_pid(ctx, 0x057E, 0x0337);
      if (!h) return p;
      if (libusb_claim_interface(h, 0) != 0) { libusb_close(h); h = nullptr; return p; }
    }
    int done = 0;
    if (!started) { unsigned char start = 0x13; if (libusb_interrupt_transfer(h, 0x02, &start, 1, &done, 50) != 0) { close(); return p; } started = true; }
    unsigned char rep[37] = {};
    const int r = libusb_interrupt_transfer(h, 0x81, rep, sizeof rep, &done, 20);
    if (r == LIBUSB_ERROR_NO_DEVICE || r == LIBUSB_ERROR_IO) { close(); return p; }
    if (r != 0 || done < 37 || rep[0] != 0x21) return p;
    for (int port = 0; port < 4; ++port) {   // the first plugged-in port drives the hand
      const unsigned char* q = rep + 1 + port * 9;
      if (!(q[0] & 0x30)) continue;
      using namespace walter_egg;
      uint16_t b = 0;
      if (q[1] & 0x01) b |= kA; if (q[1] & 0x02) b |= kB; if (q[1] & 0x10) b |= kDL; if (q[1] & 0x20) b |= kDR;
      if (q[1] & 0x40) b |= kDD; if (q[1] & 0x80) b |= kDU; if (q[2] & 0x01) b |= 0x1000;   // Start
      auto axis = [](unsigned char v) { const float a = ((float)v - 128.0f) / 100.0f; return std::clamp(a, -1.0f, 1.0f); };
      p.ok = true; p.buttons = b; p.x = axis(q[3]); p.y = axis(q[4]); p.cx = axis(q[5]); p.cy = axis(q[6]);
      return p;
    }
    return p;
  }
} g_adapter;

Pad read_xinput() {
  Pad p;
  for (DWORD i = 0; i < 4; ++i) {
    XINPUT_STATE s{};
    if (XInputGetState(i, &s) != ERROR_SUCCESS) continue;
    using namespace walter_egg;
    const WORD w = s.Gamepad.wButtons; uint16_t b = 0;
    if (w & XINPUT_GAMEPAD_A) b |= kA; if (w & XINPUT_GAMEPAD_B) b |= kB; if (w & XINPUT_GAMEPAD_DPAD_LEFT) b |= kDL;
    if (w & XINPUT_GAMEPAD_DPAD_RIGHT) b |= kDR; if (w & XINPUT_GAMEPAD_DPAD_DOWN) b |= kDD; if (w & XINPUT_GAMEPAD_DPAD_UP) b |= kDU;
    if (w & XINPUT_GAMEPAD_START) b |= 0x1000;
    auto axis = [](SHORT v) { return std::clamp((float)v / 32767.0f, -1.0f, 1.0f); };
    p.ok = true; p.buttons = b; p.x = axis(s.Gamepad.sThumbLX); p.y = axis(s.Gamepad.sThumbLY); p.cx = axis(s.Gamepad.sThumbRX); p.cy = axis(s.Gamepad.sThumbRY);
    return p;
  }
  return p;
}

// ---- the hand: a small layered window at the pointer, Melee's own hand when the disc is there
void draw_hand(POINT at) {
  const auto& img = launcher::melee_assets::hand();
  const int dpi = GetDpiForWindow(g_main), H = MulDiv(56, dpi, 96), W = img.w ? H * img.w / img.h : MulDiv(32, dpi, 96);
  BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
  void* bits = nullptr; HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
  HBITMAP bm = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bm) { DeleteDC(dc); ReleaseDC(nullptr, screen); return; }
  HGDIOBJ old = SelectObject(dc, bm);
  uint32_t* px = (uint32_t*)bits;
  for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
    uint32_t a = 0, i = 0;
    if (img.w) {   // box-sampled from the 56x80 hand: white glove, shaded by its own grey, a dark outline under it
      const int sx0 = x * img.w / W, sx1 = std::max(sx0 + 1, (x + 1) * img.w / W), sy0 = y * img.h / H, sy1 = std::max(sy0 + 1, (y + 1) * img.h / H);
      uint32_t sa = 0, n = 0;
      for (int sy = sy0; sy < sy1; ++sy) for (int sx = sx0; sx < sx1; ++sx) { sa += img.bgra[(size_t)sy * img.w + sx] >> 24; ++n; }
      a = sa / std::max(1u, n); i = 0xE8;
    } else {   // no disc: a plain pointer shape
      const bool in = x < W / 2 + y / 3 && y < H * 2 / 3 && x <= y; a = in ? 230 : 0; i = 0xF0;
    }
    const uint32_t pi = i * a / 255; px[(size_t)y * W + x] = a << 24 | pi << 16 | pi << 8 | pi;
  }
  // The fingertip is the hand's top: the pointer sits there.
  POINT dst{at.x - W / 3, at.y - MulDiv(2, dpi, 96)}, src{0, 0}; SIZE sz{W, H}; BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  UpdateLayeredWindow(g_hand, screen, &dst, &sz, dc, &src, 0, &bf, ULW_ALPHA);
  SelectObject(dc, old); DeleteObject(bm); DeleteDC(dc); ReleaseDC(nullptr, screen);
}

void send_key(WORD vk, bool down) { INPUT in{}; in.type = INPUT_KEYBOARD; in.ki.wVk = vk; in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP; SendInput(1, &in, sizeof in); }
void send_click(bool down) { INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; SendInput(1, &in, sizeof in); }
void send_wheel(int amount) { INPUT in{}; in.type = INPUT_MOUSE; in.mi.dwFlags = MOUSEEVENTF_WHEEL; in.mi.mouseData = (DWORD)amount; SendInput(1, &in, sizeof in); }

bool launcher_in_front() {
  HWND f = GetForegroundWindow();
  if (!f) return false;
  DWORD pid = 0; GetWindowThreadProcessId(f, &pid);
  return pid == GetCurrentProcessId();   // the launcher or one of its own dialogs
}

void work() {
  uint16_t last = 0; float carry_x = 0, carry_y = 0, scroll = 0;
  while (!g_quit.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    if (g_game.load() || !launcher_in_front()) {   // the game (or another program) gets the adapter
      g_adapter.close();
      if (IsWindowVisible(g_hand)) PostMessageW(g_hand, WM_APP + 2, 0, 0);
      last = 0; continue;
    }
    Pad p = g_adapter.read();
    if (!p.ok) p = read_xinput();
    if (!p.ok) continue;
    launcher::egg::feed_pad(p.buttons);
    using namespace walter_egg;
    const uint16_t pressed = p.buttons & ~last, released = last & ~p.buttons;
    last = p.buttons;
    // The stick (past a small dead zone, faster the further it leans) or the D-pad moves the hand.
    auto curve = [](float v) { const float a = std::abs(v); return a < 0.18f ? 0.0f : std::copysign((a - 0.18f) / 0.82f * (a - 0.18f) / 0.82f, v); };
    float dx = curve(p.x), dy = -curve(p.y);
    if (p.buttons & kDL) dx -= 0.35f; if (p.buttons & kDR) dx += 0.35f; if (p.buttons & kDU) dy -= 0.35f; if (p.buttons & kDD) dy += 0.35f;
    const float speed = (float)MulDiv(18, GetDpiForWindow(g_main), 96);   // pixels per poll at full tilt
    carry_x += dx * speed; carry_y += dy * speed;
    const int mx = (int)carry_x, my = (int)carry_y; carry_x -= mx; carry_y -= my;
    POINT c; GetCursorPos(&c);
    if (mx || my) { c.x += mx; c.y += my; SetCursorPos(c.x, c.y); GetCursorPos(&c); }
    if (mx || my || pressed || released) g_last_used.store(GetTickCount64());
    if (pressed & kA) send_click(true);
    if (released & kA) send_click(false);
    if (pressed & kB) { send_key(VK_ESCAPE, true); send_key(VK_ESCAPE, false); }
    if (pressed & 0x1000) { send_key(VK_RETURN, true); send_key(VK_RETURN, false); }
    scroll += -curve(p.cy) * 40.0f;
    if (std::abs(scroll) >= 40.0f) { const int n = (int)(scroll / 40.0f); send_wheel(n * 40); scroll -= n * 40.0f; g_last_used.store(GetTickCount64()); }
    if (hand_shown()) PostMessageW(g_hand, WM_APP + 1, (WPARAM)c.x, (LPARAM)c.y);
    else if (IsWindowVisible(g_hand)) PostMessageW(g_hand, WM_APP + 2, 0, 0);
  }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
  switch (m) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: return HTTRANSPARENT;   // clicks go to what is under the hand
    case WM_APP + 1: draw_hand(POINT{(LONG)wp, (LONG)lp}); if (!IsWindowVisible(w)) ShowWindow(w, SW_SHOWNOACTIVATE); return 0;
    case WM_APP + 2: ShowWindow(w, SW_HIDE); return 0;
  }
  return DefWindowProcW(w, m, wp, lp);
}
}  // namespace

void init(HWND main_window) {
  g_main = main_window;
  WNDCLASSW wc{}; wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"MeleeUnlockedHand"; RegisterClassW(&wc);
  g_hand = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"MeleeUnlockedHand", L"", WS_POPUP,
                           0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
  std::thread(work).detach();
}
void game_running(bool running) { g_game.store(running); }
bool hand_shown() { return GetTickCount64() - g_last_used.load() < kHandShownFor; }

}  // namespace launcher::pad
