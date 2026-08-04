// Convert UART to RZ code
// 0b0 -> 0b10
// 0b1 -> 0b11
static uint16_t uart_rz_table[256];

void uart_rz_init(void){
  for (int i = 0; i < 256; i++){
    uint16_t t = i;
    t = (t | (t << 4)) & 0x0F0F;
    t = (t | (t << 2)) & 0x3333;
    t = (t | (t << 1)) & 0x5555;
    uart_rz_table[i] = (t << 1) | 0x5555;
  }
}

void uart_rz_write(uint8_t data){
  uint16_t tx_data = uart_rz_table[data];

  nfc_Serial.write((uint8_t)(tx_data & 0xff));
  nfc_Serial.write((uint8_t)(tx_data >> 8));
}

bool uart_rz_read(uint8_t *data){
  uint8_t a, b, d;
  int uart_len = nfc_Serial.available();
  if (uart_len < 2) return false;

  for (int i = 0; i < uart_len - 1; i++){
    a = nfc_Serial.read();
    b = nfc_Serial.peek();

    // Decode
    uint16_t t = ((uint16_t)b << 8) | (uint16_t)a;
    t = t >> 1;
    t = t & 0x5555;
    t = (t | (t >> 1)) & 0x3333;
    t = (t | (t >> 2)) & 0x0F0F;
    t = (t | (t >> 4)) & 0x00FF;
    d = (uint8_t)t;
    
    // Check if decoding is correct
    if ((((uint16_t)b << 8) | (uint16_t)a) == uart_rz_table[d]){
      *data = d;
      nfc_Serial.read();
      return true;
    }
  }
  return false;
}

uint8_t calc_crc8(int8_t *data, int len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc ^= (uint8_t)data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
