#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "bsp_board.h"
#include "tca9555_driver.h"
#include "audio_driver.h"


static char *TAG = "app main";

#define MAX_MP3_FILE    50
static char SD_Name[MAX_MP3_FILE][100]; 
static uint16_t Search_mp3_file_count = 0;
static char music_buf[120] = {0};

static void Search_mp3_Music(void)     
{        
    Search_mp3_file_count = Folder_retrieval("/sdcard",".mp3",SD_Name,MAX_MP3_FILE);
    printf("file_count=%d\r\n",Search_mp3_file_count);
    if(Search_mp3_file_count) 
    {  
        for (int i = 0; i < Search_mp3_file_count; i++) 
        {
            ESP_LOGI("SAFASF","%s",SD_Name[i]);
        }                
        
    }                                                             
}

void app_main()
{
    ESP_ERROR_CHECK(esp_board_init(16000, 2, 16));
    tca9555_driver_init();
    esp_sdcard_init("/sdcard", 10);
    Audio_Play_Init();
    Search_mp3_Music();

    memset(music_buf,0,sizeof(music_buf));
    sprintf(music_buf,"file://sdcard/%s",SD_Name[0]);
    Audio_Play_Music(music_buf);
}

