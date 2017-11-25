// 1512406_1512474.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "1512406_1512474.h"
#include "Support.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CWinApp theApp;

extern struct AppSettings appSet;

bool checkFormat(int argc, char* argv[], string &downloadLink)
{
	if (argv[1][0] == '-' && argv[1][0] == '-') {
		if (strcmp(HTTP_VER10, argv[1])) {
			appSet.currentVersion = VER10;
			appSet.httpHeaderVer = HTTP_HEADER10;
		}
		else if (strcmp(HTTP_VER11, argv[1])) {
			appSet.currentVersion = VER11;
			appSet.httpHeaderVer = HTTP_HEADER11;
		}
		else
			return false;
		downloadLink = argv[2];
	}
	else {
		if (strcmp(HTTP_VER10, argv[2])) {
			appSet.currentVersion = VER10;
			appSet.httpHeaderVer = HTTP_HEADER10;
		}
		else if (strcmp(HTTP_VER11, argv[2])) {
			appSet.currentVersion = VER11;
			appSet.httpHeaderVer = HTTP_HEADER11;
		}
		else
			return false;
		downloadLink = argv[1];
	}
	return true;
}

int main(int argc, char* argv[])
{
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// Khoi tao Thu vien
		if (AfxSocketInit() == FALSE)
		{
			cout << "Khong the khoi tao Socket Library";
			return FALSE;
		}

		string downloadLink;

		if (!checkFormat(argc, argv, downloadLink))
			return 1;
		// Tao socket dau tien
		CSocket cs;
		Link analyzedLink(downloadLink);

		establishConnection(cs, analyzedLink);
		startDownload(cs, analyzedLink);
		// Dong ket noi
		cs.Close();
	}

	return nRetCode;
}
