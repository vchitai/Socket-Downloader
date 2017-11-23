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

Link analyzeLink(string link)
{
	vector<string> temp = split(link, "//");
	string normalizeLink = temp[temp.size() - 1];
	temp = split(normalizeLink, "/");
	string filePath = "";
	for (int i = 1; i < temp.size(); i++)
		filePath += "/" + temp[i];
	string fileName = temp[temp.size() - 1];
	if ((fileName == "") && (temp.size() > 1)) {
		fileName = temp[temp.size() - 2];
	}
	vector<string> temp2 = split(fileName, "%20");
	string reFileName = temp2[0];
	for (int i = 1; i < temp2.size(); i++)
		reFileName +=" "+ temp2[i];
	Link res;
	res.normalizedLink = normalizeLink;
	res.hostName = temp[0];
	res.filePath = filePath;
	res.fileName = reFileName;
	if (link[link.size() - 1] == '/') {
		res.type = FOLDER_TYPE;
		res.folderName = res.fileName;
		res.fileName += ".html";
	}
	else
		res.type = FILE_TYPE;
	return res;
}

string constructRequest(Link link)
{
	string option;
	if (link.type == FILE_TYPE)
		option = DOWNLOAD_FILE_OPTION;
	else
		option = DOWNLOAD_FOLDER_INDEX_OPTION;
	return "GET " + link.filePath + appSet.httpHeaderVer + "Host: " + link.hostName + option;
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

void downloadTo(CSocket & cs, Link link)
{
	string request = constructRequest(link);
	cs.Send(request.c_str(), request.size());

	ofstream os;
	if (link.type == FILE_TYPE) {
		os.open(DEFAULT_LOCATION + link.fileName, std::ofstream::out | std::ofstream::binary);
		getFileData(cs, os);
	}
	else {
		int numtries = 5;
		vector<string> fileList;
		do {
			os.open(DEFAULT_LOCATION + link.fileName, std::ofstream::out | std::ofstream::binary);
			getFolderData(cs, os);
			fileList = getCurrentFileList(DEFAULT_LOCATION + link.fileName);
			if (fileList.size() > 0)
				numtries = 0;
			else {
				cs.Close();
				establishConnection(cs, link);
				cs.Send(request.c_str(), request.size());
				numtries--;
			}
		} while (numtries > 0);
		CreateDirectory(CA2W(string(DEFAULT_LOCATION + link.folderName).c_str()), NULL);

		ofstream os1;
		for (int i = 0; i < fileList.size(); i++) {
			vector<string> tmp = split(fileList[i],"/");
			if (tmp.size() > 0) {
				CreateDirectory(CA2W(string(DEFAULT_LOCATION + link.folderName + "/" + tmp[0]).c_str()), NULL);
				continue;
			}
			Link l = analyzeLink(link.normalizedLink + fileList[i]);
			request = constructRequest(l);
			cs.Close();
			establishConnection(cs, l);
			cs.Send(request.c_str(), request.size());

			os1.open(DEFAULT_LOCATION + link.folderName + "/" + fileList[i], std::ofstream::out | std::ofstream::binary);
			getFileData(cs, os1);
			cout << fileList[i] << " DONE" << endl;
			os1.close();
		}
	}

	os.close();
}

void getFileData(CSocket & cs, ofstream & os)
{
	char*buffer = new char[BUFFER_SIZE];
	std::memset(buffer, 0, BUFFER_SIZE);

	cout << endl;
	int downloadSize;
	int remainder;
	int recvDat;
	int recvLen;
	while (1) {
		Sleep(10);
		recvLen = cs.Receive(buffer, BUFFER_SIZE);
		if (!recvLen)
			break;
		string tempBF(buffer);

		//Split to get Header
		vector<string> temp = split(tempBF, "\r\n\r\n");
		vector<string> tmp = split(temp[0], "\r\n");
		for (int i = 0; i < tmp.size();i++)
			cout << tmp[i] << 'LOL' << endl;
		if (temp.size() < 2) {
			std::memset(buffer, 0, BUFFER_SIZE);
			continue;
		}
		//Get size of file
		vector<string> temp1 = split(temp[0], "Content-Length: ");

		downloadSize = atoi(temp1[1].c_str());
		remainder = recvLen - temp[0].size() - 4;
		recvDat = 0;
		if (remainder > 0) {
			os.write(&buffer[temp[0].size() + 4], remainder);
			recvDat = remainder;
		}
		std::memset(buffer, 0, BUFFER_SIZE);
		Sleep(10);
		while ((recvLen = cs.Receive(buffer, BUFFER_SIZE)) > 0 && recvDat < downloadSize) {
			recvDat += recvLen;
			if (recvDat > downloadSize) {
				recvLen -= (recvDat - downloadSize);
				recvDat = downloadSize;
			}
			cout << '\r' << "Progress: "<< recvDat *1.0 / downloadSize * 100 << "%          ";
			os.write(buffer, recvLen);
			std::memset(buffer, 0, BUFFER_SIZE);
			Sleep(10);
		}
		cout << endl;
		break;
	}
	delete buffer;
}

void getFolderData(CSocket & cs, ofstream & os)
{
	char*buffer = new char[BUFFER_SIZE];
	std::memset(buffer, 0, BUFFER_SIZE);

	int downloadSize;
	int remainder;
	int recvDat;
	int recvLen;
	while (1) {
		Sleep(10);
		recvLen = cs.Receive(buffer, BUFFER_SIZE);
		string tempBF(buffer);

		//Split to get Header
		vector<string> temp = split(tempBF, "\r\n\r\n");
		if (temp.size() < 2) {
			std::memset(buffer, 0, BUFFER_SIZE);
			continue;
		}
		//Get size of file
		remainder = recvLen - temp[0].size() - 4;
		if (remainder > 0)
			os.write(&buffer[temp[0].size() + 4], remainder);
		std::memset(buffer, 0, BUFFER_SIZE);
		Sleep(10);
		while ((recvLen = cs.Receive(buffer, BUFFER_SIZE)) > 0) {
			os.write(buffer, recvLen);
			std::memset(buffer, 0, BUFFER_SIZE);
			Sleep(10);
		}
		break;
	}
	delete buffer;
}

vector<string> getCurrentFileList(string folderLink)
{
	ifstream fi;
	fi.open(folderLink, std::ifstream::in);
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
	return res;
}
