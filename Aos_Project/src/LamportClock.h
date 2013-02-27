/*
 * LamportClock.h
 *
 *  Created on: Feb 23, 2013
 *      Author: ravi
 */

#ifndef LAMPORTCLOCK_H_
#define LAMPORTCLOCK_H_

class LamportClock {
private:
	unsigned int clockValue;
	unsigned int clockRate;
public:
	explicit LamportClock(int clockValue = 0 , int clockRate = 1);
	void tick();
	void tick(int clockValue);
	unsigned int getClockValue() const;
	unsigned int getClockRate() const;
	void setClockValue(const int clockValue);
	void setClockRate(const int clockRate);
	virtual ~LamportClock();
};

#endif /* LAMPORTCLOCK_H_ */
