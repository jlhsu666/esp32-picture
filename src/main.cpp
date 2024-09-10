#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>

#define API_KEY "FireBase_Project_API_KEY"
#define USER_EMAIL "srl@gapps.knu.edu.tw"
#define USER_PASSWORD "12345678"
#define STORAGE_BUCKET_ID "Firebase_Bucket_ID "
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"

const char* ssid = "YOUR_AP";
const char* password = "YOUR_AP_PWD";

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27  
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
boolean takeNewPhoto = true;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;

bool taskCompleted = false;

void capturePhotoSaveLittleFS(void)
{
  camera_fb_t *fb = NULL;
  for (int i=0; i<4; i++)
  {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb)
  {
    Serial.println("Camera capture failed!");
    delay(1000);
    ESP.restart();
  }

  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  if(!file)
  {
    Serial.println("Failed to open file in writing mode!");
  }
  else
  {
    file.write(fb->buf, fb->len);
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  file.close();
  esp_camera_fb_return(fb);
}

void initWiFi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void initCamera()
{
 camera_config_t config;
 config.ledc_channel = LEDC_CHANNEL_0;
 config.ledc_timer = LEDC_TIMER_0;
 config.pin_d0 = Y2_GPIO_NUM;
 config.pin_d1 = Y3_GPIO_NUM;
 config.pin_d2 = Y4_GPIO_NUM;
 config.pin_d3 = Y5_GPIO_NUM;
 config.pin_d4 = Y6_GPIO_NUM;
 config.pin_d5 = Y7_GPIO_NUM;
 config.pin_d6 = Y8_GPIO_NUM;
 config.pin_d7 = Y9_GPIO_NUM;
 config.pin_xclk = XCLK_GPIO_NUM;
 config.pin_pclk = PCLK_GPIO_NUM;
 config.pin_vsync = VSYNC_GPIO_NUM;
 config.pin_href = HREF_GPIO_NUM;
 config.pin_sscb_sda = SIOD_GPIO_NUM;
 config.pin_sscb_scl = SIOC_GPIO_NUM;
 config.pin_pwdn = PWDN_GPIO_NUM;
 config.pin_reset = RESET_GPIO_NUM;
 config.xclk_freq_hz = 20000000;
 config.pixel_format = PIXFORMAT_JPEG; 
  
 if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
 } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
 }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void initLittleFS()
{
  if(!LittleFS.begin(true)){
    Serial.println("An Error has occured while mounting FS");
    ESP.restart();
  }
  else{
    delay(500);
    Serial.println("Little FS mounted successfully");
  }
}

void fcsUploadCallback(FCS_UploadStatusInfo info)
{
  if(info.status == firebase_fcs_upload_status_init){
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  }
  else if (info.status == firebase_fcs_upload_status_upload){
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  }
  else if (info.status == firebase_fcs_upload_status_complete){
    Serial.println("Upload Completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("ContentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", (int)meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("Etag: %s\n", meta.etag.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
  }
  else if (info.status == firebase_fcs_upload_status_error){
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}

void setup()
{
  Serial.begin(115200);
  initWiFi();
  initLittleFS();
  initCamera();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG,0);

  configF.api_key=API_KEY;
  auth.user.email=USER_EMAIL;
  auth.user.password=USER_PASSWORD;

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

}

void loop()
{
  if(takeNewPhoto){
    capturePhotoSaveLittleFS();
  }
  takeNewPhoto = false;

  if(Firebase.ready() && !taskCompleted){
    taskCompleted = true;
    Serial.print("Uploading picture...");

    if(Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID, FILE_PHOTO_PATH, mem_storage_type_flash, BUCKET_PHOTO, "image/jpeg", fcsUploadCallback ))
    {
      Serial.printf("Download URL: %s\n", fbdo.downloadURL().c_str());
    }
  }
}