#pragma once

struct PacketHeader
{
	short size;
	short code;
};

class PacketListener
{
public:
	virtual void OnReceive(const char * buffer, const size_t len) = 0;
};