#ifndef SERVEUR_H_
#define SERVEUR_H_

#include <string>
#include <vector>
#include <winsock2.h>
#include <algorithm>
#include <strstream>
#include <iostream>
#include <mutex>
#include <map>
#pragma comment( lib, "ws2_32.lib" )

const int NMAX = 200;
const int USERMAX = 30;
const int MAXMESSAGE = NMAX + USERMAX;
const int NBMESSAGESMAX = 15;

using namespace std;

class Serveur;

typedef struct {
	SOCKET* sock;
	string name;
	string ipClient;
	string portClient;
}Client;

typedef struct {
	SOCKET* sock;
	Serveur* serv;
	string ipClient;
	string portClient;
}ThreadParam;

class Serveur {
public:
	Serveur();
	void enterInformation();
	void waitConnexion();
	vector<string> identification(SOCKET sock);
	int isLoginOk(vector<string> userInfos);
	int connexion(string user, string pass);
	string receiveMessage(string user);
	int sendMessages(string message);
	int sendMessage(string user, string message);
	int getPort();
	string getHost();
	void validIP();
	void validPort();
	virtual ~Serveur();
	int createUser(vector<string> userInfos);
	void addUser(ThreadParam p, string userName);
	void deconnexion(string user);
	string addEntete(string user);
	int archiverMessage(string message);
	void init();
	void sendHistoric(string userName);

private:
	vector<string> messages;
	string host;
	int port;
	SOCKET rsock;
	map<string, Client> users;
	SOCKADDR_IN rsin;
	mutex verrouVectorUsers;
	mutex verrouVectorMessages;
	mutex verrouFichierUsers;
	mutex verrouFichierMessages;
};

#endif /* SERVEUR_H_ */

