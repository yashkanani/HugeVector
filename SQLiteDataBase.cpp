#include "SQLiteDataBase.h"
#include "quuid.h"
#include "qdebug.h"
#include <QSqlRecord>
#include <qfile.h>

SQLiteDataBase::SQLiteDataBase()
{
	uniqueName = QUuid::createUuid().toString();
	
	mydb = QSqlDatabase::addDatabase("QSQLITE", uniqueName);
	mydb.setDatabaseName(uniqueName+".db");
		
	if (connOpen()) {
		sendquery("create table Vector(value double)");
	}
}


SQLiteDataBase::~SQLiteDataBase()
{
	deleteTable("Vector");
	connClose();
	cleanDBFile();
}


bool SQLiteDataBase::connOpen() {
	bool ret = false;
	if (mydb.open()) {
		ret = true;
	}
	return ret;
}

void SQLiteDataBase::connClose() {
	mydb.close();
	return;
}


bool SQLiteDataBase::sendquery(QString val="") {
	bool ok = false;
	if (connOpen()) {
		QSqlQuery query(mydb);
			
		ok = query.exec(val);
		if (!ok) {
			qDebug() << "Error on sendquery" << query.lastError();
		}
	}
	return ok;
}

QByteArray SQLiteDataBase::readValue(QString val = "") {
	QByteArray data;
	if (connOpen()) {
		QSqlQuery query(mydb);

	    bool ok = query.exec(val);
		if (!ok) {
			qDebug() << "Error on sendquery" << query.lastError();
		}
		else {
			while (query.next()) {
				QSqlRecord record = query.record();
				int fieldNo = record.indexOf("value");
				data = record.value(fieldNo).toByteArray();
			}
		}
	}
	return data;
}



/* Insert the value at last in Vector table*/
bool SQLiteDataBase::push_back(QString& val) {
	return sendquery("insert into Vector values("+val+")");
}


qreal SQLiteDataBase::at(QString &index) {
	return readValue("SELECT * FROM Vector WHERE ROWID=" + index).toDouble();
}

bool SQLiteDataBase::deleteTable(QString tableName) {
	return sendquery("DROP TABLE "+ tableName);
}

void SQLiteDataBase::cleanDBFile() {
	auto ret = mydb.databaseName();
		QFile::remove(mydb.databaseName());
}