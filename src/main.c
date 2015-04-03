#include <pebble.h>
#include "main.h"
#include "slide_layer.h"
	
Window *window;
static Layer *window_layer;

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_CLEAR_DAY,
  RESOURCE_ID_CLEAR_NIGHT,
  RESOURCE_ID_WINDY,
  RESOURCE_ID_COLD,
  RESOURCE_ID_PARTLY_CLOUDY_DAY,
  RESOURCE_ID_PARTLY_CLOUDY_NIGHT,
  RESOURCE_ID_HAZE,
  RESOURCE_ID_CLOUD,
  RESOURCE_ID_RAIN,
  RESOURCE_ID_SNOW,
  RESOURCE_ID_HAIL,
  RESOURCE_ID_CLOUDY,
  RESOURCE_ID_STORM,
  RESOURCE_ID_FOG,
  RESOURCE_ID_NA,
};

static int invert;
static int bluetoothvibe;
//static int hourlyvibe;

static bool appStarted = false;

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,
  WEATHER_TEMPERATURE_KEY = 0x1,
  INVERT_COLOR_KEY = 0x2,	  
  BLUETOOTHVIBE_KEY = 0x3,
//  HOURLYVIBE_KEY = 0x4,
  CITY_KEY = 0x5
};

SlideLayer *slide_layer[4];

BitmapLayer *layer_conn_img;
GBitmap *img_bt_connect;
GBitmap *img_bt_disconnect;

BitmapLayer *icon_layer;
GBitmap *icon_bitmap = NULL;

TextLayer *temp_layer;
TextLayer *city_layer;
TextLayer *layer_date_text;
Layer* weather_holder;

static GBitmap *s_time_format_bitmap;
static BitmapLayer *s_time_format_layer;

static GFont *steelfish;

int cur_day = -1;

BitmapLayer *layer_batt_img;
GBitmap *img_battery_100;
GBitmap *img_battery_90;
GBitmap *img_battery_80;
GBitmap *img_battery_70;
GBitmap *img_battery_60;
GBitmap *img_battery_50;
GBitmap *img_battery_40;
GBitmap *img_battery_30;
GBitmap *img_battery_20;
GBitmap *img_battery_10;
GBitmap *img_battery_charge;
int charge_percent = 0;

InverterLayer *inverter_layer = NULL;

static AppSync sync;
static uint8_t sync_buffer[128];

GBitmap *background_image;
static BitmapLayer *background_image_layer;


static void set_container_image(GBitmap **bmp_image, BitmapLayer *bmp_layer, const int resource_id, GPoint origin) {
  GBitmap *old_image = *bmp_image;

  *bmp_image = gbitmap_create_with_resource(resource_id);
#ifdef PBL_PLATFORM_BASALT
  GRect bitmap_bounds = gbitmap_get_bounds((*bmp_image));
#else
  GRect bitmap_bounds = (*bmp_image)->bounds;
#endif
  GRect frame = GRect(origin.x, origin.y, bitmap_bounds.size.w, bitmap_bounds.size.h);
  bitmap_layer_set_bitmap(bmp_layer, *bmp_image);
  layer_set_frame(bitmap_layer_get_layer(bmp_layer), frame);

  if (old_image != NULL) {
  	gbitmap_destroy(old_image);
  }
}


void set_invert_color(bool invert) {
  if (invert && inverter_layer == NULL) {
    // Add inverter layer
    Layer *window_layer = window_get_root_layer(window);

    inverter_layer = inverter_layer_create(GRect(7, 82, 128, 72));
    layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));
  } else if (!invert && inverter_layer != NULL) {
    // Remove Inverter layer
    layer_remove_from_parent(inverter_layer_get_layer(inverter_layer));
    inverter_layer_destroy(inverter_layer);
    inverter_layer = NULL;
  }
  // No action required
}

static void sync_tuple_changed_callback(const uint32_t key,
                                        const Tuple* new_tuple,
                                        const Tuple* old_tuple,
                                        void* context) {	

  // App Sync keeps new_tuple in sync_buffer, so we may use it directly
  switch (key) {
    case WEATHER_ICON_KEY:
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }
      icon_bitmap = gbitmap_create_with_resource(
          WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
    break;
	  
	case CITY_KEY:
      text_layer_set_text(city_layer, new_tuple->value->cstring);
    break;

    case WEATHER_TEMPERATURE_KEY:
      text_layer_set_text(temp_layer, new_tuple->value->cstring);
      break;

	case INVERT_COLOR_KEY:
      invert = new_tuple->value->uint8 != 0;
	  persist_write_bool(INVERT_COLOR_KEY, invert);
      set_invert_color(invert);
      break;
	  
    case BLUETOOTHVIBE_KEY:
      bluetoothvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(BLUETOOTHVIBE_KEY, bluetoothvibe);
      break;      
	  /*
    case HOURLYVIBE_KEY:
      hourlyvibe = new_tuple->value->uint8 != 0;
	  persist_write_bool(HOURLYVIBE_KEY, hourlyvibe);	  
      break;
*/
  }
}

unsigned short get_display_hour(unsigned short hour) {
  if (clock_is_24h_style()) {
    return hour;
  }

  unsigned short display_hour = hour % 12;

  // Converts "0" to "12"
  return display_hour ? display_hour : 12;
}

void tick_handler(struct tm *tick_time, TimeUnits units_changed) {	
	
	 static char date_text[] = "xxx xxx 00xx xxx";

	int new_cur_day = tick_time->tm_year*1000 + tick_time->tm_yday;
    if (new_cur_day != cur_day) {
        cur_day = new_cur_day;

	switch(tick_time->tm_mday)
  {
    case 1 :
    case 21 :
    case 31 :
      strftime(date_text, sizeof(date_text), "%a, %est %b", tick_time);
      break;
    case 2 :
    case 22 :
      strftime(date_text, sizeof(date_text), "%a, %end %b", tick_time);
      break;
    case 3 :
    case 23 :
      strftime(date_text, sizeof(date_text), "%a, %erd %b", tick_time);
      break;
    default :
      strftime(date_text, sizeof(date_text), "%a, %eth %b", tick_time);
      break;
  }
    text_layer_set_text(layer_date_text, date_text);	
	
    }; 
 
unsigned short display_hour = get_display_hour(tick_time->tm_hour);

   slide_layer_animate_to(slide_layer[0], display_hour / 10);

  if (!clock_is_24h_style()) {
    if (display_hour/10 >= 1) {
      layer_set_hidden(slide_layer_get_layer(slide_layer[0]), false);

    }  else {
      layer_set_hidden(slide_layer_get_layer(slide_layer[0]), true);

    }
  }

    slide_layer_animate_to(slide_layer[1], display_hour % 10);
	
    slide_layer_animate_to(slide_layer[2], tick_time->tm_min / 10);
    slide_layer_animate_to(slide_layer[3], tick_time->tm_min % 10);

 if (!clock_is_24h_style()) {		
    if (display_hour / 10 == 0) {	
    	layer_set_hidden(bitmap_layer_get_layer(s_time_format_layer), false);

	} else {
		  layer_set_hidden(bitmap_layer_get_layer(s_time_format_layer), true);
    }	 
 } 

 if (!clock_is_24h_style()) {
	 if (tick_time->tm_hour >= 12) {
       set_container_image(&s_time_format_bitmap, s_time_format_layer, RESOURCE_ID_IMAGE_PM_MODE, GPoint(6, 54));
	 } else {
       set_container_image(&s_time_format_bitmap, s_time_format_layer, RESOURCE_ID_IMAGE_AM_MODE, GPoint(6, 54));
	 }
 }			   
}    

void handle_battery(BatteryChargeState charge_state) {

    if (charge_state.is_charging) {
        bitmap_layer_set_bitmap(layer_batt_img, img_battery_charge);
    } else {
        if (charge_state.charge_percent <= 10) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_10);
        } else if (charge_state.charge_percent <= 20) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_20);
        } else if (charge_state.charge_percent <= 30) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_30);
		} else if (charge_state.charge_percent <= 40) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_40);
		} else if (charge_state.charge_percent <= 50) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_50);
    	} else if (charge_state.charge_percent <= 60) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_60);	
        } else if (charge_state.charge_percent <= 70) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_70);
		} else if (charge_state.charge_percent <= 80) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_80);
		} else if (charge_state.charge_percent <= 90) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_90);
		} else if (charge_state.charge_percent <= 100) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_100);			
			   
        if (charge_state.charge_percent < charge_percent) {
            if (charge_state.charge_percent==20){
                vibes_double_pulse();
            } else if(charge_state.charge_percent==10){
                vibes_long_pulse();
            }
        }
    }
    charge_percent = charge_state.charge_percent;   
  }
}

void handle_bluetooth(bool connected) {
    if (connected) {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
    } else {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_disconnect);
    }

    if (appStarted && bluetoothvibe) {
      
        vibes_long_pulse();
	}
}
void force_update(void) {
    handle_battery(battery_state_service_peek());
    handle_bluetooth(bluetooth_connection_service_peek());
}

void window_load(Window *window){
  window_layer = window_get_root_layer(window);
	
	
  for (int i=0; i<4; i++){
    slide_layer[i] = slide_layer_create(NUM_COORDS[i]);
    layer_add_child(window_layer, slide_layer_get_layer(slide_layer[i]));
  }
	
	
  background_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
  BitmapLayer *background_image_layer = bitmap_layer_create(GRect(6, 81, 132, 72));
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  layer_add_child(window_layer, (Layer *) background_image_layer);

	
  if (!clock_is_24h_style()) {
  s_time_format_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_AM_MODE);
#ifdef PBL_PLATFORM_BASALT
  GRect bitmap_bounds = gbitmap_get_bounds(s_time_format_bitmap);
#else
  GRect bitmap_bounds = s_time_format_bitmap->bounds;
#endif
  GRect frame = GRect(6, 54, bitmap_bounds.size.w, bitmap_bounds.size.h);  // 6,10
  s_time_format_layer = bitmap_layer_create(frame);
  bitmap_layer_set_bitmap(s_time_format_layer, s_time_format_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_time_format_layer));
//  layer_set_hidden(bitmap_layer_get_layer(s_time_format_layer), true);
  }
	
	// layers

  Layer *weather_holder = layer_create(GRect(0, 0, 144, 168 ));
  layer_add_child(window_layer, weather_holder);

  icon_layer = bitmap_layer_create(GRect(7, 81, 128, 68)); // 42,81,58,50
  layer_add_child(weather_holder, bitmap_layer_get_layer(icon_layer));

  steelfish = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_STEELFISH_29));

  temp_layer = text_layer_create(GRect(36, 80, 100, 40));
  text_layer_set_text_color(temp_layer, GColorBlack);
  text_layer_set_background_color(temp_layer, GColorClear);
//  text_layer_set_font(temp_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_font(temp_layer, steelfish);
  text_layer_set_text_alignment(temp_layer, GTextAlignmentRight);
  layer_add_child(weather_holder, text_layer_get_layer(temp_layer));
	
  city_layer = text_layer_create(GRect(9, 129, 90, 24));
  text_layer_set_text_color(city_layer, GColorBlack);
  text_layer_set_background_color(city_layer, GColorClear);
  text_layer_set_font(city_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
 // text_layer_set_font(temp_layer, steelfish);	
  text_layer_set_text_alignment(city_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(city_layer));
	
  layer_date_text = text_layer_create(GRect(9, 77, 45, 120));
  text_layer_set_text_color(layer_date_text, GColorBlack);
  text_layer_set_background_color(layer_date_text, GColorClear);
  text_layer_set_font(layer_date_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
 // text_layer_set_font(layer_date_text, steelfish);	
  text_layer_set_text_alignment(layer_date_text, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(layer_date_text));

	layer_conn_img  = bitmap_layer_create(GRect(121, 114, 11, 18));
    layer_batt_img  = bitmap_layer_create(GRect(97, 136, 35, 11));

	bitmap_layer_set_bitmap(layer_batt_img, img_battery_100);
    bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);

	layer_add_child(window_layer, bitmap_layer_get_layer(layer_batt_img));
    layer_add_child(window_layer, bitmap_layer_get_layer(layer_conn_img)); 

	// resources
	img_bt_connect     = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTHON);
    img_bt_disconnect  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTHOFF);
	
    img_battery_100   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_090_100);
    img_battery_90   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_080_090);
    img_battery_80   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_070_080);
    img_battery_70   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_060_070);
    img_battery_60   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_050_060);
    img_battery_50   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_040_050);
    img_battery_40   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_030_040);
    img_battery_30    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_020_030);
    img_battery_20    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_010_020);
    img_battery_10    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_000_010);
    img_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATT_CHARGING);
	
	 // handlers
    battery_state_service_subscribe(&handle_battery);
    bluetooth_connection_service_subscribe(&handle_bluetooth);

	 // draw first frame
    force_update();

}

void window_unload(Window *window){

  fonts_unload_custom_font(steelfish);  
	
  text_layer_destroy( layer_date_text );
  text_layer_destroy( city_layer );
  text_layer_destroy( temp_layer );

  layer_remove_from_parent(bitmap_layer_get_layer(icon_layer));
  bitmap_layer_destroy(icon_layer);
  gbitmap_destroy(icon_bitmap);
  icon_bitmap = NULL;
	
  layer_destroy( weather_holder );

  layer_remove_from_parent(bitmap_layer_get_layer(s_time_format_layer));
  bitmap_layer_destroy(s_time_format_layer);
  gbitmap_destroy(s_time_format_bitmap);
  s_time_format_bitmap = NULL;
	
  gbitmap_destroy(background_image);
  bitmap_layer_destroy(background_image_layer);
  background_image = NULL;

  layer_remove_from_parent(bitmap_layer_get_layer(layer_batt_img));
  bitmap_layer_destroy(layer_batt_img);
  gbitmap_destroy(img_battery_100);
  gbitmap_destroy(img_battery_90);
  gbitmap_destroy(img_battery_80);
  gbitmap_destroy(img_battery_70);
  gbitmap_destroy(img_battery_60);
  gbitmap_destroy(img_battery_50);
  gbitmap_destroy(img_battery_40);
  gbitmap_destroy(img_battery_30);
  gbitmap_destroy(img_battery_20);
  gbitmap_destroy(img_battery_10);
  gbitmap_destroy(img_battery_charge);

  layer_remove_from_parent(bitmap_layer_get_layer(layer_conn_img));
  bitmap_layer_destroy(layer_conn_img);
  gbitmap_destroy(img_bt_connect);
  gbitmap_destroy(img_bt_disconnect);
  img_bt_connect = NULL;
  img_bt_disconnect = NULL;
	
  for (int i=0; i<4; i++){
	layer_remove_from_parent(slide_layer_get_layer(slide_layer[i]));
    slide_layer_destroy(slide_layer[i]);
	slide_layer[i] = NULL;
  }

//  layer_remove_from_parent(window_layer);
//  layer_destroy(window_layer);
	
}

void handle_init(void) {
  window = window_create();
	  window_set_background_color(window, GColorBlack);

  const int inbound_size = 128;
  const int outbound_size = 128;
  app_message_open(inbound_size, outbound_size);	
	
char *sys_locale = setlocale(LC_ALL, "");
  // we're not supporting chinese yet
  if (strcmp("zh_CN", sys_locale) || strcmp("zh_TW", sys_locale) == 0) {
    setlocale(LC_ALL, "en_US");
  }
	
	window_set_window_handlers(window, (WindowHandlers) {
  		.load = window_load,
  		.unload = window_unload,
      });

  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, (uint8_t) 14),
	TupletCString(CITY_KEY, ""),
    TupletCString(WEATHER_TEMPERATURE_KEY, ""),
    TupletInteger(INVERT_COLOR_KEY, persist_read_bool(INVERT_COLOR_KEY)),
	TupletInteger(BLUETOOTHVIBE_KEY, persist_read_bool(BLUETOOTHVIBE_KEY)),
//    TupletInteger(HOURLYVIBE_KEY, persist_read_bool(HOURLYVIBE_KEY)),
  };

  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values,
                ARRAY_LENGTH(initial_values), sync_tuple_changed_callback,
                NULL, NULL);

  appStarted = true;

  tick_timer_service_subscribe(MINUTE_UNIT, (TickHandler) tick_handler);

  window_stack_push(window, true);

} //end handle_init

void handle_deinit(void) {
	
  app_sync_deinit(&sync);
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}