/*
 * Code written by Chia Jiun Wei @ 1 May 2017
 * <J.W.Chia@tudelft.nl>
 *
 * DSPI: a library to provide full hardware-driven SPI functionality
 * to the TI MSP432 family of microcontrollers. It is possible to use
 * this library in Energia (the Arduino port for MSP microcontrollers)
 * or in other toolchains.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License 
 * version 3, both as published by the Free Software Foundation.
 *
 */
 
#ifndef DSPI_AA_H
#define DSPI_AA_H

#include <driverlib.h>
// Device specific includes
#include "inc/msp432p4111.h"

class DSPI_A
{
private: 
    /* MSP specific modules */
    uint32_t module;

    void _initMain( void );

public:
    DSPI_A();
    ~DSPI_A();

    void initMaster(unsigned int speed );
    uint8_t transfer( uint8_t data );

protected:

};

#endif  /* DSPI_H_ */
