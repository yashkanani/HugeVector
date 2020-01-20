#pragma once
#ifndef hugecontainer_h__
#define hugecontainer_h__

#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <qvector.h>
#include <QSharedData>
#include <QSharedDataPointer>
#include <QExplicitlySharedDataPointer>
#include <QTemporaryFile>
#include <QDebug>
#include <memory>
#include <initializer_list>


namespace HugeContainers {
	template <class ValueType>
	class HugeContainer;
}

template <class ValueType>
QDataStream& operator<<(QDataStream &out, const HugeContainers::HugeContainer<ValueType>& cont);

template <class ValueType>
QDataStream& operator>>(QDataStream &in, HugeContainers::HugeContainer<ValueType>& cont);


namespace HugeContainers {
	//! Removes any leftover data from previous crashes
	inline void cleanUp() {
		QDirIterator cleanIter{ QDir::tempPath(), QStringList(QStringLiteral("HugeContainerData*")), QDir::Files | QDir::Writable | QDir::CaseSensitive | QDir::NoDotAndDotDot };
		while (cleanIter.hasNext()) {
			cleanIter.next();
			QFile::remove(cleanIter.filePath());
		}

	}

	template <class ValueType>
	class HugeContainer
	{
		static_assert(std::is_default_constructible<ValueType>::value, "ValueType must provide a default constructor");
		static_assert(std::is_copy_constructible<ValueType>::value, "ValueType must provide a copy constructor");
	private:
		template <class ValueType>
		struct ContainerObjectData : public QSharedData
		{
			bool m_isAvailable;
			union ObjectData
			{
				explicit ObjectData(qint64 fp)
					:m_fPos(fp)
				{}
				explicit ObjectData(ValueType* v)
					:m_val(v)
				{}
				qint64 m_fPos;
				ValueType* m_val;
			} m_data;
			explicit ContainerObjectData(qint64 fp)
				:QSharedData()
				, m_isAvailable(false)
				, m_data(fp)
			{}
			explicit ContainerObjectData(ValueType* v)
				:QSharedData()
				, m_isAvailable(true)
				, m_data(v)
			{
				Q_ASSERT(v);
			}
			~ContainerObjectData()
			{
				if (m_isAvailable)
					delete m_data.m_val;
			}
			ContainerObjectData(const ContainerObjectData& other)
				:QSharedData(other)
				, m_isAvailable(other.m_isAvailable)
				, m_data(other.m_data.m_fPos)
			{
				if (m_isAvailable)
					m_data.m_val = new ValueType(*(other.m_data.m_val));
			}
		};

		template <class ValueType>
		class ContainerObject
		{
			QExplicitlySharedDataPointer<ContainerObjectData<ValueType> > m_d;
		public:
			explicit ContainerObject(qint64 fPos)
				:m_d(new ContainerObjectData<ValueType>(fPos))
			{}
			explicit ContainerObject(ValueType* val)
				:m_d(new ContainerObjectData<ValueType>(val))
			{}
			ContainerObject(const ContainerObject& other) = default;
			bool isAvailable() const { return m_d->m_isAvailable; }
			qint64 fPos() const { return m_d->m_data.m_fPos; }
			const ValueType* val() const { Q_ASSERT(m_d->m_isAvailable); return m_d->m_data.m_val; }
			ValueType* val() { Q_ASSERT(m_d->m_isAvailable); m_d.detach(); return m_d->m_data.m_val; }
			void setFPos(qint64 fp)
			{
				if (!m_d->m_isAvailable && m_d->m_data.m_fPos == fp)
					return;
				m_d.detach();
				if (m_d->m_isAvailable)
					delete m_d->m_data.m_val;
				m_d->m_data.m_fPos = fp;
				m_d->m_isAvailable = false;
			}
			void setVal(ValueType* vl)
			{
				m_d.detach();
				if (m_d->m_isAvailable)
					delete m_d->m_data.m_val;
				m_d->m_data.m_val = vl;
				m_d->m_isAvailable = true;
			}
		};


		template <class ValueType>
		class HugeContainerData : public QSharedData
		{
		public:
			using ItemMapType = QVector<ContainerObject<ValueType>>;
			std::unique_ptr<QTemporaryFile> m_memoryMap;
			std::unique_ptr<QTemporaryFile> m_device;

			HugeContainerData()
				: QSharedData()
				, m_device(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
				, m_memoryMap(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
			{
				if (!m_device->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a temporary file");
				m_device->seek(0);
				if (!m_memoryMap->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a temporary file");
				m_memoryMap->seek(0);
			}
			~HugeContainerData() = default;
			
			HugeContainerData(HugeContainerData& other)
				: QSharedData(other)
				, m_device(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
				, m_memoryMap(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
			{
				if (!m_device->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a temporary file");
				other.m_device->seek(0);
				qint64 totalSize = other.m_device->size();
				for (; totalSize > 1024; totalSize -= 1024)
					m_device->write(other.m_device->read(1024));
				m_device->write(other.m_device->read(totalSize));

				if (!m_memoryMap->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a temporary file");
				other.m_memoryMap->seek(0);
				totalSize = other.m_memoryMap->size();
				for (; totalSize > 1024; totalSize -= 1024)
					m_memoryMap->write(other.m_memoryMap->read(1024));
				m_memoryMap->write(other.m_memoryMap->read(totalSize));

			}

		};

		QExplicitlySharedDataPointer<HugeContainerData<ValueType>> m_d;


		qint64 writeInMap(const QByteArray& block) const
		{
			if (!m_d->m_device->isWritable())
				return -1;

			auto pos = m_d->m_device->pos();
			m_d->m_device->seek(pos);
			if (m_d->m_device->write(block) >= 0) {
				return pos;
			}
			return -1;
			
		}

		void removeFromMap(qint64 pos) const {
			/*auto fileIter = m_d->m_memoryMap->find(pos);
			Q_ASSERT(fileIter != m_d->m_memoryMap->end());
			if (fileIter.value())
				return;
			fileIter.value() = true;
			m_d->m_memoryMap->erase(fileIter);*/
		}


	
		qint64 writeElementInMap(const ValueType& val) const
		{
			QByteArray block;
			{
				QDataStream writerStream(&block, QIODevice::WriteOnly);
				writerStream << val;
			}

			const qint64 result = writeInMap(block);
			return result;
		}


		bool saveQueue(std::unique_ptr<ContainerObject<ValueType>>& valToWrite ) const {
			bool allOk = false;
			const qint64 result = writeElementInMap(*(valToWrite->val()));
			if (result >= 0) {
				valToWrite->setFPos(result);
				allOk = true;
			}
			return allOk;
		}

		bool enqueueValue(std::unique_ptr<ValueType>& val) const
		{
			auto tempVal = std::make_unique<ContainerObject<ValueType>>(val.release());
			if (saveQueue(tempVal)) {
				return true;
			}
		    return false;
		}

		std::unique_ptr<ValueType> valueFromBlock(const uint& index) const
		{
			QByteArray block = readBlock(index);
			if (block.isEmpty())
				return nullptr;
			auto result = std::make_unique<ValueType>();
			QDataStream readerStream(block);
			readerStream >> *result;
			return result;
		}

		QByteArray readBlock(const uint& index ) const
		{
			//if (Q_UNLIKELY(!m_d->m_device->isReadable()))
			//	return QByteArray();
			//m_d->m_device->setTextModeEnabled(false);
			//
			//auto itemIter = m_d->m_itemsMap->begin() + index;       //  get iterator at particular position
			//Q_ASSERT(itemIter != m_d->m_itemsMap->end());
			//Q_ASSERT(!itemIter->isAvailable());
			//
			//auto fileIter = m_d->m_memoryMap->constFind(itemIter->fPos());
			//Q_ASSERT(fileIter != m_d->m_memoryMap->constEnd());
			//if (fileIter.value())
			//	return QByteArray();
			//
			//auto nextIter = fileIter + 1;
			//m_d->m_device->seek(fileIter.key());
			
			QByteArray result;
			/*if (nextIter == m_d->m_memoryMap->constEnd())
				result = m_d->m_device->readAll();
			else
				result = m_d->m_device->read(nextIter.key() - fileIter.key());*/
			
			return result;
		}


	public:

		HugeContainer()
			:m_d(new HugeContainerData<ValueType>{})
		{
		}

		HugeContainer(const HugeContainer& other) = default;
		HugeContainer& operator=(const HugeContainer& other) = default;
		HugeContainer& operator=(HugeContainer&& other) Q_DECL_NOTHROW {
			swap(other);
			return *this;
		}


		void swap(HugeContainer<ValueType>& other) Q_DECL_NOTHROW {
			std::swap(m_d, other.m_d);
		}

		
		void push_back(const ValueType &val) {
			m_d.detach();
			auto tempval = std::make_unique<ValueType>(val);
			enqueueValue(tempval);
		}
		
		/*void push_back(ValueType* val)
		{
			if (!val)
				return;
			m_d.detach();
			std::unique_ptr<ValueType> tempval(val);
			enqueueValue(tempval);
			
		}*/

		/*if index is not correct then append to the vector*/
		//void insert(uint index, const ValueType &val) {

		//	if (!correctIndex(index))
		//		index = size();
		//
		//	m_d.detach();
		//	auto tempval = std::make_unique<ValueType>(val);
		//	m_d->m_itemsMap->insert(index, ContainerObject<ValueType>(tempval.release()));
		//	saveQueue(index);
		//}


		//void insert(const uint& index, ValueType* val)
		//{
		//	if (!val)
		//		return;

		//	if (!correctIndex(index))
		//		return;

		//	m_d.detach();
		//	std::unique_ptr<ValueType> tempval(val);

		//	m_d->m_itemsMap->insert(index, ContainerObject<ValueType>(tempval.release()));
		//	saveQueue(index);
		//}


		///* Must be put correct index for finding value */
		//const ValueType& at(const uint& index)
		//{
		//	Q_ASSERT(!isEmpty());

		//	auto valueIter = m_d->m_itemsMap->begin() + index;
		//	Q_ASSERT(valueIter != m_d->m_itemsMap->end());
		//	
		//	auto result = valueFromBlock(index);
		//	Q_ASSERT(result);
		//	
		//	m_d.detach();
		//	return *(result.release());
		//}

		//bool removeAt(const uint& index)
		//{
		//	/*if (!correctIndex(index))
		//		return false;
		//	m_d.detach();
		//	auto itemIter = m_d->m_itemsMap->begin() + index;
		//	Q_ASSERT(itemIter != m_d->m_itemsMap->end());
		//	removeFromMap(itemIter->fPos());
		//	m_d->m_itemsMap->erase(itemIter);*/
		//	return true;
		//}

		//void clear()
		//{
		//	if (isEmpty())
		//		return;
		//	m_d.detach();
		//	if (!m_d->m_device->resize(0)) {
		//		Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to resize temporary file");
		//	}
		//	m_d->m_itemsMap->clear();
		//	m_d->m_memoryMap->clear();
		//	m_d->m_memoryMap->insert(0, true);
		//}

		//int count() const
		//{
		//	return size();
		//}
		//int size() const
		//{
		//	return m_d->m_itemsMap->size();
		//}
		//bool isEmpty() const
		//{
		//	return m_d->m_itemsMap->isEmpty();
		//}

		//bool correctIndex(const uint& index) {
		//	return ((index >= 0) && (m_d->m_itemsMap->size() > index));
		//}

		//int memMapsize() const
		//{
		//	return m_d->m_memoryMap->size();
		//}

		//inline const ValueType& first() const
		//{
		//	Q_ASSERT(!isEmpty());
		//	return at(0);
		//}

		//inline ValueType& first()
		//{
		//	Q_ASSERT(!isEmpty());
		//	return const_cast<ValueType&>(at(0));
		//}

		//inline const ValueType& last() const
		//{
		//	Q_ASSERT(!isEmpty());
		//	return at(size() - 1);
		//}

		//inline ValueType& last()
		//{
		//	Q_ASSERT(!isEmpty());
		//	return const_cast<ValueType&>(at(size() - 1));
		//}
	    
	};

}
#endif // hugecontainer_h__