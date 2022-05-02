#include "PacketStreamers.h"
#include <iterator>
#include <array>
#include <mutex>
#include <cstring>
#include "crc.h"

namespace kmk
{
	// The main reports max size is 8205 bytes and we only request it one at a time. 
	// Add some more space for configuration setting reports that might also be returned
	constexpr uint16_t MAX_REPORT_SIZE = 8500;

	constexpr uint8_t  FRAME_BYTE = 0xC0;
	constexpr uint8_t  ESC_BYTE = 0xDB;
	constexpr uint8_t  ESC_FRAME_BYTE = 0xDC;
	constexpr uint8_t  ESC_ESC_BYTE = 0xDD;


	SerialPacketStreamer::SerialPacketStreamer(size_t bufferSize)
		: _writeIndex(0)
		, _enableDataRecovery(false)
		, _lastDataTime()
	{
		_buffer.resize(bufferSize);
	}

	void SerialPacketStreamer::Clear()
	{
		std::lock_guard<std::mutex> lock(_bufferMutex);
		_writeIndex = 0;
	}


	bool SerialPacketStreamer::AddIncomingData(const BYTE* pData, size_t dataSize)
	{
		std::lock_guard<std::mutex> lock(_bufferMutex);
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

		if (UpdateDataRecoveryStatus(now))
		{
			// Append data to the buffer if there is space
			if (_writeIndex + dataSize > _buffer.size())
			{
				return false;
			}

			std::memcpy(&_buffer[_writeIndex], pData, dataSize);
			_writeIndex += dataSize;
		}
		_lastDataTime = now;
		return true;
	}

	bool SerialPacketStreamer::UpdateDataRecoveryStatus(std::chrono::steady_clock::time_point now)
	{
		if (_enableDataRecovery)
		{
			// We now want to wait for an idle time of > 100ms before we start accepting data again
			if (now > _lastDataTime + std::chrono::milliseconds(100))
			{
				// Recovered
				_enableDataRecovery = false;
			}
		}
		return !_enableDataRecovery;
	}

	bool SerialPacketStreamer::ReadPacket(std::vector<BYTE>& dataOut)
	{
		std::lock_guard<std::mutex> lock(_bufferMutex);

		// We need at least 2 bytes for the packet length
		if (_writeIndex < 2)
		{
			return false; // Not enough data
		}

		uint16_t* pPacketSize = reinterpret_cast<uint16_t*>(&_buffer[0]);
		if (*pPacketSize == 0 || *pPacketSize > MAX_REPORT_SIZE)
		{
			// Packet size is invalid - critical error!
			// Attempt a recovery and clear all data currently in the buffer
			_enableDataRecovery = true;
			_writeIndex = 0;
			throw std::runtime_error("Corrupt data detected - Invalid Packet Size");
		}

		// Ok we have a size but do we have enough data for the packet?
		if (_writeIndex < *pPacketSize)
		{
			return false; // Not enough data yet
		}

		// Verify the crc which will be the last two bytes
		uint16_t* pPacketCrc = reinterpret_cast<uint16_t*>(&_buffer[*pPacketSize - 2]);
		if (*pPacketCrc == 0 || *pPacketCrc == kmk::crc::CalculateCrc(&_buffer[0], *pPacketSize - 2))
		{
			// Take the data out
			dataOut.resize(*pPacketSize);
			std::copy(_buffer.begin(), std::next(_buffer.begin(), *pPacketSize), dataOut.begin());

			// Move remaining data to start of buffer
			std::copy(std::next(_buffer.begin(), *pPacketSize), std::next(_buffer.begin(), _writeIndex), _buffer.begin());
			_writeIndex -= *pPacketSize;
			return true;
		}
		else
		{
			// Invalid crc is a critical error as the data has become corrupted and we can not trust any of it
			_enableDataRecovery = true;
			_writeIndex = 0;
			throw std::runtime_error("Corrupt data detected - Crc failed");
		}		
	}

	void SerialPacketStreamer::PrepareForSend(const std::vector<BYTE>& dataToSend, std::vector<BYTE>& preparedDataOut)
	{
		// Standard serial does not change the data so copy to prepared buffer
		preparedDataOut = dataToSend;
	}

	FramedPacketStreamer::FramedPacketStreamer()
		: _writeIndex(0)
		, _firstByteEscaped(false)
	{
		_buffer.resize(MAX_REPORT_SIZE);

		// Create buffers for complete packets and add to the pool
		_poolBuffer.resize(MAX_REPORT_SIZE * PacketPoolSize);
		for (size_t i = 0; i < PacketPoolSize; ++i)
		{
			_packetPool.push_back(&_poolBuffer[PacketPoolSize * i]);
		}
	}

	FramedPacketStreamer::~FramedPacketStreamer()
	{
		_packetPool.clear();
		_packetsReady.clear();
	}

	void FramedPacketStreamer::Clear()
	{
		std::lock_guard<std::mutex> lock(_bufferMutex);
		_writeIndex = 0;
		_firstByteEscaped = false;

		// Move all packets back to the pool
		std::lock_guard<std::mutex> poolLock(_poolMutex);
		for (auto i : _packetsReady)
		{
			_packetPool.push_back(i);
		}
		_packetsReady.clear();
	}

	bool FramedPacketStreamer::AddIncomingData(const BYTE* pData, size_t dataSize)
	{
		// Framed data contains escaped characters. When we find an escaped character sequence replace with the correct character
		std::lock_guard<std::mutex> lock(_bufferMutex);
		size_t readIndex = 0;

		if (_firstByteEscaped)
		{
			// The first byte being read in was escaped in the previous chunk. Treat it as such
			_buffer[_writeIndex++] = pData[readIndex++] == ESC_ESC_BYTE ? ESC_BYTE : FRAME_BYTE;
			_firstByteEscaped = false;
		}

		while (readIndex < dataSize)
		{
			if (pData[readIndex] == FRAME_BYTE)
			{
				// End of a frame marker
				// First check that the length of the packet is sufficient for a size. 
				// If it is then compare the size with the number of bytes we have read.
				// If they match then we have a valid packet. If not then remove the corrupt data from the buffer
				if (_writeIndex >= 2)
				{
					uint16_t* pSize = reinterpret_cast<uint16_t*>( &_buffer[0]);
					if (*pSize == _writeIndex)
					{
						// Verify the crc which will be the last two bytes
						uint16_t* pPacketCrc = reinterpret_cast<uint16_t*>(&_buffer[_writeIndex - 2]);
						if (*pPacketCrc == 0 || *pPacketCrc == kmk::crc::CalculateCrc(&_buffer[0], _writeIndex - 2))
						{
							// We have a full packet. Add to recieved queue
							std::lock_guard<std::mutex> poolLock(_poolMutex);
							if (_packetPool.size() > 0)
							{
								BYTE* pPoolBuf = _packetPool.front();
								std::memcpy(pPoolBuf, &_buffer[0], *pSize);
								_packetPool.pop_front();
								_packetsReady.push_back(pPoolBuf);
							}
						}
					}
				}

				_writeIndex = 0;
				++readIndex;
			}
			else if (_writeIndex >= _buffer.size())
			{
				// We have run out of space in the packet buffer. No packet should be bigger than the buffer so something has gone wrong (missing frame byte in corrupted data?)
				// Discard the buffer as we will have to wait for another frame byte, fail the crc and discard
				_writeIndex = 0;
			}
			else if (pData[readIndex] == ESC_BYTE)
			{
				if (readIndex + 1 >= dataSize)
				{
					// We have a escape character sequence that is split between buffers. Tag that we expect the first
					// byte of the next AddData request to contain the actual escaped character
					_firstByteEscaped = true;
					break;
				}
				else
				{
					// Convert the escaped byte into the correct non escaped byte value
					_buffer[_writeIndex++] = pData[readIndex + 1] == ESC_ESC_BYTE ? ESC_BYTE : FRAME_BYTE;
					readIndex += 2; // Skip escape and character
				}
			}
			else
			{
				_buffer[_writeIndex++] = pData[readIndex++];
			}
		}
		return true;
	}

	bool FramedPacketStreamer::ReadPacket(std::vector<BYTE>& dataOut)
	{
		std::lock_guard<std::mutex> lock(_poolMutex);

		if (_packetsReady.size() != 0)
		{
			BYTE* pPacketData = *_packetsReady.begin();
			uint16_t* pPacketSize = reinterpret_cast<uint16_t*>(pPacketData);
			std::memcpy(&dataOut[0], pPacketData, *pPacketSize);

			// Return to pool
			_packetsReady.pop_front();
			_packetPool.push_back(pPacketData);
			return true;
		}
		
		return false;
	}

	void FramedPacketStreamer::PrepareForSend(const std::vector<BYTE>& dataToSend, std::vector<BYTE>& preparedDataOut)
	{
		size_t numToEscape = 0;
		// Add escapes to any special characters
		for (size_t i = 0; i < dataToSend.size(); ++i)
		{
			if (dataToSend[i] == FRAME_BYTE || dataToSend[i] == ESC_BYTE)
			{
				++numToEscape;
			}
		}

		// Copy byte for byte and add escapes
		size_t writeIndex = 0;
		preparedDataOut.resize(dataToSend.size() + numToEscape + 1);
 
		if (numToEscape == 0)
		{
			std::copy(dataToSend.begin(), dataToSend.end(), preparedDataOut.begin());
		}
		else
		{
			for (size_t readIndex = 0; readIndex < dataToSend.size(); ++readIndex)
			{
				switch (dataToSend[readIndex])
				{
				case FRAME_BYTE:
					preparedDataOut[writeIndex++] = ESC_BYTE;
					preparedDataOut[writeIndex++] = ESC_FRAME_BYTE;
					break;
				case ESC_BYTE:
					preparedDataOut[writeIndex++] = ESC_BYTE;
					preparedDataOut[writeIndex++] = ESC_ESC_BYTE;
					break;
				default:
					preparedDataOut[writeIndex++] = dataToSend[readIndex];
				}

			}
		}
		// Finally add Framing byte at end
		preparedDataOut[preparedDataOut.size() - 1] = FRAME_BYTE;
	}

}
