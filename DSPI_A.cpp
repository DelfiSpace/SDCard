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
 
 #include "DSPI_A.h"
 
DSPI_A * DSPI_A_obj;		// pointer DSPI classes


/**** ISR/IRQ Handles ****/
void EUSCIA1_IRQHandler_SPI( void )
{
	uint8_t status = MAP_SPI_getEnabledInterruptStatus( EUSCI_A1_BASE );
	MAP_SPI_clearInterruptFlag( EUSCI_A1_BASE, status );

	/* Transmit interrupt flag */
    if (status & EUSCI_A_SPI_TRANSMIT_INTERRUPT)
    {
        MAP_SPI_transmitData( EUSCI_A1_BASE, DSPI_A_obj->_handleTransmit() );
    }

    /* Receive interrupt flag */
    if (status & EUSCI_A_SPI_RECEIVE_INTERRUPT)
    {
        DSPI_A_obj->_handleReceive( MAP_SPI_receiveData(EUSCI_A1_BASE) );
    }
}

/**** CONSTRUCTORS Default ****/
DSPI_A::DSPI_A()
{	//MSP432 launchpad used EUSCI_A0_SPI as default
	this->module = EUSCI_A1_SPI_BASE;
	DSPI_A_obj = this;

	user_onTransmit = 0;
	user_onReceive = 0;
}

/**** DESTRUCTORS Reset the module ****/
DSPI_A::~DSPI_A()
{
	MAP_SPI_disableModule(this->module);
	MAP_SPI_unregisterInterrupt(this->module);
	DSPI_A_obj = 0;
}

void DSPI_A::initMaster( unsigned int speed )
{
    MAP_SPI_disableModule(this->module);    //disable SPI operation for configuration settings

    _initMain();    //SPI pins init

    eUSCI_SPI_MasterConfig config;
    config.selectClockSource    = EUSCI_A_SPI_CLOCKSOURCE_SMCLK;    // SMCLK Clock Source
    config.clockSourceFrequency = MAP_CS_getSMCLK();
    config.desiredSpiClock      = speed; //150kHz
    config.msbFirst             = EUSCI_A_SPI_MSB_FIRST;                                    // MSB first, macro found in spi.h
    config.spiMode                = EUSCI_SPI_3PIN;
    config.clockPhase              = EUSCI_A_CTLW0_CKPH;  // MODE0
    config.clockPolarity           = 0;

    MAP_SPI_initMaster(this->module, &config);
    MAP_SPI_enableModule(this->module);
}

/**** Read and write 1 byte of data ****/
uint8_t DSPI_A::transfer(uint8_t data)
{
	// transfer can only be used WITHOUT interrupts
	if ((user_onTransmit) || (user_onReceive))
	{
		return 0;
	}

	// ensure the transmitter is ready to transmit data
	while (!(MAP_SPI_getInterruptStatus(this->module, EUSCI_A_SPI_TRANSMIT_INTERRUPT )));
	MAP_SPI_transmitData(this->module, data);
	
	// wait for a byte to be received	
	while (!(MAP_SPI_getInterruptStatus(this->module, EUSCI_A_SPI_RECEIVE_INTERRUPT )));
	return MAP_SPI_receiveData(this->module);
}

/**** TX Interrupt Handler ****/
void DSPI_A::onTransmit( uint8_t (*islHandle)( void ) )
{
	user_onTransmit = islHandle;
	if ( islHandle )
	{
		// enable the transmit interrupt but do not clear the flag: this is done to ensure
		// that the interrupt fires straight away so that the transmit buffer can be filled
		// the first time
		MAP_SPI_enableInterrupt( this->module, EUSCI_A_SPI_TRANSMIT_INTERRUPT );
	}
	else
	{
		// disable transmit interrupt
		MAP_SPI_disableInterrupt( this->module, EUSCI_A_SPI_TRANSMIT_INTERRUPT) ;
	}
}

/**** RX Interrupt Handler ****/
void DSPI_A::onReceive( void (*islHandle)(uint8_t) )
{
	user_onReceive = islHandle;
	if ( islHandle )
	{
		// clear the receive interrupt to avoid spurious triggers the first time
		MAP_SPI_clearInterruptFlag( this->module, EUSCI_A_SPI_RECEIVE_INTERRUPT );
		// enable receive interrupt
		MAP_SPI_enableInterrupt( this->module, EUSCI_A_SPI_RECEIVE_INTERRUPT );
	}
	else
	{
		// disable receive interrupt
		MAP_SPI_disableInterrupt( this->module, EUSCI_A_SPI_RECEIVE_INTERRUPT );
	}
}

/**** PRIVATE ****/
/**** Initialise SPI Pin Configuration based on EUSCI used ****/
void DSPI_A::_initMain( void )
{
    //UCA1CLK
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN1 , GPIO_PRIMARY_MODULE_FUNCTION);
    //UCA1SOMI
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN2 , GPIO_PRIMARY_MODULE_FUNCTION);
    //UCA1SIMO
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P2, GPIO_PIN3 , GPIO_PRIMARY_MODULE_FUNCTION);

    // transmit / receive interrupt request handler
    MAP_SPI_registerInterrupt(this->module, EUSCIA1_IRQHandler_SPI);
}

/**
 * Internal process handling the tx, and calling the user's interrupt handles
 */
uint8_t DSPI_A::_handleTransmit( void )
{
	// do something only if there is a handler registered
	if (user_onTransmit)
	{
		// call the user-defined data transfer handler
		return user_onTransmit();
	}
	return 0;
}

/**
 * Internal process handling the rx, and calling the user's interrupt handles
 */
void DSPI_A::_handleReceive( uint8_t data )
{
	// do something only if there is a handler registered
	if (user_onReceive)
	{
		// call the user-defined data transfer handler
		user_onReceive(data);
	}
}
