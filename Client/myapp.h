#pragma once
#include <wx/wx.h>


class MyApp : public wxApp
{
  public:
    virtual bool OnInit();
	virtual int OnExit();
private:
	class MySession * session_;
};
