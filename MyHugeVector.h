#pragma once
#include "SQLiteDataBase.h"
class MyHugeVector
{
private:
	SQLiteDataBase dataBase;

public:

	void push_back(qreal value);
	qreal at(uint index);

	MyHugeVector();
	~MyHugeVector();
};

