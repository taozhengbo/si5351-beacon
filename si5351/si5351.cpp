//
// Author: Alexander Sholohov <ra9yer@yahoo.com>
//
// License: MIT
//

#include "si5351.h"


#define USE_ARDUINO_PROGMEM
#define USE_ARDUINO_I2C



#ifdef USE_ARDUINO_PROGMEM
#  include <avr/pgmspace.h>
#  define code PROGMEM
#else
#  define code 
#endif

#include "radio/register_map.h"

#define SI5351_I2C_ADDRESS (0x60)



#ifdef USE_ARDUINO_I2C
#include <Arduino.h>

#include <Wire.h>

//---------------------------------------------------------------------------------
static void i2c_write(uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(SI5351_I2C_ADDRESS); 
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

//---------------------------------------------------------------------------------
static void i2c_bust_write(uint8_t reg, uint8_t* buf, size_t count)
{
    Wire.beginTransmission(SI5351_I2C_ADDRESS); 
    Wire.write(reg);
    for( size_t i=0; i<count; i++ )
    {
        Wire.write(buf[i]);
    }
    Wire.endTransmission();
}


//---------------------------------------------------------------------------------
static uint8_t i2c_read1b(uint8_t reg)
{
    Wire.beginTransmission(SI5351_I2C_ADDRESS); 
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(SI5351_I2C_ADDRESS, 1); 
    unsigned char res = Wire.read(); 
    return res;
}


#endif  /* USE_ARDUINO_I2C */

//---------------------------------------------------------------------------------
Si5351::Si5351(int /*instance*/)
{
}



//---------------------------------------------------------------------------------
void Si5351::initialize()
{
    // initailize procedure from application note.

    // Disable output
    i2c_write(3, 0xff);

    // powerdown all output drivers
    for( size_t reg=16; reg <= 23; reg++ )
    {
        i2c_write(reg, 0x80);
    }

    // Write new configuration to device using
    // the contents of the register map
    // generated by ClockBuilder Desktop. This
    // step also powers up the output drivers.
    for( size_t i=0; i<NUM_REGS_MAX; i++ )
    {
#ifdef USE_ARDUINO_PROGMEM
        Reg_Data item;
        memcpy_P(&item, &Reg_Store[i], sizeof(Reg_Data));
#else        
        Reg_Data const& item = Reg_Store[i];
#endif        
        i2c_write(item.Reg_Addr, item.Reg_Val);

    }

    // softreset pllA+pllB
    i2c_write (177, 0xac); 

}

//---------------------------------------------------------------------------------
void Si5351::enableOutput(Si5351::OutPin outputNumber, bool enable)
{
    unsigned char val = i2c_read1b(3);

    unsigned bitMask = 1 << outputNumber;
    if( enable )
    {
        i2c_write(3, val & ~bitMask);
    }
    else
    {
        i2c_write(3, val | bitMask);
    }

}


//---------------------------------------------------------------------------------
void Si5351::enableShutDown(Si5351::OutPin outputNumber, bool shutdown)
{
    uint8_t reg = 16;
    switch(outputNumber)
    {
        case OUT_0:
            reg = 16;
            break;
        case OUT_1:
            reg = 17;
            break;
        case OUT_2:
            reg = 18;
            break;
    }

    unsigned char val = i2c_read1b(reg);
    if( shutdown )
    {
        val |= 0x80;
    }
    else
    {
        val &= ~0x80;
    }

    i2c_write(reg, val);
}


//---------------------------------------------------------------------------------
void Si5351::setupPower(Si5351::OutPin outputNumber, PowerLevel powerLevel)
{
    uint8_t reg = 16;
    switch(outputNumber)
    {
        case OUT_0:
            reg = 16;
            break;
        case OUT_1:
            reg = 17;
            break;
        case OUT_2:
            reg = 18;
            break;
    }

    unsigned char val = i2c_read1b(reg);
    val &= 0xfc;
    val |= powerLevel & 3;
    i2c_write(reg, val);
}


//---------------------------------------------------------------------------------
void Si5351::setupPLLParams(Si5351::PLL pllNumber, uint16_t a, uint32_t b, uint32_t c)
{
    uint32_t f  = (uint32_t)128 * b / c;
    uint32_t p1 = (uint32_t)128 * a + f - 512; // 18bit
    uint32_t p2 = (uint32_t)128 * b - c * f;   // 20bit
    uint32_t p3 = c;                           // 20bit


    unsigned char buf[8];
    size_t cnt = 0;
    buf[cnt++] = p3 >> 8; // 26
    buf[cnt++] = p3;   // 27
    buf[cnt++] = (p1 >> 16) & 0x3;  // 28
    buf[cnt++] = p1 >> 8;  // 29
    buf[cnt++] = p1;  // 30
    buf[cnt++] = ((p3 >> 12) & 0xf0) | ((p2 >> 16) & 0xf); // 31
    buf[cnt++] = p2 >> 8;  // 32
    buf[cnt++] = p2;  // 33


    uint8_t reg = 26;
    switch(pllNumber)
    {
        case PLL_A:
            reg = 26;
            break;
        case PLL_B:
            reg = 34;
            break;
    }

    i2c_bust_write(reg, buf, 8);

}

//---------------------------------------------------------------------------------
void Si5351::setupMultisyncParams(Si5351::OutPin multisyncNumber, unsigned multisyncDivider, unsigned r)
{
    unsigned n = 0;
    unsigned lr = r;
    while( lr > 1)
    {
        n++;
        lr = lr >> 1;
    }

    uint32_t p1 = 128 * multisyncDivider - 512;
    uint32_t p2 = 0;
    uint32_t p3 = 1;
    unsigned divby4 = (multisyncDivider==4)? 3 : 0;

    unsigned char buf[8];
    size_t cnt = 0;
    buf[cnt++] = p3 >> 8; // 42
    buf[cnt++] = p3 ; // 43
    buf[cnt++] = (n << 4) | (divby4 << 2) | ((p1 >> 16) & 3); // 44
    buf[cnt++] = p1 >> 8; // 45
    buf[cnt++] = p1; // 46
    buf[cnt++] = ((p3 >> 12) & 0xf0) | ((p2 >> 16) & 0xf); // 47
    buf[cnt++] = p2 >> 8; // 48
    buf[cnt++] = p2; // 49

    uint8_t reg = 42;
    switch(multisyncNumber)
    {
        case OUT_0:
            reg = 42;
            break;
        case OUT_1:
            reg = 50;
            break;
        case OUT_2:
            reg = 58;
            break;
    }

    i2c_bust_write(reg, buf, 8);

}

//---------------------------------------------------------------------------------
void Si5351::resetPLL(Si5351::PLL pllNumber)
{
    uint8_t val = 0;
    switch(pllNumber)
    {
        case PLL_A:
            val = 0x80;
            break;
        case PLL_B:
            val = 0x20;
            break;
    }

    i2c_write(177, val);
}