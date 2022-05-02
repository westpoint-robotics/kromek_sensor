#include "Heatshrink.hpp"

//#pragma warning(disable : 4200)
extern "C"
{
#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"
}
#include <cstring>


	Heatshrink::Heatshrink(int window, int lookahead) : m_window(window), m_lookahead(lookahead)
	{
	}

	Heatshrink::~Heatshrink()
	{
		
	}

	bool Heatshrink::Expand(const uint8_t *input, uint32_t inputSize, uint8_t *expanded, uint32_t expandedSize, uint32_t *bytesOut)
	{
		bool status = true;
		size_t remain = inputSize, sunk, polled;

		// Size the poll buffer to match the compression window
		size_t outputSize = 1ul << m_window;
		uint8_t *output = new uint8_t[outputSize];
		*bytesOut = 0;

		// Just use the window size as the decoder size
		heatshrink_decoder* decoder = heatshrink_decoder_alloc((uint16_t)outputSize, m_window, m_lookahead);

		// While input data remains and all is ok
		while (remain > 0 && status)
		{
			// Sink as much input as possible, calc how much is sinked and how much we have left
			status = IS_OK(heatshrink_decoder_sink(decoder, const_cast<uint8_t*>(&input[inputSize - remain]), remain, &sunk));
			remain -= sunk;

			// If we know we're done call finish, it should return more as we've not polled yet
			if (remain == 0 && status)
				status = HSDR_FINISH_MORE == heatshrink_decoder_finish(decoder);

			// While there is more data to poll, add it to the expanded buffer
			HSD_poll_res result = HSDR_POLL_MORE;
			while (result == HSDR_POLL_MORE && status)
			{
				status = IS_OK(result = heatshrink_decoder_poll(decoder, &output[0], outputSize, &polled));
				if (status)
				{
					if (*bytesOut + polled > expandedSize)
					{
						// Error not enough buffer!
						int copyBytes = expandedSize - *bytesOut;
						std::memcpy(&expanded[*bytesOut], output, copyBytes);
						status = false;
					}
					else
					{
						std::memcpy(&expanded[*bytesOut], output, polled);
						*bytesOut += polled;
					}
				}
			}

			// We are done and have polled as much as possible, this should return done
			if (remain == 0 && status)
				status = HSDR_FINISH_DONE == heatshrink_decoder_finish(decoder);
		}

		heatshrink_decoder_free(decoder);
		delete[] output;
		return status;
	}
