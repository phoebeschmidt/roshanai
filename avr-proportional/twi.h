/* 
   twi.h
  
   Interfaces to send data on the twi bus to the PCA9685 (PWN module)
   
   From example code provided by Atmel, modified by FLG by CSW, 6/2016
    
*/

#ifndef TWI_H
#define TWI_H

#define TWI_SLA_PCA9685 0x80  
#define TWI_PCA9685_READ  1
#define TWI_PCA9685_WRITE 0

void twi_init(void);
int pca9685_write(uint8_t addr, int len, uint8_t *buf);

#endif //TWI_H