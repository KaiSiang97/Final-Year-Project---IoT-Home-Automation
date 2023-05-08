#include <soc/soc.h>           // Disable brownour problems
#include <soc/rtc_cntl_reg.h>  // Disable brownour problems
#include "camera_init.h"
#include "wifi_init.h"
#include "camera_frame.h"
#include "time.h"
#include "pir_init.h"
#include <ESP32Ping.h>

// PIR and Door Initiate
#define LED_PIN 12
#define Alarm_PIN 15
bool LEDstate = HIGH;
bool Alarmstate = LOW;
bool reboot_request = false;

String devstr =  "ESP32-CAM Security";
int max_frames = 150;
int frame_interval = 0;          // 0 = record at full speed, 100 = 100 ms delay between frames
float speed_up_factor = 1;          // 1 = play at realtime, 0.5 = slow motion, 10 = speedup 10x

//Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

camera_fb_t * fb = NULL;
camera_fb_t * vid_fb = NULL;

TaskHandle_t the_camera_loop_task;
void the_camera_loop (void* pvParameter) ;


bool video_ready = false;
bool picture_ready = false;

bool avi_enabled = false;
int avi_buf_size = 0;
int idx_buf_size = 0;

bool isMoreDataAvailable();
int currentByte;
uint8_t* fb_buffer;
size_t fb_length;

bool isMoreDataAvailable() {
  return (fb_length - currentByte);
}

uint8_t getNextByte() {
  currentByte++;
  return (fb_buffer[currentByte - 1]);
}

int avi_ptr;
uint8_t* avi_buf;
size_t avi_len;

bool avi_more() {
  return (avi_len - avi_ptr);
}

uint8_t avi_next() {
  avi_ptr++;
  return (avi_buf[avi_ptr - 1]);
}

bool dataAvailable = false;
uint8_t * psram_avi_buf = NULL;
uint8_t * psram_idx_buf = NULL;
uint8_t * psram_avi_ptr = 0;
uint8_t * psram_idx_ptr = 0;
char strftime_buf[64];

String getIp()
{
  WiFiClient client;
  while (client.connect("api.ipify.org", 80)) 
  {
      //Serial.println("connected");
      client.println("GET / HTTP/1.0");
      client.println("Host: api.ipify.org");
      client.println();
      delay(5000);
      String line;
      while(client.available())
      {
        line = client.readStringUntil('\n');
      }
      return line;
  }
}

void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.printf("Got a message %s\n", text);
    
    String from_name = bot.messages[i].from_name;
    if (text == "/Start") {
      String welcome = "Welcome , " + from_name + "\n";
      welcome += "ESP32Cam Telegram Bot!\n";
      welcome += "Use the following commands to interact with the ESP32-CAM.\n";
      welcome += "/Photo : Takes a New Photo\n";
      welcome += "/Flash : Toggles Camera Flash LED \n";
      welcome += "/Clip : Record Short Video Clip\n";
      welcome += "/Status : To Check Device Status\n";
      welcome += "/ServerStatus : Check the Web Server Status\n";
      welcome += "/Reboot : Reboot the Device\n";
      welcome += "/Start : Start the Commands\n";
      welcome += "\nConfigure the PIR\n";
      welcome += "/Enable : Enable PIR\n";
      welcome += "/Disable : Disable PIR\n";
      welcome += "\nConfigure the clip\n";
      welcome += "/Enavi : Enable AVI\n";
      welcome += "/Disavi : Disable AVI\n";
      welcome += "\nDoor Control\n";
      welcome += "/Open : Door Open\n";
      welcome += "/Alarm : Sounding Alarm\n";
      welcome += "/Disalarm : Dis-Sounding Alarm\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/Flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      Serial.println("Change Flash LED State!");
      bot.sendMessage(CHAT_ID, "Change Flash LED State!", "");
    }
    if (text == "/Reboot") {
      Serial.println("Reboot Requested!");
      bot.sendMessage(chat_id, "Reboot Requested!", "");
      reboot_request = true;
    }
    if (text == "/Enable") {
      Serial.println("PIR Enabled!");
      bot.sendMessage(chat_id, "PIR Enabled!", "");
      pir_enabled = true;
    }
    if (text == "/Disable") {
      Serial.println("PIR Disabled!");
      bot.sendMessage(chat_id, "PIR Disabled!", "");
      pir_enabled = false;
    }
    if (text == "/Enavi") {
      Serial.println("AVI Enabled!");
      bot.sendMessage(chat_id, "AVI Enabled!", "");
      avi_enabled = true;
    }
    if (text == "/Disavi") {
      Serial.println("AVI Disabled!");
      bot.sendMessage(chat_id, "AVI Disabled!", "");
      avi_enabled = false;
    }
    if (text == "/Open") {
      LEDstate = !LEDstate;
      Serial.println("Door Open!");
      bot.sendMessage(chat_id, "Door Open!", "");
      delay(500);
      digitalWrite(LED_PIN, LEDstate);
      delay(5000);          // Delay 5 seconds
      LEDstate = !LEDstate;
      Serial.println("Door Close!");
      bot.sendMessage(chat_id, "Door Close!", "");
      delay(500);
      digitalWrite(LED_PIN, LEDstate);
    }
    if (text == "/Alarm") {
      Serial.println("Sound Alarm!");
      Alarmstate = HIGH;
      bot.sendMessage(chat_id, "Sound Alarm!", "");
      digitalWrite(Alarm_PIN, Alarmstate);
    }
    if (text == "/Disalarm") {
      Serial.println("Dis-Sound Alarm!");
      Alarmstate = LOW;
      bot.sendMessage(chat_id, "Dis-Sound Alarm!", "");
      digitalWrite(Alarm_PIN, Alarmstate);
    }
    if (text == "/Status") {
      String stat = "Device: " + devstr + "\nSSID: " + String(WiFi.SSID()) + 
      "\nip: " +  WiFi.localIP().toString() + "\nFlash: " + flashState + "\nPIR Enabled: " + pir_enabled + "\nAVI Enabled: " + avi_enabled + "\nAlarm: " + Alarmstate;
      stat = stat + "\nQuality: " + quality;

      bot.sendMessage(CHAT_ID, stat, "");
    }
    
    if (text == "/ServerStatus") {
      // Register for a callback function that will be called when data is received
      String IP1 = "192.168.1.184";
      String IP2 = "192.168.43.184";
      bool success1 = Ping.ping("192.168.1.7");
      bool success2 = Ping.ping("192.168.43.13");
      String ip = getIp();
      if (success1)
      {
        String stat = "Server Private IP: http://" + IP1 + "\nPrivate Streaming: 80/stream" + "\nPrivate Capture: 81/capture" +
        "\nServer Public IP: http://" + ip + "\nPublic Streaming: 8080/stream" + "\nPublic Capture: 8081/capture";
        bot.sendMessage(CHAT_ID, stat, "");
        Serial.println(stat);
      }
      else if (success2)
      {
        String stat = "Server Private IP: http://" + IP2 + "\nPrivate Streaming: 80/stream" + "\nPrivate Capture: 81/capture" +
        "\nServer Public IP: http://" + ip + "\nPublic Streaming: 8080/stream" + "\nPublic Capture: 8081/capture";
        bot.sendMessage(CHAT_ID, stat, "");
        Serial.println(stat);
      }
      else {
        String stat = "Server is not available now";
        bot.sendMessage(CHAT_ID, stat, "");
        Serial.println(stat);
      }
    }
    
    // fb_count = 4, to make it take the current photo
    for (int j = 0; j < 4; j++) {
      camera_fb_t * newfb = esp_camera_fb_get();
      if (!newfb) {
        Serial.println("Camera Capture Failed!");
      } else {
        esp_camera_fb_return(newfb);
        delay(10);
      }
    }
    if (text == "/Photo") {
      fb = NULL;
      fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera Capture Failed!");
        bot.sendMessage(CHAT_ID, "Camera Capture Failed!", "");
        return;
      }
      Serial.println("New photo request");
      bot.sendMessage(chat_id, "New photo request", "");
      currentByte = 0;
      fb_length = fb->len;
      fb_buffer = fb->buf;
      String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
      "image/jpeg", CHAT_ID, fb_length,isMoreDataAvailable, getNextByte, nullptr, nullptr);
      esp_camera_fb_return(fb);
      Serial.println("done!");
    }
    // fb_count = 4, to make it take the current photo
    for (int j = 0; j < 4; j++) {
      camera_fb_t * newfb = esp_camera_fb_get();
      if (!newfb) {
        Serial.println("Camera Capture Failed!");
      } else {
        esp_camera_fb_return(newfb);
        delay(10);
      }
    }
    if (text == "/Clip") {
      // record the video
      bot.longPoll =  0;
      if (avi_enabled){
        Serial.println("Clip Requested!");
        bot.sendMessage(CHAT_ID, "Clip Requested!", "");
        xTaskCreatePinnedToCore( the_camera_loop, "the_camera_loop", 10000, NULL, 1, &the_camera_loop_task, 1);
      }
      else {
        Serial.println("AVI is not enable!");
        bot.sendMessage(CHAT_ID, "AVI is not enable!", "");
        picture_ready = true;
      }

      if (the_camera_loop_task == NULL) {
        Serial.printf("do_the_steaming_task failed to start! %d\n", the_camera_loop_task);
      }
    }
  }
}

struct tm timeinfo;
time_t now;

camera_fb_t * fb_curr = NULL;
camera_fb_t * fb_next = NULL;

#define fbs 8 // how many kb of static ram for psram -> sram buffer for sd write - not really used because not dma for sd

long avi_start_time = 0;
long avi_end_time = 0;
long current_frame_time;
long last_frame_time;

void the_camera_loop (void* pvParameter) {

  vid_fb = get_good_jpeg(); // esp_camera_fb_get();
  if (!vid_fb) {
    Serial.println("Camera Capture Failed!");
    //bot.sendMessage(chat_id, "Camera Capture Failed!", "");
    return;
  }

  if (avi_enabled) {
    frame_cnt = 0;

    ///////////////////////////// start a movie
    avi_start_time = millis();
    Serial.printf("\nStart the avi ... at %d\n", avi_start_time);
    Serial.printf("Framesize %d, quality %d, length %d seconds\n\n", framesize, quality,  max_frames * frame_interval / 1000);

    fb_next = get_good_jpeg();                     // should take zero time
    last_frame_time = millis();
    start_avi();

    ///////////////////////////// all the frames of movie

    for (int j = 0; j < max_frames - 1 ; j++) { // max_frames
      current_frame_time = millis();

      if (current_frame_time - last_frame_time < frame_interval) {
        if (frame_cnt < 5 || frame_cnt > (max_frames - 5) )Serial.printf("frame %d, delay %d\n", frame_cnt, (int) frame_interval - (current_frame_time - last_frame_time));
        delay(frame_interval - (current_frame_time - last_frame_time));             // delay for timelapse
      }

      last_frame_time = millis();
      frame_cnt++;

      if (frame_cnt !=  1) esp_camera_fb_return(fb_curr);
      fb_curr = fb_next;           // we will write a frame, and get the camera preparing a new one

      another_save_avi(fb_curr );
      fb_next = get_good_jpeg();               // should take near zero, unless the sd is faster than the camera, when we will have to wait for the camera

      digitalWrite(33, frame_cnt % 2);
      if (movi_size > avi_buf_size * .95) break;
    }

    ///////////////////////////// stop a movie
    Serial.println("End the Avi");

    esp_camera_fb_return(fb_curr);
    frame_cnt++;
    fb_curr = fb_next;
    fb_next = NULL;
    another_save_avi(fb_curr );
    digitalWrite(33, frame_cnt % 2);
    esp_camera_fb_return(fb_curr);
    fb_curr = NULL;
    end_avi();                                // end the movie
    digitalWrite(33, HIGH);          // light off
    avi_end_time = millis();
    float fps = 1.0 * frame_cnt / ((avi_end_time - avi_start_time) / 1000) ;
    Serial.printf("End the avi at %d.  It was %d frames, %d ms at %.2f fps...\n", millis(), frame_cnt, avi_end_time - avi_start_time, fps);
    frame_cnt = 0;             // start recording again on the next loop
    video_ready = true;
  }
  Serial.println("Deleting the camera loop task");
  delay(100);
  vTaskDelete(the_camera_loop_task);
  Serial.println("Done");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// start_avi - open the files and write in headers
//

void start_avi() {

  Serial.println("Starting an avi ");

  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "DoorCam %F %H.%M.%S.avi", &timeinfo);

  psram_avi_ptr = 0;
  psram_idx_ptr = 0;

  memcpy(buf + 0x40, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0xA8, frameSizeData[framesize].frameWidth, 2);
  memcpy(buf + 0x44, frameSizeData[framesize].frameHeight, 2);
  memcpy(buf + 0xAC, frameSizeData[framesize].frameHeight, 2);

  psram_avi_ptr = psram_avi_buf;
  psram_idx_ptr = psram_idx_buf;
  memcpy( psram_avi_ptr, buf, AVIOFFSET);
  psram_avi_ptr += AVIOFFSET;

  startms = millis();

  jpeg_size = 0;
  movi_size = 0;
  uVideoLen = 0;
  idx_offset = 4;
} // end of start avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  another_save_avi saves another frame to the avi file, uodates index
//           -- pass in a fb pointer to the frame to add
//

void another_save_avi(camera_fb_t * fb ) {

  int fblen;
  fblen = fb->len;
  int fb_block_length;
  uint8_t* fb_block_start;

  jpeg_size = fblen;

  remnant = (4 - (jpeg_size & 0x00000003)) & 0x00000003;

  long bw = millis();
  long frame_write_start = millis();
  memcpy(psram_avi_ptr, dc_buf, 4);

  int jpeg_size_rem = jpeg_size + remnant;
  print_quartet(jpeg_size_rem, psram_avi_ptr + 4);
  fb_block_start = fb->buf;

  if (fblen > fbs * 1024 - 8 ) {                     // fbs is the size of frame buffer static
    fb_block_length = fbs * 1024;
    fblen = fblen - (fbs * 1024 - 8);
    memcpy( psram_avi_ptr + 8, fb_block_start, fb_block_length - 8);
    fb_block_start = fb_block_start + fb_block_length - 8;
  }
  else {
    fb_block_length = fblen + 8  + remnant;
    memcpy( psram_avi_ptr + 8, fb_block_start, fb_block_length - 8);
    fblen = 0;
  }

  psram_avi_ptr += fb_block_length;

  while (fblen > 0) {
    if (fblen > fbs * 1024) {
      fb_block_length = fbs * 1024;
      fblen = fblen - fb_block_length;
    }
    else {
      fb_block_length = fblen  + remnant;
      fblen = 0;
    }
    
    memcpy( psram_avi_ptr, fb_block_start, fb_block_length);
    psram_avi_ptr += fb_block_length;
    fb_block_start = fb_block_start + fb_block_length;
  }

  movi_size += jpeg_size;
  uVideoLen += jpeg_size;

  print_2quartet(idx_offset, jpeg_size, psram_idx_ptr);
  psram_idx_ptr += 8;
  idx_offset = idx_offset + jpeg_size + remnant + 8;
  movi_size = movi_size + remnant;

} // end of another_pic_avi

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  end_avi writes the index, and closes the files
//

void end_avi() {

  Serial.println("End of avi - closing the files");
  if (frame_cnt <  5 ) {
    Serial.println("Recording screwed up, less than 5 frames, forget index\n");
  }
  else {
    elapsedms = millis() - startms;

    float fRealFPS = (1000.0f * (float)frame_cnt) / ((float)elapsedms) * speed_up_factor;
    float fmicroseconds_per_frame = 1000000.0f / fRealFPS;
    uint8_t iAttainedFPS = round(fRealFPS) ;
    uint32_t us_per_frame = round(fmicroseconds_per_frame);

    //Modify the MJPEG header from the beginning of the file, overwriting various placeholders

    print_quartet(movi_size + 240 + 16 * frame_cnt + 8 * frame_cnt, psram_avi_buf + 4);
    print_quartet(us_per_frame, psram_avi_buf + 0x20);
    
    unsigned long max_bytes_per_sec = (1.0f * movi_size * iAttainedFPS) / frame_cnt;
    print_quartet(max_bytes_per_sec, psram_avi_buf + 0x24);
    print_quartet(frame_cnt, psram_avi_buf + 0x30);
    print_quartet(frame_cnt, psram_avi_buf + 0x8c);
    print_quartet((int)iAttainedFPS, psram_avi_buf + 0x84);
    print_quartet(movi_size + frame_cnt * 8 + 4, psram_avi_buf + 0xe8);

    Serial.println(F("\n*** Video recorded and saved ***\n"));
    Serial.printf("Recorded %5d frames in %5d seconds\n", frame_cnt, elapsedms / 1000);
    Serial.printf("File size is %u bytes\n", movi_size + 12 * frame_cnt + 4);
    Serial.printf("Adjusted FPS is %5.2f\n", fRealFPS);
    Serial.printf("Max data rate is %lu bytes/s\n", max_bytes_per_sec);
    Serial.printf("Frame duration is %d us\n", us_per_frame);
    Serial.printf("Average frame length is %d bytes\n", uVideoLen / frame_cnt);
    Serial.printf("Writng the index, %d frames\n", frame_cnt);

    memcpy (psram_avi_ptr, idx1_buf, 4);
    psram_avi_ptr += 4;

    print_quartet(frame_cnt * 16, psram_avi_ptr);
    psram_avi_ptr += 4;

    psram_idx_ptr = psram_idx_buf;

    for (int i = 0; i < frame_cnt; i++) {
      memcpy (psram_avi_ptr, dc_buf, 4);
      psram_avi_ptr += 4;
      memcpy (psram_avi_ptr, zero_buf, 4);
      psram_avi_ptr += 4;
      memcpy (psram_avi_ptr, psram_idx_ptr, 8);
      psram_avi_ptr += 8;
      psram_idx_ptr += 8;
    }
  }

  Serial.println("---");
  digitalWrite(33, HIGH);
}


void setup() {
  // put your setup code here, to run once:
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // Init Serial Monitor
  Serial.begin(115200);

  // Set LED Flash as output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);
  pinMode(LED_PIN, OUTPUT);        // Simulate the door
  digitalWrite(LED_PIN, LEDstate);
  pinMode(Alarm_PIN, OUTPUT);
  digitalWrite(Alarm_PIN, Alarmstate);

  pinMode(33, OUTPUT);             // little red led on back of chip
  digitalWrite(33, LOW);           // turn on the red LED on the back of chip
  avi_buf_size = 3000 * 1024; // = 3000 kb = 60 * 50 * 1024;
  idx_buf_size = 200 * 10 + 20;
  psram_avi_buf = (uint8_t*)ps_malloc(avi_buf_size);
  if (psram_avi_buf == 0) Serial.printf("psram_avi allocation failed\n");
  psram_idx_buf = (uint8_t*)ps_malloc(idx_buf_size); // save file in psram
  if (psram_idx_buf == 0) Serial.printf("psram_idx allocation failed\n");


  // Config and init the camera
  camera_setup();

  // Connect to Wi-Fi
  wifisetup();
  char tzchar[80];
  configTime(28800, 0, "pool.ntp.org");// 8*60*60 = 28800, day light = 0 as no change for day
  setupinterrupts();
  Serial.println(" ");
  String stat = "PIR Initailized!\n";
  for (int i = 0; i < 5; i++) {
    Serial.print( digitalRead(PIRpin) ); Serial.print(", ");
    stat += String(digitalRead(PIRpin)) + ",";
  }
  Serial.println(" ");
  stat += "\nDevice Started\nDevice: " + devstr + 
  "\nWi-Fi SSID: " + WiFi.SSID() + "\nip: " +  WiFi.localIP().toString() + "\n/Start";
  bot.sendMessage(CHAT_ID, stat, "");
  Serial.println("Started");

  digitalWrite(33, HIGH);         // turn off the red LED on the back of the ESP32
}

void loop() {
  // put your main code here, to run repeatedly:
  if (reboot_request) {
    String stat = "Rebooting on request\nDevice: " + devstr + "\nWi-Fi SSID: " + WiFi.SSID() + 
    "\nip: " +  WiFi.localIP().toString() + "\nPlease Wait for Rebooting!";
    bot.sendMessage(CHAT_ID, stat, "");
    delay(10000);
    ESP.restart();
  }

  if (picture_ready) {
    picture_ready = false;
    // fb_count = 4, to make it take the current photo
  for (int j = 0; j < 4; j++) {
    camera_fb_t * newfb = esp_camera_fb_get();
    if (!newfb) {
      Serial.println("Camera Capture Failed!");
    } else {
      esp_camera_fb_return(newfb);
      delay(10);
    }
  }
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera Capture Failed!");
    bot.sendMessage(CHAT_ID, "Camera Capture Failed!", "");
    return;
  }
  digitalWrite(33, LOW);          // light on
  currentByte = 0;
  fb_length = fb->len;
  fb_buffer = fb->buf;

  Serial.println("Picture from Clip!");
  bot.sendMessage(CHAT_ID, "Picture from Clip!");
  String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
                "image/jpeg", CHAT_ID, fb_length, isMoreDataAvailable, getNextByte, nullptr, nullptr);
  esp_camera_fb_return(fb);
  digitalWrite(33, HIGH);          // light oFF
  }

  //if (PIRstate && pir_enabled) {
  if (PIRstate) {
    PIRstate = false;
    // fb_count = 4, to make it take the current photo
    for (int j = 0; j < 4; j++) {
      camera_fb_t * newfb = esp_camera_fb_get();
      if (!newfb) {
        Serial.println("Camera Capture Failed!");
      } else {
        esp_camera_fb_return(newfb);
        delay(10);
      }
    }
    fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera Capture Failed!");
      bot.sendMessage(CHAT_ID, "Camera Capture Failed!", "");
      return;
    }
    Serial.println("Motion Detected!");
    bot.sendMessage(CHAT_ID, "Motion Detected!", "");
    currentByte = 0;
    fb_length = fb->len;
    fb_buffer = fb->buf;
    String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
    "image/jpeg", CHAT_ID, fb_length,isMoreDataAvailable, getNextByte, nullptr, nullptr);
    esp_camera_fb_return(fb);
    Serial.println("Done!");
    bot.sendMessage(CHAT_ID, "Done!", "");
  }

  if (video_ready) {
    video_ready = false;
    digitalWrite(33, LOW);          // light on
    avi_buf = psram_avi_buf;

    avi_ptr = 0;
    avi_len = psram_avi_ptr - psram_avi_buf;
    Serial.println("Please Wait, about 2 to 3 Minutes!");
    bot.sendMessage(CHAT_ID, "Please Wait, about 2 to 3 Minutes!");
    String sent2 = bot.sendMultipartFormDataToTelegram("sendDocument", "document", strftime_buf,
                   "image/jpeg", CHAT_ID, psram_avi_ptr - psram_avi_buf,
                   avi_more, avi_next, nullptr, nullptr);
  
    Serial.println("Done!");
    bot.sendMessage(CHAT_ID, "Done!", "");
    digitalWrite(33, HIGH);          // light off
    bot.longPoll = 5;
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("\nGot response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
