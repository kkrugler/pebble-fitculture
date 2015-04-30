#include <pebble.h>
#include "running.h"

typedef enum {
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_DONE
} RunningState;

// Inter-station gap needed for half-way calculation
static const int SECONDS_PER_STATION_SWITCH = 7;

// Max length of any integer value when converted to a string
static const int MAX_VALUE_LEN = 3;

static const bool ANIMATED = true;

// How long to display status ("HALF", "NEXT")
static const int STATUS_DURATION = 2;

static Window *sWindow;
static TextLayer *text_layer_circuit;
static TextLayer *text_layer_station;
static TextLayer *text_layer_timeleft;
static TextLayer *text_layer_status;

static int sNumCircuits;
static int sCurCircuit;

static int sNumStations;
static int sCurStation;

// Base number of seconds per circuit
static int sSecondsPerCircuit;

// Number of seconds per each station (dynamic)
static int sSecondsPerStation;

// Remaining seconds for the current station
static int sSecondsForCurStation;

static bool sStatusSet = false;
static time_t sStatusStartTime;

static RunningState sState = STATE_RUNNING;

static time_t sEndTime;

static char *sWallTime = NULL;

static char *get_time(void) {
    if (sWallTime == NULL) {
        sWallTime = (char *)malloc(40);
    }
    
    clock_copy_time_string(sWallTime, 40);
    
    // Trim off trailing " am" or " pm" from string
    if (!clock_is_24h_style()) {
        sWallTime[strlen(sWallTime) - 3] = '\0';
    }
    
    return sWallTime;
}

static void handle_select(ClickRecognizerRef recognizer, void *context) {
    if (sState == STATE_RUNNING) {
        sState = STATE_PAUSED;
        text_layer_set_text(text_layer_status, "PAUSE");
    } else if (sState == STATE_PAUSED) {
        sState = STATE_RUNNING;
        text_layer_set_text(text_layer_status, get_time());
    } else {
        // TODO beep to indicate you can't pause when done?
    }
}

static void handle_up(ClickRecognizerRef recognizer, void *context) {
}

static void handle_down(ClickRecognizerRef recognizer, void *context) {
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, handle_select);
  window_single_click_subscribe(BUTTON_ID_UP, handle_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, handle_down);
}

static int getStationTime(void) {
    if (sCurCircuit == 1) {
        // Since we're in the first circuit, add 10% to the target time
        int secondsPerCircuit = (sSecondsPerCircuit * 11) / 10;
        return secondsPerCircuit / sNumStations;
    } else if (sCurCircuit == 2) {
        return sSecondsPerCircuit / sNumStations;
    } else {
        // Calculate exactly how much time is needed for each of the remaining stations
        // This adjusts for any rounding errors w/integer math in previous durations.
        time_t curTime = time(NULL);
        time_t remainingTime = sEndTime - curTime;
        int remainingStations = (sNumStations - sCurStation) + 1;
        return remainingTime / remainingStations;
    }
}

static TextLayer *makeTextLayer(Layer *window_layer, int top, const char *label, int value) {
  TextLayer *labelLayer = text_layer_create((GRect) { .origin = { 0, top }, .size = { 85, 30 } });
  text_layer_set_font(labelLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(labelLayer, GTextAlignmentRight);
  text_layer_set_text(labelLayer, label);
  layer_add_child(window_layer, text_layer_get_layer(labelLayer));
  
  TextLayer *valueLayer = text_layer_create((GRect) { .origin = {90, top}, .size = {54, 30 } });
  text_layer_set_font(valueLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(valueLayer, GTextAlignmentLeft);
  
  char *valueStr = (char *)malloc(MAX_VALUE_LEN + 1);
  snprintf(valueStr, MAX_VALUE_LEN + 1, "%d", value);
  text_layer_set_text(valueLayer, valueStr);
  layer_add_child(window_layer, text_layer_get_layer(valueLayer));

  return valueLayer;
}

static void update_value(TextLayer *layer, int value, char* suffix) {
    char* valueStr = (char *)text_layer_get_text(layer);
    snprintf(valueStr, MAX_VALUE_LEN + 1, "%d%s", value, suffix);
    text_layer_set_text(layer, valueStr);
}

static void update_status(void) {
    if (!sStatusSet) {
        return;
    }
    
    // See if we're past the time when status should be shown
    time_t delta = time(NULL) - sStatusStartTime;
    if (delta > STATUS_DURATION) {
        sStatusSet = false;
    }
}

static void set_status(char *status) {
    sStatusSet = true;
    sStatusStartTime = time(NULL);
    text_layer_set_text(text_layer_status, status);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    if (sState == STATE_DONE) {
        return;
    } else if (sState == STATE_PAUSED) {
        vibes_short_pulse();
        return;
    }
    
    update_status();
    
    sSecondsForCurStation -= 1;
  
    if (sSecondsForCurStation == 0) {
        set_status("NEXT");
        
        vibes_long_pulse();
    
        sCurStation += 1;
        if (sCurStation > sNumStations) {
            sCurStation = 1;
            sCurCircuit += 1;
            
            if (sCurCircuit > sNumCircuits) {
                set_status("DONE");

                vibes_long_pulse();
                vibes_long_pulse();
                vibes_long_pulse();
                
                sState = STATE_DONE;
                return;
            }
        }
    
        sSecondsPerStation = getStationTime();
        sSecondsForCurStation = sSecondsPerStation;
        
        update_value(text_layer_station, sCurStation, "");
        update_value(text_layer_circuit, sCurCircuit, "");
    } else {
        int halfway = (sSecondsPerStation - SECONDS_PER_STATION_SWITCH) / 2;
        if (sSecondsForCurStation == halfway) {
            set_status("HALF");
            vibes_short_pulse();
        }
    }
  
    update_value(text_layer_timeleft, sSecondsForCurStation, "s");
    
    if (!sStatusSet) {
        text_layer_set_text(text_layer_status, get_time());
    }
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
  
    text_layer_circuit = makeTextLayer(window_layer, 5, "Circuit:", sCurCircuit);
    text_layer_station = makeTextLayer(window_layer, 35, "Station:", sCurStation);
    text_layer_timeleft = makeTextLayer(window_layer, 65, "Time:", sSecondsForCurStation);
  
    text_layer_status = text_layer_create((GRect) { .origin = { 0, 100 }, .size = { 144, 60 } });
    text_layer_set_font(text_layer_status, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(text_layer_status, GTextAlignmentCenter);
    
    // TODO install an update_proc drawing routine for the window_layer, and use that to
    // draw a horizontal dividing line.
    
    text_layer_set_text(text_layer_status, get_time());
    layer_add_child(window_layer, text_layer_get_layer(text_layer_status));
        
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void window_unload(Window *window) {
    // TODO this won't destroy the label text layers that we created.
    text_layer_destroy(text_layer_circuit);
    text_layer_destroy(text_layer_station);
    text_layer_destroy(text_layer_timeleft);
    
    text_layer_destroy(text_layer_status);
    
    // TODO this API doesn't make sense to me, as you don't pass in the handler
    // to be unsubscribed.
    tick_timer_service_unsubscribe();
}

void init_running_window(int numCircuits, int numStations, time_t endTime) {
    sNumCircuits = numCircuits;
    sCurCircuit = 1;
      
    sNumStations = numStations;
    sCurStation = 1;
      
    sEndTime = endTime;

    time_t curTime = time(NULL);
    time_t remainingTime = sEndTime - curTime;

    // Calculate how long each circuit should last
    sSecondsPerCircuit = remainingTime / numCircuits;
    
    // Now calculate how long the current station should last
    sSecondsPerStation = getStationTime();
    sSecondsForCurStation = sSecondsPerStation;
    
    sWindow = window_create();
    window_set_click_config_provider(sWindow, click_config_provider);
    window_set_window_handlers(sWindow, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
      
    window_stack_push(sWindow, ANIMATED);
}

