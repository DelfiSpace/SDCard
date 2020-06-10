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
 
#ifndef DSPI_A_H
#define DSPI_A_H

#include <driverlib.h>
// Device specific includes
#if defined (__MSP432P401R__)
    #include "inc/msp432p401r.h"
#elif defined (__MSP432P4111__)
    #include "inc/msp432p4111.h"
#endif

class DSPI_A
{
private: 
	/* MSP specific modules */
	uint32_t module;
	uint8_t modulePort;
	uint16_t modulePins;
	
	/* Internal states */
	uint8_t (*user_onTransmit)( void );
	void (*user_onReceive)( uint8_t );
	
	void _initMain( void ); 
	
	/* stub functions to handle interrupts */
	uint8_t _handleTransmit( void );
	void _handleReceive(uint8_t);

	/* Interrupt handlers: they are declared as friends to be accessible from outside
	   the class (the interrupt engine) but have access to member functions */
	friend void EUSCIA1_IRQHandler_SPI( void );
	
public:
	DSPI_A();
	~DSPI_A();
	
	void initMaster(unsigned int speed );
	uint8_t transfer( uint8_t data );

	void onTransmit(uint8_t(*islHandle)( void ));
	void onReceive(void(*islHandle)( uint8_t ));

protected:

};

#endif	/* DSPI_H_ */
