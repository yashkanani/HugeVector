#pragma once
#include <qsqldatabase.h>
#include "qsqlquery.h"
#include "qsqlerror.h"

class SQLiteDataBase
{
	QSqlDatabase mydb;
	QString uniqueName;
	bool connOpen();
	void connClose();

	inline bool sendquery(QString val);
	inline QByteArray readValue(QString val);
	bool deleteTable(QString tableName);
	inline void cleanDBFile();
public:
	
	bool push_back(QString& val);
	qreal at(QString &index);


	SQLiteDataBase();
	~SQLiteDataBase();
};

