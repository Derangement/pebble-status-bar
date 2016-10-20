#pragma once
#include <pebble.h>

//------------//
// Data Types //
//------------//

//status bar catalog and respective items
typedef struct status_bar_item_s status_bar_item_t;
typedef struct status_bar_item_catalog_s status_bar_item_catalog_t;
	
// Border Distances (lower means closer to screen border)
typedef enum {
	STATUS_BAR_BORDER_DISTANCE_SYSTEM_ICON,
	STATUS_BAR_BORDER_DISTANCE_SYSTEM_TEXT,
	STATUS_BAR_BORDER_DISTANCE_CLOSE,
	STATUS_BAR_BORDER_DISTANCE_MEDIUM,
	STATUS_BAR_BORDER_DISTANCE_FAR
} status_bar_border_distance_t;


//------------------//
// Status Bar Items //
//------------------//

//constructor, destructor
status_bar_item_t *status_bar_item_create(
	GTextAlignment alignment,
	status_bar_border_distance_t distance,
	uint32_t item_id,
	uint32_t icon_resource_id,
	bool requires_phone_connection
);
void status_bar_item_destroy( status_bar_item_t *item );
void status_bar_item_destroy_recursive( status_bar_item_t *item );

//getters
GTextAlignment status_bar_item_get_alignment( status_bar_item_t *item );
status_bar_border_distance_t status_bar_item_get_distance( status_bar_item_t *item );
bool status_bar_item_get_requires_phone_connection( status_bar_item_t *item );	
GBitmap *status_bar_item_get_icon( status_bar_item_t *item );
char *status_bar_item_get_text( status_bar_item_t *item );	
status_bar_item_t *status_bar_item_get_next( status_bar_item_t *item );

//setters
void status_bar_item_set_text( status_bar_item_t *item, char *text );
void status_bar_item_load_new_icon( status_bar_item_t *item, uint32_t icon_resource_id );
void status_bar_item_load_icon( status_bar_item_t *item );
void status_bar_item_unload_icon( status_bar_item_t *item );
	

//-------------------------//
// Status Bar Item Catalog //
//-------------------------//

//constructor, destructor
void status_bar_item_catalog_init( size_t item_id_count );
void status_bar_item_catalog_deinit(void);

//getters
status_bar_item_t *status_bar_item_catalog_find( uint32_t item_id );
status_bar_item_t *status_bar_item_catalog_get_first(void);

//setters
void status_bar_item_catalog_insert( status_bar_item_t *item );		//inserts with lower priority than last
