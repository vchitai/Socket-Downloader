#pragma once

#include "afxsock.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <windows.h>

using namespace std;

#define BUFFER_SIZE 65536
#define DEFAULT_LOCATION "1512406_1512474_"
#define FILE_TYPE 0
#define FOLDER_TYPE 1
#define HTTP_VER10 "--http1.0"
#define HTTP_VER11 "--http1.1"
#define VER10 0
#define VER11 1
#define HTTP_HEADER10 " HTTP/1.0\r\n"
#define HTTP_HEADER11 " HTTP/1.1\r\n"
#define DOWNLOAD_FILE_OPTION "\r\n\r\nAccept-Encoding: identity\r\n\r\nConnection: keep-alive\r\n\r\n Keep-Alive: 30000\r\n"
#define DOWNLOAD_FOLDER_INDEX_OPTION "\r\n\r\nUser-Agent: fetch.c\r\n\r\nAccept-Encoding: identity\r\n\r\nConnection: keep-alive\r\n\r\n Keep-Alive: 30000\r\n"
#define CHUNKED 1
#define NORMAL 0
#define WAIT_TIME 10

struct Link {
	string normalizedLink;
	string normalizedName;
	string hostName;
	string targetPath;
	string targetName;
	int type;
	Link(string link);
};

struct AppSettings {
	int currentVersion;
	string httpHeaderVer;
};

struct ResponseHeaderAnalyzer {
	int responseVersion;
	int responseCode;
	bool transferType;
	int contentLength;
	ResponseHeaderAnalyzer(string responseHeader);
};

vector<string> split(string data, string token);
string constructRequest(Link link);
struct sockaddr_in getAddr(Link link);
void establishConnection(CSocket & cs, Link link);
void downloadTo(CSocket &cs, Link link);
void getFileData(CSocket &cs, ofstream &os);
void getFolderData(CSocket &cs, ofstream &os);
vector<string> getCurrentFileList(string targetLink);

void startDownload(CSocket &cs, Link link);
void downloadFile(CSocket &cs, Link link, string subFolder);
void updateProgress(int &recvDat, int &recvLen, int downloadSize);
void transferDataNormal(CSocket &cs, ofstream &os, char* buffer, int start, int recvLen, int contentLength);
void transferDataChunked(CSocket &cs, ofstream &os, char* buffer, int start, int recvLen);