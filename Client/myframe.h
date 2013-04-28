#pragma once

#include <wx/wx.h>
#include "board.h"
#include "packetlistener.h"

class MySession;

class MyFrame : public wxFrame, public PacketListener
{
public:
    MyFrame(const wxString& title, MySession * session);
	~MyFrame();

	void OnPaint(wxPaintEvent & event);
	void OnClick(wxMouseEvent & event);
	void OnWorkerEvent(wxThreadEvent& event);
	void OnLoginMenu(wxCommandEvent& event);

	virtual void OnReceive(const char * buffer, const size_t len);

	

protected:
	void SendSetPiece(const int index) const;
	
private:
	Board board_;
	MySession * session_;
};

