#include "mysession.h"
#include <SDKDDKVer.h>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "packetlistener.h"
//#include "../common/common.h"

using boost::asio::ip::tcp;

struct MySession_Impl
{
	MySession_Impl() 
		: socket(io_service)
		, read_buffer(new char[max_buffer_size], max_buffer_size)
		, worker(NULL)
	{}

	~MySession_Impl()
	{
		if (worker)
		{
			io_service.stop();
			delete worker;
			worker = NULL;
		}
		
		delete [] boost::asio::buffer_cast<char*>(read_buffer);
	}

	boost::asio::io_service io_service;
	tcp::socket socket;
	boost::thread * worker;
	boost::asio::mutable_buffer read_buffer;
	enum { max_buffer_size = 1024 };
};

MySession::MySession() 
	: impl_(new MySession_Impl())
	, listener_(NULL)
{
}


MySession::~MySession()
{
	if (impl_)
	{
		delete impl_;
		impl_ = NULL;
	}
}

void MySession::set_listener(PacketListener * listener)
{
	listener_ = listener;
}

void MySession::connect( const char * ip, const int port )
{
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), port);

	impl_->socket.async_connect
	(
		endpoint,
		[this]( const boost::system::error_code & error )
		{
			if ( false == error )
			{
				std::cout << "connected" << std::endl;
				this->do_read();
			}
			else
			{
				delete this;
			}
		}
	);

	impl_->worker = new boost::thread(boost::bind(&boost::asio::io_service::run, &impl_->io_service));
}

void MySession::write(const short packet_code, const char * buf, const size_t len)
{
	char * write_ptr = new char[len];
	memcpy(write_ptr, buf, len);

	impl_->io_service.post
	(
		[this, packet_code, write_ptr, len]()
		{
			this->do_write(packet_code, write_ptr, len);
		}
	);
}

void MySession::do_read()
{
	impl_->socket.async_read_some
	(
		boost::asio::buffer(impl_->read_buffer),
		[this](const boost::system::error_code & error, std::size_t bytes_transferred)
		{
			if (false == error)
			{
				if (listener_)
				{
					const char * read_ptr = boost::asio::buffer_cast<char*>(impl_->read_buffer);
					listener_->OnReceive( read_ptr, bytes_transferred );
				}
				this->do_read();
			}
			else
			{
				delete this;
			}
		}
	);
}

void MySession::do_write(const short packet_code, const char * body_buf, const size_t body_len)
{
	const int header_size = sizeof(PacketHeader);
	char * header_buf = new char[header_size];
	PacketHeader header;
	header.size = body_len;
	header.code = packet_code;
	memcpy(header_buf, &header, header_size);

	std::vector<boost::asio::const_buffer> buffer_list;
	buffer_list.push_back( boost::asio::const_buffer(header_buf, header_size) );
	buffer_list.push_back( boost::asio::const_buffer(body_buf, body_len) );

	boost::asio::async_write
	(
		impl_->socket,
		buffer_list,
		[this, header_buf, body_buf](const boost::system::error_code & error, std::size_t bytes_transferred)
		{
			if (false == error)
			{
				std::cout << "do_write : " << bytes_transferred << std::endl;
				delete [] header_buf;
				delete [] body_buf;
			}
			else
			{
				delete this;
			}
		}
	);
}