#include "MyHugeVector.h"
#include "qdebug.h"


MyHugeVector::MyHugeVector()
{

}

MyHugeVector::~MyHugeVector()
{
	
}

void MyHugeVector::push_back(qreal value) {
	QString _val = QString::number(value);
	dataBase.push_back(_val);  // return bool on successful 
}

qreal MyHugeVector::at(uint index) {
	QString _val = QString::number(index);
	return dataBase.at(_val);
}