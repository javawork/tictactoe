#include "myapp.h"
#include "myframe.h"
#include "mysession.h"

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
	session_ = new MySession();
	session_->connect("127.0.0.1", 25251);
    MyFrame *myframe = new MyFrame(wxT("tic-tac-toe"), session_);
	session_->set_listener(myframe);
	myframe->Show(true);

    return true;
}

int MyApp::OnExit()
{
	if ( session_ )
	{
		delete session_;
		session_ = NULL;
	}
	return 0;
}