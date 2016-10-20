#include <pebble.h>
#include "include/core_status_bar.h"
#include "include/window_status_bar.h"


//------------//
// Data Types //
//------------//

//status bar custom items and respective catalog
struct status_bar_item_s {
	GTextAlignment alignment;
	status_bar_border_distance_t distance;
	
	uint32_t id;
	uint32_t icon_resource_id;
	bool requires_phone_connection;
	
	GBitmap *icon;
	char *text;
	
	status_bar_item_t *next;
};

struct status_bar_item_catalog_s {
	status_bar_item_t *first;
	status_bar_item_t **last_next_ptr;
	
	status_bar_item_t **id_table;		//array of pointers to items: id_table[item_id] points to the item with that id.
};


//-------------//
// Static vars //
//-------------//

static status_bar_item_catalog_t *s_status_bar_item_catalog = NULL;


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
){
	status_bar_item_t *item = malloc( sizeof(*item) );
	
	item->alignment = alignment;
	item->distance = distance;
	item->id = item_id;
	item->icon_resource_id = icon_resource_id;
	item->requires_phone_connection = requires_phone_connection;
	item->icon = NULL;
	item->text = NULL;
	item->next = NULL;
	
	return item;
}

void status_bar_item_destroy( status_bar_item_t *item ){
	if( NULL != item->icon ){
		gbitmap_destroy( item->icon );
	}
	
	free(item);	
}

void status_bar_item_destroy_recursive( status_bar_item_t *item ){
	if( NULL != item ){	
		status_bar_item_destroy_recursive( item->next );
		status_bar_item_destroy( item );
	}
}


//getters
inline GTextAlignment status_bar_item_get_alignment( status_bar_item_t *item ){
	return item->alignment;
}

inline status_bar_border_distance_t status_bar_item_get_distance( status_bar_item_t *item ){
	return item->distance;
}

inline bool status_bar_item_get_requires_phone_connection( status_bar_item_t *item ){
	return item->requires_phone_connection;
}

inline GBitmap *status_bar_item_get_icon( status_bar_item_t *item ){
	return item->icon;
}

inline char *status_bar_item_get_text( status_bar_item_t *item ){
	return item->text;
}

inline status_bar_item_t *status_bar_item_get_next( status_bar_item_t *item ){
	return item->next;
}


//setters
void status_bar_item_set_text( status_bar_item_t *item, char *text ){
	// update text
	item->text = text;
	
	// if item is currently shown, mark curent status bar as dirty
	if( NULL != item->icon ){
		status_bar_window_t *status_bar_window = get_current_status_bar_window();
		if( NULL != status_bar_window ){
			status_bar_window_mark_layout_dirty( status_bar_window );
		}
	}
}


void status_bar_item_load_new_icon( status_bar_item_t *item, uint32_t icon_resource_id ){
	if( (item->icon_resource_id == icon_resource_id) && (NULL != item->icon) ){
		return;										//if icon_resource_id didn't change, and icon was already loaded, do nothing
	}
	
	// update icon_resource_id
	item->icon_resource_id = icon_resource_id;
	
	// update icon
	if( NULL != item->icon ){
		gbitmap_destroy( item->icon );
	}
	item->icon = gbitmap_create_with_resource( icon_resource_id );
	
	
	// mark curent status bar as dirty
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	if( NULL != status_bar_window ){
		status_bar_window_mark_layout_dirty( status_bar_window );
	}
}

void status_bar_item_load_icon( status_bar_item_t *item ){
	if( NULL != item->icon ){						//if icon was already loaded, do nothing
		return;
	}
	
	// update icon
	item->icon = gbitmap_create_with_resource( item->icon_resource_id );
	
	
	// mark curent status bar as dirty
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	if( NULL != status_bar_window ){
		status_bar_window_mark_layout_dirty( status_bar_window );
	}
}

void status_bar_item_unload_icon( status_bar_item_t *item ){
	if( NULL == item->icon ){						//if icon was already not loaded, do nothing
		return;
	}
	
	// update icon
	gbitmap_destroy( item->icon );
	item->icon = NULL;
	
	
	// mark curent status bar as dirty
	status_bar_window_t *status_bar_window = get_current_status_bar_window();
	if( NULL != status_bar_window ){
		status_bar_window_mark_layout_dirty( status_bar_window );
	}
}


//-------------------------//
// Status Bar Item Catalog //
//-------------------------//

//constructor, destructor
void status_bar_item_catalog_init( size_t item_id_count ){
	if( NULL != s_status_bar_item_catalog ){			//if catalog has already been initialized, do nothing
		return;
	}
	
	s_status_bar_item_catalog = malloc( sizeof(*s_status_bar_item_catalog) );
	
	s_status_bar_item_catalog->first = NULL;
	s_status_bar_item_catalog->last_next_ptr = &(s_status_bar_item_catalog->first);
	
	s_status_bar_item_catalog->id_table = calloc(		//array with item_id_max elements, all initially NULL
		item_id_count,
		sizeof( *(s_status_bar_item_catalog->id_table) )
	);
}

void status_bar_item_catalog_deinit(void){
	if( NULL == s_status_bar_item_catalog ){			//if catalog has not been initialized, do nothing
		return;
	}
	
	status_bar_item_destroy_recursive( s_status_bar_item_catalog->first );
	
	free( s_status_bar_item_catalog->id_table );
	free( s_status_bar_item_catalog );
}


//getters
status_bar_item_t *status_bar_item_catalog_find( uint32_t item_id ){
	if( NULL == s_status_bar_item_catalog ){			//if catalog has not been initialized, return nothing
		return NULL;
	} else {
		return s_status_bar_item_catalog->id_table[item_id];
	}
}

inline status_bar_item_t *status_bar_item_catalog_get_first(void){
	return s_status_bar_item_catalog->first;
}


//setters
void status_bar_item_catalog_insert( status_bar_item_t *item ){
	if( NULL == s_status_bar_item_catalog ){				//if catalog has not been initialized, destroy item instead
		status_bar_item_destroy( item );
		return;
	}
	
	//add item to END of catalog (lower priority than previous last)
	*(s_status_bar_item_catalog->last_next_ptr) = item;
	s_status_bar_item_catalog->last_next_ptr = &(item->next);
	
	//also point to item from id_table
	s_status_bar_item_catalog->id_table[item->id] = item;
}

	