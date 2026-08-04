#include <SPI.h>

#define MCU_LED_1 PA1
#define PIN_SS    10
#define PIN_MOSI  11
#define PIN_MISO  12
#define PIN_SCK   13

#define TAG_NUM   3

#define ERR_COUNT 1000   // number of polls per tag used for the error-rate measurements reported in the paper

HardwareSerial nfc_Serial(PB11, PB10);
HardwareSerial stlink_Serial(PA3, PA2);
SPISettings spiSettings(4000000, MSBFIRST, SPI_MODE1);

void write_cmd(uint8_t cmd){
  cmd = 0b11000000 | (0b00111111 & cmd);

  digitalWrite(PIN_SS, LOW);

  SPI.transfer(cmd);

  digitalWrite(PIN_SS, HIGH);
}

void write_register(uint8_t reg, uint8_t data){
  reg = 0b00000000 | (0b00111111 & reg);
  digitalWrite(PIN_SS, LOW);

  SPI.transfer(reg);
  SPI.transfer(data);

  digitalWrite(PIN_SS, HIGH);
}

void setup(){
  stlink_Serial.begin(1000 * 1000);
  pinMode(MCU_LED_1, OUTPUT);

  // UART pins High-Z
  pinMode(PB10, INPUT);
  pinMode(PB11, INPUT);

  SPI.begin();
  SPI.beginTransaction(spiSettings);
  pinMode(PIN_SS, OUTPUT);
  digitalWrite(PIN_SS, HIGH);
  
  // NFC setup
  write_register(0x00, 0b00000101);   // Enable antenna drive, 13.56MHz output frequency, MCU_CLK off
  write_register(0x01, 0b00000000);   // VDD5V amrefV_{dd_rf}
  write_cmd(0xd6);    // Adjust Regulators
  write_cmd(0xc0);    // Set Default
  write_cmd(0xea);    // Trigger RC calibration
  write_register(0x03, 0b00001100);   // AM modulation
  write_register(0x28, 0b00100000);   // AM modulation index 10%, Output driver resistance 1 Ohm
  write_register(0x02, 0b11001000);   // Enable oscillator/regulator, Enable transmission
  write_register(0x0b, 0b00010100);   // Filter settings
  write_register(0x0c, 0b00001000);   // Demodulation method
  write_register(0x0d, 0b11100000);
  write_cmd(0xd5);    // Reset RX Gain
  write_cmd(0xdc);    // Enter Transparent Mode
  SPI.end();

  // Initialize communication UART
  nfc_Serial.setTxInvert();
  nfc_Serial.setRxInvert();
  nfc_Serial.begin(100 * 1000);
  uart_rz_init();
}

void loop(){
  for (uint8_t tag_num = 0; tag_num < TAG_NUM; tag_num++){
    // Send request every 20ms per tag
    uint32_t start_time = micros();

    // Send request to T0 tag
    uart_rz_write('T');
    uart_rz_write('0' + tag_num);
    nfc_Serial.flush();
    int uart_len = nfc_Serial.available();
    for (int i = 0; i < uart_len; i++){
      nfc_Serial.read();
    }
    delay(1);

    // Process response from A
    int rx_status = 0;
    int8_t rx_data[4] = {0};
    digitalWrite(MCU_LED_1, HIGH);
    while (micros() - start_time < 1400){
      rx_status += uart_rz_read((uint8_t*)&rx_data[rx_status]);
      if (rx_status >= 4) break;
    }

    // Measure error rate
    // Calculate error rate from the last ERR_COUNT data points
    #ifdef ERR_COUNT
    static uint32_t req_count = 0;
    static uint32_t err_count[TAG_NUM] = {0};
    if (tag_num == TAG_NUM - 1){
      req_count++;
      if (req_count >= ERR_COUNT) {
        for (int i = 0; i < TAG_NUM; i++){
          stlink_Serial.print(err_count[i]);
          stlink_Serial.print(',');
        }
        stlink_Serial.print(ERR_COUNT);
        stlink_Serial.print('\n');
        stlink_Serial.flush();
        req_count = 0;
        for (int i = 0; i < TAG_NUM; i++) err_count[i] = 0;
      }
    }
    #endif

    // If response received, send to USB serial
    if (rx_status >= 4 && calc_crc8(rx_data, 3) == (uint8_t)rx_data[3]){
      #ifndef ERR_COUNT
      stlink_Serial.print(start_time);
      stlink_Serial.print(",T");
      stlink_Serial.print(tag_num);
      for (int i = 0; i < 3; i++){
        stlink_Serial.print(',');
        stlink_Serial.print(rx_data[i]);
      }
      stlink_Serial.print('\n');
      stlink_Serial.flush();
      #endif
    }
    else{
      #ifdef ERR_COUNT
      err_count[tag_num]++;
      #endif
    }

    // Wait for the next transmission request
    while (micros() - start_time < 1500) delayMicroseconds(10);
  }
}
