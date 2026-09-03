/*
 * lvgl_filesystem.c
 *
 *  Created on: Aug 21, 2026
 *      Author: ishan
 */
#include "lvgl.h"
#include <string.h>
#include "esp_log.h"
#include <stdio.h>
#include "esp_spiffs.h"
#include "misc/lv_fs.h"

static const char* TAG = "[LVGL_FS]";

static void * spiffs_fs_open(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
 	char fullpath[256];
 	snprintf(fullpath, sizeof(fullpath), "/spiffs/%s",path);
 	ESP_LOGI(TAG, "Trying to open file in callback: %s", path);
 	
 	const char* mode_str;

     if(mode == LV_FS_MODE_WR) 
 	{
 		mode_str = "wb";		
     }
     else if(mode == LV_FS_MODE_RD) 
 	{
 		mode_str = "rb";				
     }
     else 
 	{
 		mode_str = "rb+";		
     }
 	
 	FILE* f = fopen(fullpath, mode_str);
 	if(f == NULL)
 	{
 		ESP_LOGE(TAG, "Failed to open file system via LVGL in callback: %s", fullpath);
 	}
     return f;
}


static lv_fs_res_t spiffs_fs_seek(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
     /*Add your code here*/
	 FILE* fp = file_p;
	 
	 int fwhence = 0;
	 if (whence == LV_FS_SEEK_SET)
	 {
		fwhence = SEEK_SET;
	 }
	 else if(whence == LV_FS_SEEK_CUR)
	 {
		fwhence = SEEK_CUR;
	 }
	 else 
	 {
	 	fwhence = SEEK_END;
	 }
	 
	 if(fseek(fp,pos,fwhence) == 0)
	 {
		ESP_LOGI(TAG, "seek successful");
		return LV_FS_RES_OK;
	 }
	 else 
	 {
		ESP_LOGE(TAG, "seek failed!!!");
		return LV_FS_RES_FS_ERR;	 
	 }

}

static lv_fs_res_t spiffs_fs_close(lv_fs_drv_t * drv, void * file_p)
{
	 FILE* fp = file_p;
	 ESP_LOGI(TAG, "spiffs close");
     /*Add your code here*/
	fclose(fp);
     return LV_FS_RES_OK;
}
  
static lv_fs_res_t spiffs_fs_read(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
	 FILE* fp = file_p;
	 *br = fread(buf, sizeof(uint8_t),btr, fp);
     /*Add your code here*/
	 ESP_LOGI(TAG, "spiffs read: input elements to read: %u, read elements: %u", btr, *br);

     return LV_FS_RES_OK;
} 
 
static lv_fs_res_t spiffs_fs_tell(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
	FILE* fp = file_p;
	*pos_p = ftell(fp);	
     /*Add your code here*/
	 ESP_LOGI(TAG, "spiffs tell position: %u", *pos_p);
     return LV_FS_RES_OK;
 }

 
 void init_get_spiffs_info()
 {
 	ESP_LOGI(TAG, "Initializing SPIFFS");

 	esp_vfs_spiffs_conf_t conf = {
 	  .base_path = "/spiffs",
 	  .partition_label = NULL,
 	  .max_files = 9,
 	  .format_if_mount_failed = false
 	};

 	// Use settings defined above to initialize and mount SPIFFS filesystem.
 	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.
 	esp_err_t ret = esp_vfs_spiffs_register(&conf);

 	if (ret != ESP_OK) {
 	    if (ret == ESP_FAIL) {
 	        ESP_LOGE(TAG, "Failed to mount or format filesystem");
 	    } else if (ret == ESP_ERR_NOT_FOUND) {
 	        ESP_LOGE(TAG, "Failed to find SPIFFS partition");
 	    } else {
 	        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
 	    }
 	    return;
 	}

 	size_t total = 0, used = 0;
 	ret = esp_spiffs_info(NULL, &total, &used);
 	if (ret != ESP_OK) {
 	    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
 	} else {
 	    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
 	}
 	
 	
 	static lv_fs_drv_t drv;                   /*Needs to be static or global*/
 	lv_fs_drv_init(&drv);                     /*Basic initialization*/

 	drv.letter = 'S';                         /*An uppercase letter to identify the drive */
 	drv.cache_size = 0;           /*Cache size for reading in bytes. 0 to not cache.*/

 	drv.ready_cb = NULL;               /*Callback to tell if the drive is ready to use */
 	drv.open_cb = spiffs_fs_open;                 /*Callback to open a file */
 	drv.close_cb = spiffs_fs_close;               /*Callback to close a file */
 	drv.read_cb = spiffs_fs_read;                 /*Callback to read a file */
 	drv.write_cb = NULL;               /*Callback to write a file */
 	drv.seek_cb = spiffs_fs_seek;                 /*Callback to seek in a file (Move cursor) */
 	drv.tell_cb = spiffs_fs_tell;                 /*Callback to tell the cursor position  */

 	drv.dir_open_cb = NULL;         /*Callback to open directory to read its content */
 	drv.dir_read_cb = NULL;         /*Callback to read a directory's content */
 	drv.dir_close_cb = NULL;       /*Callback to close a directory */

 	drv.user_data = NULL;             /*Any custom data if required*/

 	lv_fs_drv_register(&drv);                 /*Finally register the drive*/	
 	
 //	lv_fs_file_t lvF;
 //	
 //	lv_fs_res_t res = lv_fs_open(&lvF, "S:testpng.png", LV_FS_MODE_RD);
 //	if(res != LV_FS_RES_OK)
 //	{
 //		ESP_LOGE(TAG, "Failed to open file system via LVGL");
 //		
 //	}

 //	lv_image_header_t img_header;
 //	lv_res_t res = lv_image_decoder_get_info("S:testpng.png", &img_header);	
 //	if(res == LV_RES_OK)
 //	{
 //		ESP_LOGI(TAG, "testpng can be decoded: %d %d", img_header.h, img_header.w);		
 //	}
 //	else 
 //	{
 //		ESP_LOGI(TAG,"PNG file can not be decoded");
 //	}
 		
 }
