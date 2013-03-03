/*
 * Cornet.h
 *
 *  Created on: Feb 24, 2013
 *      Author: ravi
 */

#ifndef CORNET_H_
#define CORNET_H_
#include <string>
#include <list>

class Cornet {
private:
	std::string parent;
	std::list<std::string> requestList;
public:
	Cornet();
	void addElement(std::string);
	std::string getElement();
	bool isEmpty();
	size_t size();
	virtual ~Cornet();
};


#endif /* CORNET_H_ */
