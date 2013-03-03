/*
 * LamportClock.cpp
 *
 *  Created on: Feb 23, 2013
 *      Author: ravi
 */

#include "LamportClock.h"

LamportClock::LamportClock(int clockValue , int clockRate ) {
	// TODO Auto-generated constructor stub
	this->clockValue = clockValue;
	this->clockRate = clockRate;

}

void LamportClock::tick(){
	this->clockValue += this->clockRate;
}

void LamportClock::tick(int clockValue){
	clockValue += this->clockRate;
	if(this->clockValue < clockValue) {
		this->clockValue = clockValue;
	}
}
LamportClock::~LamportClock() {
	// TODO Auto-generated destructor stub
}

void LamportClock::setClockRate(const int clockRate) {
	this->clockRate = clockRate;
}

void LamportClock::setClockValue(const int clockValue) {
	this->clockValue = clockValue;
}

unsigned int LamportClock::getClockRate() const{
	return this->clockRate;
}

unsigned int LamportClock::getClockValue() const{
	return this->clockValue;
}
