#pragma once

class PacketListener;

class MySession
{
public:
	MySession();
	~MySession();

	void connect( const char * ip, const int port );
	void write(const short packet_code, const char * buf, const size_t len);
	void set_listener(PacketListener * listener);

private:
	void do_read();
	void do_write( const short packet_code, const char * buf, const size_t len );

private:
	struct MySession_Impl * impl_;
	PacketListener * listener_;
};

