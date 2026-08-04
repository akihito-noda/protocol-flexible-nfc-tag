#include <Wire.h>

#define ADXL376_ADDRESS         0x1d
#define ADXL376_REG_SOFT_RESET  0x1f
#define ADXL376_REG_TAP_THRESH  0x2f
#define ADXL376_REG_TAP_DUR     0x30
#define ADXL376_REG_TAP_LATENT  0x31
#define ADXL376_REG_TAP_WINDOW  0x32
#define ADXL376_REG_AXIS_MASK   0x43
#define ADXL376_REG_FILTER_CTL  0x2c
#define ADXL376_REG_POWER_CTL   0x2d
#define ADXL376_REG_STATUS2     0x45
#define ADXL376_REG_SERIAL_NUMBER_3 0x04
#define ADXL376_REG_SERIAL_NUMBER_2 0x05
#define ADXL376_REG_SERIAL_NUMBER_1 0x06
#define ADXL376_REG_SERIAL_NUMBER_0 0x07
#define ADXL376_REG_XDATA       0x08
#define ADXL376_REG_YDATA       0x09
#define ADXL376_REG_ZDATA       0x0a
#define ADXL376_REG_STATUS      0x0b

// 1-byte register write
void adxl367_reg_write(uint8_t reg, uint8_t data){
  Wire.beginTransmission(ADXL376_ADDRESS);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

// Read 1-byte register
uint8_t adxl367_reg_read_1byte(uint8_t reg){
  Wire.beginTransmission(ADXL376_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(ADXL376_ADDRESS, 1);
  return Wire.read();
}

// n-byte register read
void adxl367_reg_read_nbyte(uint8_t reg, uint8_t* data, uint8_t len){
  Wire.beginTransmission(ADXL376_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom(ADXL376_ADDRESS, len);
  for (int i = 0; i < len; i++){
    *(data + i) = Wire.read();
  }
}

// Initialization for 8-bit acceleration reading (without FIFO)
void adxl367_init_acceleration_8bit(void){
  uint8_t data;
  Wire.begin();

  // Software reset
  adxl367_reg_write(ADXL376_REG_SOFT_RESET, 0x52);
  delay(10);

  // Setting the output data rate range
  data = 0x23;
  data &= ~(0b111 | (0b11 << 6));
  data |= 0b101;      // Output data rate setting 0b011: 100 Hz
  data |= 0b00 << 6;  // Range settings: 2g:0b00, 4g:0b01, 8g:0b10
  adxl367_reg_write(ADXL376_REG_FILTER_CTL, data);

  // Switch to measurement mode
  adxl367_reg_write(ADXL376_REG_POWER_CTL, 0x2);

  delay(100);
}

// 8-bit acceleration reading
void adxl367_acceleration_read_8bit(uint8_t *data){
  while ((adxl367_reg_read_1byte(ADXL376_REG_STATUS) & 0x1) == 0){
    delayMicroseconds(10);
  }
  adxl367_reg_read_nbyte(ADXL376_REG_XDATA, data, 3);
}
