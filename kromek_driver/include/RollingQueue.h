#pragma once

#include <types.h>
#include <vector>
#include "Lock.h"

namespace kmk
{

// Queue of memory buffers + timestamps that rolls back around to the beginning of the buffer when space runs out.
class RollingQueue
{
private:
	std::vector<unsigned char> _data;
	std::vector<int64_t> _timestamps;
	int _bufferSize;
	int _totalBuffers;
	int _readIndex;
	int _writeIndex;
	int _numEntries; 
	kmk::CriticalSection _criticalSection;

	// Increment the value and rollover to the start if necessary
	int IncrementCounter(int currentVal);

public:
	RollingQueue(int bufferSize, int numBuffers);
	~RollingQueue();

	bool Enqueue(int64_t timeStamp, unsigned char *pData, size_t dataSize);
	bool Dequeue(unsigned char *pDataOut, size_t dataSize, int64_t &timestampOut);

	void Clear();
	bool IsEmpty();
};

}
