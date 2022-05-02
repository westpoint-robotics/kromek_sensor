#pragma once
#include "types.h"
#include <vector>
#include "stdafx.h"
#include <mutex>
#include <memory>
#include <list>

namespace kmk
{
	// Represents a buffer in which streamed raw data is added and packets can be read out of as chunks of bytes
	class IPacketStreamer
	{
	public:
		virtual bool AddIncomingData(const BYTE* pData, size_t dataSize) = 0;
		virtual bool ReadPacket(std::vector<BYTE> &dataOut)  = 0;
		virtual void Clear() = 0;

		// Convert raw data to send over the comms
		virtual void PrepareForSend(const std::vector<BYTE>& dataToSend, std::vector<BYTE>& preparedDataOut) = 0;

	};

	typedef std::shared_ptr<IPacketStreamer> IPacketStreamerPtr;

	// Stream of raw data comming in a packet format (D3/D3S/D4)
	class SerialPacketStreamer : public IPacketStreamer
	{
	protected:
		std::vector<BYTE> _buffer;
		size_t _writeIndex;
		std::mutex _bufferMutex;

		// Whether we are trying to recover from data corruption on the pipe
		bool _enableDataRecovery;

		// Time of the last packet that was received
		std::chrono::steady_clock::time_point _lastDataTime;

		// Returns true if the data should be accepted and false if it should be thrown away
		bool UpdateDataRecoveryStatus(std::chrono::steady_clock::time_point tpNow);

	public:
		SerialPacketStreamer(size_t maxBufferSize = 204800);

		virtual bool AddIncomingData(const BYTE* pData, size_t dataSize) override;
		virtual bool ReadPacket(std::vector<BYTE> &dataOut) override;
		virtual void Clear() override;

		virtual void PrepareForSend(const std::vector<BYTE>& dataToSend, std::vector<BYTE>& preparedDataOut) override;
	};

	// Stream of framed data that needs to be unescaped on receipt
	class FramedPacketStreamer : public IPacketStreamer
	{
	private:
		const size_t PacketPoolSize = 5;

		std::vector<BYTE> _buffer;
		size_t _writeIndex;
		std::mutex _bufferMutex;
		bool _firstByteEscaped;

		std::mutex _poolMutex;
		std::vector<BYTE> _poolBuffer;
		std::list<BYTE*> _packetsReady;
		std::list<BYTE*> _packetPool;

	public:
		FramedPacketStreamer();
		virtual ~FramedPacketStreamer();

		virtual bool AddIncomingData(const BYTE* pData, size_t dataSize) override;
		virtual bool ReadPacket(std::vector<BYTE>& dataOut) override;
		virtual void Clear() override;
		virtual void PrepareForSend(const std::vector<BYTE>& dataToSend, std::vector<BYTE>& preparedDataOut) override;
	};

}