#include <pebble.h>
#include "running.h"

static int MAX_LABEL_LEN = 50;

static Window *sWindow;
static TextLayer *text_layer_about;
static TextLayer *text_layer_stations;
static int sNumStations = 16;
static char *sStationText;

static void set_stations() {
    snprintf(sStationText, MAX_LABEL_LEN + 1, "Stations: %d", sNumStations);
    text_layer_set_text(text_layer_stations, sStationText);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Figure out the end time, by making it the next hour from where we are
  time_t curTime = time(NULL);
  int curHour = curTime / 3600;
  int nextHour = curHour + 1;
  time_t nextTime = nextHour * 3600;
  
  init_running_window(3, sNumStations, nextTime);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (sNumStations > 1) {
    sNumStations -= 1;
    set_stations();
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (sNumStations < 99) {
      sNumStations += 1;
      set_stations();
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
  
    text_layer_about = text_layer_create((GRect) { .origin = { 0, 10 }, .size = { 144, 80 } });
    text_layer_set_font(text_layer_about, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text(text_layer_about, "FitCulture Flow Timer");
    text_layer_set_text_alignment(text_layer_about, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(text_layer_about));

    sStationText = (char *)malloc(MAX_LABEL_LEN + 1);
  
    text_layer_stations = text_layer_create((GRect) { .origin = { 10, 90 }, .size = { 130, 30 } });
    text_layer_set_font(text_layer_stations, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    set_stations();
    text_layer_set_text_alignment(text_layer_stations, GTextAlignmentLeft);
    layer_add_child(window_layer, text_layer_get_layer(text_layer_stations));
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer_stations);
    text_layer_destroy(text_layer_about);
}

void init_settings_window(void) {
  sWindow = window_create();
  window_set_click_config_provider(sWindow, click_config_provider);
  window_set_window_handlers(sWindow, (WindowHandlers) {
	  .load = window_load,
    .unload = window_unload,
  });
  
  const bool animated = true;
  window_stack_push(sWindow, animated);
}

void deinit_settings_window(void) {
  window_destroy(sWindow);
}

