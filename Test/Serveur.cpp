

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "Serveur.h"
#include <iostream>
#include <vector>
#include <boost/regex.hpp>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <ctime>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

using namespace std;


DWORD WINAPI EchoHandler(void* param)
{
	ThreadParam* p = (ThreadParam*) param;
	SOCKET* sd = p->sock;
	Serveur* serv = p->serv;

	vector<string> userInfos = serv->identification(*sd);

	string envoi = "ok";
	bool continuer = true;
	if (userInfos.empty()) {
		envoi = "connexion refusee";
		continuer = false;
	}
	if(continuer) {
		int res = serv->isLoginOk(userInfos);
		if (res == 2) {
			envoi = "connexion refusee";
			continuer = false;
		}
		else if (res == 1) {
			if (serv->createUser(userInfos)) {
				envoi = "connexion refusee";
				continuer = false;
			}
		}
	}

	int iResult = send(*sd, envoi.c_str(), envoi.size()+1, 0);
	if (iResult == SOCKET_ERROR) {
		printf("Erreur du send: %d\n", WSAGetLastError());
		closesocket(*sd);
		return 0;
	}
	
	// Cas où y a eu une erreur
	if (!continuer) {
		closesocket(*sd);
		return 0;
	}
	serv->addUser(*p, userInfos[0]); // Ajout du client à la structure de données
	serv->sendHistoric(userInfos[0]);

	while (true) {
		string messageReceived = serv->receiveMessage(userInfos[0]);
		if (messageReceived != "") {
			//TODO enregistrements
			// Envoi du message
			string messageToSend = serv->addEntete(userInfos[0]) + messageReceived;
			serv->archiverMessage(messageToSend);
			serv->sendMessages(messageToSend);
		}
		else {
			break;
		}
	}
	//suppression du client
	serv->deconnexion(userInfos[0]);
	return 0;
}

void Serveur::deconnexion(string user) {
	map<string, Client>::iterator it;
	this->verrouVectorUsers.lock();
	it = this->users.find(user);
	closesocket(*(it->second.sock));
	delete it->second.sock;
	this->users.erase(it);
	this->verrouVectorUsers.unlock();
}


void Serveur::sendHistoric(string userName) {

	this->verrouVectorMessages.lock();
	vector<string> messagesToSend = this->messages;
	this->verrouVectorMessages.unlock();
	if (messagesToSend.size() > NBMESSAGESMAX) {
		messagesToSend.erase(messagesToSend.begin(), messagesToSend.begin() + (messagesToSend.size() - NBMESSAGESMAX));
	}
	vector<string>::iterator it;
	string size = std::to_string(messagesToSend.size());
	string ok = "nok";
	int res = 0;
	while (ok != "ok") {
		res = this->sendMessage(userName, size);
		if (res == 2) {
			this->deconnexion(userName);
			return;
		}
		ok = this->receiveMessage(userName);
	}
	it = messagesToSend.begin();
	while( it<messagesToSend.end()) {
		res = this->sendMessage(userName, *it);
		if (res == 2) {
			this->deconnexion(userName);
			return;
		}
		ok = this->receiveMessage(userName);
		if (ok == "ok") {
			it++;
		}
	}
}


int Serveur::archiverMessage(string message) {
	this->verrouFichierMessages.lock();
	ofstream fichier("../Serveur/bd/messages.txt", ios::out | ios::app);

	if (!fichier)
	{
		this->verrouFichierMessages.unlock();
		return 1;
		
	}
	fichier << message << endl;
	fichier.close();

	this->messages.push_back(message);
	if (this->messages.size() > NBMESSAGESMAX) {
		this->messages.erase(this->messages.begin(), this->messages.begin() + 1);
	}
	this->verrouFichierMessages.unlock();
	return 0;
}

void Serveur::init() {
	ofstream fichierUser("../Serveur/bd/users.txt", ios::out | ios::app);
	fichierUser.close();
	ofstream fichierToCreate("../Serveur/bd/messages.txt", ios::out | ios::app);
	fichierToCreate.close();

	ifstream fichier("../Serveur/bd/messages.txt", ios::in);  // on ouvre le fichier en lecture
	if (!fichier) {		
		return;
	}
	string ligne;
	while (getline(fichier, ligne)) {
		this->messages.push_back(ligne);
	}

	if (this->messages.size() > NBMESSAGESMAX) {
		this->messages.erase(this->messages.begin(), this->messages.begin() + (this->messages.size() - NBMESSAGESMAX));
	}

	fichier.close();
}

string Serveur::addEntete(string user) {
	this->verrouVectorUsers.lock();
	Client client = this->users[user];
	this->verrouVectorUsers.unlock();
	time_t t = time(0);   // get time now
	struct tm * now = localtime(&t);
	string res = "[" + client.name + " - " + client.ipClient + ":" + client.portClient + " - " + to_string(now->tm_year + 1900) + "-" 
				+ to_string(now->tm_mon + 1) + "-" + to_string(now->tm_mday) + "@" + to_string(now->tm_hour) + ":" 
				+ to_string(now->tm_min) + ":" + to_string(now->tm_sec) + "]: ";
	return res;	
}

int Serveur::createUser(vector<string> userInfos) {
	this->verrouFichierUsers.lock();
	ofstream fichier("../Serveur/bd/users.txt", ios::out | ios::app); 

	if (fichier)
	{
		fichier << userInfos.at(0) << '$' << userInfos.at(1) << endl;
		fichier.close();
		this->verrouFichierUsers.unlock();
		return 0;
	}
	cerr << "Impossible d'ouvrir le fichier !" << endl;
	this->verrouFichierUsers.unlock();
	return 1;
	
}

void Serveur::addUser(ThreadParam p, string userName) {
	Client* client = new Client();
	client->sock = p.sock;
	client->name = userName;
	client->ipClient = p.ipClient;
	client->portClient = p.portClient;

	this->verrouVectorUsers.lock();
	this->users[userName] = *(client);
	this->verrouVectorUsers.unlock();
}


Serveur::Serveur() {
	this->host = "";
	this->port = 5000;
	this->messages = vector<string>();
}

Serveur::~Serveur() {
	// TODO Auto-generated destructor stub
}

void Serveur::enterInformation() {
	this->validIP();
	this->validPort();
}

void Serveur::validIP() {
	bool continu = true;
	boost::regex ipTest("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
	while (continu) {
		cout << "Veuillez entrer l'adresse IP du serveur:" << endl;
		string ip;
		cin >> ip;
		if (boost::regex_match(ip, ipTest)) {
			continu = false;
			this->host = ip;
		}
		else {
			cout << "L'adresse doit etre valide!" << endl;
		}
	}
}

void Serveur::validPort() {
	bool continu = true;
	while (continu) {
		cout << "Veuillez entrer le port du serveur (entre 5000 et 5050):" << endl;
		int portGiven;
		cin >> portGiven;
		if (5000 <= portGiven  && portGiven <= 5050) {
			continu = false;
			this->port = portGiven;
		}
		else {
			cout << "Le port doit etre valide ! (Entre 5000 et 5050)" << endl;
		}
	}
}

void Serveur::waitConnexion() {
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		cerr << "Error at WSAStartup()\n" << endl;
		return;
	}
	this->rsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->rsock == INVALID_SOCKET) {
		cerr << "Error at socket()" << endl;
		WSACleanup();
		return;
	}
	char option[] = "1";
	setsockopt(this->rsock, SOL_SOCKET, SO_REUSEADDR, option, sizeof(option));
	hostent *thisHost;

	thisHost = gethostbyname(this->host.c_str());
	char* ip;
	ip = inet_ntoa(*(struct in_addr*) *thisHost->h_addr_list);
	this->rsin.sin_addr.s_addr = inet_addr(ip);
	this->rsin.sin_family = AF_INET;
	this->rsin.sin_port = htons(this->port);
	if (::bind(this->rsock, (SOCKADDR *) &(this->rsin), sizeof(this->rsin)) == SOCKET_ERROR) {
		cerr << "bind() failed." << endl;
		closesocket(this->rsock);
		WSACleanup();
		return ;
	}
	if (listen(this->rsock, 30) == SOCKET_ERROR) {
		cerr << "Error listening on socket." << endl;
		closesocket(this->rsock);
		WSACleanup();
		return ;
	}
	cout << "Serveur initialise et pret" << endl;
	while (true) {
		sockaddr_in sinRemote;
		int nAddrSize = sizeof(sinRemote);
		// Create a SOCKET for accepting incoming requests.
		// Accept the connection.
		SOCKET s = accept(this->rsock, (sockaddr*)&sinRemote, &nAddrSize);
		SOCKET* sd = new SOCKET(s);
		if (*sd != INVALID_SOCKET) {
			cout << "Connection acceptee De : " <<
				inet_ntoa(sinRemote.sin_addr) << ":" <<
				ntohs(sinRemote.sin_port) << "." <<
				endl;

			DWORD nThreadID;
			ThreadParam* param = new ThreadParam();
			param->sock = sd;
			param->serv = this;
			param->ipClient = inet_ntoa(sinRemote.sin_addr);
			param->portClient = to_string(ntohs(sinRemote.sin_port));
			CreateThread(0, 0, EchoHandler, (void*) param, 0, &nThreadID);
		}
		else {
			cerr << "Echec d'une connection." << endl;
			closesocket(this->rsock);
			// return 1;
		}
	}
}

//renvoie un vector contenant [utilisateur,password], il est vide si il y a une erreur
vector<string> Serveur::identification(SOCKET sock) {
	char motRecu[USERMAX+1];
	vector<string> user;
	int iResult = recv(sock, motRecu, USERMAX+1, 0);
	if (iResult > 0) {
		user.push_back(string(motRecu));
	}
	else {
		printf("Erreur de reception : %d\n", WSAGetLastError());
		user.clear();
		return user;
	}
	iResult = recv(sock, motRecu, USERMAX+1, 0);
	if (iResult > 0) {
		user.push_back(string(motRecu));
	}
	else {
		printf("Erreur de reception : %d\n", WSAGetLastError());
		user.clear();
		return user;
	}
	return user;
}

string Serveur::receiveMessage(string user) {
	char motRecu[MAXMESSAGE+1];
	string resultat = "";
	int iResult = recv(*(users[user].sock), motRecu, MAXMESSAGE+1, 0);
	if (iResult > 0) {
		resultat = string(motRecu);
	}
	else {
		printf("Un client s'est deconnecte\n");
	}
	return resultat;
}

int Serveur::sendMessage(string user, string message) {

	if (message.size() > MAXMESSAGE) {
		cout << "Erreur : Le mot saisie depasse les 200 caracteres" << endl;
		return 1;
	}
	int res = 0;
	this->verrouVectorUsers.lock();
	string envoi = message;
	int iResult = send(*(users[user].sock), envoi.c_str(), envoi.size()+1, 0);

	if (iResult == SOCKET_ERROR) {
		printf("Erreur du send: %d\n", WSAGetLastError());
		res = 2;
	}
	this->verrouVectorUsers.unlock();
	return res;
}

int Serveur::sendMessages(string message) {
	//TODO
	this->verrouVectorUsers.lock();
	for (std::map<string, Client>::iterator it = this->users.begin(); it != this->users.end(); ++it) {
		string user = it->first;
		this->verrouVectorUsers.unlock();
		this->sendMessage(user, message);
		this->verrouVectorUsers.lock();
	}
	this->verrouVectorUsers.unlock();
	return 0;

}

string Serveur::getHost() {
	return this->host;
}

int Serveur::getPort() {
	return this->port;
}

int Serveur::isLoginOk(vector<string> userInfos) {
	string userName = userInfos.at(0);
	string password = userInfos.at(1);

	// On interdit le caractère $
	std::size_t foundCaracUser = userName.find("$");
	std::size_t foundCaracPass = password.find("$");
	if (foundCaracUser != std::string::npos || foundCaracPass != std::string::npos) {
		return 2;
	}

	this->verrouFichierUsers.lock();
	ifstream fichier("../Serveur/bd/users.txt", ios::in);  // on ouvre le fichier en lecture

	if (!fichier)
	{
		ofstream fichierToCreate("../Serveur/bd/users.txt" );
		fichierToCreate.close();
		this->verrouFichierUsers.unlock();
		return 2;
	}

	string ligne;
	bool continuer = true;
	int res = 1;
	while (getline(fichier, ligne))
	{
		vector<string> userInfos;
		boost::split(userInfos, ligne, boost::is_any_of("$"));

		if (userInfos.size() != 2) {
			res = 2;
			break;
		}
		else if (userName == userInfos.at(0) && password == userInfos.at(1)) {
			res = 0;
			break;
		}
		else if (userName == userInfos.at(0) && password != userInfos.at(1)) {
			res = 2;
			break;
		}
	}
	fichier.close();
	this->verrouFichierUsers.unlock();
	return res;
	
}


int main(void) {
	Serveur serv;
	serv.enterInformation();
	serv.init();
	serv.waitConnexion();
	return 0;
}


