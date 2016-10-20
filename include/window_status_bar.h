#pragma once
#include <pebble.h>
#include "core_status_bar.h"


//-----------//
// Constants //
//-----------//

// Window and Layers
#define STATUS_BAR_WINDOW_WIDTH 144
#define STATUS_BAR_WINDOW_HEIGHT 168
#define CUSTOM_STATUS_BAR_LAYER_HEIGHT 20


// Action Bars
#define STATUS_BAR_WINDOW_ACTION_BAR_ADJUST_HEIGHT ( -CUSTOM_STATUS_BAR_LAYER_HEIGHT )


// Generic status-bar items
#define STATUS_BAR_ITEM_DISTANCE 4
#define STATUS_BAR_ITEM_INTERNAL_DISTANCE 1				//distance between icon and text, inside an item

#define STATUS_BAR_CLOCK_TEXT_DISTANCE_OFFSET ( 0 - STATUS_BAR_ITEM_DISTANCE )
#define STATUS_BAR_AM_PM_TEXT_DISTANCE_OFFSET ( 2 - STATUS_BAR_ITEM_DISTANCE )
#define STATUS_BAR_BATTERY_TEXT_DISTANCE_OFFSET ( 2 - STATUS_BAR_ITEM_DISTANCE)
#define STATUS_BAR_BORDER_DISTANCE_OFFSET ( 3 - STATUS_BAR_ITEM_DISTANCE)


#define STATUS_BAR_ICON_RECOMMENDED_WIDTH 11
#define STATUS_BAR_TEXT_ADJUST_Y (-4)
#define STATUS_BAR_TEXT_WIDTH_MAX ( \
	STATUS_BAR_WINDOW_WIDTH \
	- 2 * STATUS_BAR_ICON_RECOMMENDED_WIDTH \
	- 4 * STATUS_BAR_ITEM_DISTANCE \
	- 2 * STATUS_BAR_BORDER_DISTANCE_OFFSET \
)


//Battery Regions
#define STATUS_BAR_BATTERY_CHARGE_MAX 100
#define STATUS_BAR_BATTERY_CHARGE_THRESHOLD 30

#define STATUS_BAR_WATCH_FULL_MISSING_PERCENT 20
#define STATUS_BAR_WATCH_EMPTY_MISSING_PERCENT 100
#define STATUS_BAR_WATCH_BATTERY_X 3
#define STATUS_BAR_WATCH_BATTERY_Y 5

#define STATUS_BAR_PHONE_FULL_MISSING_PERCENT 0
#define STATUS_BAR_PHONE_EMPTY_MISSING_PERCENT 85
#define STATUS_BAR_PHONE_BATTERY_X 3
#define STATUS_BAR_PHONE_BATTERY_Y 3


// Text buffers
#define STATUS_BAR_TIME_TEXT_BUFFER_SIZE 6			//"mm:ss"
#define STATUS_BAR_TIME_SUFFIX_TEXT_BUFFER_SIZE 3	//"PM"
#define STATUS_BAR_BATTERY_TEXT_BUFFER_SIZE 4		//"100"


// Service Handlers
#define STATUS_BAR_WINDOW_TICK_UNITS ( MINUTE_UNIT | HOUR_UNIT )


// Colors and Image Compositing Modes
#define STATUS_BAR_WINDOW_COLOR_BACKGROUND GColorBlack
#define STATUS_BAR_WINDOW_COLOR_FOREGROUND GColorWhite

#define STATUS_BAR_COMP_OP_NORMAL GCompOpSet			//image black -> screen white
#define STATUS_BAR_COMP_OP_INVERTED GCompOpOr			//image white -> screen white


//------------//
// Data Types //
//------------//

//status bar window layouts, layout-items, and layout-item parts
typedef struct status_bar_window_layout_item_parts_s {
	int8_t distance_offset;
	
	char *text;
	GFont text_font;
	
	GBitmap *icon;
	
	BatteryChargeState *battery_state;
	int battery_full_missing_percent;	//how much charge% can be missing, and still show a full battery icon
	int battery_empty_missing_percent;	//how much charge% needs to be missing, for battery icon to show as empty
	GPoint battery_icon_origin;
} status_bar_window_layout_item_parts_t;

typedef struct status_bar_window_layout_item_s status_bar_window_layout_item_t;
typedef struct status_bar_window_layout_s status_bar_window_layout_t;
	
//status bar windows themselves, and the data globally shared between them
typedef struct status_bar_window_s status_bar_window_t;
typedef struct status_bar_window_globals_s status_bar_window_globals_t;

	

//--------------------------------//
// Status Bar Window Layout Items //
//--------------------------------//
	
status_bar_window_layout_item_t *status_bar_window_layout_item_create(
	GTextAlignment alignment,
	status_bar_border_distance_t distance,
	status_bar_window_layout_item_parts_t item_parts
);
void status_bar_window_layout_item_destroy( status_bar_window_layout_item_t *item );
void status_bar_window_layout_item_destroy_recursive( status_bar_window_layout_item_t *item );

int8_t status_bar_window_layout_item_render_icon( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x );
int8_t status_bar_window_layout_item_render_text( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x );
int8_t status_bar_window_layout_item_render( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x );


//--------------------------//
// Status Bar Window Layout //
//--------------------------//

status_bar_window_layout_t *status_bar_window_layout_create(void);
void status_bar_window_layout_destroy( status_bar_window_layout_t *status_bar_window_layout );

bool status_bar_window_layout_add_item(		//returns true if successfully added, false if item wouldn't fit
	status_bar_window_layout_t *status_bar_window_layout,
	GTextAlignment alignment,
	status_bar_border_distance_t distance,
	status_bar_window_layout_item_parts_t item_parts
);

void status_bar_window_mark_layout_dirty( status_bar_window_t *status_bar_window );
void status_bar_window_build_layout( status_bar_window_t *status_bar_window );


//----------------------------//
//     Status Bar Window      //
// Constructor and Destructor //
//----------------------------//

status_bar_window_t *status_bar_window_create( bool hide_time );
void status_bar_window_destroy( status_bar_window_t *status_bar_window );


//---------------------//
// Getters and Setters //
//---------------------//

status_bar_window_t *get_current_status_bar_window(void);
status_bar_window_t *window_get_status_bar_window( Window *window );

Window *status_bar_window_get_window(status_bar_window_t *status_bar_window );
Layer *status_bar_window_get_status_bar_layer(status_bar_window_t *status_bar_window );
Layer *status_bar_window_get_body_layer(status_bar_window_t *status_bar_window );


//----------------------------------------//
// Replacements for core pebble functions //
//----------------------------------------//

void *status_bar_window_get_user_data(status_bar_window_t *status_bar_window );
void status_bar_window_set_user_data(status_bar_window_t *status_bar_window, void *user_data );

void status_bar_window_set_window_handlers( status_bar_window_t *status_bar_window, WindowHandlers handlers );

void status_bar_window_tick_timer_service_subscribe( status_bar_window_t *status_bar_window, TimeUnits tick_units, TickHandler handler);
void status_bar_window_tick_timer_service_unsubscribe( status_bar_window_t *status_bar_window );

void status_bar_window_connection_service_subscribe(ConnectionHandlers handlers);
void status_bar_window_connection_service_unsubscribe(void);

void status_bar_window_battery_state_service_subscribe(BatteryStateHandler handler);
void status_bar_window_battery_state_service_unsubscribe(void);
	
