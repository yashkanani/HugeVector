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
		
		typedef struct Frame
		{
			explicit Frame::Frame(qint64 fp, qint64 fs)
				: m_fPos(fp), m_fSize(fs)
			{};
			qint64 m_fPos;
			qint64 m_fSize;
		} Frame;


		template <class ValueType>
		struct ContainerObjectData : public QSharedData
		{
			bool m_isAvailable;
			union ObjectData
			{
				explicit ObjectData(qint64 fp, qint64 fs)
					:m_frame(fp,fs)
				{}
				explicit ObjectData(ValueType* v)
					:m_val(v)
				{}

				Frame m_frame;
				ValueType* m_val;
			} m_data;
			explicit ContainerObjectData(qint64 fp, qint64 fs)
				:QSharedData()
				, m_isAvailable(false)
				, m_data(fp,fs)
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
				, m_data(other.m_data.m_frame.m_fPos, other.m_data.m_frame.m_fSize)
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
			explicit ContainerObject(qint64 fPos, qint64 fSize)
				:m_d(new ContainerObjectData<ValueType>(fPos, fSize))
			{}
			explicit ContainerObject(ValueType* val)
				:m_d(new ContainerObjectData<ValueType>(val))
			{}

			ContainerObject(const ContainerObject& other) = default;
			bool isAvailable() const { return m_d->m_isAvailable; }
			
			qint64 fPos() const { return m_d->m_data.m_frame.m_fPos; }
			qint64 fSize() const { return m_d->m_data.m_frame.m_fSize; }

			const ValueType* val() const { Q_ASSERT(m_d->m_isAvailable); return m_d->m_data.m_val; }
			ValueType* val() { Q_ASSERT(m_d->m_isAvailable); m_d.detach(); return m_d->m_data.m_val; }
			

			void setFPos(const Frame& m_frame) {
				setFPos(m_frame.m_fPos, m_frame.m_fSize);
			}

			void setFPos(qint64 fp, qint64 fs)
			{
				if (!m_d->m_isAvailable && m_d->m_data.m_frame.m_fPos == fp)
					return;
				m_d.detach();
				if (m_d->m_isAvailable)
					delete m_d->m_data.m_val;
				m_d->m_data.m_frame.m_fPos = fp;
				m_d->m_data.m_frame.m_fSize = fs;
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
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a data file");
				m_device->seek(0);
				if (!m_memoryMap->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a memoryMap file");
				m_memoryMap->seek(0);
			}
			~HugeContainerData() = default;
			
			HugeContainerData(HugeContainerData& other)
				: QSharedData(other)
				, m_device(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
				, m_memoryMap(std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX")))
			{
				if (!m_device->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a data file");
				other.m_device->seek(0);
				qint64 totalSize = other.m_device->size();
				for (; totalSize > 1024; totalSize -= 1024)
					m_device->write(other.m_device->read(1024));
				m_device->write(other.m_device->read(totalSize));

				if (!m_memoryMap->open())
					Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a memoryMap file");
				other.m_memoryMap->seek(0);
				totalSize = other.m_memoryMap->size();
				for (; totalSize > 1024; totalSize -= 1024)
					m_memoryMap->write(other.m_memoryMap->read(1024));
				m_memoryMap->write(other.m_memoryMap->read(totalSize));

			}

		};

		QExplicitlySharedDataPointer<HugeContainerData<ValueType>> m_d;


		qint64 writeInData(const QByteArray& block) const
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


	
		Frame writeElementInData(const ValueType& val) const
		{
			QByteArray block;
			{
				QDataStream writerStream(&block, QIODevice::WriteOnly);
				writerStream << val;
			}

			Frame result(-1, -1);
			const qint64 pos = writeInData(block);
			if (pos >= 0) {
				result = Frame(pos, block.size());
			}
			 
			return result;
		}

		bool writeInMap(const QByteArray& block) const
		{
			if (!m_d->m_memoryMap->isWritable())
				return false;
			
			if (m_d->m_memoryMap->write(block) >= 0) {
				return true;
			}
			return false;

		}

		bool writeElementInMap(const Frame& val) const
		{
			QByteArray block;
			{
				QDataStream writerStream(&block, QIODevice::WriteOnly);
				writerStream << val.m_fPos;
				writerStream << val.m_fSize;
			}

			const bool result = writeInMap(block);
			return result;
		}


		/* rewrite is very expensive functionality */
		bool reWriteMap(const uint& index, const uint& at) const {
			auto readPos = index * sizeof(Frame);
			auto writePos = at * sizeof(Frame);

			std::unique_ptr<QTemporaryFile> tempFile;
			tempFile = std::make_unique<QTemporaryFile>(QDir::tempPath() + QDir::separator() + QStringLiteral("HugeContainerDataXXXXXX"));

			if (!tempFile->open())
				Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to create a temporary file");

			/* write the all address from particular location into temp file */
			m_d->m_memoryMap->seek(readPos);
			qint64 totalSize = m_d->m_memoryMap->size() - readPos;
			for (; totalSize > 1024; totalSize -= 1024)
				tempFile->write(m_d->m_memoryMap->read(1024));
			tempFile->write(m_d->m_memoryMap->read(totalSize));

			/* write the all address from temp file to new location in memoryMap file */
			tempFile->seek(0);
			m_d->m_memoryMap->seek(writePos);

			totalSize = tempFile->size();
			for (; totalSize > 1024; totalSize -= 1024)
				m_d->m_memoryMap->write(tempFile->read(1024));
			m_d->m_memoryMap->write(tempFile->read(totalSize));

			auto lastPos = m_d->m_memoryMap->pos();
			m_d->m_memoryMap->resize(lastPos);

			return true;
		}


		bool saveQueue(std::unique_ptr<ContainerObject<ValueType>>& valToWrite, const int &index) const {
			bool allOk = false;
			
			/*Write the value in DataFile*/
			const Frame result = writeElementInData(*(valToWrite->val()));
			if (result.m_fPos >= 0) {
				/*
				    Whenever push_back funcation is called at that time 
				    elements is append in file. 
				*/
				if (index < 0) {
				   allOk = writeElementInMap(result); 
				}
				else {
				 /* 
				    Whenever insert funcation is called at that time 
				    Address file will rewrite and elements is write at 
					particular location. 
				 */
					if (reWriteMap(index, index + 1)) {
						auto pos = m_d->m_memoryMap->pos();
						m_d->m_memoryMap->seek(index * sizeof(Frame));
						allOk = writeElementInMap(result);
						m_d->m_memoryMap->seek(pos);
					}
				}
			}

			return allOk;
		}

		bool enqueueValue(std::unique_ptr<ValueType>& val,const int index = -1) const
		{
			auto tempVal = std::make_unique<ContainerObject<ValueType>>(val.release());
			if (saveQueue(tempVal, index)) {
				return true;
			}
		    return false;
		}

		std::unique_ptr<ValueType> valueFromBlock(const uint& index) const
		{
			/*read address of data*/
			QByteArray rawFram = readMap(index);
			if (rawFram.isEmpty()) {
				return nullptr;
			}

			/* decode address and size */
			auto frame = std::make_unique<Frame>(-1,-1);
			QDataStream Stream(rawFram);
			Stream >> frame->m_fPos;
			Stream >> frame->m_fSize;

			/*read data*/
			QByteArray block = readData(*frame);
			if (block.isEmpty())
				return nullptr;

			/*decode data*/
			auto result = std::make_unique<ValueType>();
			QDataStream readerStream(block);
			readerStream >> *result;
			return result;
		}


		QByteArray readData(const Frame& dataFrame) const
		{
			if (Q_UNLIKELY(!m_d->m_device->isReadable()))
				return QByteArray();
			m_d->m_device->setTextModeEnabled(false);
			
			auto tempPos = m_d->m_device->pos();
			m_d->m_device->seek(dataFrame.m_fPos);
			
			QByteArray result;
			result = m_d->m_device->read(dataFrame.m_fSize);
			
			m_d->m_device->seek(tempPos);
			return result;
		}


		QByteArray readMap(const uint& index) const {

			if (Q_UNLIKELY(!m_d->m_device->isReadable()))
				return QByteArray();
			m_d->m_memoryMap->setTextModeEnabled(false);
			
			auto tempPos = m_d->m_memoryMap->pos();

			auto startPos = index * sizeof(Frame);
			m_d->m_memoryMap->seek(startPos);

			QByteArray result; 
			result = m_d->m_memoryMap->read(sizeof(Frame)); 
			
			m_d->m_memoryMap->seek(tempPos);
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
		
		void push_back(ValueType* val)
		{
			if (!val)
				return;
			m_d.detach();
			std::unique_ptr<ValueType> tempval(val);
			enqueueValue(tempval);
			
		}

		/*
		  if index is same as size() then value append to the file
		  if index is correct then insert the value at particular location.  
		*/
		void insert(uint index, const ValueType &val) {
			
			m_d.detach();
			auto tempval = std::make_unique<ValueType>(val);
			
			if (index != size()) {
				Q_ASSERT(correctIndex(index));
				enqueueValue(tempval, index);
			}
			else {
				enqueueValue(tempval);
			}
		}


		void insert(const uint& index, ValueType* val)
		{
			if (!val)
				return;

			m_d.detach();
			std::unique_ptr<ValueType> tempval(val);

			if (index != size()) {
				Q_ASSERT(correctIndex(index));
				enqueueValue(tempval, index);
			}
			else {
				enqueueValue(tempval);
			}
		}


		/* Must be put correct index for finding value */
		const ValueType& at(const uint& index)
		{
			Q_ASSERT(correctIndex(index));

			auto result = valueFromBlock(index);
			Q_ASSERT(result);
			
			m_d.detach();
			return *(result.release());
		}

		bool removeAt(const uint& index)
		{
			m_d.detach();
			reWriteMap(index+1, index);
			return true;
		}

		void clear()
		{
			if (isEmpty())
				return;
			m_d.detach();
			if (!m_d->m_device->resize(0)) {
				Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to resize data file");
			}
			if (!m_d->m_memoryMap->resize(0)) {
				Q_ASSERT_X(false, "HugeContainer::HugeContainer", "Unable to resize memoryMap file");
			}
			
		}

		int count() const
		{
			return size();
		}
		int size() const
		{
			return (m_d->m_memoryMap->size()/sizeof(Frame));
		}
		bool isEmpty() const
		{
			bool ret = true;
			if (m_d->m_memoryMap->pos() != 0) {
				ret = false;
			}
			return ret;
		}

		bool correctIndex(const uint& index) {
			return ((index >= 0) && (this->size() > index));
		}

		inline const ValueType& first() const
		{
			Q_ASSERT(!isEmpty());
			return at(0);
		}

		inline ValueType& first()
		{
			Q_ASSERT(!isEmpty());
			return const_cast<ValueType&>(at(0));
		}

		inline const ValueType& last() const
		{
			Q_ASSERT(!isEmpty());
			return at(size() - 1);
		}

		inline ValueType& last()
		{
			Q_ASSERT(!isEmpty());
			return const_cast<ValueType&>(at(size() - 1));
		}
	    
	};

}
#endif // hugecontainer_h__