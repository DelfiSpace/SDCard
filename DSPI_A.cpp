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

/**** CONSTRUCTORS Default ****/
DSPI_A::DSPI_A()
{   //MSP432 launchpad used EUSCI_A0_SPI as default
    this->module = EUSCI_A1_SPI_BASE;
}

/**** DESTRUCTORS Reset the module ****/
DSPI_A::~DSPI_A()
{
    MAP_SPI_disableModule(this->module);
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
    // ensure the transmitter is ready to transmit data
    while (!(MAP_SPI_getInterruptStatus(this->module, EUSCI_A_SPI_TRANSMIT_INTERRUPT )));
    MAP_SPI_transmitData(this->module, data);

    // wait for a byte to be received
    while (!(MAP_SPI_getInterruptStatus(this->module, EUSCI_A_SPI_RECEIVE_INTERRUPT )));
    return MAP_SPI_receiveData(this->module);
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
}
