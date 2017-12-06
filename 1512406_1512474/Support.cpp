#include "stdafx.h"
#include "Support.h"

struct AppSettings appSet;

vector<string> split(string data, string token)
{
	vector<string> output;
	size_t pos = string::npos; // size_t to avoid improbable overflow
	do
	{
		pos = data.find(token);
		output.push_back(data.substr(0, pos));
		if (string::npos != pos)
			data = data.substr(pos + token.size());
	} while (string::npos != pos);
	if (output[output.size() - 1] == "")
		output.pop_back();
	return output;
}

string constructRequest(Link link)
{
	string option;
	if (link.type == FILE_TYPE)
		option = DOWNLOAD_FILE_OPTION;
	else {
		option = DOWNLOAD_FOLDER_INDEX_OPTION;
		link.targetName += '/';
	}
	return "GET " + link.targetPath + link.targetName + appSet.httpHeaderVer + "Host: " + link.hostName + option;
}

struct sockaddr_in getAddr(Link link)
{
	struct hostent* ips = gethostbyname(link.hostName.c_str());
	struct sockaddr_in  addr;
	memcpy(&addr.sin_addr, ips->h_addr_list[0], ips->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	std::memset(addr.sin_zero, '\0', sizeof addr.sin_zero);

	return addr;
}

void establishConnection(CSocket & cs, Link link)
{
	struct sockaddr_in addr = getAddr(link);
	cs.Create();
	cs.Connect((struct sockaddr *)&addr, sizeof addr);
}

vector<string> getCurrentFileList(string targetLink)
{
	ifstream fi;
	fi.open(targetLink, std::ifstream::in);

	vector<string> res;
	string line;
	while (!fi.eof()) {
		getline(fi, line);
		vector<string> tmp1 = split(line, "href=\"");
		if (tmp1.size() < 2)
			continue;
		for (int i = 1; i < tmp1.size(); i += 2) {
			string fn = split(tmp1[i], "\"")[0];
			if (fn[0] != '?')
				res.push_back(fn);
		}
	}

	fi.close();
	return res;
}

void startDownload(CSocket & cs, Link link)
{
	if (link.type == FILE_TYPE) {
		cout << "Downloading File: " << link.normalizedName << endl;
		downloadFile(cs, link, "");
	}
	else {
		cout << "Downloading Folder: " << link.normalizedLink << endl;
		string subFolder = string(DEFAULT_LOCATION) + link.targetName + "/";
		wstring subFolderL(subFolder.begin(),subFolder.end());
		if (!CreateDirectory(subFolderL.c_str(), NULL)) {
			int retCode = GetLastError();
			if (retCode == 183) {
				remove(subFolder.c_str());
				if (!CreateDirectory(subFolderL.c_str(), NULL)) {
					cout << "Folder already exist" << endl;
				}
			}
			else {
				cout << "Error when create Folder at code: " << retCode << endl;
				return;
			}
		}
		downloadFile(cs, link, "");
		cout << "Getting File & Folder List Complete. " << endl;
		vector<string> fileList = getCurrentFileList("1512406_1512474_" + link.normalizedName);
		cout << "Found " << fileList.size() << " files and folders" << endl;
		for (int i = 0; i < fileList.size(); i++) {
			Link fileLink(link.normalizedLink + fileList[i]);
			if (fileLink.type == FOLDER_TYPE) {
				string subFolderO = subFolder + fileLink.targetName + "/";
				wstring subFolderLO(subFolderO.begin(), subFolderO.end());
				if (!CreateDirectory(subFolderLO.c_str(), NULL)) {
					int retCode = GetLastError();
					if (retCode == 183)
						cout << "Folder already exist " << endl;
					else
						cout << "Can't Create Folder at Error Code: " << GetLastError() << endl;
					continue;
				} 
				else 
					cout << "Created Folder: " << fileLink.targetName << endl;
			}
			else {
				cout << "Downloading File: " << fileLink.normalizedName << endl;
				cs.Close();
				establishConnection(cs, fileLink);
				downloadFile(cs, fileLink, subFolder);
			}
		}
	}
}

void downloadFile(CSocket & cs, Link link, string subFolder)
{
	string request = constructRequest(link);
	cs.Send(request.c_str(), request.size());

	ofstream os;
	if (subFolder == "")
		os.open(DEFAULT_LOCATION + link.normalizedName, std::ofstream::out | std::ofstream::binary);
	else
		os.open(subFolder + "/" + DEFAULT_LOCATION + link.normalizedName, std::ofstream::out | std::ofstream::binary);
	char*buffer = new char[BUFFER_SIZE];
	std::memset(buffer, 0, BUFFER_SIZE);

	Sleep(WAIT_TIME);
	int recvLen = cs.Receive(buffer, BUFFER_SIZE);
	//Split to get Header
	vector<string> tmp = split(string(buffer), "\r\n\r\n");

	//Get size of file
	ResponseHeaderAnalyzer rha(tmp[0]);
	if (rha.responseCode != 200)
		return;

	int headerSize = tmp[0].size();
	recvLen = recvLen - headerSize - 4;
	if (rha.transferType == CHUNKED)
		transferDataChunked(cs, os, buffer, headerSize + 4, recvLen);
	else
		transferDataNormal(cs, os, buffer, headerSize + 4, recvLen, rha.contentLength);

	delete buffer;
	os.close();
}

void transferDataNormal(CSocket &cs, ofstream &os, char* buffer, int start, int recvLen, int contentLength)
{
	int recvDat = 0;
	if (recvLen > 0) {
		recvDat = recvLen;
		os.write(buffer + start, recvLen);
		std::memset(buffer, 0, BUFFER_SIZE);
	}
	Sleep(WAIT_TIME);
	while ((contentLength == -1 || recvDat < contentLength) && (recvLen = cs.Receive(buffer, BUFFER_SIZE)) > 0) {
		os.write(buffer, recvLen);
		if (contentLength != -1)
			updateProgress(recvDat, recvLen, contentLength);
		else {
			recvDat += recvLen;
			cout << "\rReceived: " << recvDat << "(bytes)";
		}
		std::memset(buffer, 0, BUFFER_SIZE);
		Sleep(WAIT_TIME);
	}
	cout << endl;
}

void updateProgress(int &recvDat, int &recvLen, int downloadSize)
{
	recvDat += recvLen;
	if (recvDat > downloadSize) {
		recvLen -= (recvDat - downloadSize);
		recvDat = downloadSize;
	}
	cout << '\r' << "Progress: " << recvDat *1.0 / downloadSize * 100 << "%          ";
}

void transferDataChunked(CSocket &cs, ofstream &os, char* buffer, int start, int recvLen)
{
	bool finished = false;
	int recvDat = 0;
	int remain = 0;
	int loop = 0;
	do {
		int startChunkSize = start;
		if (loop > 0 && remain > 0) {
			start = remain;
			recvLen -= start;
			os.write(buffer, start);
			startChunkSize = start + 2;
		}
		while (startChunkSize < recvLen + start) {
			int chunkSize = 0;
			int endChunkSize = startChunkSize;
			string sizeHex = "";
			while (buffer[endChunkSize] != '\r') {
				sizeHex += buffer[endChunkSize];
				endChunkSize++;
			}
			if (sizeHex == "0") {
				finished = true;
				break;
			}
			chunkSize = stoi(sizeHex, 0, 16);
			int cpy = recvLen - (endChunkSize + 2 - start);
			bool goNext = false;
			if (chunkSize < cpy) {
				cpy = chunkSize;
				startChunkSize = chunkSize + 4 + endChunkSize;
			}
			else {
				chunkSize -= cpy;
				startChunkSize = chunkSize + 2;
				goNext = true;
				remain = chunkSize;
			}
			os.write(buffer + endChunkSize + 2, cpy);
			recvDat += cpy;
			cout << "\rReceived: " << recvDat << "(bytes)";
			if (goNext)
				break;
		}
		start = 0;
		Sleep(WAIT_TIME);
		loop++;
		std::memset(buffer, 0, BUFFER_SIZE);
	} while (!finished && (recvLen = cs.Receive(buffer, BUFFER_SIZE)) > 0);
	cout << endl;
}

Link::Link(string link) 
{
	vector<string> atoms = split(link, "//");
	normalizedLink = atoms[atoms.size() - 1];

	atoms = split(normalizedLink, "/");
	int numComponents = atoms.size();
	hostName = atoms[0];
	targetPath = "/";
	for (int i = 1; i < atoms.size()-1; i++)
		targetPath += atoms[i] + "/";

	targetName = atoms[atoms.size() - 1];
	atoms = split(targetName, "%20");
	normalizedName = atoms[0];
	for (int i = 1; i < atoms.size(); i++)
		normalizedName += " " + atoms[i];
	size_t dot = targetName.find('.');
	if (numComponents == 1 || link[link.size() - 1] == '/' || dot == std::string::npos) {
		if (link[link.size() - 1] != '/') {
			targetPath += '/';
			normalizedLink += '/';
		}
		type = FOLDER_TYPE;
		normalizedName += ".html";
	}
	else
		type = FILE_TYPE;
}

ResponseHeaderAnalyzer::ResponseHeaderAnalyzer(string responseHeader)
{
	vector<string> field = split(responseHeader, "\r\n");
	responseVersion = field[0][7] - '0';
	responseCode = atoi(field[0].substr(9, 3).c_str());
	contentLength = -1;
	transferType = NORMAL;
	for (int i = 1; i < field.size(); i++) {
		vector<string> tmp = split(field[i], ": ");
		if (tmp[0] == "Content-Length") {
			contentLength = atoi(tmp[1].c_str());
		}
		else if (tmp[0] == "Transfer-Encoding") {
			if (tmp[1].substr(0,7) == "chunked")
				transferType = CHUNKED;
		}
	}
	transferType == (contentLength == -1) || (transferType == NORMAL);
}