#include <pebble.h>
#include "include/core_status_bar.h"
#include "include/window_status_bar.h"


//------------//
// Data Types //
//------------//

//status bar window layouts and layout-items
struct status_bar_window_layout_item_s {
	GTextAlignment alignment;
	status_bar_border_distance_t distance;
	
	status_bar_window_layout_item_parts_t parts;
	
	uint8_t width;
	status_bar_window_layout_item_t *next;
};

struct status_bar_window_layout_s {
	uint8_t left_width;
	uint8_t center_width;
	uint8_t right_width;
	
	status_bar_border_distance_t left_max_distance;
	status_bar_border_distance_t center_max_distance;
	status_bar_border_distance_t right_max_distance;	
	
	status_bar_window_layout_item_t *left_first;		//leftmost
	status_bar_window_layout_item_t **left_last_next_ptr;
		
	status_bar_window_layout_item_t *center_first;		//leftmost
	status_bar_window_layout_item_t **center_last_next_ptr;
	
	status_bar_window_layout_item_t *right_first;	//rightmost
	status_bar_window_layout_item_t **right_last_next_ptr;
};


//status bar windows themselves, and the data globally shared between them
struct status_bar_window_s {
	//internal window and handlers
	Window *window;
	WindowHandlers window_handlers;
	
	//internal layers
	Layer *layer_status_bar;
	Layer *layer_body;
	
	//stored values for current state
	bool hide_time;
	status_bar_window_layout_t *layout;
	
	//pointer to more data, in case some other window type is built on top of status_bar_window
	void *user_data;
	
};

struct status_bar_window_globals_s {
	//existing windows
	size_t num_windows;
	status_bar_window_t *current_window;
	
	//resources and fonts
	GBitmap *res_icon_phone;
	GBitmap *res_icon_battery;
	GBitmap *res_icon_charging;
	GBitmap *res_icon_charging_half;
	
	GFont res_gothic_18_bold;
	GFont res_gothic_14;
	
	//current system status
	char curr_time_text_buffer[STATUS_BAR_TIME_TEXT_BUFFER_SIZE];
	char curr_time_suffix_text_buffer[STATUS_BAR_TIME_SUFFIX_TEXT_BUFFER_SIZE];
	char watch_battery_text_buffer[STATUS_BAR_BATTERY_TEXT_BUFFER_SIZE];
	bool is_connected_to_phone;
	BatteryChargeState watch_battery_state;
	
	//service callback handlers
	TimeUnits tick_units;
	TickHandler tick_handler;
	BatteryStateHandler battery_handler;
	ConnectionHandlers connection_handlers;
};


//-------------//
// Static vars //
//-------------//

static status_bar_window_globals_t *s_status_bar_window_globals = NULL;



//--------------------------------//
// Status Bar Window Layout Items //
//--------------------------------//

status_bar_window_layout_item_t *status_bar_window_layout_item_create(
	GTextAlignment alignment,
	status_bar_border_distance_t distance,
	status_bar_window_layout_item_parts_t item_parts
){
	status_bar_window_layout_item_t *item = malloc( sizeof(*item) );
	
	item->alignment = alignment;
	item->distance = distance;
	item->parts = item_parts;
	item->next = NULL;
	item->width = STATUS_BAR_ITEM_DISTANCE + item->parts.distance_offset;
	
	//find icon width, if any
	if( NULL != item_parts.icon ){
		GRect bounds = gbitmap_get_bounds(item_parts.icon);
		item->width += bounds.size.w;
	}
	
	//find text width, if any
	if( NULL != item_parts.text && NULL != item_parts.text_font ){
		if( NULL != item_parts.icon ){
			item->width += STATUS_BAR_ITEM_INTERNAL_DISTANCE;
		}
		
		GSize text_size = graphics_text_layout_get_content_size(
			item_parts.text,
			item_parts.text_font,
			GRect(0, 0, STATUS_BAR_TEXT_WIDTH_MAX, CUSTOM_STATUS_BAR_LAYER_HEIGHT),
			GTextOverflowModeTrailingEllipsis,
			alignment
		);
		item->width += text_size.w;
	}
	
	return item;
}

void status_bar_window_layout_item_destroy( status_bar_window_layout_item_t *item ){
	free( item );
}

void status_bar_window_layout_item_destroy_recursive( status_bar_window_layout_item_t *item ){
	if( NULL != item ){	
		status_bar_window_layout_item_destroy_recursive( item->next );
		status_bar_window_layout_item_destroy( item );
	}
}


//returns the width, in pixels, of the rendered icon
int8_t status_bar_window_layout_item_render_icon( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x ){
	if( NULL == item->parts.icon ){										//if there's no icon, do nothing
		return 0;
	}
	
	int internal_distance = 0;
	if( NULL != item->parts.text && NULL != item->parts.text_font ){	//add internal offset between text and icon, if needed
		internal_distance = STATUS_BAR_ITEM_INTERNAL_DISTANCE;
	}
	
	GRect bounds = gbitmap_get_bounds(item->parts.icon);

	int icon_x = offset_x;
	int icon_y = ( CUSTOM_STATUS_BAR_LAYER_HEIGHT - bounds.size.h + 1 ) / 2;		//( the "+1" makes it round up )

	if( item->alignment == GTextAlignmentRight){
		icon_x = STATUS_BAR_WINDOW_WIDTH - icon_x - bounds.size.w - internal_distance;
	}

	graphics_context_set_compositing_mode( ctx, STATUS_BAR_COMP_OP_NORMAL );
	graphics_draw_bitmap_in_rect( 
		ctx, item->parts.icon,
		GRect(
			icon_x,
			icon_y,
			bounds.size.w,
			bounds.size.h
		)
	);

	if( NULL != item->parts.battery_state ){
		GRect battery_icon_bounds = gbitmap_get_bounds( s_status_bar_window_globals->res_icon_charging );
		battery_icon_bounds.origin.x += icon_x + item->parts.battery_icon_origin.x;
		battery_icon_bounds.origin.y += icon_y + item->parts.battery_icon_origin.y;

		int missing_charge_percent = ( STATUS_BAR_BATTERY_CHARGE_MAX - item->parts.battery_state->charge_percent );

		if( item->parts.battery_state->is_charging ){		// || item->parts.battery_state->is_plugged ){
			if( item->parts.battery_state->charge_percent <= STATUS_BAR_BATTERY_CHARGE_THRESHOLD ){
				//draw "empty" charging icon
				graphics_context_set_compositing_mode( ctx, STATUS_BAR_COMP_OP_NORMAL );
				graphics_draw_bitmap_in_rect( ctx, s_status_bar_window_globals->res_icon_charging, battery_icon_bounds );

			} else if( missing_charge_percent < STATUS_BAR_BATTERY_CHARGE_THRESHOLD ){
				//draw "full" charging icon
				graphics_context_set_compositing_mode( ctx, STATUS_BAR_COMP_OP_INVERTED );
				graphics_draw_bitmap_in_rect( ctx, s_status_bar_window_globals->res_icon_charging, battery_icon_bounds );

			} else {
				//draw "halfway" charging icon
				graphics_context_set_compositing_mode( ctx, STATUS_BAR_COMP_OP_NORMAL );
				graphics_draw_bitmap_in_rect( ctx, s_status_bar_window_globals->res_icon_charging_half, battery_icon_bounds );
			}

		} else {
			
			//recalculate Y and height to account for how fully charged the device is (round toward full)
			if( missing_charge_percent < item->parts.battery_full_missing_percent ){
				missing_charge_percent = item->parts.battery_full_missing_percent;
				
			} else if( missing_charge_percent > item->parts.battery_empty_missing_percent ){
				missing_charge_percent = item->parts.battery_empty_missing_percent;
				
			}
			int	missing_h =  (missing_charge_percent - item->parts.battery_full_missing_percent) *
					battery_icon_bounds.size.h /
					(item->parts.battery_empty_missing_percent - item->parts.battery_full_missing_percent);
			
			battery_icon_bounds.origin.y += missing_h;
			battery_icon_bounds.size.h -= missing_h;

			//fill charged rectangle
			graphics_context_set_fill_color( ctx, STATUS_BAR_WINDOW_COLOR_FOREGROUND );
			graphics_fill_rect( ctx, battery_icon_bounds, 0, GCornerNone );
		}
	}

	return bounds.size.w + internal_distance;
}

//returns the width, in pixels, of the rendered text
int8_t status_bar_window_layout_item_render_text( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x ){
	if( NULL == item->parts.text || NULL == item->parts.text_font ){			//if there's no text or no font, do nothing
		return 0;
	}
	
	GSize text_size = graphics_text_layout_get_content_size(
		item->parts.text,
		item->parts.text_font,
		GRect(0, 0, STATUS_BAR_TEXT_WIDTH_MAX, CUSTOM_STATUS_BAR_LAYER_HEIGHT),
		GTextOverflowModeTrailingEllipsis,
		item->alignment	
	);

	int text_x = offset_x;
	if( item->alignment == GTextAlignmentRight){
		text_x = STATUS_BAR_WINDOW_WIDTH - text_x - text_size.w;
	}

	graphics_context_set_text_color( ctx, STATUS_BAR_WINDOW_COLOR_FOREGROUND );
	graphics_draw_text(
		ctx, item->parts.text, item->parts.text_font,
		GRect(
			text_x,
			STATUS_BAR_TEXT_ADJUST_Y + CUSTOM_STATUS_BAR_LAYER_HEIGHT - text_size.h,
			text_size.w,
			text_size.h
		),
		GTextOverflowModeTrailingEllipsis,
		item->alignment,
		NULL
	);

	return text_size.w;
}

//returns the new value for offset_x, after rendering the given icon/text
int8_t status_bar_window_layout_item_render( status_bar_window_layout_item_t *item, GContext *ctx, int8_t offset_x ){
	offset_x += STATUS_BAR_ITEM_DISTANCE + item->parts.distance_offset;
		
	//render text before icon (if right aligned)
	if( item->alignment == GTextAlignmentRight ){
		offset_x += status_bar_window_layout_item_render_text( item, ctx, offset_x );
	}
	
	//render icon
	offset_x += status_bar_window_layout_item_render_icon( item, ctx, offset_x );
	
	//render text after icon (if not right aligned)
	if( item->alignment != GTextAlignmentRight ){
		offset_x += status_bar_window_layout_item_render_text( item, ctx, offset_x );
	}
	
	//return updated X offset
	return offset_x;
}

//--------------------------//
// Status Bar Window Layout //
//--------------------------//

status_bar_window_layout_t *status_bar_window_layout_create(void){
	status_bar_window_layout_t *status_bar_window_layout = malloc( sizeof(*status_bar_window_layout) );
	
	status_bar_window_layout->left_width = 0;
	status_bar_window_layout->center_width = 0;
	status_bar_window_layout->right_width = 0;
	
	status_bar_window_layout->left_max_distance = STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON;
	status_bar_window_layout->center_max_distance = STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON;
	status_bar_window_layout->right_max_distance = STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON;
	
	status_bar_window_layout->left_first = NULL;
	status_bar_window_layout->left_last_next_ptr = &(status_bar_window_layout->left_first);
	
	status_bar_window_layout->center_first = NULL;
	status_bar_window_layout->center_last_next_ptr = &(status_bar_window_layout->center_first);
	
	status_bar_window_layout->right_first = NULL;
	status_bar_window_layout->right_last_next_ptr = &(status_bar_window_layout->right_first);
		
	return status_bar_window_layout;
}

void status_bar_window_layout_destroy( status_bar_window_layout_t *status_bar_window_layout ){
	status_bar_window_layout_item_destroy_recursive( status_bar_window_layout->left_first );
	status_bar_window_layout_item_destroy_recursive( status_bar_window_layout->center_first );
	status_bar_window_layout_item_destroy_recursive( status_bar_window_layout->right_first );
	
	free( status_bar_window_layout );
}


bool status_bar_window_layout_add_item(		//returns true if successfully added, false if item wouldn't fit
	status_bar_window_layout_t *status_bar_window_layout,
	GTextAlignment alignment,
	status_bar_border_distance_t distance,
	status_bar_window_layout_item_parts_t item_parts
){
	
	
	uint8_t *curr_side_width;
	status_bar_window_layout_item_t **curr_side_next;
	status_bar_window_layout_item_t ***curr_side_last_next_ptr;
	status_bar_border_distance_t *curr_side_max_distance;
	switch( alignment ){
	  case GTextAlignmentLeft:
		curr_side_width = &(status_bar_window_layout->left_width);
		curr_side_next = &(status_bar_window_layout->left_first);
		curr_side_last_next_ptr = &(status_bar_window_layout->left_last_next_ptr);
		curr_side_max_distance = &(status_bar_window_layout->left_max_distance);
		break;
		
	  case GTextAlignmentRight:
		curr_side_width = &(status_bar_window_layout->right_width);
		curr_side_next = &(status_bar_window_layout->right_first);
		curr_side_last_next_ptr = &(status_bar_window_layout->right_last_next_ptr);
		curr_side_max_distance = &(status_bar_window_layout->right_max_distance);
		break;
		
	  case GTextAlignmentCenter:
		curr_side_width = &(status_bar_window_layout->center_width);
		curr_side_next = &(status_bar_window_layout->center_first);
		curr_side_last_next_ptr = &(status_bar_window_layout->center_last_next_ptr);
		curr_side_max_distance = &(status_bar_window_layout->center_max_distance);
		break;

	  default:
		//this should't happen, but if it does, abort operation
		return false;
	}

	status_bar_window_layout_item_t *item = status_bar_window_layout_item_create( alignment, distance, item_parts );

	*curr_side_width += item->width;
	bool should_abort = false;
	
	if( status_bar_window_layout->center_width == 0){	// [Left      ...       Right]
		if( 
			status_bar_window_layout->left_width + STATUS_BAR_ITEM_DISTANCE + status_bar_window_layout->right_width >
			STATUS_BAR_WINDOW_WIDTH
		){
			should_abort = true;
		}
			
	} else {											// [Left ... Center ... Right]
		
		if(
			( alignment != GTextAlignmentRight ) &&		// [Left ... Cen|            ]
			(
				2 * ( status_bar_window_layout->left_width + STATUS_BAR_ITEM_DISTANCE ) + status_bar_window_layout->center_width >
				STATUS_BAR_WINDOW_WIDTH
			)
		){
			should_abort = true;
			
		} else if(
			( alignment != GTextAlignmentLeft ) &&		// [            |er ... Right]
			(
				status_bar_window_layout->center_width +  2 * ( STATUS_BAR_ITEM_DISTANCE + status_bar_window_layout->right_width ) >
				STATUS_BAR_WINDOW_WIDTH
			)
		){
			should_abort = true;
		}
		
	}
	
	if( should_abort ){
		*curr_side_width -= item->width;
		status_bar_window_layout_item_destroy( item );
		return false;
	}
	
	if( distance >= *curr_side_max_distance ){	//quick way of finding the end of layout in O(1) time		
		curr_side_next = *curr_side_last_next_ptr;
		*curr_side_max_distance = distance;
		
	} else {									//finding position in the middle of the layout is still O(N)
		while( NULL != *curr_side_next ){
			if( distance < (*curr_side_next)->distance ){
				break;
			} else {
				curr_side_next = &( (*curr_side_next)->next );
			}
		}

	}
	
	//remember who comes after item (if there's no next item, remember it became the new last)
	if(	NULL != *curr_side_next ){
		item->next = *curr_side_next;
	} else {
		*curr_side_last_next_ptr = &(item->next);
	}

	*curr_side_next = item;
	
	return true;
}
	

void status_bar_window_mark_layout_dirty( status_bar_window_t *status_bar_window ){
	if( NULL != status_bar_window->layout ){
		status_bar_window_layout_destroy( status_bar_window->layout );
		status_bar_window->layout = NULL;
	}
	
	layer_mark_dirty( status_bar_window->layer_status_bar );
}


void status_bar_window_build_layout( status_bar_window_t *status_bar_window ){
	if( NULL != status_bar_window->layout ) return;
	
	status_bar_window->layout = status_bar_window_layout_create();

	if( !status_bar_window->hide_time ){
		// current time
		status_bar_window_layout_add_item(
			status_bar_window->layout, GTextAlignmentCenter, STATUS_BAR_BORDER_DISTANCE_SYSTEM_TEXT,
			(status_bar_window_layout_item_parts_t){
				.distance_offset = STATUS_BAR_CLOCK_TEXT_DISTANCE_OFFSET,
				
				.text = s_status_bar_window_globals->curr_time_text_buffer,
				.text_font = s_status_bar_window_globals->res_gothic_18_bold
			}
		);
		
		if( !clock_is_24h_style() ){
			// AM/PM
			status_bar_window_layout_add_item(
				status_bar_window->layout, GTextAlignmentCenter, STATUS_BAR_BORDER_DISTANCE_SYSTEM_TEXT,
				(status_bar_window_layout_item_parts_t){
					.distance_offset = STATUS_BAR_AM_PM_TEXT_DISTANCE_OFFSET,
					
					.text = s_status_bar_window_globals->curr_time_suffix_text_buffer,
					.text_font = s_status_bar_window_globals->res_gothic_14
				}
			);
		}
	}
	
	// Battery Icon
	status_bar_window_layout_add_item(
		status_bar_window->layout, GTextAlignmentRight, STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON,
		(status_bar_window_layout_item_parts_t){
			.distance_offset = STATUS_BAR_BORDER_DISTANCE_OFFSET,
			
			.icon = s_status_bar_window_globals->res_icon_battery,
			
			.battery_state = &(s_status_bar_window_globals->watch_battery_state),
			.battery_full_missing_percent = STATUS_BAR_WATCH_FULL_MISSING_PERCENT,
			.battery_empty_missing_percent = STATUS_BAR_WATCH_EMPTY_MISSING_PERCENT,
			.battery_icon_origin = GPoint(
				STATUS_BAR_WATCH_BATTERY_X,
				STATUS_BAR_WATCH_BATTERY_Y
			)
		}
	);
	
	
	// Phone Icon
	if( s_status_bar_window_globals->is_connected_to_phone ){
		status_bar_window_layout_add_item(
			status_bar_window->layout, GTextAlignmentLeft, STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON,
			(status_bar_window_layout_item_parts_t){
				.distance_offset = STATUS_BAR_BORDER_DISTANCE_OFFSET,
				
				.icon = s_status_bar_window_globals->res_icon_phone,
				
				/*
				.battery_state = &(s_status_bar_window_globals->phone_battery_state),
				.battery_full_missing_percent = STATUS_BAR_PHONE_FULL_MISSING_PERCENT,
				.battery_empty_missing_percent = STATUS_BAR_PHONE_EMPTY_MISSING_PERCENT,
				.battery_icon_origin = GPoint(
					STATUS_BAR_PHONE_BATTERY_X,
					STATUS_BAR_PHONE_BATTERY_Y
				),
				*/
			}
		);
	}
	
	
	// Catalog items
	status_bar_item_t *item;
	for( item = status_bar_item_catalog_get_first(); NULL != item; item = status_bar_item_get_next(item) ){
		if( 
			( NULL != status_bar_item_get_icon(item) ) &&
			( !status_bar_item_get_requires_phone_connection(item) || s_status_bar_window_globals->is_connected_to_phone )
		){
			status_bar_window_layout_add_item(
				status_bar_window->layout, status_bar_item_get_alignment(item), status_bar_item_get_distance(item),
				(status_bar_window_layout_item_parts_t){
					.icon = status_bar_item_get_icon(item),
					.text = status_bar_item_get_text(item),
					.text_font = s_status_bar_window_globals->res_gothic_14
				}
			);
		}
	}
	
	
	// Battery charge percent text
	status_bar_window_layout_add_item( status_bar_window->layout, GTextAlignmentRight, STATUS_BAR_BORDER_DISTANCE_SYSTEM_TEXT,
		(status_bar_window_layout_item_parts_t){
			.distance_offset = STATUS_BAR_BATTERY_TEXT_DISTANCE_OFFSET,
			
			.text =  s_status_bar_window_globals->watch_battery_text_buffer,
			.text_font = s_status_bar_window_globals->res_gothic_18_bold
		}
	);
}


//---------------------//
// Rendering functions //
//---------------------//

static void render_status_bar_layer( struct Layer *layer, GContext *ctx ) {	
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	int8_t offset_x;
	status_bar_window_layout_item_t *item;	
	
	//build layout, if it's been marked as dirty
	status_bar_window_build_layout( status_bar_window );
	
	//render left items
	offset_x = 0;
	for( item = status_bar_window->layout->left_first; NULL != item; item = item->next ){
		offset_x = status_bar_window_layout_item_render( item, ctx, offset_x );	
	}
	
	//render center items
	offset_x = ( STATUS_BAR_WINDOW_WIDTH - status_bar_window->layout->center_width ) / 2;
	for( item =  status_bar_window->layout->center_first; NULL != item; item = item->next ){
		offset_x = status_bar_window_layout_item_render( item, ctx, offset_x );	
	}
	
	//render right items
	offset_x = 0;
	for( item =  status_bar_window->layout->right_first; NULL != item; item = item->next ){
		offset_x = status_bar_window_layout_item_render( item, ctx, offset_x );
	}
}


//------------------//
// Service Handlers //
//------------------//


static void status_bar_window_tick_handler(struct tm *tick_time, TimeUnits units_changed ){
	
	if( clock_is_24h_style() ){
		strftime( s_status_bar_window_globals->curr_time_text_buffer, STATUS_BAR_TIME_TEXT_BUFFER_SIZE, "%H:%M", tick_time );
	} else {
		strftime( s_status_bar_window_globals->curr_time_text_buffer, STATUS_BAR_TIME_TEXT_BUFFER_SIZE, "%I:%M", tick_time );
		strftime( s_status_bar_window_globals->curr_time_suffix_text_buffer, STATUS_BAR_TIME_SUFFIX_TEXT_BUFFER_SIZE, "%p", tick_time );
	}
	
	//strip leading zero, since both %H and %I generate stuff like "09"
	if( s_status_bar_window_globals->curr_time_text_buffer[0] == '0' ){
		memmove(
			s_status_bar_window_globals->curr_time_text_buffer,
			s_status_bar_window_globals->curr_time_text_buffer + 1,
			STATUS_BAR_TIME_TEXT_BUFFER_SIZE-1
		);
	}
	
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	status_bar_window_mark_layout_dirty( status_bar_window );
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed ){
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	
	//call our tick handler, when necessary
	if(
		!status_bar_window->hide_time &&
		( (units_changed == 0) || (units_changed & STATUS_BAR_WINDOW_TICK_UNITS) )
	){
		status_bar_window_tick_handler(tick_time, units_changed);
	}
	
	//also call user's handler, if appropriate
	if(
		NULL != s_status_bar_window_globals->tick_handler &&
		( (units_changed == 0) || (units_changed & s_status_bar_window_globals->tick_units) )
	){
		s_status_bar_window_globals->tick_handler(tick_time, units_changed);
	}
}


static void pebble_app_connection_handler( bool connected ){
	s_status_bar_window_globals->is_connected_to_phone = connected;
	
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	status_bar_window_mark_layout_dirty( status_bar_window );
	
	//also call user's handler, if appropriate
	if( NULL != s_status_bar_window_globals->connection_handlers.pebble_app_connection_handler ){
		s_status_bar_window_globals->connection_handlers.pebble_app_connection_handler( connected );
	}
}


static void battery_handler( BatteryChargeState charge ){
	s_status_bar_window_globals->watch_battery_state = charge;
	snprintf( s_status_bar_window_globals->watch_battery_text_buffer, STATUS_BAR_BATTERY_TEXT_BUFFER_SIZE, "%d", charge.charge_percent );
	
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	status_bar_window_mark_layout_dirty( status_bar_window );
	
	//also call user's handler, if appropriate
	if( NULL != s_status_bar_window_globals->battery_handler ){
		s_status_bar_window_globals->battery_handler( charge );
	}
}

//-----------------//
// Window Handlers //
//-----------------//

static void handle_window_load( Window* window ) {
	status_bar_window_t *status_bar_window = window_get_status_bar_window( window );
	Layer *root_layer = window_get_root_layer(window);
	
	// layer_status_bar
	status_bar_window->layer_status_bar = layer_create( GRect(0, 0, STATUS_BAR_WINDOW_WIDTH, CUSTOM_STATUS_BAR_LAYER_HEIGHT) );
	layer_set_update_proc( status_bar_window->layer_status_bar, render_status_bar_layer );
	layer_add_child(root_layer, status_bar_window->layer_status_bar );
	
	// layer_body
	status_bar_window->layer_body = layer_create(
		GRect(0, CUSTOM_STATUS_BAR_LAYER_HEIGHT, STATUS_BAR_WINDOW_WIDTH, STATUS_BAR_WINDOW_HEIGHT - CUSTOM_STATUS_BAR_LAYER_HEIGHT )
	);
	layer_add_child(root_layer, status_bar_window->layer_body );
	
	
	//also call custom handler, if any
	WindowHandler handler = status_bar_window->window_handlers.load;
	if( NULL!= handler){
		handler( window );
	}
}


static void handle_window_unload(Window* window) {
	status_bar_window_t *status_bar_window = window_get_status_bar_window( window );
	
	//also call custom handler, if any
	WindowHandler handler = status_bar_window->window_handlers.unload;
	if( NULL!= handler){
		handler( window );
	}
	
	//destroy layout, if one has been calculated
	if( NULL != status_bar_window->layout ){
		status_bar_window_layout_destroy( status_bar_window->layout );
		status_bar_window->layout = NULL;
	}
	
	//destroy window contents	
	layer_destroy( status_bar_window->layer_status_bar );
	layer_destroy( status_bar_window->layer_body );
	
	status_bar_window_destroy( status_bar_window );
}

static void handle_window_appear(Window* window) {
	status_bar_window_t *status_bar_window = window_get_status_bar_window( window );
	
	s_status_bar_window_globals->current_window = status_bar_window;
	
	//setup tick handler
	if( status_bar_window->hide_time ){
		if( NULL != s_status_bar_window_globals->tick_handler ){
			tick_timer_service_subscribe( s_status_bar_window_globals->tick_units, tick_handler );
		} else {
			tick_timer_service_unsubscribe();
		}
		
	} else {
		tick_timer_service_subscribe( s_status_bar_window_globals->tick_units | STATUS_BAR_WINDOW_TICK_UNITS, tick_handler );
		time_t now = time(NULL);
		status_bar_window_tick_handler( localtime(&now), 0 );
	}
	
	//setup connection handler (this also marks the current layout as dirty)
	connection_service_subscribe(
		(ConnectionHandlers) {
			.pebble_app_connection_handler = pebble_app_connection_handler,
			.pebblekit_connection_handler = s_status_bar_window_globals->connection_handlers.pebblekit_connection_handler
		}
	);
	pebble_app_connection_handler( connection_service_peek_pebble_app_connection() );
	
	//setup battery handler (this also marks the current layout as dirty)
	battery_state_service_subscribe( battery_handler );
	battery_handler( battery_state_service_peek() );
	
	
	//also call custom handler, if any
	WindowHandler handler = status_bar_window->window_handlers.appear;
	if( NULL!= handler){
		handler( window );
	}
}

static void handle_window_disappear(Window* window) {
	status_bar_window_t *status_bar_window = window_get_status_bar_window( window );
	
	//also call custom handler, if any
	WindowHandler handler = status_bar_window->window_handlers.disappear;
	if( NULL!= handler){
		handler( window );
	}
	
	//unsubscribe from all necessary handlers
	tick_timer_service_unsubscribe();
	connection_service_unsubscribe();
	battery_state_service_unsubscribe();
	
	s_status_bar_window_globals->current_window = NULL;
}


//------------------------------//
//      Status Bar Window       //
// Constructors and Destructors //
//------------------------------//

static status_bar_window_globals_t *status_bar_window_globals_create(void){
	status_bar_window_globals_t *status_bar_window_globals = malloc( sizeof(*status_bar_window_globals) );
	
	status_bar_window_globals->num_windows = 0;
	status_bar_window_globals->current_window = NULL;
	
	// textures
	status_bar_window_globals->res_icon_phone = gbitmap_create_with_resource(RESOURCE_ID_ICON_STATUS_BAR_PHONE);
	status_bar_window_globals->res_icon_battery = gbitmap_create_with_resource(RESOURCE_ID_ICON_STATUS_BAR_BATTERY);
	status_bar_window_globals->res_icon_charging = gbitmap_create_with_resource(RESOURCE_ID_ICON_STATUS_BAR_CHARGING);
	status_bar_window_globals->res_icon_charging_half = gbitmap_create_with_resource(RESOURCE_ID_ICON_STATUS_BAR_CHARGING_HALF);
		
	// fonts
	status_bar_window_globals->res_gothic_18_bold = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
	status_bar_window_globals->res_gothic_14 = fonts_get_system_font(FONT_KEY_GOTHIC_14);
	
	// service handler callbacks
	status_bar_window_globals->tick_units = 0;
	status_bar_window_globals->tick_handler = NULL;
	status_bar_window_globals->battery_handler = NULL;
	status_bar_window_globals->connection_handlers = (ConnectionHandlers){
		.pebble_app_connection_handler = NULL,
		.pebblekit_connection_handler = NULL
	};	
		
	return status_bar_window_globals;
}


static void status_bar_window_globals_destroy(status_bar_window_globals_t *status_bar_window_globals){	
	gbitmap_destroy( status_bar_window_globals->res_icon_battery );
	gbitmap_destroy( status_bar_window_globals->res_icon_phone );
	gbitmap_destroy( status_bar_window_globals->res_icon_charging );
	gbitmap_destroy( status_bar_window_globals->res_icon_charging_half );

	free( status_bar_window_globals );
}


status_bar_window_t *status_bar_window_create( bool hide_time ){
	if( NULL == s_status_bar_window_globals ){
		s_status_bar_window_globals = status_bar_window_globals_create(); 
	}
	s_status_bar_window_globals->num_windows++;
	
	status_bar_window_t *status_bar_window = malloc( sizeof(*status_bar_window) );
	
	//window
	status_bar_window->window = window_create();
	window_set_user_data( status_bar_window->window, status_bar_window );
	window_set_background_color(status_bar_window->window, STATUS_BAR_WINDOW_COLOR_BACKGROUND );
	
	#ifndef PBL_SDK_3
		window_set_fullscreen(status_bar_window->window, true);
	#endif
  
	
	//window handlers
	status_bar_window->window_handlers = (WindowHandlers) {
		.load = NULL,
		.unload = NULL,
		.appear = NULL,
		.disappear = NULL,
	};
	window_set_window_handlers(status_bar_window->window, (WindowHandlers) {
		.load = handle_window_load,
		.unload = handle_window_unload,
		.appear = handle_window_appear,
		.disappear = handle_window_disappear,
	});
	
	
	//internal status
	status_bar_window->user_data = NULL;
	status_bar_window->layout = NULL;
	status_bar_window->hide_time = hide_time;
	
	return status_bar_window;
}


void status_bar_window_destroy( status_bar_window_t *status_bar_window ){
	if( NULL != status_bar_window->layout ){
		status_bar_window_layout_destroy( status_bar_window->layout );
	}
	
	window_destroy( status_bar_window->window );
	free( status_bar_window );
	
	if( 0 == --(s_status_bar_window_globals->num_windows) ){
		status_bar_window_globals_destroy(s_status_bar_window_globals);
		s_status_bar_window_globals = NULL;
	}
}


//---------------------//
// Getters and Setters //
//---------------------//

status_bar_window_t *get_current_status_bar_window(void){
	if( NULL == s_status_bar_window_globals ){
		return NULL;
	} else {
		return s_status_bar_window_globals->current_window;
	}
}

status_bar_window_t *window_get_status_bar_window( Window *window ){
	return window_get_user_data( window );
}


Window *status_bar_window_get_window(status_bar_window_t *status_bar_window ){
	return status_bar_window->window;
}

Layer *status_bar_window_get_status_bar_layer(status_bar_window_t *status_bar_window ){
	return status_bar_window->layer_status_bar;
}

Layer *status_bar_window_get_body_layer(status_bar_window_t *status_bar_window ){
	return status_bar_window->layer_body;
}

	
//----------------------------------------//
// Replacements for core pebble functions //
//----------------------------------------//

// Window user data
void *status_bar_window_get_user_data(status_bar_window_t *status_bar_window ){
	if( NULL == status_bar_window ){
		return NULL;
	} else {
		return status_bar_window->user_data;
	}
}

void status_bar_window_set_user_data(status_bar_window_t *status_bar_window, void *user_data ){
	if( NULL != status_bar_window ){
		status_bar_window->user_data = user_data;
	}
}


// Window handlers
void status_bar_window_set_window_handlers( status_bar_window_t *status_bar_window, WindowHandlers handlers ){
	status_bar_window->window_handlers = handlers;
}


// Tick timer service
void status_bar_window_tick_timer_service_subscribe( status_bar_window_t *status_bar_window, TimeUnits tick_units, TickHandler handler){
	s_status_bar_window_globals->tick_units = tick_units;
	s_status_bar_window_globals->tick_handler = handler;
	
	if( status_bar_window->hide_time ){
		tick_timer_service_subscribe( tick_units, tick_handler );		
	} else {
		tick_timer_service_subscribe( tick_units | STATUS_BAR_WINDOW_TICK_UNITS, tick_handler );
	}
	
	time_t now = time(NULL);
	s_status_bar_window_globals->tick_handler( localtime(&now), 0 );
}

void status_bar_window_tick_timer_service_unsubscribe( status_bar_window_t *status_bar_window ){
	s_status_bar_window_globals->tick_units = 0;
	s_status_bar_window_globals->tick_handler = NULL;
	
	if( status_bar_window->hide_time ){
		tick_timer_service_unsubscribe();
	} else {
		tick_timer_service_subscribe( STATUS_BAR_WINDOW_TICK_UNITS, tick_handler );
	}
	
}


// Connection service
void status_bar_window_connection_service_subscribe(ConnectionHandlers handlers){
	s_status_bar_window_globals->connection_handlers = handlers;
	
	connection_service_subscribe(
		(ConnectionHandlers) {
			.pebble_app_connection_handler = pebble_app_connection_handler,
			.pebblekit_connection_handler = s_status_bar_window_globals->connection_handlers.pebblekit_connection_handler
		}
	);
}

void status_bar_window_connection_service_unsubscribe(void){
	s_status_bar_window_globals->connection_handlers = (ConnectionHandlers) {
		.pebble_app_connection_handler = NULL,
		.pebblekit_connection_handler = NULL
	};
	
	connection_service_subscribe(
		(ConnectionHandlers) {
			.pebble_app_connection_handler = pebble_app_connection_handler,
			.pebblekit_connection_handler = NULL
		}
	);
}


// Battery service
void status_bar_window_battery_state_service_subscribe(BatteryStateHandler handler){
	s_status_bar_window_globals->battery_handler = handler;
}

void status_bar_window_battery_state_service_unsubscribe(void){
	s_status_bar_window_globals->battery_handler = NULL;
}
