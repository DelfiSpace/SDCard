/*
 * SDCard.cpp
 *
 *  Created on: 10 Jun 2020
 *      Author: Casper
 */



#include "SDCard.h"

SDCard::SDCard(DSPI_A* DSPI_in, uint32_t CS_port, uint32_t CS_pin){
    this->SPISD = DSPI_in;
    this->CS_PIN = CS_pin;
    this->CS_PORT = CS_port;
    //chip select
    MAP_GPIO_setAsOutputPin(CS_PORT, CS_PIN);
    MAP_GPIO_setOutputHighOnPin( CS_PORT, CS_PIN );
}

void SDCard::sendDummy(){
    for(int k = 0; k < 20;  k++)
    {
    //        while (!(MAP_SPI_getInterruptStatus(EUSCI_A1_SPI_BASE, EUSCI_A_SPI_TRANSMIT_INTERRUPT )));
    //        MAP_SPI_transmitData(EUSCI_A1_SPI_BASE, 0xff);//Write FF for HIGH DI (10 times = 80 cycles)
    //        while (!(MAP_SPI_getInterruptStatus(EUSCI_A1_SPI_BASE, EUSCI_A_SPI_RECEIVE_INTERRUPT )));
        SPISD->transfer(0xff);
    }
}
void SDCard::select(){
    MAP_GPIO_setOutputLowOnPin( CS_PORT, CS_PIN );
}

void SDCard::unselect(){
    MAP_GPIO_setOutputHighOnPin( CS_PORT, CS_PIN );
}

uint8_t SDCard::sendCmd(uint8_t cmdNumber, uint32_t payload){
    waitForReady();

    char CMD[6] = {0x40 + cmdNumber, (uint8_t)(payload >> 24), (uint8_t)(payload >> 16), (uint8_t)(payload >> 8), (uint8_t)(payload), (uint8_t) 0};
    CMD[5] = (getCRC7(CMD, 5) << 1) | 1;
    //Console::log(" #CMD: %x %x %x %x %x %x", CMD[0],CMD[1],CMD[2],CMD[3],CMD[4],CMD[5]);

    for(int j = 0; j < 6; j++){
        SPISD->transfer(CMD[j]);
    }

    uint8_t R1;
    do {
        R1 = SPISD->transfer(0xff);
    } while ((R1 & 0x80) != 0);

    return R1;
}

void SDCard::waitForReady(){
    uint8_t reply;
    do {
        reply = SPISD->transfer(0xff);
    } while ((reply) != 0xff);
}

void SDCard::getArray(uint8_t Buff[], int size){
    for(int k = 0; k < size; k++){
        Buff[k] = SPISD->transfer(0xff);
    }
}
