/**
  ******************************************************************************
  * File Name          : bmp280.c
  * Description        : This file provides code for the BMP280 Bosch pressure
  *                      and temperature sensor.
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "bmx280.h"

/* Private variables ---------------------------------------------------------*/
static uint8_t calib1[28];
static uint8_t dataBuf[8];
static uint8_t id[4];
static bmx280_t bmp280;

static BMP280_S32_t t_fine = 0;
static uint16_t dig_T1 = 0;
static int16_t dig_T2 = 0;
static int16_t dig_T3 = 0;
static uint16_t dig_P1 = 0;
static int16_t dig_P2 = 0;
static int16_t dig_P3 = 0;
static int16_t dig_P4 = 0;
static int16_t dig_P5 = 0;
static int16_t dig_P6 = 0;
static int16_t dig_P7 = 0;
static int16_t dig_P8 = 0;
static int16_t dig_P9 = 0;

static BMP280_S32_t temperature;
static BMP280_U32_t pressure;

static double preciseTemperature;
static double precisePressure;



/* Private function prototypes -----------------------------------------------*/
static void BMP280_Write(uint8_t cmd, uint8_t data);
static void BMP280_Read(void);
static double bmp280_compensate_T_double(BMP280_S32_t adc_T);
static double bmp280_compensate_P_double(BMP280_S32_t adc_P);
static BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T);
static BMP280_U32_t bmp280_compensate_P_int32(BMP280_S32_t adc_P);









////////////////////////////////////////////////////////////////////////////////

/**
  * @brief  BMX280 Initialization procedure
  * @param  none
  * @retval uint8_t Status of a connected sensor stated
  *         by checking family ID
  */
uint8_t BMP280_Init(void) {
  uint8_t status = 0;
  bmp280.Lock = 1;

  /* Get family ID of a sensor */
  id[0] = SensorID;
  SPI_Read(id, 1);
  Delay(1);
  /* Get out if wrong family ID was gotten */
  if (id[1] != BMP280_ID) return (status);

  /* Get calibration data */
  calib1[0] = Calib1;
  SPI_Read(calib1, 26);
  Delay(1);

  uint8_t *tmp = 0;
  tmp = calib1;

  tmp += 1;
  dig_T1 = *(uint16_t*)(tmp);
  tmp += 2;
  dig_T2 = *(int16_t*)(tmp);
  tmp += 2;
  dig_T3 = *(int16_t*)(tmp);

  tmp += 2;
  dig_P1 = *(uint16_t*)(tmp);
  tmp += 2;
  dig_P2 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P3 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P4 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P5 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P6 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P7 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P8 = *(int16_t*)(tmp);
  tmp += 2;
  dig_P9 = *(int16_t*)(tmp);

  status = 1;
  bmp280.Lock = 0;
  return (status);
}







/**
  * @brief  Collect data from sensor. According to BMP280 datasheet, "force mode" leads to delay
  *         5.5ms - 6.4ms. When presure and(or) temperature oversampling get higher, it's leading
  *         to increase the measurment delay.
  *         
  * @param  none
  * @retval none
  */
static void BMP280_Read(void) {
  bmp280.Lock = 1;

  BMP280_Write(CtrlMeasure, (1 << TemperatureOvs_Pos) | (1 << PressureOvs_Pos) | (ForceMode << Mode_Pos));
  
  dataBuf[1] = Measuring;
  while (dataBuf[1] == Measuring) {
    dataBuf[0] = StatusSensor;
    SPI_Read(dataBuf, 2);
  }

  Delay(10);
  dataBuf[0] = CollectData;
  SPI_Read(dataBuf, 6);

  bmp280.Lock = 0;
}





/**
  * @brief  Write command to a sensor.
  * @param  none
  * @retval none
  */
static void BMP280_Write(uint8_t cmd, uint8_t data) {
  bmp280.Lock = 1;

  dataBuf[0] = cmd & WriteMask;
  dataBuf[1] = data;
  
  SPI_Write(dataBuf, 2);
  Delay(1);

  bmp280.Lock = 0;
}






/**
  * @brief  Convert temperature into human readable format.
  * @param  none
  * @retval pointer to variable with converted temperature.
  */
BMP280_S32_t* BMP280_ReadT(void) {
  BMP280_Read();

  BMP280_S32_t tmp_T = 0;
  tmp_T = ((dataBuf[4] << 16) | (dataBuf[5] << 8) | dataBuf[6]) >> 4;

  temperature = bmp280_compensate_T_int32(tmp_T);

  return (&temperature);
}





/**
  * @brief  Convert pressure into human readable format.
  * @param  none
  * @retval pointer to variable with converted pressure.
  */
BMP280_U32_t* BMP280_ReadP(void) {
  BMP280_S32_t tmp_P = 0;

  if (t_fine) {
    tmp_P = ((dataBuf[1] << 16) | (dataBuf[2] << 8) | dataBuf[3]) >> 4;
    pressure = bmp280_compensate_P_int32(tmp_P);
  }

  return (&pressure);
}






/**
  * @brief  Convert precise temperature into human readable format.
  * @param  none
  * @retval pointer to variable with converted temperature.
  */
double* BMP280_ReadPT(void) {
  BMP280_Read();
  
  double tmp_T = 0;

  if (t_fine) {
    tmp_T = ((dataBuf[4] << 16) | (dataBuf[5] << 8) | dataBuf[6]) >> 4;
    preciseTemperature = bmp280_compensate_T_double(tmp_T);
  }

  return (&preciseTemperature);
}





/**
  * @brief  Convert precise pressure into human readable format.
  * @param  none
  * @retval pointer to variable with converted pressure.
  */
double* BMP280_ReadPP(void) {
  double tmp_P = 0;

  if (t_fine) {
    tmp_P = ((dataBuf[1] << 16) | (dataBuf[2] << 8) | dataBuf[3]) >> 4;
    precisePressure = bmp280_compensate_P_double(tmp_P);
  }

  return (&precisePressure);
}



/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/*
  Here is a bunch of functions provided by Bosch sensor engineering 
  and proposed by BMP280 datasheet. They were set here with minory 
  changed. The logig was kept.
*/
static double bmp280_compensate_T_double(BMP280_S32_t adc_T) {
  double var1, var2, T;
  var1 = (((double)adc_T) / 16384.0 - ((double)dig_T1) / 1024.0) * ((double)dig_T2);
  var2 = ((((double)adc_T) / 131072.0 - ((double)dig_T1) / 8192.0) * (((double)adc_T) / 131072.0 - ((double)dig_T1) / 8192.0)) * ((double)dig_T3);
  t_fine = (BMP280_S32_t)(var1 + var2);
  T = (var1 + var2) / 5120.0;
  return (T);
}

// Returns pressure in Pa as double. Output value of “96386.2” equals 96386.2 Pa = 963.862 hPa
static double bmp280_compensate_P_double(BMP280_S32_t adc_P) {
  double var1, var2, p;
  var1 = ((double)t_fine / 2.0) - 64000.0;
  var2 = var1 * var1 * ((double)dig_P6) / 32768.0;
  var2 = var2 + var1 * ((double)dig_P5) * 2.0;
  var2 = (var2 / 4.0) + (((double)dig_P4) * 65536.0);
  var1 = (((double)dig_P3) * var1 * var1 / 524288.0 + ((double)dig_P2) * var1) / 524288.0;
  var1 = (1.0 + var1 / 32768.0) * ((double)dig_P1);
  if (var1 == 0.0) {
    return (0); // avoid exception caused by division by zero
  }
  p = 1048576.0 - (double)adc_P;
  p = (p - (var2 / 4096.0)) * 6250.0 / var1;
  var1 = ((double)dig_P9) * p * p / 2147483648.0;
  var2 = p * ((double)dig_P8) / 32768.0;
  p = p + (var1 + var2 + ((double)dig_P7)) / 16.0;
  return (p);
}

// Returns temperature in DegC, resolution is 0.01 DegC. Output value of “5123” equals 51.23 DegC.
// t_fine carries fine temperature as global value
static BMP280_S32_t bmp280_compensate_T_int32(BMP280_S32_t adc_T) {
  BMP280_S32_t var1, var2, T;
  var1 = ((((adc_T >> 3) - ((BMP280_S32_t)dig_T1 << 1))) * ((BMP280_S32_t)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((BMP280_S32_t)dig_T1)) * ((adc_T >> 4) - ((BMP280_S32_t)dig_T1))) >> 12) * ((BMP280_S32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  T = (t_fine * 5 + 128) >> 8;
  return (T);
}

// Returns pressure in Pa as unsigned 32 bit integer. Output value of “96386” equals 96386 Pa = 963.86 hPa
static BMP280_U32_t bmp280_compensate_P_int32(BMP280_S32_t adc_P) {
  BMP280_S32_t var1, var2;
  BMP280_U32_t p;
  var1 = (((BMP280_S32_t)t_fine) >> 1) - (BMP280_S32_t)64000;
  var2 = (((var1 >> 2) * (var1 >> 2)) >> 11 ) * ((BMP280_S32_t)dig_P6);
  var2 = var2 + ((var1 * ((BMP280_S32_t)dig_P5)) << 1);
  var2 = (var2 >> 2) + (((BMP280_S32_t)dig_P4) << 16);
  var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13 )) >> 3) + ((((BMP280_S32_t)dig_P2) * var1) >> 1)) >> 18;
  var1 = ((((32768 + var1)) * ((BMP280_S32_t)dig_P1)) >> 15);
  if (var1 == 0) {
    return (0); // avoid exception caused by division by zero
  }
  p = (((BMP280_U32_t)(((BMP280_S32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
  if (p < 0x80000000) {
    p = (p << 1) / ((BMP280_U32_t)var1);
  } else {
    p = (p / (BMP280_U32_t)var1) * 2;
  }
  var1 = (((BMP280_S32_t)dig_P9) * ((BMP280_S32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
  var2 = (((BMP280_S32_t)(p >> 2)) * ((BMP280_S32_t)dig_P8)) >> 13;
  p = (BMP280_U32_t)((BMP280_S32_t)p + ((var1 + var2 + dig_P7) >> 4));
  return (p);
}
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
