#include <pebble.h>
#include <ctype.h>

// ── Colour palette ───────────────────────────────────────────────────
#define C_BG GColorBlack   // background never changes
static GColor s_c_hot;     // primary phosphor (theme-dependent)
static GColor s_c_dim;     // dim phosphor / labels
static GColor s_c_scan;    // scanlines

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
#define BATT_X      (188 - BATT_BODY_W)   // = 170
#define BATT_Y       10

// Percentage text sits to the left of the battery icon
#define BATT_TXT_X   128
#define BATT_TXT_W    40

// Clock  (FONT_48 renders glyphs ~58 px tall on emery)
#define CLOCK_X      MARGIN
#define CLOCK_Y      36
#define CLOCK_W      CONTENT_W
#define CLOCK_H      58

// Cursor block placed after "HH:MM" (~5 chars × ~30 px each = ~150 px)
#define CURSOR_X    (MARGIN + 150)
#define CURSOR_Y    (CLOCK_Y + 6)
#define CURSOR_W     10
#define CURSOR_H     42

// Log lines (FONT_18)
#define LOG_X       MARGIN
#define LOG_W       CONTENT_W
#define LOG_H        24
#define LOG1_Y      106
#define LOG2_Y      132
#define LOG3_Y      158

// Prompt line
#define PROMPT_Y    186
#define PROMPT_H     24

// ── Animation ─────────────────────────────────────────────────────────
#define ANIM_INTERVAL_MS   50
#define SCROLL_STEP        44
#define TYPE_STEP           2

typedef enum { ANIM_IDLE, ANIM_SCROLL_OUT, ANIM_TYPE_IN } AnimPhase;

// ── Settings ──────────────────────────────────────────────────────────
#define SETTINGS_KEY 1

typedef struct {
  bool   scanlines;
  bool   clock_cursor;
  bool   prompt_cursor;
  int8_t color_scheme;  // 0=green, 1=amber, 2=cyan, 3=white
} ClaySettings;

static ClaySettings s_settings;

// ── State ─────────────────────────────────────────────────────────────
static Window *s_win;
static Layer  *s_root;
static GFont   s_f48, s_f18, s_f13;

static char s_time[6]  = "--:--";
static char s_date[24] = "> --- -- ---";
static char s_wx[28]   = "> SYNC...";
static char s_wind[24] = "> WIND: ----";
static int  s_batt     = 100;
static bool s_blink_on = true;

static AnimPhase   s_anim_phase  = ANIM_IDLE;
static int         s_scroll_off  = 0;
static int         s_type_chars  = 0;
static int         s_type_total  = 0;
static AppTimer   *s_anim_timer  = NULL;

// Pending buffers (hold next values until scroll completes)
static char s_next_time[6];
static char s_next_date[24];

// ── Settings functions ────────────────────────────────────────────────
static void prv_apply_settings(void) {
  switch (s_settings.color_scheme) {
    case 1:  // Amber phosphor
      s_c_hot  = GColorOrange;
      s_c_dim  = GColorWindsorTan;
      s_c_scan = GColorBulgarianRose;
      break;
    case 2:  // Cyan
      s_c_hot  = GColorCyan;
      s_c_dim  = GColorTiffanyBlue;
      s_c_scan = GColorMidnightGreen;
      break;
    case 3:  // White
      s_c_hot  = GColorWhite;
      s_c_dim  = GColorLightGray;
      s_c_scan = GColorDarkGray;
      break;
    default:  // 0 = Green phosphor (default)
      s_c_hot  = GColorMediumAquamarine;
      s_c_dim  = GColorIslamicGreen;
      s_c_scan = GColorDarkGreen;
      break;
  }
}

static void prv_default_settings(void) {
  s_settings.scanlines     = true;
  s_settings.clock_cursor  = true;
  s_settings.prompt_cursor = true;
  s_settings.color_scheme  = 0;
}

static void prv_load_settings(void) {
  prv_default_settings();
  persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
  prv_apply_settings();
}

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

// ── Drawing helpers ───────────────────────────────────────────────────
static void draw_partial(GContext *ctx, const char *str, GFont font,
                         GRect rect, int reveal) {
  if (reveal <= 0) return;
  static char buf[32];
  int len = (int)strlen(str);
  int n = reveal < len ? reveal : len;
  // Advance past any UTF-8 continuation bytes at the cut point
  while (n < len && (str[n] & 0xC0) == 0x80) n++;
  strncpy(buf, str, n);
  buf[n] = '\0';
  graphics_draw_text(ctx, buf, font, rect,
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void draw_battery(GContext *ctx) {
  // Body outline
  graphics_context_set_stroke_color(ctx, s_c_dim);
  graphics_draw_round_rect(ctx, GRect(BATT_X, BATT_Y, BATT_BODY_W, BATT_BODY_H), 1);

  // Nub (positive terminal on right)
  graphics_context_set_fill_color(ctx, s_c_dim);
  graphics_fill_rect(ctx,
    GRect(BATT_X + BATT_BODY_W, BATT_Y + 2, BATT_NUB_W, BATT_NUB_H),
    0, GCornerNone);

  // Fill proportional to charge (inside the body with 2px inset)
  int inner_w = BATT_BODY_W - 4;
  int fill_w  = (s_batt * inner_w) / 100;
  if (fill_w < 1) fill_w = 1;
  graphics_context_set_fill_color(ctx, s_c_hot);
  graphics_fill_rect(ctx,
    GRect(BATT_X + 2, BATT_Y + 2, fill_w, BATT_BODY_H - 4),
    0, GCornerNone);

  // Percentage text to the left of the icon
  static char batt_buf[8];
  snprintf(batt_buf, sizeof(batt_buf), "%d%%", s_batt);
  graphics_context_set_text_color(ctx, s_c_hot);
  graphics_draw_text(ctx, batt_buf, s_f13,
    GRect(BATT_TXT_X, STATUS_Y, BATT_TXT_W, STATUS_H),
    GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void draw(Layer *l, GContext *ctx) {
  GRect b = layer_get_bounds(l);

  // 1. Background
  graphics_context_set_fill_color(ctx, C_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // 2. Scanlines (CRT effect — runtime toggle)
  if (s_settings.scanlines) {
    graphics_context_set_stroke_color(ctx, s_c_scan);
    for (int y = 0; y < b.size.h; y += 4)
      graphics_draw_line(ctx, GPoint(0, y), GPoint(b.size.w, y));
  }

  // 3. Status bar — "PEB://TIME2"
  graphics_context_set_text_color(ctx, s_c_hot);
  graphics_draw_text(ctx, "PEB://TIME2", s_f13,
    GRect(MARGIN, STATUS_Y, 120, STATUS_H),
    GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // 4. Battery icon + percentage
  draw_battery(ctx);

  // 5. Divider line
  graphics_context_set_stroke_color(ctx, s_c_dim);
  graphics_draw_line(ctx, GPoint(MARGIN, DIVIDER_Y), GPoint(188, DIVIDER_Y));

  // 6. Content — phase-aware
  graphics_context_set_text_color(ctx, s_c_hot);

  if (s_anim_phase == ANIM_SCROLL_OUT) {
    // Scroll existing content upward off-screen
    int off = s_scroll_off;
    if (CLOCK_Y - off >= DIVIDER_Y)
      graphics_draw_text(ctx, s_time, s_f48,
        GRect(CLOCK_X, CLOCK_Y - off, CLOCK_W, CLOCK_H),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    if (LOG1_Y - off >= DIVIDER_Y)
      graphics_draw_text(ctx, s_wx, s_f18,
        GRect(LOG_X, LOG1_Y - off, LOG_W, LOG_H),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    if (LOG2_Y - off >= DIVIDER_Y)
      graphics_draw_text(ctx, s_wind, s_f18,
        GRect(LOG_X, LOG2_Y - off, LOG_W, LOG_H),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    if (LOG3_Y - off >= DIVIDER_Y)
      graphics_draw_text(ctx, s_date, s_f18,
        GRect(LOG_X, LOG3_Y - off, LOG_W, LOG_H),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    if (PROMPT_Y - off >= DIVIDER_Y)
      graphics_draw_text(ctx, "> _", s_f18,
        GRect(LOG_X, PROMPT_Y - off, LOG_W, PROMPT_H),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    // No cursor during scroll-out

  } else if (s_anim_phase == ANIM_TYPE_IN) {
    // Reveal new content character by character
    int rem = s_type_chars;
    draw_partial(ctx, s_time,  s_f48,
                 GRect(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H), rem);
    rem -= (int)strlen(s_time);
    draw_partial(ctx, s_wx,   s_f18,
                 GRect(LOG_X, LOG1_Y, LOG_W, LOG_H), rem);
    rem -= (int)strlen(s_wx);
    draw_partial(ctx, s_wind, s_f18,
                 GRect(LOG_X, LOG2_Y, LOG_W, LOG_H), rem);
    rem -= (int)strlen(s_wind);
    draw_partial(ctx, s_date, s_f18,
                 GRect(LOG_X, LOG3_Y, LOG_W, LOG_H), rem);
    rem -= (int)strlen(s_date);
    draw_partial(ctx, "> _",  s_f18,
                 GRect(LOG_X, PROMPT_Y, LOG_W, PROMPT_H), rem);
    // No cursor during type-in

  } else {
    // ANIM_IDLE — normal drawing
    graphics_draw_text(ctx, s_time, s_f48,
      GRect(CLOCK_X, CLOCK_Y, CLOCK_W, CLOCK_H),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    // Clock cursor (runtime toggle + blink)
    if (s_settings.clock_cursor && s_blink_on) {
      graphics_context_set_fill_color(ctx, s_c_hot);
      graphics_fill_rect(ctx,
        GRect(CURSOR_X, CURSOR_Y, CURSOR_W, CURSOR_H),
        0, GCornerNone);
    }

    graphics_context_set_text_color(ctx, s_c_hot);
    graphics_draw_text(ctx, s_wx, s_f18,
      GRect(LOG_X, LOG1_Y, LOG_W, LOG_H),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_wind, s_f18,
      GRect(LOG_X, LOG2_Y, LOG_W, LOG_H),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, s_date, s_f18,
      GRect(LOG_X, LOG3_Y, LOG_W, LOG_H),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    // Prompt cursor (runtime toggle + blink)
    const char *prompt = (s_settings.prompt_cursor && s_blink_on) ? "> _" : ">  ";
    graphics_draw_text(ctx, prompt, s_f18,
      GRect(LOG_X, PROMPT_Y, LOG_W, PROMPT_H),
      GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

// ── Animation callback ─────────────────────────────────────────────────
static void anim_cb(void *ctx) {
  s_anim_timer = NULL;

  if (s_anim_phase == ANIM_SCROLL_OUT) {
    s_scroll_off += SCROLL_STEP;
    if (s_scroll_off >= 200) {
      // Copy pending buffers into active buffers
      strncpy(s_time, s_next_time, sizeof(s_time));
      s_time[sizeof(s_time) - 1] = '\0';
      strncpy(s_date, s_next_date, sizeof(s_date));
      s_date[sizeof(s_date) - 1] = '\0';
      // Compute total chars to reveal (s_wx and s_wind updated live from inbox)
      s_type_total = (int)strlen(s_time) + (int)strlen(s_wx) +
                     (int)strlen(s_wind) + (int)strlen(s_date) + 3; // "> _"
      s_type_chars = 0;
      s_anim_phase = ANIM_TYPE_IN;
    }
    layer_mark_dirty(s_root);
    s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_cb, NULL);

  } else if (s_anim_phase == ANIM_TYPE_IN) {
    s_type_chars += TYPE_STEP;
    if (s_type_chars >= s_type_total) {
      s_type_chars = s_type_total;
      s_anim_phase = ANIM_IDLE;
      layer_mark_dirty(s_root);
      // Don't re-register — animation complete
    } else {
      layer_mark_dirty(s_root);
      s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_cb, NULL);
    }
  }
}

// ── Tick handler ──────────────────────────────────────────────────────
static void tick_handler(struct tm *t, TimeUnits u) {
  // Write new values into pending buffers
  strftime(s_next_time, sizeof(s_next_time),
    clock_is_24h_style() ? "%H:%M" : "%I:%M", t);

  char tmp[20];
  strftime(tmp, sizeof(tmp), "%a %d %b", t);
  for (int i = 0; tmp[i]; i++) tmp[i] = (char)toupper((unsigned char)tmp[i]);
  snprintf(s_next_date, sizeof(s_next_date), "> %s", tmp);

  // Request weather refresh every 30 min
  if (t->tm_min % 30 == 0) {
    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK) {
      dict_write_uint8(iter, MESSAGE_KEY_REQUEST_WEATHER, 1);
      app_message_outbox_send();
    }
  }

  // Cancel any in-progress animation and start scroll-out
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  s_anim_phase = ANIM_SCROLL_OUT;
  s_scroll_off = 0;
  s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, anim_cb, NULL);
}

// ── Blink timer ───────────────────────────────────────────────────────
static void blink_cb(void *ctx) {
  s_blink_on = !s_blink_on;
  layer_mark_dirty(s_root);
  app_timer_register(530, blink_cb, NULL);
}

// ── Battery service ───────────────────────────────────────────────────
static void battery_handler(BatteryChargeState state) {
  s_batt = state.charge_percent;
  layer_mark_dirty(s_root);
}

// ── AppMessage inbox ──────────────────────────────────────────────────
static void inbox_received_callback(DictionaryIterator *it, void *ctx) {
  // Weather
  Tuple *temp  = dict_find(it, MESSAGE_KEY_TEMPERATURE);
  Tuple *cond  = dict_find(it, MESSAGE_KEY_CONDITIONS);
  Tuple *wind  = dict_find(it, MESSAGE_KEY_WIND);

  if (temp && cond) {
    snprintf(s_wx, sizeof(s_wx), "> %d\xc2\xb0""F %s",
             (int)temp->value->int32, cond->value->cstring);
  }
  if (wind) {
    snprintf(s_wind, sizeof(s_wind), "> WIND: %s", wind->value->cstring);
  }

  // Settings
  Tuple *t_scanlines     = dict_find(it, MESSAGE_KEY_SETTINGS_SCANLINES);
  Tuple *t_clock_cursor  = dict_find(it, MESSAGE_KEY_SETTINGS_CLOCK_CURSOR);
  Tuple *t_prompt_cursor = dict_find(it, MESSAGE_KEY_SETTINGS_PROMPT_CURSOR);
  Tuple *t_color_scheme  = dict_find(it, MESSAGE_KEY_SETTINGS_COLOR_SCHEME);

  bool settings_changed = false;
  if (t_scanlines)     { s_settings.scanlines     = t_scanlines->value->int32 != 0;       settings_changed = true; }
  if (t_clock_cursor)  { s_settings.clock_cursor  = t_clock_cursor->value->int32 != 0;    settings_changed = true; }
  if (t_prompt_cursor) { s_settings.prompt_cursor = t_prompt_cursor->value->int32 != 0;   settings_changed = true; }
  if (t_color_scheme)  { s_settings.color_scheme  = (int8_t)t_color_scheme->value->int32; settings_changed = true; }

  if (settings_changed) {
    prv_apply_settings();
    prv_save_settings();
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
  prv_load_settings();
  s_batt = (int)battery_state_service_peek().charge_percent;

  Layer *wl = window_get_root_layer(win);
  GRect bounds = layer_get_bounds(wl);

  // Load custom fonts
  s_f48 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_48));
  s_f18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_18));
  s_f13 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TERMINAL_13));

  // Single root drawing layer
  s_root = layer_create(bounds);
  layer_set_update_proc(s_root, draw);
  layer_add_child(wl, s_root);

  // Seed time + date immediately (direct write — no animation on first load)
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  strftime(s_time, sizeof(s_time),
    clock_is_24h_style() ? "%H:%M" : "%I:%M", t);
  char tmp[20];
  strftime(tmp, sizeof(tmp), "%a %d %b", t);
  for (int i = 0; tmp[i]; i++) tmp[i] = (char)toupper((unsigned char)tmp[i]);
  snprintf(s_date, sizeof(s_date), "> %s", tmp);
  layer_mark_dirty(s_root);
}

static void window_unload(Window *win) {
  if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
  layer_destroy(s_root);
  fonts_unload_custom_font(s_f48);
  fonts_unload_custom_font(s_f18);
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
  app_message_open(512, 64);

  // Blink timer
  app_timer_register(530, blink_cb, NULL);
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
