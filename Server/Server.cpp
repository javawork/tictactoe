#include <stdio.h>
#include <tchar.h>
#include <SDKDDKVer.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/scoped_array.hpp>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/text_format.h>

#include "../common/tictactoe.pb.h"
#include "board.h"

using namespace std;
using namespace google;

using boost::asio::ip::tcp;

const int packet_header_size = 4;
const int max_packet_size = 128;

struct PacketHeader
{
	short size;
	short code;
};

//----------------------------------------------------------------------

//----------------------------------------------------------------------

class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void write(const short packet_code, const char * body_buf, const size_t body_len) = 0;
  virtual int get_id() = 0;
};

typedef boost::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
	void join(chat_participant_ptr participant)
	{
		participants_.insert(participant);

		if ( participants_.size() == 2 )
		{
			this->send_start();
			board_.init();
		}
	}

	void leave(chat_participant_ptr participant)
	{
		participants_.erase(participant);
	}

	void send_start()
	{
		using namespace google;

		tictactoe::SStart pkt;
			
		int piece_type = 0;
		std::set<chat_participant_ptr>::iterator iter = participants_.begin();
		std::cout << "Start" << std::endl;
		while(iter != participants_.end() )
		{
			tictactoe::SStart::User * user = pkt.add_list();
			const int id = (*iter)->get_id();
			const int piece = ++piece_type;
			std::cout << id << ", " << piece << std::endl;
			user->set_id(id);
			user->set_piece_type(piece);
			++iter;
		}

		int body_size = pkt.ByteSize();
		boost::scoped_array<char> body_buf(new char[body_size]);

		protobuf::io::ArrayOutputStream os(body_buf.get(), body_size);
		pkt.SerializeToZeroCopyStream(&os);

		std::for_each(
			participants_.begin(), participants_.end(), 
			boost::bind( &chat_participant::write, _1, tictactoe::S_START, body_buf.get(), body_size ) );
	}

	void send_setpiece(const int pos, const int piece)
	{
		tictactoe::SSetPiece send;

		send.set_piecetype(piece);
		send.set_pos(pos);
		int body_size = send.ByteSize();
		boost::scoped_array<char> body_buf(new char[body_size]);

		using namespace google;
		protobuf::io::ArrayOutputStream os(body_buf.get(), body_size);
		send.SerializeToZeroCopyStream(&os);

		std::for_each(
			participants_.begin(), participants_.end(), 
			boost::bind( &chat_participant::write, _1, tictactoe::S_SETPIECE, body_buf.get(), body_size ) );
	}

	void send_end(const int piece)
	{
		tictactoe::SEnd send;

		send.set_winner_piece(piece);
		int body_size = send.ByteSize();
		boost::scoped_array<char> body_buf(new char[body_size]);

		using namespace google;
		protobuf::io::ArrayOutputStream os(body_buf.get(), body_size);
		send.SerializeToZeroCopyStream(&os);

		std::for_each(
			participants_.begin(), participants_.end(), 
			boost::bind( &chat_participant::write, _1, tictactoe::S_END, body_buf.get(), body_size ) );

		std::cout << "End, " << piece << std::endl;
	}

	void on_receive(const boost::asio::const_buffer & buffer)
	{
		const char * read_ptr = boost::asio::buffer_cast<const char*>(buffer);
		std::size_t len = boost::asio::buffer_size(buffer);

		PacketHeader header;
		memcpy(&header, read_ptr, sizeof(PacketHeader));

		if ( header.code == tictactoe::C_SETPIECE )
		{
			tictactoe::CSetPiece recv;

			using namespace google;
			protobuf::io::ArrayInputStream is(&read_ptr[sizeof(PacketHeader)], len-sizeof(PacketHeader));
			recv.ParseFromZeroCopyStream(&is);

			std::cout << "C_SetPiece, " << recv.piecetype() << std::endl;

			board_.change_piece( recv.pos(), recv.piecetype() );

			this->send_setpiece( recv.pos(), recv.piecetype() );

			if ( board_.check_winner( recv.piecetype() ) )
			{
				this->send_end(recv.piecetype());
				board_.init();
			}
		}
	}

private:
	std::set<chat_participant_ptr> participants_;
	Board board_;
};

//----------------------------------------------------------------------

static int g_id_count = 0;

class chat_session
  : public chat_participant,
    public boost::enable_shared_from_this<chat_session>
{
public:
	chat_session(boost::asio::io_service& io_service, chat_room& room)
		: socket_(io_service)
		, room_(room)
		, read_buf_(new char[max_packet_size], max_packet_size)
	{
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		std::cout << "session::start\n";
		char* p1 = boost::asio::buffer_cast<char*>(read_buf_);
		boost::asio::async_read(socket_,
			boost::asio::buffer(p1, packet_header_size),
			boost::bind(
				&chat_session::handle_read_header, shared_from_this(),
				boost::asio::placeholders::error));
	}

	void handle_read_header(const boost::system::error_code& error)
	{
		if (!error)
		{
			char* p1 = boost::asio::buffer_cast<char*>(read_buf_);
			short packet_body_size = (short)(*p1);
			p1 += packet_header_size;
			boost::asio::async_read(socket_,
				boost::asio::buffer(p1, packet_body_size),
				boost::bind(&chat_session::handle_read_body, shared_from_this(),
				boost::asio::placeholders::error));
		}
		else
		{
			room_.leave(shared_from_this());
		}
	}

	void on_receive()
	{
		char* p1 = boost::asio::buffer_cast<char*>(read_buf_);
		PacketHeader header;
		memcpy(&header, p1, sizeof(PacketHeader));

		if ( header.code == tictactoe::C_LOGIN)
		{
			id_ = ++g_id_count;
			tictactoe::SLogin pkt;
			pkt.set_id(id_);
			int body_size = pkt.ByteSize();
			char * body_buf = new char[body_size];

			using namespace google;
			protobuf::io::ArrayOutputStream os(body_buf, body_size);
			pkt.SerializeToZeroCopyStream(&os);

			this->write(tictactoe::S_LOGIN, body_buf, body_size);
			room_.join(shared_from_this());
		}
		else
		{
			room_.on_receive(boost::asio::const_buffer(p1, header.size+packet_header_size));
		}
	}

	void handle_read_body(const boost::system::error_code& error)
	{
		if (!error)
		{
			this->on_receive();
			char* p1 = boost::asio::buffer_cast<char*>(read_buf_);
			boost::asio::async_read(socket_,
				boost::asio::buffer(p1, packet_header_size),
				boost::bind(&chat_session::handle_read_header, shared_from_this(),
				boost::asio::placeholders::error));
		}
		else
		{
			room_.leave(shared_from_this());
		}
	}

	virtual void write(const short packet_code, const char * buf, const size_t len)
	{
		const int header_size = sizeof(PacketHeader);
		char * header_buf = new char[header_size];
		PacketHeader header;
		header.size = len;
		header.code = packet_code;
		memcpy(header_buf, &header, header_size);

		char * body_buf = new char[len];
		memcpy(body_buf, buf, len);

		std::vector<boost::asio::const_buffer> buffer_list;
		buffer_list.push_back( boost::asio::const_buffer(header_buf, header_size) );
		buffer_list.push_back( boost::asio::const_buffer(body_buf, len) );

		boost::asio::async_write
		(
			socket_,
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

	virtual int get_id()
	{
		return id_;
	}

private:
	tcp::socket socket_;
	chat_room& room_;
	boost::asio::mutable_buffer read_buf_;
	int id_;
};

typedef boost::shared_ptr<chat_session> chat_session_ptr;

//----------------------------------------------------------------------

class chat_server
{
public:
	chat_server(boost::asio::io_service& io_service, const tcp::endpoint& endpoint)
		: io_service_(io_service)
		, acceptor_(io_service, endpoint)
	{
		start_accept();
	}

	void start_accept()
	{
		chat_session_ptr new_session(new chat_session(io_service_, room_));
		acceptor_.async_accept(new_session->socket(),
			boost::bind(&chat_server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
	}

	void handle_accept(chat_session_ptr session,const boost::system::error_code& error)
	{
		if (!error)
		{
			session->start();
		}

		start_accept();
	}

private:
	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	chat_room room_;
};

typedef boost::shared_ptr<chat_server> chat_server_ptr;
typedef std::list<chat_server_ptr> chat_server_list;

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
	try
	{
		int port = 25251;
		boost::asio::io_service io_service;

		tcp::endpoint endpoint(tcp::v4(), port);
		chat_server_ptr server(new chat_server(io_service, endpoint));

		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}


