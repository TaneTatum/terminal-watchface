#include <pebble.h>
#include <ctype.h>

// ── Feature toggles ──────────────────────────────────────────────────
#define SCANLINES 1   // CRT scanline overlay (every 4 rows)
#define BLINK     1   // blinking cursor + prompt underscore

// ── Colour palette ───────────────────────────────────────────────────
#define C_BG   GColorBlack           // #000000 – background
#define C_HOT  GColorMediumAquamarine // #55FFAA – primary phosphor
#define C_DIM  GColorIslamicGreen    // #00AA00 – dim phosphor / labels
#define C_SCAN GColorDarkGreen       // #005500 – scanlines

// ── Layout constants (emery 200 × 228) ───────────────────────────────
#define MARGIN       12
#define CONTENT_W   176   // 200 - 2*12
#define DIVIDER_Y    31

// Status bar
#define STATUS_Y      8
#define STATUS_H     16

// Battery icon (right-aligned, 18×9 body + 2×5 nub)
#define BATT_BODY_W  18
#define BATT_BODY_H   9
#define BATT_NUB_W    2
#define BATT_NUB_H    5
// Right edge of nub at x=188; body right edge at x=186
#define BATT_X      (188 - BATT_BODY_W)   // = 170
#define BATT_Y       10

// Percentage text sits to the left of the battery icon
#define BATT_TXT_X   128
#define BATT_TXT_W    40

// Clock  (FONT_36 renders glyphs ~44 px tall on emery)
#define CLOCK_X      MARGIN
#define CLOCK_Y      44
#define CLOCK_W      CONTENT_W
#define CLOCK_H      50

// Cursor block placed after "HH:MM" (~5 chars × ~21 px each = ~105 px)
// Adjust CURSOR_X if the font renders at a different width
#define CURSOR_X    (MARGIN + 108)
#define CURSOR_Y    (CLOCK_Y + 6)
#define CURSOR_W      8
#define CURSOR_H     32

// Log lines (FONT_15)
#define LOG_X       MARGIN
#define LOG_W       CONTENT_W
#define LOG_H        20
#define LOG1_Y      116
#define LOG2_Y      140
#define LOG3_Y      164

// Prompt line
#define PROMPT_Y    196
#define PROMPT_H     20

// ── State ─────────────────────────────────────────────────────────────
static Window *s_win;
static Layer  *s_root;
static GFont   s_f36, s_f15, s_f13;

static char s_time[6]  = "--:--";
static char s_date[24] = "> --- -- ---";
static char s_wx[28]   = "> SYNC...";
static char s_wind[24] = "> WIND: ----";
static int  s_batt     = 100;
static bool s_blink_on = true;

// ── Drawing ───────────────────────────────────────────────────────────
static void draw_battery(GContext *ctx) {
  // Body outline
  graphics_context_set_stroke_color(ctx, C_DIM);
  graphics_draw_round_rect(ctx, GRect(BATT_X, BATT_Y, BATT_BODY_W, BATT_BODY_H), 1);

  // Nub (positive terminal on right)
  graphics_context_set_fill_color(ctx, C_DIM);
  graphics_fill_rect(ctx,
    GRect(BATT_X + BATT_BODY_W, BATT_Y + 2, BATT_NUB_W, BATT_NUB_H),
    0, GCornerNone);

  // Fill proportional to charge (inside the body with 2px inset)
  int inner_w = BATT_BODY_W - 4;
  int fill_w  = (s_batt * inner_w) / 100;
  if (fill_w < 1) fill_w = 1;
  graphics_context_set_fill_color(ctx, C_HOT);
  graphics_fill_rect(ctx,
    GRect(BATT_X + 2, BATT_Y + 2, fill_w, BATT_BODY_H - 4),
    0, GCornerNone);

  // Percentage text to the left of the icon
  static char batt_buf[8];
  snprintf(batt_buf, sizeof(batt_buf), "%d%%", s_batt);
  graphics_context_set_text_color(ctx, C_HOT);
  graphics_draw_text(ctx, batt_buf, s_f13,
    GRect(BATT_TXT_X, STATUS_Y, BATT_TXT_W, STATUS_H),
    GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void draw(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);

  // 1. Background
  graphics_context_set_fill_color(ctx, C_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // 2. Scanlines (CRT effect)
#if SCANLINES
  graphics_context_set_stroke_color(ctx, C_SCAN);
  for (int y = 0; y < b.size.h; y += 4) {
    graphics_draw_line(ctx, GPoint(0, y), GPoint(b.size.w, y));
  }
#endif

  // 3. Status bar — "PEB://TIME2"
  graphics_context_set_text_color(ctx, C_HOT);
  graphics_draw_text(ctx, "PEB://TIME2", s_f13,
    GRect(MARGIN, STATUS_Y, 120, STATUS_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // 4. Battery icon + percentage
  draw_battery(ctx);

  // 5. Divider line
  graphics_context_set_stroke_color(ctx, C_DIM);
  graphics_draw_line(ctx, GPoint(MARGIN, DIVIDER_Y), GPoint(188, DIVIDER_Y));

  // 6. Clock
  graphics_context_set_text_color(ctx, C_HOT);
  graphics_draw_text(ctx, s_time, s_f36,
    GRect(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // 7. Cursor block (blinks)
#if BLINK
  if (s_blink_on) {
    graphics_context_set_fill_color(ctx, C_HOT);
    graphics_fill_rect(ctx,
      GRect(CURSOR_X, CURSOR_Y, CURSOR_W, CURSOR_H),
      0, GCornerNone);
  }
#else
  graphics_context_set_fill_color(ctx, C_HOT);
  graphics_fill_rect(ctx,
    GRect(CURSOR_X, CURSOR_Y, CURSOR_W, CURSOR_H),
    0, GCornerNone);
#endif

  // 8. Log lines (weather, wind, date)
  graphics_context_set_text_color(ctx, C_HOT);
  graphics_draw_text(ctx, s_wx, s_f15,
    GRect(LOG_X, LOG1_Y, LOG_W, LOG_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s_wind, s_f15,
    GRect(LOG_X, LOG2_Y, LOG_W, LOG_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, s_date, s_f15,
    GRect(LOG_X, LOG3_Y, LOG_W, LOG_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // 9. Prompt line
  graphics_draw_text(ctx,
#if BLINK
    s_blink_on ? "> _" : ">  ",
#else
    "> _",
#endif
    s_f15,
    GRect(LOG_X, PROMPT_Y, LOG_W, PROMPT_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// ── Tick handler ──────────────────────────────────────────────────────
static void tick_handler(struct tm *t, TimeUnits u) {
  // Update time string
  strftime(s_time, sizeof(s_time),
    clock_is_24h_style() ? "%H:%M" : "%I:%M", t);

  // Update date string: "WED 24 JUN"
  char tmp[20];
  strftime(tmp, sizeof(tmp), "%a %d %b", t);
  for (int i = 0; tmp[i]; i++) tmp[i] = (char)toupper((unsigned char)tmp[i]);
  snprintf(s_date, sizeof(s_date), "> %s", tmp);

  // Request weather refresh every 30 min
  if (t->tm_min % 30 == 0) {
    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK) {
      dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
      app_message_outbox_send();
    }
  }

  layer_mark_dirty(s_root);
}

// ── Blink timer ───────────────────────────────────────────────────────
#if BLINK
static void blink_cb(void *ctx) {
  s_blink_on = !s_blink_on;
  layer_mark_dirty(s_root);
  app_timer_register(530, blink_cb, NULL);
}
#endif

// ── Battery service ───────────────────────────────────────────────────
static void battery_handler(BatteryChargeState state) {
  s_batt = state.charge_percent;
  layer_mark_dirty(s_root);
}

// ── AppMessage inbox ──────────────────────────────────────────────────
static void inbox_received_callback(DictionaryIterator *it, void *ctx) {
  Tuple *temp  = dict_find(it, MESSAGE_KEY_TEMPERATURE);
  Tuple *cond  = dict_find(it, MESSAGE_KEY_CONDITIONS);
  Tuple *wind  = dict_find(it, MESSAGE_KEY_WIND);

  if (temp && cond) {
    snprintf(s_wx, sizeof(s_wx), "> %s %dF",
             cond->value->cstring, (int)temp->value->int32);
  }
  if (wind) {
    snprintf(s_wind, sizeof(s_wind), "> WIND: %s", wind->value->cstring);
  }
  layer_mark_dirty(s_root);
}

static void inbox_dropped_callback(AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_callback(DictionaryIterator *it,
                                   AppMessageResult reason, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ── Window lifecycle ──────────────────────────────────────────────────
static void window_load(Window *win) {
  Layer *wl = window_get_root_layer(win);
  GRect bounds = layer_get_bounds(wl);

  // Load custom fonts
  s_f36 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_36));
  s_f15 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_15));
  s_f13 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_13));

  // Single root drawing layer
  s_root = layer_create(bounds);
  layer_set_update_proc(s_root, draw);
  layer_add_child(wl, s_root);

  // Seed time + date immediately
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  tick_handler(t, MINUTE_UNIT);

  // Battery seeded via battery_handler callback registered in init()
}

static void window_unload(Window *win) {
  layer_destroy(s_root);
  fonts_unload_custom_font(s_f36);
  fonts_unload_custom_font(s_f15);
  fonts_unload_custom_font(s_f13);
}

// ── App lifecycle ─────────────────────────────────────────────────────
static void init(void) {
  s_win = window_create();
  window_set_background_color(s_win, GColorBlack);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load   = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_win, true);

  // Services
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);

  // AppMessage — register before open
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_open(256, 64);

  // Blink timer
#if BLINK
  app_timer_register(530, blink_cb, NULL);
#endif
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_win);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
