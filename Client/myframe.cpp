#include "myframe.h"
#include <boost/date_time.hpp>
#include "mysession.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/text_format.h>
#include "../common/tictactoe.pb.h"
#include <boost/scoped_array.hpp>

static wxColour Color_Green = wxColour( 34, 139, 34 );
static wxColour Color_Khaki = wxColour( 240, 230, 140 );
static wxColour Color_Red = wxColour( 255, 0, 0 );
static wxColour Color_Blue = wxColour( 0, 0, 255 );
static wxColour Color_Black = wxColour( 0, 0, 0 );
static wxColour Color_PT_White = wxColour( 250, 250, 250 );
static wxColour Color_PT_Dark = wxColour( 0, 0, 0 );

enum
{
	TIC_THREAD_EVENT = wxID_HIGHEST+1,
	TIC_LOGIN_COMMAND_ID,
	TIC_TIMER_ID,
};

static void ShowMessageBox( const wxString& message )
{
	wxMessageDialog dlg( NULL, message, "tic-tac-toe", wxOK);
	dlg.ShowModal();
	dlg.Destroy();
}

static void draw_square(wxPaintDC & dc, const SquareList & list);
static void draw_piece(wxPaintDC & dc, const SquareList & list);

MyFrame::MyFrame(const wxString& title, MySession * session)
	: wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(380, 400))
	, session_(session)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	board_.init();
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(MyFrame::OnPaint));
	this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(MyFrame::OnClick));
	this->Connect(TIC_THREAD_EVENT, wxEVT_THREAD, wxThreadEventHandler(MyFrame::OnWorkerEvent));
	this->SetDoubleBuffered(true);

	wxMenuBar * menubar = new wxMenuBar; 
	wxMenu * menu = new wxMenu;

	menu->Append(TIC_LOGIN_COMMAND_ID, wxT("Login"));
	menubar->Append(menu, wxT("Menu"));
	this->SetMenuBar(menubar);
	this->Connect(TIC_LOGIN_COMMAND_ID, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(MyFrame::OnLoginMenu));

	this->CreateStatusBar(3);
	this->SetStatusText(wxT("Ready"), 0);
}

MyFrame::~MyFrame()
{
	google::protobuf::ShutdownProtobufLibrary();
}

void MyFrame::OnPaint(wxPaintEvent& event)
{
	wxPaintDC dc(this);
	const SquareList & square_list = board_.get_square_list();
	draw_square(dc, square_list);
	draw_piece(dc, square_list);
	//this->AddPendingEvent();
}

static void draw_square(wxPaintDC & dc, const SquareList & list)
{
	SquareList::const_iterator iter = list.begin();
	for ( ; iter != list.end(); ++iter)
	{
		Square s = (*iter);
		dc.SetBrush( *wxTRANSPARENT_BRUSH );
		dc.DrawRectangle(s.x - size_square, s.y - size_square, size_square, size_square);
	}
}

static void draw_piece(wxPaintDC & dc, const SquareList & list)
{
	SquareList::const_iterator iter = list.begin();
	for ( ; iter != list.end(); ++iter)
	{
		Square s = (*iter);
		if ( s.own == Piece_None)
		{
			continue;
		}
		else if ( s.own == Piece_White)
		{
			dc.SetBrush( Color_PT_White );
			dc.DrawCircle(s.x-(size_square/2), s.y-(size_square/2), 35);
		}
		else if ( s.own == Piece_Dark)
		{
			dc.SetBrush( Color_PT_Dark );
			dc.DrawCircle(s.x-(size_square/2), s.y-(size_square/2), 35);
		}
	}
}

void MyFrame::OnClick(wxMouseEvent & event)
{
	const int x = event.GetPosition().x;
	const int y = event.GetPosition().y;
	int index = board_.get_matched_index( x, y );
	if (invalid_index != index)
	{
		board_.change_piece(index, Piece_Dark);

		this->SendSetPiece(index);

		//this->Refresh();
	}
	event.Skip();
}

void MyFrame::OnWorkerEvent(wxThreadEvent& event)
{
	this->Refresh();
}

void MyFrame::SendSetPiece(const int index) const
{
	tictactoe::CSetPiece pkt;
	pkt.set_pos(index);
	pkt.set_piecetype(board_.get_piece());
	int body_size = pkt.ByteSize();
	boost::scoped_array<char> body_buf(new char[body_size]); 

	using namespace google;
	protobuf::io::ArrayOutputStream os(body_buf.get(), body_size);
	pkt.SerializeToZeroCopyStream(&os);

	session_->write(tictactoe::C_SETPIECE, body_buf.get(), body_size);
}

void MyFrame::OnReceive(const char * buffer, const size_t len)
{
	PacketHeader header;
	memcpy( &header, buffer, sizeof(PacketHeader) );
	if ( header.code == tictactoe::S_LOGIN )
	{
		tictactoe::SLogin pkt;
		using namespace google;
		protobuf::io::ArrayInputStream is(&buffer[sizeof(PacketHeader)], len-sizeof(PacketHeader));
		pkt.ParseFromZeroCopyStream(&is);
		board_.set_id(pkt.id());
		this->SetStatusText(wxT("Login Seccess"), 0);
	}
	else if ( header.code == tictactoe::S_START )
	{
		tictactoe::SStart pkt;
		using namespace google;
		protobuf::io::ArrayInputStream is(&buffer[sizeof(PacketHeader)], len-sizeof(PacketHeader));
		pkt.ParseFromZeroCopyStream(&is);

		for (int i=0; i<pkt.list_size(); ++i)
		{
			tictactoe::SStart::User user = pkt.list(i);
			if ( user.id() != board_.get_id() )
				continue;

			char buf[8] = {0,};
			_itoa(board_.get_id(), buf, 10);
			this->SetStatusText(buf, 1);

			board_.set_piece( user.piece_type() );
			if ( board_.get_piece() == Piece_White )
			{
				this->SetStatusText(wxT("White"), 2);
			}
			else
			{
				this->SetStatusText(wxT("Black"), 2);
			}
		}

		this->SetStatusText(wxT("Game Start"), 0);
	}
	else if ( header.code == tictactoe::S_SETPIECE )
	{
		tictactoe::SSetPiece pkt;
		using namespace google;
		protobuf::io::ArrayInputStream is(&buffer[sizeof(PacketHeader)], len-sizeof(PacketHeader));
		pkt.ParseFromZeroCopyStream(&is);
		board_.change_piece(pkt.pos(), pkt.piecetype());

		wxThreadEvent thread_event(wxEVT_THREAD, TIC_THREAD_EVENT);
		wxQueueEvent( this, thread_event.Clone() );
	}
	else if ( header.code == tictactoe::S_END )
	{
		tictactoe::SEnd pkt;
		using namespace google;
		protobuf::io::ArrayInputStream is(&buffer[sizeof(PacketHeader)], len-sizeof(PacketHeader));
		pkt.ParseFromZeroCopyStream(&is);

		if (pkt.winner_piece() == board_.get_piece())
		{
			ShowMessageBox("You Won!");
		}
		else
		{
			ShowMessageBox("You Lost!");
		}
	}
}

void MyFrame::OnLoginMenu(wxCommandEvent& event)
{
	using namespace google;
	tictactoe::CLogin pkt;
	const int body_size = pkt.ByteSize();
	boost::scoped_array<char> body_buf(new char[body_size]);

	// 버퍼에 직렬화
	protobuf::io::ArrayOutputStream os(body_buf.get(), body_size);
	pkt.SerializeToZeroCopyStream(&os);
	session_->write(tictactoe::C_LOGIN, body_buf.get(), body_size);
}