/*
 * weather_app_gui.c
 *
 *  Created on: Sep 1, 2026
 *      Author: ishan
 */

 // LVGL includes
 #include "core/lv_obj.h"
 #include "display/lv_display.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
 #include "widgets/button/lv_button.h"
 #include "widgets/label/lv_label.h"
 #include "widgets/table/lv_table.h"
 #include "widgets/keyboard/lv_keyboard.h"
 #include "widgets/textarea/lv_textarea.h"
 #include "widgets/image/lv_image.h"
 #include "weather_images.h"
 #include "widgets/spinner/lv_spinner.h"
 #include "others/observer/lv_observer.h"
 #include "widgets/checkbox/lv_checkbox.h"
 #include "widgets/dropdown/lv_dropdown.h"

 // esp/wifi stack includes
 #include "esp_wifi_types_generic.h"
 #include "esp_wifi.h"
 #include "esp_netif_net_stack.h"
 #include "esp_netif.h"
 #include "esp_event.h"
 #include "esp_http_client.h"
 #include "esp_task_wdt.h"

 // generic includes
 #include <stdint.h>
 #include <stdio.h>
 #include "esp_log.h"
 #include "lwjson/lwjson.h"


 #define MAX_WIFI_CONNECT_FAILURES	5
 #define WIFI_CONNECTED_BIT 			BIT0
 #define WIFI_FAIL_BIT      			BIT1 
 #define MAX_RESPONSE_SIZE			1024

   
 // screens
static lv_obj_t* welcome_screen;
static lv_obj_t* aplist_screen;
static lv_obj_t* wifi_pwd_kb_screen;
static lv_obj_t* wifi_connect_process;
static lv_obj_t* weather_info_screen;
static lv_obj_t* weather_settings_screen;

// welcome screen widgets
static lv_obj_t* no_wifi_connection_label;
static lv_obj_t* connect_btn;
static lv_obj_t* connect_btn_label;

// AP list screen widgets
static lv_obj_t* table_wifi_sta;
static lv_obj_t* rescan_btn;
static lv_obj_t* rescan_btn_label;

// enter pwd scree widgets
static lv_obj_t* password_textarea;
static lv_obj_t* password_keyboard;

// weather info screen widgets

LV_IMAGE_DECLARE(image_weather_sun);
LV_IMAGE_DECLARE(image_weather_cloud);
LV_IMAGE_DECLARE(image_weather_rain);
LV_IMAGE_DECLARE(image_weather_thunder);
LV_IMAGE_DECLARE(image_weather_snow);
LV_IMAGE_DECLARE(image_weather_night);
LV_IMAGE_DECLARE(image_weather_temperature);
LV_IMAGE_DECLARE(image_weather_humidity);	
LV_IMAGE_DECLARE(image_wind);	
extern const lv_image_dsc_t wind;

static lv_obj_t* weather_image;
static lv_obj_t* wind_image;

static lv_obj_t* text_label_date;
static lv_obj_t* text_label_time;
static lv_obj_t* text_label_temp;
static lv_obj_t* text_label_humidity;
static lv_obj_t* text_label_windspeed;
static lv_obj_t* text_label_weather_description;
static lv_obj_t * text_label_time_location;
static lv_obj_t * settings_button;

static lv_obj_t* wifi_connected_label;
static lv_obj_t* error_occurred_label;

// wifi connection in process screen widgets
static lv_obj_t * connecting_wifi_spinner;
static lv_obj_t * connecting_wifi_label;

// settings screen widgets
static lv_obj_t * close_settings_button;

static lv_obj_t* city_select_label;
static lv_obj_t* temperature_unit_label;
static lv_obj_t* city_select_list;
static lv_obj_t* temp_unit_selector;
static lv_subject_t current_city_index;
static lv_obj_t* celcius_cb;
static lv_obj_t* farenheit_cb;
static lv_obj_t* close_settings_label;

// constants
uint8_t number_of_ap = 10;
static char update_time_location[30] = {0};

static char input_wifi_password[64];
static char input_wifi_station[64];
static char meteo_url_buffer[256];
static uint8_t total_response_buffer[MAX_RESPONSE_SIZE];

char current_date[20] = {0};
char current_time[20] = {0};
char temperature_str[9]= {0};
char humidity_str[8]= {0};
char windspeed_str[15]= {0};
char weather_description[20] = {0};
int is_day;
int weather_code;
int humidity;
float temperature;
float windspeed;

// local copy of weather data for LVGL GUI
char gui_current_date[20] = {0};
char gui_current_time[20] = {0};
char gui_temperature_str[9] = {0};
char gui_humidity_str[8]= {0};
char gui_windspeed_str[15]= {0};
char gui_weather_description[20]= {0};


static EventGroupHandle_t wifi_event_group;

static const char wifi_tag[] = "[WIFI Connect]";
static const char weather_station_tag[] = "[Weather Data]";

static unsigned int selected_option_index = 0;
unsigned int wifi_status = WIFI_FAIL_BIT;
unsigned int wifi_retry = 0;
static unsigned int total_reponse_len = 0;


static const char* meteo_url = "http://api.open-meteo.com/v1/forecast";
static const char* current_weather_data_query_fields = "current=temperature_2m,wind_speed_10m,precipitation,rain,weather_code,relative_humidity_2m,is_day";
static const char* hourly_weather_forecast_fields = "hourly=temperature_2m,wind_speed_10m,precipitation,rain,weather_code,relative_humidity_2m,is_day";
const char degree_symbol_c[] = "\u00B0C";
const char degree_symbol_f[] = "\u00B0F";
const char* current_temp_unit = degree_symbol_c;
static const char* const location_munich = "Munich";
static const char* const location_mumbai = "Mumbai";
static const char* const location_paris = "Paris";
static const char* const munich_latitude_longitude = "latitude=48.1371&longitude=11.5761";
static const char* const mumbai_latitude_longitude = "latitude=19.0760&longitude=72.8774";
static const char* const paris_latitude_longitude = "latitude=48.8566&longitude=2.3522";
static const char* const temp_farenheit_param = "temperature_unit=fahrenheit";

static const char* current_city = location_munich;
static const char* current_location = munich_latitude_longitude;

SemaphoreHandle_t triggerWifiConnect;
SemaphoreHandle_t weatherDataReadySemaphore;
SemaphoreHandle_t wifiReadySemaphore;

SemaphoreHandle_t weatherdataMutex;

esp_err_t _http_event_handler(esp_http_client_event_t *evt);
esp_http_client_handle_t weather_data_http_client;
static lwjson_token_t tokens[128];
static lwjson_t lwjson;
bool settings_screen = false;	
bool weather_info_active = false;

// forward declaration
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void get_weather_description(int code);
void build_meteo_weather_url();

// event callbacks
void connect_to_station(lv_event_t * e)
{
	lv_obj_t * obj = lv_event_get_target_obj(e);
	uint32_t col;
	uint32_t row;
	lv_table_get_selected_cell(obj, &row, &col);
	const char* wifi_sta_name = lv_table_get_cell_value(obj, row, col);
	ESP_LOGI(wifi_tag, "selected: %s", wifi_sta_name);
	snprintf(input_wifi_station,sizeof(input_wifi_station), "%s", wifi_sta_name);	
	
	lv_screen_load_anim(wifi_pwd_kb_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);

}

void keyboard_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if(code == LV_EVENT_VALUE_CHANGED)
	{
		 const char* text = lv_textarea_get_text(password_textarea);
		 
		 uint32_t btn_id = lv_keyboard_get_selected_button(password_keyboard);
		 const char* key = lv_keyboard_get_button_text(password_keyboard, btn_id);
		 if(strcmp(key,LV_SYMBOL_OK) == 0)
		 {
			ESP_LOGI(wifi_tag, "Current input: %s", text);	
			snprintf(input_wifi_password, sizeof(input_wifi_password), "%s", text);		

			lv_screen_load_anim(wifi_connect_process, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
			xSemaphoreGive(triggerWifiConnect);
			// this is not loading the spinner screen
			// connect_selected_sta();						
		 }
	}
	
}

void temperature_unit_event_handler(lv_event_t *e)
{
	lv_obj_t* object = lv_event_get_target_obj(e);

	lv_event_code_t code = lv_event_get_code(e);

	if(object == celcius_cb)
	{
		lv_obj_remove_state(farenheit_cb, LV_STATE_CHECKED);	
		current_temp_unit = degree_symbol_c;	
	}	
	else if(object == farenheit_cb)
	{
		lv_obj_remove_state(celcius_cb, LV_STATE_CHECKED);				
		current_temp_unit = degree_symbol_f;	
	}		
}

void weather_setting_btn_cb(lv_event_t *e)
{
	settings_screen = true;
	weather_info_active = false;
	lv_screen_load_anim(weather_settings_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
}

void close_setting_btn_cb(lv_event_t *e)
{
	build_meteo_weather_url();
	settings_screen = false;	
	weather_info_active = true;
	
	esp_http_client_set_url(weather_data_http_client, meteo_url_buffer);	
	ESP_LOGI(weather_station_tag,"Built URL: %s", meteo_url_buffer);	

	lv_screen_load_anim(weather_info_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);	
}

void select_city_dropdown_cb(lv_event_t *e)
{
	lv_obj_t* city_select_dropdown = lv_event_get_target_obj(e);
	char buf[10];
	lv_dropdown_get_selected_str(city_select_dropdown, buf, sizeof(buf));

	if(strcmp(buf,"Munich") == 0)
	{
		current_location = munich_latitude_longitude;		
		current_city = location_munich;
		selected_option_index = 0;
	}
	else if(strcmp(buf,"Mumbai") == 0)
	{
		current_location = mumbai_latitude_longitude;				
		current_city = location_mumbai;		
		selected_option_index = 1;
	}
	else if(strcmp(buf,"Paris") == 0)
	{
		current_location = paris_latitude_longitude;						
		current_city = location_paris;		
		selected_option_index = 2;
	}	
	
}

void scan_wifi_station(lv_event_t * e)
{
	uint16_t number = 10;
	wifi_ap_record_t ap_info[10];
	uint16_t ap_count = 0;
	memset(ap_info, 0, sizeof(ap_info));

	wifi_event_group = xEventGroupCreate();

	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// set wifi mode to wifi station	
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	// set the configuration
	// start the wifi
	ESP_ERROR_CHECK(esp_wifi_start());
	esp_wifi_scan_start(NULL, true);

	ESP_LOGI(wifi_tag, "Max AP number ap_info can hold = %u", number);
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

	ESP_LOGI(wifi_tag, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
	for (int i = 0; i < number; i++)
	{
		lv_table_set_cell_value(table_wifi_sta, i, 0, (char*)ap_info[i].ssid);
	    ESP_LOGI(wifi_tag, "SSID \t\t%s", ap_info[i].ssid);
	    ESP_LOGI(wifi_tag, "RSSI \t\t%d", ap_info[i].rssi);
	}

	ESP_ERROR_CHECK(esp_wifi_stop());

	
	lv_screen_load_anim(aplist_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
}

void create_welcome_screen()
{
	welcome_screen = lv_obj_create(NULL);
	no_wifi_connection_label = lv_label_create(welcome_screen);
	lv_obj_align(no_wifi_connection_label, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_text_font((lv_obj_t*) no_wifi_connection_label, &lv_font_montserrat_14, 0);
	lv_label_set_text(no_wifi_connection_label, "No Wifi Connection!!");

	connect_btn = lv_button_create(welcome_screen);
	lv_obj_align(connect_btn, LV_ALIGN_CENTER, 0, 40);
	lv_obj_add_event_cb(connect_btn, scan_wifi_station, LV_EVENT_CLICKED, NULL);

	connect_btn_label = lv_label_create(connect_btn);
	lv_label_set_text(connect_btn_label, "Connect");		
	
}

void create_ap_list_screen()
{
	aplist_screen = lv_obj_create(NULL);
	
	table_wifi_sta = lv_table_create(aplist_screen);
	lv_table_set_row_count(table_wifi_sta, number_of_ap);
	lv_table_set_column_count(table_wifi_sta, 1);
	lv_table_set_column_width(table_wifi_sta, 0, 250);
	lv_obj_align(table_wifi_sta, LV_ALIGN_TOP_MID, 10, 10);
	lv_obj_set_size(table_wifi_sta, 250,180);
	lv_obj_add_event_cb(table_wifi_sta, connect_to_station, LV_EVENT_VALUE_CHANGED, NULL);


	rescan_btn = lv_button_create(aplist_screen);
	lv_obj_align(rescan_btn, LV_ALIGN_CENTER, 0, 90);
//	lv_obj_add_event_cb(rescan_btn, weather_setting_btn_cb, LV_EVENT_CLICKED, aplist_screen);

	rescan_btn_label = lv_label_create(rescan_btn);
	lv_label_set_text(rescan_btn_label, "Rescan");	

}

void create_wifi_pwd_screen()
{
	wifi_pwd_kb_screen = lv_obj_create(NULL);
	
	password_textarea = lv_textarea_create(wifi_pwd_kb_screen);
    lv_obj_set_align(password_textarea, LV_ALIGN_TOP_MID);
    lv_obj_set_y(password_textarea, 10);
    lv_obj_set_width(password_textarea, lv_pct(90));
    lv_textarea_set_one_line(password_textarea, true);
    lv_textarea_set_placeholder_text(password_textarea, "Type password here...");
	
    password_keyboard = lv_keyboard_create(wifi_pwd_kb_screen);
    lv_obj_set_align(password_keyboard, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_size(password_keyboard, lv_pct(100), lv_pct(60));
    lv_keyboard_set_mode(password_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
		
	lv_keyboard_set_textarea(password_keyboard, password_textarea);
	lv_obj_add_event_cb(password_keyboard, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
}

void create_weather_info_screen()
{
	weather_info_screen = lv_obj_create(NULL);

	wifi_connected_label = lv_label_create(weather_info_screen);
	lv_obj_align(wifi_connected_label, LV_ALIGN_TOP_RIGHT, -10, 10);
	lv_label_set_text(wifi_connected_label, "");		

	weather_image = lv_image_create(weather_info_screen);
	lv_obj_align(weather_image, LV_ALIGN_CENTER, -80, -20);

	settings_button = lv_button_create(weather_info_screen);
	lv_obj_t* setting_label = lv_label_create(settings_button);
	lv_obj_set_style_bg_opa(settings_button, LV_OPA_TRANSP,0);		  
	lv_obj_align(settings_button, LV_ALIGN_BOTTOM_LEFT, 10, -10);
	lv_obj_add_event_cb(settings_button, weather_setting_btn_cb, LV_EVENT_CLICKED, NULL);
	lv_label_set_text(setting_label, LV_SYMBOL_SETTINGS);

	text_label_weather_description = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_weather_description, LV_ALIGN_BOTTOM_MID, 0, -40);
	lv_obj_set_style_text_font((lv_obj_t*) text_label_weather_description, &lv_font_montserrat_14, 0);

	// Create a text label for the time and timezone aligned center in the bottom of the screen
	text_label_time_location = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_time_location, LV_ALIGN_BOTTOM_MID, 0, -10);

	///////////////////////////////////////////////////////////////////////////////////////////////////////
		
	text_label_date = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_date, LV_ALIGN_CENTER, 70, -70);
	  	  
	lv_obj_t * weather_image_temperature = lv_image_create(weather_info_screen);
	lv_image_set_src(weather_image_temperature, &image_weather_temperature);
	lv_obj_align(weather_image_temperature, LV_ALIGN_CENTER, 30, -35);
	  
	text_label_temp = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_temp, LV_ALIGN_CENTER, 70, -35);
	lv_obj_set_style_text_font((lv_obj_t*) text_label_temp, &lv_font_montserrat_14, 0);

	lv_obj_t * weather_image_humidity = lv_image_create(weather_info_screen);
	lv_image_set_src(weather_image_humidity, &image_weather_humidity);
	lv_obj_align(weather_image_humidity, LV_ALIGN_CENTER, 30, 00);
		  
	text_label_humidity = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_humidity, LV_ALIGN_CENTER, 70, 00);
	lv_obj_set_style_text_font((lv_obj_t*) text_label_humidity, &lv_font_montserrat_14, 0);

	text_label_windspeed = lv_label_create(weather_info_screen);
	lv_obj_align(text_label_windspeed, LV_ALIGN_CENTER, 90, 30);
	lv_obj_set_style_text_font((lv_obj_t*) text_label_windspeed, &lv_font_montserrat_14, 0);

	lv_obj_t* wind_image = lv_image_create(weather_info_screen);
	lv_image_set_src(wind_image, &wind);
	lv_obj_align(wind_image, LV_ALIGN_CENTER, 30, 30);	
}

void create_wifi_connect_process_screen()
{
	wifi_connect_process = lv_obj_create(NULL);	
	
	connecting_wifi_spinner = lv_spinner_create(wifi_connect_process);
	lv_obj_set_size(connecting_wifi_spinner, 80, 80);
	lv_spinner_set_anim_params(connecting_wifi_spinner, 1000, 270);
	lv_obj_align(connecting_wifi_spinner, LV_ALIGN_CENTER, 0, 0);

	connecting_wifi_label = lv_label_create(wifi_connect_process);
	lv_obj_align(connecting_wifi_label, LV_ALIGN_CENTER, 0, -60);
	lv_label_set_text(connecting_wifi_label, "Connecting...");		
}

void create_weather_settings_screen()
{
	weather_settings_screen = lv_obj_create(NULL);
	
	close_settings_button = lv_button_create(weather_settings_screen);
	lv_obj_add_event_cb(close_settings_button, close_setting_btn_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_set_style_bg_opa(close_settings_button, LV_OPA_TRANSP,0);		  
	lv_obj_align(close_settings_button, LV_ALIGN_BOTTOM_LEFT, 10, -10);

	close_settings_label = lv_label_create(close_settings_button);
	lv_label_set_text(close_settings_label, LV_SYMBOL_CLOSE);		

	city_select_label = lv_label_create(weather_settings_screen);
	lv_label_set_text(city_select_label, "Select city:");
	lv_obj_align(city_select_label, LV_ALIGN_TOP_LEFT, 10, 10);
	lv_obj_set_style_text_font((lv_obj_t*) city_select_label, &lv_font_montserrat_14, 0);

	city_select_list = lv_dropdown_create(weather_settings_screen);
	lv_obj_align(city_select_list, LV_ALIGN_TOP_LEFT, 10, 30);
	lv_dropdown_set_options(city_select_list, "Munich\nMumbai\nParis");
	lv_dropdown_set_selected(city_select_list, selected_option_index);
	lv_obj_add_event_cb(city_select_list, select_city_dropdown_cb, LV_EVENT_VALUE_CHANGED, NULL);

	temperature_unit_label = lv_label_create(weather_settings_screen);
	lv_label_set_text(temperature_unit_label, "Select unit:");
	lv_obj_align(temperature_unit_label, LV_ALIGN_TOP_LEFT, 10, 70);
	lv_obj_set_style_text_font((lv_obj_t*) temperature_unit_label, &lv_font_montserrat_14, 0);
		
	celcius_cb = lv_checkbox_create(weather_settings_screen);
	lv_checkbox_set_text(celcius_cb, "Celcius");
	lv_obj_align(celcius_cb, LV_ALIGN_TOP_LEFT, 10, 100);
	lv_obj_add_event_cb(celcius_cb, temperature_unit_event_handler, LV_EVENT_VALUE_CHANGED,NULL);

	farenheit_cb = lv_checkbox_create(weather_settings_screen);
	lv_checkbox_set_text(farenheit_cb, "Farenheit");
	lv_obj_align(farenheit_cb, LV_ALIGN_TOP_LEFT, 10, 130);
	lv_obj_add_event_cb(farenheit_cb, temperature_unit_event_handler, LV_EVENT_VALUE_CHANGED,NULL);

	if(strcmp(current_temp_unit,degree_symbol_c) == 0)
	{
		lv_obj_set_state(celcius_cb, LV_STATE_CHECKED, true);		
		lv_obj_remove_state(farenheit_cb, LV_STATE_CHECKED);			
	}		
	else 
	{
		lv_obj_set_state(farenheit_cb, LV_STATE_CHECKED, true);		
		lv_obj_remove_state(celcius_cb, LV_STATE_CHECKED);				
	}
	
}

void build_gui()
{
	create_welcome_screen();
	create_ap_list_screen();
	create_wifi_pwd_screen();
	create_weather_info_screen();
	create_weather_settings_screen();
	create_wifi_connect_process_screen();
}

void start_weather_app_gui()
{
	build_gui();
	lv_screen_load(welcome_screen);
}

void update_weather_labels()
{	
	if(weather_info_active == false)
	{
		return;		
	}
	
	if(xSemaphoreTake(weatherdataMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
	{
		if(current_date[0] != '\0')
		{
			memcpy(gui_current_date, current_date, sizeof(gui_current_date));			
		}
		
		if(temperature_str[0] != '\0')
		{
			memcpy(gui_temperature_str, temperature_str, sizeof(gui_temperature_str));			
		}
		
		if(humidity_str[0] != '\0')
		{
			memcpy(gui_humidity_str, humidity_str, sizeof(gui_humidity_str));			
		}
		
		if(weather_description[0] != '\0')
		{
			memcpy(gui_weather_description, weather_description, sizeof(gui_weather_description));			
		}
				
		if(windspeed_str[0] != '\0')
		{
			memcpy(gui_windspeed_str, windspeed_str, sizeof(gui_windspeed_str));			
		}
		
		if(current_time[0] != '\0')
		{
			memcpy(gui_current_time, current_time, sizeof(gui_current_time));			
		}		
								
		xSemaphoreGive(weatherdataMutex);
	}

	get_weather_description(weather_code);
	lv_label_set_text(text_label_date, gui_current_date);	
	lv_label_set_text(text_label_temp, gui_temperature_str);
	lv_label_set_text(text_label_humidity, gui_humidity_str);
	lv_label_set_text(text_label_weather_description, gui_weather_description);
	lv_label_set_text(text_label_windspeed, gui_windspeed_str);				
	snprintf(update_time_location, sizeof(update_time_location), "%s | %s", gui_current_time, current_city);
	lv_label_set_text(text_label_time_location, update_time_location);					

}

void start_weather_data_update()
{
	weather_info_active = true;
	lv_screen_load_anim(weather_info_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
	xSemaphoreGive(wifiReadySemaphore);			
}


static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
 	{
 		ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
 		ESP_LOGI(wifi_tag,"STA IP: " IPSTR,IP2STR(&event->ip_info.ip));
 		wifi_retry=0;
 		xEventGroupSetBits(wifi_event_group,WIFI_CONNECTED_BIT);
	}
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
 	if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
 	{
 		ESP_LOGI(wifi_tag,"Connecting to AP....");
 		esp_wifi_connect();
 	}
 	else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
 	{
 		if(wifi_retry < MAX_WIFI_CONNECT_FAILURES)
 		{
 			ESP_LOGI(wifi_tag,"Reconnecting to AP....");
 			esp_wifi_connect();
 			wifi_retry++;
			
 		}	
 		else
 		{
 			ESP_LOGI(wifi_tag,"Could not connect to WIFI!!");
 			xEventGroupSetBits(wifi_event_group,WIFI_FAIL_BIT);
			wifi_retry = 0;
 		}
	}
}

void connect_selected_sta(void* arg)
{	
	while(1)
	{
		if(xSemaphoreTake(triggerWifiConnect, portMAX_DELAY) == pdTRUE)
		{	
		 	ESP_LOGI(wifi_tag,"Connecting to: %s", input_wifi_station);
		 	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
			wifi_event_group = xEventGroupCreate();
		
		 	wifi_config_t wifi_config = {
		 		.sta = {
		// 			.ssid = "Vodafone-67F4",
		// 			.password="nxPymbJTYJ9F2dZM",
		                       
		 			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
		 			.pmf_cfg = {
		 				.capable=true,
		 				.required=false
		 			},
		 			},
		 		};
			
			memcpy(wifi_config.sta.ssid, input_wifi_station, sizeof(input_wifi_station));
			memcpy(wifi_config.sta.password, input_wifi_password, sizeof(input_wifi_password));
			
		 	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
		 	// set connect to wifi event handler	
		 	esp_event_handler_instance_t wifi_handler_event_instance;
		 	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event_handler,NULL,&wifi_handler_event_instance));
		
		 	// set obtained IP event handler
		 	esp_event_handler_instance_t got_ip_event_instance;
		 	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&ip_event_handler,NULL,&got_ip_event_instance));
		 	
		 	ESP_ERROR_CHECK(esp_wifi_start());
		 	
		 	// wait until either WIFI_SUCCESS or WIFI_FAILURE bits are set in the wifi_event_group
		 	EventBits_t bits = xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
		 	
		 	// check the set value inside bits
		 	if (bits & WIFI_CONNECTED_BIT)
		 	{
		 		wifi_status = WIFI_CONNECTED_BIT;
				
				// deregister handlers and delete event group
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,got_ip_event_instance));
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_handler_event_instance));
				vEventGroupDelete(wifi_event_group);
				start_weather_data_update();
				
		 	}
		 	else if(bits & WIFI_FAIL_BIT)
		 	{
				// deregister handlers and delete event group
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,got_ip_event_instance));
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_handler_event_instance));
				vEventGroupDelete(wifi_event_group);
				ESP_ERROR_CHECK(esp_wifi_stop());
		 		wifi_status = WIFI_FAIL_BIT;	
				// transition to list of AP screen
				lv_screen_load_anim(aplist_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
		 	}
		 	else
		 	{
				// deregister handlers and delete event group
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,got_ip_event_instance));
				ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_handler_event_instance));
				vEventGroupDelete(wifi_event_group);
				ESP_ERROR_CHECK(esp_wifi_stop());
		 		ESP_LOGE(wifi_tag,"Unexpected event");
		 		wifi_status = WIFI_FAIL_BIT;
				// transition to list of AP screen
				lv_screen_load_anim(aplist_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
				
		 	}
		}
	}
}

void build_meteo_weather_url()
{
	uint32_t offset = 0;
	memset(meteo_url_buffer, 0, sizeof(meteo_url_buffer));
	
	snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "%s", meteo_url);
	offset += strlen(meteo_url);

	snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "?");
	offset += 1;

	snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "%s", current_location);
	offset += strlen(current_location);

	snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "&");
	offset += 1;
		
	snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "%s", current_weather_data_query_fields);
	offset += strlen(current_weather_data_query_fields);					
	
	if(strcmp(current_temp_unit,degree_symbol_f) == 0)
	{
		snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "&");
		offset += 1;		

		snprintf(meteo_url_buffer + offset, sizeof(meteo_url_buffer), "%s", temp_farenheit_param);
		offset += strlen(temp_farenheit_param);				
	}	
	
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{			
	switch(evt->event_id) 
	{
        case HTTP_EVENT_ERROR:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_HEADERS_COMPLETE:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_ON_HEADERS_COMPLETE");
            break;
		case HTTP_EVENT_ON_DATA:		
			if((total_reponse_len+evt->data_len) >= MAX_RESPONSE_SIZE)
			{
				ESP_LOGI(weather_station_tag, "Reponse too big.....");
			}
			else 
			{
				memcpy(total_response_buffer + total_reponse_len, evt->data, evt->data_len);
				total_reponse_len += evt->data_len;
				ESP_LOGI(weather_station_tag, "HTTP_EVENT_ON_DATA, cur_len=%d", total_reponse_len);			
			}
            break;		
			
		case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_ON_FINISH, content_len = %u", total_reponse_len);
			total_reponse_len = 0;
			xSemaphoreGive(weatherDataReadySemaphore);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(weather_station_tag, "HTTP_EVENT_REDIRECT");
            break;
        default:
            break;			
			
	}	
	return ESP_OK;
}

void initialize_http_client()
{		
	build_meteo_weather_url();
	ESP_LOGI(weather_station_tag, "HTTP request with url => %s", meteo_url);

	esp_http_client_config_t weather_data_http_client_config = 
	{
		.method = HTTP_METHOD_GET,
        .url = meteo_url_buffer,
        .event_handler = _http_event_handler,
        .disable_auto_redirect = true,
	};
		
	weather_data_http_client = esp_http_client_init(&weather_data_http_client_config);		
	lwjson_init(&lwjson, tokens, LWJSON_ARRAYSIZE(tokens));	
}

static void get_weather_description(int code) {
  switch (code) {
    case 0:
	case 1: 
      if(is_day==1) { lv_image_set_src(weather_image, &image_weather_sun); }
      else { lv_image_set_src(weather_image, &image_weather_night); }
      memcpy(weather_description, "CLEAR SKY", sizeof(weather_description));
      break;
    case 2: 
      lv_image_set_src(weather_image, &image_weather_cloud);
	  memcpy(weather_description, "PARTLY CLOUDY", sizeof(weather_description));
      break;
    case 3:
      lv_image_set_src(weather_image, &image_weather_cloud);
      memcpy(weather_description, "OVERCAST", sizeof(weather_description));
      break;
    case 45:
      lv_image_set_src(weather_image, &image_weather_cloud);
      memcpy(weather_description, "FOG", sizeof(weather_description));
      break;
    case 48:
      lv_image_set_src(weather_image, &image_weather_cloud);
	  memcpy(weather_description, "DEPOSITING RIME FOG", sizeof(weather_description));
      break;
    case 51:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "DRIZZLE LIGHT INTENSITY", sizeof(weather_description));
      break;
    case 53:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "DRIZZLE MODERATE INTENSITY", sizeof(weather_description));
      break;
    case 55:
      lv_image_set_src(weather_image, &image_weather_rain); 
	  memcpy(weather_description, "DRIZZLE DENSE INTENSITY", sizeof(weather_description));
      break;
    case 56:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "FREEZING DRIZZLE LIGHT", sizeof(weather_description));
      break;
    case 57:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "FREEZING DRIZZLE DENSE", sizeof(weather_description));
      break;
    case 61:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "RAIN SLIGHT INTENSITY", sizeof(weather_description));
      break;
    case 63:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "RAIN MODERATE INTENSITY", sizeof(weather_description));
      break;
    case 65:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "RAIN HEAVY INTENSITY", sizeof(weather_description));
      break;
    case 66:
      lv_image_set_src(weather_image, &image_weather_rain);
	  memcpy(weather_description, "FREEZING RAIN LIGHT INTENSITY", sizeof(weather_description));
      break;
   case 67:
     lv_image_set_src(weather_image, &image_weather_rain);
	 memcpy(weather_description, "FREEZING RAIN HEAVY INTENSITY", sizeof(weather_description));
     break;
   case 71:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description, "SNOW FALL SLIGHT INTENSITY", sizeof(weather_description));
     break;
   case 73:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description, "SNOW FALL MODERATE INTENSITY", sizeof(weather_description));
     break;
   case 75:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description, "SNOW FALL HEAVY INTENSITY", sizeof(weather_description));
     break;
   case 77:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description, "SNOW GRAINS", sizeof(weather_description));
     break;
   case 80:
     lv_image_set_src(weather_image, &image_weather_rain);
	 memcpy(weather_description,"RAIN SHOWERS SLIGHT", sizeof(weather_description));
     break;
   case 81:
     lv_image_set_src(weather_image, &image_weather_rain);
	 memcpy(weather_description,"RAIN SHOWERS MODERATE", sizeof(weather_description));
     break;
   case 82:
     lv_image_set_src(weather_image, &image_weather_rain);
	 memcpy(weather_description,"RAIN SHOWERS VIOLENT", sizeof(weather_description));
     break;
   case 85:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description,"SNOW SHOWERS SLIGHT", sizeof(weather_description));
     break;
   case 86:
     lv_image_set_src(weather_image, &image_weather_snow);
	 memcpy(weather_description,"SNOW SHOWERS HEAVY", sizeof(weather_description));
     break;
   case 95:
     lv_image_set_src(weather_image, &image_weather_thunder);
	 memcpy(weather_description,"THUNDERSTORM", sizeof(weather_description));
     break;
   case 96:
     lv_image_set_src(weather_image, &image_weather_thunder);
	 memcpy(weather_description,"THUNDERSTORM SLIGHT HAIL", sizeof(weather_description));
     break;
   case 99:
     lv_image_set_src(weather_image, &image_weather_thunder);
	 memcpy(weather_description, "THUNDERSTORM HEAVY HAIL", sizeof(weather_description));
     break;
    default: 
	  memcpy(weather_description, "UNKNOWN WEATHER CODE", sizeof(weather_description));
      break;
  }
}

void parse_weather_data_json(void* arg)
{
	while(1)
	{			
		if(xSemaphoreTake(weatherDataReadySemaphore, portMAX_DELAY) == pdTRUE)
		{
			esp_task_wdt_reset();

			if (lwjson_parse(&lwjson, (char*)total_response_buffer) == lwjsonOK) 
			{
			    const lwjson_token_t* t;
			
			    /* Find custom key in JSON */
			    if ((t = lwjson_find(&lwjson, "current")) != NULL) 
				{
			        ESP_LOGI(weather_station_tag,"Key found with data type: %d\r\n", (int)t->type);
					if(t->type == LWJSON_TYPE_OBJECT)
					{
						for (const lwjson_token_t* tkn = lwjson_get_first_child(t); tkn != NULL; tkn = tkn->next) 
						{					
							if(xSemaphoreTake(weatherdataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
							{								
								if(strncmp(tkn->token_name,"time",tkn->token_name_len) == 0)
								{
									char* found = strchr(tkn->u.str.token_value,'T');
									if(found)
									{
										unsigned int date_len = found - tkn->u.str.token_value;
										unsigned int time_len = tkn->u.str.token_value_len - date_len;
										
										snprintf(current_date, date_len+1, "%s", tkn->u.str.token_value);
										snprintf(current_time, time_len, "%s", tkn->u.str.token_value+date_len+1);
										ESP_LOGI(weather_station_tag, "%s %s", current_date, current_time);
										
									}
								}
								else if(strncmp(tkn->token_name,"temperature_2m",tkn->token_name_len) == 0)
								{
									temperature = tkn->u.num_real;
									snprintf(temperature_str,sizeof(temperature_str),"%.2f%s", temperature, current_temp_unit);
								}
								else if(strncmp(tkn->token_name,"wind_speed_10m",tkn->token_name_len) == 0)
								{
									windspeed = tkn->u.num_real;
									snprintf(windspeed_str,sizeof(windspeed_str),"%.2f km/h", windspeed);
								}
								else if(strncmp(tkn->token_name,"precipitation",tkn->token_name_len) == 0)
								{
									
								}
								else if(strncmp(tkn->token_name,"rain",tkn->token_name_len) == 0)
								{
									
								}					
								else if(strncmp(tkn->token_name,"weather_code",tkn->token_name_len) == 0)
								{
									weather_code = tkn->u.num_int;
								}
								else if(strncmp(tkn->token_name,"relative_humidity_2m",tkn->token_name_len) == 0)
								{
									humidity = tkn->u.num_int;		
									snprintf(humidity_str,sizeof(humidity_str), "%d %%", humidity);
								}
								else if(strncmp(tkn->token_name,"is_day",tkn->token_name_len) == 0)
								{
									is_day = tkn->u.num_int;
								}
								
								xSemaphoreGive(weatherdataMutex);
							}
							
						}
					}
					else
					{
						ESP_LOGI(weather_station_tag, "Type of \"current\" token is not OBJECT: %d", (int)t->type);			
					}
			    }
				else 
				{
					ESP_LOGI(weather_station_tag, "Could not parse the current data token in JSON!!");	
				}
		
			    lwjson_free(&lwjson);
				ESP_LOGI(weather_station_tag, "%s, %s, %f, %f, %d, %d, %d", current_date, current_time, temperature, windspeed, weather_code, humidity, is_day);																									
				
		
			}
			else 
			{
				ESP_LOGI(weather_station_tag, "Could not parse JSON: \n %s", total_response_buffer);
			}
			
			memset(total_response_buffer,0,sizeof(total_response_buffer));
		}	
		else 
		{
			esp_task_wdt_reset();

		}		
		
	}
	
}

