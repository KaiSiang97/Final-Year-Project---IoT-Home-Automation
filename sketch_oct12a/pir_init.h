const int PIRpin = 13;
bool PIRstate = false; // we start, assuming no motion detected
bool pir_enabled = false;
static void IRAM_ATTR PIR_ISR(void* arg) ;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// setup some interupts during booting
//
//  int read13 = digitalRead(13); -- pir for video

static void setupinterrupts() {

  Serial.print("Setup PIRpin = ");
  Serial.print(PIRpin);
  Serial.printf("\nPIR Reading\n");
  for (int i = 0; i < 5; i++) {
    Serial.print( digitalRead(PIRpin) ); Serial.print(", ");
  }
  Serial.println(" ");

  esp_err_t err = gpio_isr_handler_add((gpio_num_t)PIRpin, &PIR_ISR, (void *) PIRpin);  
  if (err != ESP_OK){
    Serial.printf("handler add failed with error 0x%x \r\n", err); 
  }
  err = gpio_set_intr_type((gpio_num_t)PIRpin, GPIO_INTR_POSEDGE);
  if (err != ESP_OK){
    Serial.printf("set intr type failed with error 0x%x \r\n", err);
  }
  delay(5000);
  Serial.printf("PIR Sensor Initilized!");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  PIR_ISR - interupt handler for PIR  - starts or extends a video
//

static void IRAM_ATTR PIR_ISR(void* arg) {
  int PIRstatus = digitalRead(PIRpin) + digitalRead(PIRpin) + digitalRead(PIRpin) ;
  if (PIRstatus == 3) {
    if (pir_enabled) {
      PIRstate = true;
    }
    PIRstatus = 0;
  }
}
