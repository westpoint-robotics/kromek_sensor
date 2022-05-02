#include "stdafx.h"
#include "RollingQueue.h"
#include <cstring>

namespace kmk
{

RollingQueue::RollingQueue(int bufferSize, int numBuffers)
: _bufferSize(bufferSize)
, _totalBuffers(numBuffers)
, _readIndex(0)
, _writeIndex(0)
, _numEntries(0)
{
	_data.resize(bufferSize * numBuffers);
	_timestamps.resize(numBuffers);
}

RollingQueue::~RollingQueue()
{
}

// Increment the value and rollover to the start if necessary
int RollingQueue::IncrementCounter(int currentVal)
{
	++currentVal;
	return (currentVal >= _totalBuffers) ? 0 : currentVal;
}

bool RollingQueue::Enqueue(int64_t timeStamp, unsigned char *pData, size_t dataSize)
{
	if (dataSize > (size_t)_bufferSize)
		return false;

	kmk::Lock lock (_criticalSection);
    memcpy(&_data[_writeIndex * _bufferSize], pData, dataSize);
	_timestamps[_writeIndex] = timeStamp;
	_writeIndex = IncrementCounter(_writeIndex);

	if (_numEntries == _totalBuffers)
	{
		// Queue is full so we just replaced the oldest entry, move the read pointer
		_readIndex = IncrementCounter(_readIndex);
	}
	else
	{
		++_numEntries;
	}

	return true;
}

bool RollingQueue::Dequeue(unsigned char *pDataOut, size_t dataSize, int64_t &timeStampOut)
{
    if (dataSize != (size_t)_bufferSize)
		return false;

	kmk::Lock lock (_criticalSection);

	if (_numEntries == 0)
		return false;

	memcpy(pDataOut, &_data[_readIndex * _bufferSize], _bufferSize);
	timeStampOut = _timestamps[_readIndex];
	_readIndex = IncrementCounter(_readIndex);
	--_numEntries;
	return true;
}

void RollingQueue::Clear()
{
	kmk::Lock lock (_criticalSection);
	_numEntries = 0;
	_readIndex = 0;
	_writeIndex = 0;
}

bool RollingQueue::IsEmpty()
{
	kmk::Lock lock (_criticalSection);

	// The write will always be 1 behind the read unless the reach catches up with the
	return _numEntries == 0;
}

}
