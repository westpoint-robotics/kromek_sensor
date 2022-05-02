#ifndef DSC_STORAGE_HEATSHRINK_H_
#define DSC_STORAGE_HEATSHRINK_H_

#pragma once
#include <stdint.h>

class Heatshrink
{
public:
	Heatshrink(int window = 10, int lookahead = 5);
	virtual ~Heatshrink();

	// Simple decompression
	// expandedSize buffer should hold twice the input or 1 window (1 << m_window) which-ever is larger
	bool Expand(const uint8_t *input, uint32_t inputSize, uint8_t *expanded, uint32_t expandedSize, uint32_t *bytesOut);

protected:
	bool IS_OK(int hsr) { return hsr >= 0; }

	int m_window;
	int m_lookahead;
};


#endif // DSC_STORAGE_HEATSHRINK_H_
