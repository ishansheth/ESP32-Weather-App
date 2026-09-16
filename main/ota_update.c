/*
 * ota_update.c
 *
 *  Created on: Sep 16, 2026
 *      Author: ishan
 */
 #include "esp_ota_ops.h"
 #include "esp_http_client.h"
 #include "esp_app_format.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
 #include "portmacro.h"
 #include "esp_log.h"
 
 #include "widgets/label/lv_label.h"
 #include "misc/lv_event.h"
#include <sys/_types.h>

 #define BUFFSIZE 1024

extern lv_obj_t* processing_screen;
extern lv_obj_t* weather_info_screen;
extern lv_obj_t* processing_label;

esp_http_client_handle_t ota_update_http_client;
static const char update_tag[] = "[App Update]";
static char ota_write_data[BUFFSIZE + 1] = { 0 };
static char progress_percent_text[32];

bool is_ota_download_on = false;
bool is_ota_download_complete = false;
bool ota_update_result = true;

SemaphoreHandle_t triggerOtaUpdate;
SemaphoreHandle_t updateOTADownloadProgress;
SemaphoreHandle_t updateOTAResultFlag;

void show_status_messagebox(char* message, lv_obj_t* parent);
 
static void http_cleanup(esp_http_client_handle_t client)
{
//     esp_http_client_cleanup(client);
	 esp_http_client_close(client);
}


 void app_update_button_cb(lv_event_t * e)
 {		
	ESP_LOGI(update_tag, "triggering FW update.....");
	lv_screen_load_anim(processing_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);
	is_ota_download_on = true;
	esp_err_t err = esp_http_client_open(ota_update_http_client, 0);
	if(err != ESP_OK)
	{
		ESP_LOGE(update_tag, "Failed to open HTTP connection: %s", esp_err_to_name(err));		
		lv_screen_load_anim(weather_info_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);	
		show_status_messagebox("No connection to firmware server",weather_info_screen);
	}
	else 
	{	
		xSemaphoreGive(triggerOtaUpdate);
	}

 }	
 
void update_ota_download_precentage_label()
{
	if(is_ota_download_on)
	{
		if(xSemaphoreTake(updateOTADownloadProgress, portMAX_DELAY) == pdTRUE)
		{
			lv_label_set_text(processing_label, progress_percent_text);			 							
			xSemaphoreGive(updateOTADownloadProgress);
		}
	}
}

 void check_ota_update_state()
 {
	if(is_ota_download_on)
	{
		if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
		{
			if(is_ota_download_complete && !ota_update_result)
			{
				// if update failed
				show_status_messagebox("Firmware update failed!!",weather_info_screen);
				lv_screen_load_anim(weather_info_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300,0,false);				
				is_ota_download_complete = false;
				ota_update_result = true;
				is_ota_download_on = false;
			}
			
			xSemaphoreGive(updateOTAResultFlag);			
		}		
	}
	
 }
 
 bool isUpdateNecessary(char* new_version, char* running_version)
 {
	unsigned int new_version_parts[3] = {0};
	unsigned int running_version_parts[3] = {0};

	int last_dot = 0;
	unsigned int idx = 0;
	char version_part[3];
	unsigned i = 0;

	for(i = 0; i < strlen(new_version); i++)
	{
		if(new_version[i] == '.')
		{
			strncpy(version_part, &new_version[last_dot], i - last_dot);
			new_version_parts[idx] = atoi(version_part);
			last_dot = i+1;
			idx += 1;	
		}		
	}
	strncpy(version_part, &new_version[last_dot], i - last_dot);
	new_version_parts[idx] = atoi(version_part);

	last_dot = 0;
	idx = 0;

	for(i = 0; i < strlen(running_version); i++)
	{
		if(running_version[i] == '.')
		{
			strncpy(version_part, &running_version[last_dot], i - last_dot);
			running_version_parts[idx] = atoi(version_part);
			last_dot = i+1;
			idx += 1;	
		}		
	}
	strncpy(version_part, &running_version[last_dot], i - last_dot);
	running_version_parts[idx] = atoi(version_part);
	
	if(running_version_parts[0] > new_version_parts[0])
	{
		return false;
	}
	else if(running_version_parts[0] < new_version_parts[0])
	{
		return true;			
	}
	else
	{
		// equal major
		if(running_version_parts[1] > new_version_parts[1])		
		{
			return false;
			
		}
		else if(running_version_parts[1] < new_version_parts[1])
		{
			return true;						
		}
		else 
		{
			// equal minor
			if(running_version_parts[2] > new_version_parts[2])		
			{
				return false;				
			}
			else if(running_version_parts[2] < new_version_parts[2])
			{
				return true;						
			}
			else 
			{
				// equal patch
				return false;				
			}		
		}
	}	
 }

 
 void ota_update_task(void* parameter)
 {
	while(1)
	{
		if(xSemaphoreTake(triggerOtaUpdate, portMAX_DELAY) == pdTRUE)
		{			
		 	esp_ota_handle_t update_handle = 0 ;
					 	
		 	const esp_partition_t *configured = esp_ota_get_boot_partition();
		 	const esp_partition_t *running = esp_ota_get_running_partition();
		 	const esp_partition_t *update_partition = NULL;
		 	unsigned int binary_file_length = 0;
			 	
		 	if (configured != running) 
		 	{
		 	    ESP_LOGW(update_tag, "Configured OTA boot partition at offset 0x%08"PRIx32", but running from offset 0x%08"PRIx32,
		 	             configured->address, running->address);
		 	    ESP_LOGW(update_tag, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
		 	}
		 	ESP_LOGI(update_tag, "Running partition type %d subtype %d (offset 0x%08"PRIx32")",
		 	         running->type, running->subtype, running->address);
			 			 
		 	update_partition = esp_ota_get_next_update_partition(NULL);
		 	assert(update_partition != NULL);
		 	ESP_LOGI(update_tag, "Writing to partition subtype %d at offset 0x%"PRIx32,
		 	         update_partition->subtype, update_partition->address);
			 	
		 	esp_http_client_fetch_headers(ota_update_http_client);
				
		 	bool image_header_was_checked = false;
			
			unsigned int total_image_size = esp_http_client_get_content_length(ota_update_http_client);
				
			ESP_LOGI(update_tag, "OTA image size: %u", total_image_size);
		 	esp_err_t err;
				
		 	while (1) 
		 	{
		 	    int data_read = esp_http_client_read(ota_update_http_client, ota_write_data, BUFFSIZE);
		 		if(data_read < 0)
		 		{
		 			ESP_LOGE(update_tag, "Error: SSL data read error");		
		 			http_cleanup(ota_update_http_client);	
					
					if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
					{
						ota_update_result = false;
						is_ota_download_complete = true;
						xSemaphoreGive(updateOTAResultFlag);									
					}
					
					break;
		 		}
		 		else if(data_read > 0)
		 		{
		 			if (image_header_was_checked == false) 
		 			{
		 				image_header_was_checked = true;
			 				
		 				esp_app_desc_t new_app_info;
		 				esp_app_desc_t running_app_info;
		 				memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
		 				ESP_LOGI(update_tag, "New firmware version: %s", new_app_info.version);
		 	
		 		        if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) 
		 				{
		 		            ESP_LOGI(update_tag, "Running firmware version: %s", running_app_info.version);
		 				}		
		 				
						if(!isUpdateNecessary(new_app_info.version, running_app_info.version))
						{
							ESP_LOGE(update_tag, "New version is older than running verrsion");
							
							if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
							{
								ota_update_result = false;
								is_ota_download_complete = true;
								xSemaphoreGive(updateOTAResultFlag);									
							}							
							break;							
						}
						
		 				err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
		 											
		 				if(err != ESP_OK)
		 				{
		 					ESP_LOGE(update_tag, "esp_ota_begin failed!!");
		 					http_cleanup(ota_update_http_client);
		 					esp_ota_abort(update_handle);
							
							if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
							{
								ota_update_result = false;
								is_ota_download_complete = true;
								xSemaphoreGive(updateOTAResultFlag);									
							}														
							break;
		 				}
		 			}
		 			
		 			err = esp_ota_write( update_handle, (const void *)ota_write_data, data_read);
		 			if (err != ESP_OK) 
		 			{
		 				ESP_LOGE(update_tag, "esp_ota_write failed!!");
		 				http_cleanup(ota_update_http_client);			
		 				esp_ota_abort(update_handle);	
						
						if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
						{
							ota_update_result = false;
							is_ota_download_complete = true;
							xSemaphoreGive(updateOTAResultFlag);									
						}																				
						break;
		 			}
		 	
		 			binary_file_length += data_read;
						
					if(xSemaphoreTake(updateOTADownloadProgress, portMAX_DELAY) == pdTRUE)
					{
						float progress_percent = ((float)binary_file_length / (float)total_image_size)*100;
						ESP_LOGI(update_tag, "until now: %d, total size: %d, percentage: %.2f", binary_file_length, total_image_size, progress_percent);
						snprintf(progress_percent_text, sizeof(progress_percent_text), "Updating.... %.2f%%", progress_percent);
						xSemaphoreGive(updateOTADownloadProgress);
					}
		 			ESP_LOGI(update_tag, "Written image length %d", binary_file_length);
			 	
		 		}
		 		else if(data_read == 0)
		 		{
		 			if (errno == ECONNRESET || errno == ENOTCONN) 
					{
		 			    ESP_LOGE(update_tag, "Connection closed, errno = %d", errno);
		 			    break;
		 			}
		 			if (esp_http_client_is_complete_data_received(ota_update_http_client) == true) {
		 			    ESP_LOGI(update_tag, "Connection closed");
		 			    break;
		 			}
			 	
		 		}
		 	}
			 	
		 	ESP_LOGI(update_tag, "Total Write binary data length: %d", binary_file_length);
			
		 	if (esp_http_client_is_complete_data_received(ota_update_http_client) == true) 
		 	{
				err = esp_ota_end(update_handle);
				if (err == ESP_OK) 
				{
					err = esp_ota_set_boot_partition(update_partition);
					if (err == ESP_OK) 
					{
						ESP_LOGI(update_tag, "Prepare to restart system!");
						esp_restart();				
					}
					else 
					{
						ESP_LOGE(update_tag, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
						http_cleanup(ota_update_http_client);
						esp_ota_abort(update_handle);					
						
						if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
						{
							ota_update_result = false;
							is_ota_download_complete = true;
							xSemaphoreGive(updateOTAResultFlag);			
							
						}

					}					
				}
				else 
				{
				    if (err == ESP_ERR_OTA_VALIDATE_FAILED) 
					{
				        ESP_LOGE(update_tag, "Image validation failed, image is corrupted");
				    } 
					else 
					{
				        ESP_LOGE(update_tag, "esp_ota_end failed (%s)!", esp_err_to_name(err));
				    }
				    http_cleanup(ota_update_http_client);
					esp_ota_abort(update_handle);				

					if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
					{
						ota_update_result = false;
						is_ota_download_complete = true;
						xSemaphoreGive(updateOTAResultFlag);									
					}
										
				}
			}	
			else 
			{
				ESP_LOGE(update_tag, "Error in receiving complete file");
				http_cleanup(ota_update_http_client);
				esp_ota_abort(update_handle);			
				if(xSemaphoreTake(updateOTAResultFlag, portMAX_DELAY) == pdTRUE)
				{
					ota_update_result = false;
					is_ota_download_complete = true;
					xSemaphoreGive(updateOTAResultFlag);									
				}
			}			 	
		}		
	}
}
 
 void initialize_ota_update_client()
 {
	esp_http_client_config_t config = 
	{
	    .url = "http://192.168.0.164:8000/app-template.bin",
	    .timeout_ms = 5000,
	    .keep_alive_enable = true,
	};

	ota_update_http_client = esp_http_client_init(&config);
	if (ota_update_http_client == NULL) 
	{
	    ESP_LOGE(update_tag, "Failed to initialise HTTP connection");
	}

}
