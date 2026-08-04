#define TAG_NUM 0

void setup() {
  // put your setup code here, to run once:
  Serial.begin(100000);
  pinMode(3, OUTPUT);
  
  // Initialize ADXL367
  adxl367_init_acceleration_8bit();
  
  uart_rz_init();
}

void loop() {
  uint8_t rx_data;
  static int8_t acceleration_data[4];
  while (!uart_rz_read(&rx_data));
  if (rx_data != 'T') return;

  while (!uart_rz_read(&rx_data));
  if (rx_data != '0' + TAG_NUM) return;
  
  uart_rz_write(acceleration_data[0]);
  uart_rz_write(acceleration_data[1]);
  uart_rz_write(acceleration_data[2]);
  uart_rz_write(acceleration_data[3]);

  adxl367_acceleration_read_8bit(acceleration_data);

  // Calculate CRC
  acceleration_data[3] = calc_crc8(acceleration_data, 3);
}
