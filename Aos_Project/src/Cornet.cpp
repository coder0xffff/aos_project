/*
 * Cornet.cpp
 *
 *  Created on: Feb 24, 2013
 *      Author: ravi
 */

#include "Cornet.h"
using namespace std;

Cornet::Cornet() {
	parent.clear();

}

Cornet::~Cornet() {
	// TODO Auto-generated destructor stub
}

void Cornet::addElement(std::string node) {
	if(parent.empty()) {
		parent = node;
	}
	else {
		requestList.push_back(node);
	}

}

string Cornet::getElement() {
	string node;
	if(requestList.empty()) {
		node = parent;
		parent.clear();
	}
	else {
		node = requestList.front();
		requestList.pop_front();
	}
	return node;
}

bool Cornet::isEmpty() {
	if(parent.empty())
		return true;
	else
		return false;
}

size_t Cornet::size() {
	if(parent.empty())
		return 0;
	else
		return requestList.size() + 1;
}
