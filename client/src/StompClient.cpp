#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <memory>

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

void getFramesFromServer(ConnectionHandler*, StompProtocol&, volatile bool&);
ConnectionHandler* handleLogin(string&);
vector<string> split(const string&, char);

int main(int argc, char *argv[])
{
	cout << "STOMP Client started.\n" << endl;

	StompProtocol stompProtocol;
	string username;
	ConnectionHandler* connectionHandler = handleLogin(username);
	if (connectionHandler == nullptr) {
		cout << "Exiting...\n" << endl;
		return 0;
	}
	stompProtocol.setUsername(username);

	volatile bool shouldTerminate = false;
	std::thread listener(getFramesFromServer, connectionHandler, std::ref(stompProtocol), std::ref(shouldTerminate));

	while (!shouldTerminate) {
        const short bufsize = 1024;
        char buf[bufsize];
        cin.getline(buf, bufsize);
        string line(buf);

        if (line.empty()) continue;
        stompProtocol.processKeyboardCommand(line, connectionHandler);
    }

	if (listener.joinable()) listener.join();
	delete connectionHandler;
	return 0;
}

void getFramesFromServer(ConnectionHandler* connectionHandler, StompProtocol& stompProtocol, volatile bool& shouldTerminate)
{
	while (!shouldTerminate) {
		string frame;
		if (!connectionHandler->getFrameAscii(frame, '\0')) {
			cout << "Disconnected. Exiting...\n" << endl;
			shouldTerminate = true;
			break;
		}

		if (!stompProtocol.processServerFrame(frame)) {
            shouldTerminate = true;
            break;
        }
	}
}

ConnectionHandler* handleLogin(string& username) 
{
	while (true) 
	{
        const short bufsize = 1024;
        char buf[1024];  
		
		cout << "Enter login command: " << endl;
        cin.getline(buf, bufsize);
        string line(buf);
        if (line.empty()) continue;

        vector<string> args = split(line, ' ');
        if (args.size() > 0 && args[0] == "exit") {
             return nullptr;	// Handle exit command
        }
        if (args.size() < 4 || args[0] != "login") {
            cout << "Error: usage is 'login {host:port} {username} {password}'" << endl;
            continue;
        }

        string hostPort = args[1];
        username = args[2];
        string password = args[3];
        vector<string> hostPortSplit = split(hostPort, ':');
        if (hostPortSplit.size() != 2) {
            cout << "Error: Invalid host:port format" << endl;
            continue;
        }
        string host = hostPortSplit[0];
        short port = (short)stoi(hostPortSplit[1]);

        std::unique_ptr<ConnectionHandler> handler(new ConnectionHandler(host, port));
        if (!handler->connect()) {
            cerr << "Cannot connect to " << host << ":" << port << endl;
            continue;
        }

        string frame = "CONNECT\n"
                       "accept-version:1.2\n"
                       "host:stomp.cs.bgu.ac.il\n"
                       "login:" + username + "\n"
                       "passcode:" + password + "\n"
                       "\n"
                       "\0";
		if (!handler->sendFrameAscii(frame, '\0')) {
				cout << "Disconnected. Exiting...\n" << endl;
				return nullptr;
		}

		string answer;
		if (!handler->getFrameAscii(answer, '\0')) {
			cout << "Disconnected. Exiting...\n" << endl;
			return nullptr;
		}

		cout << "Reply: " << answer << endl;
		if (answer.find("CONNECTED") != string::npos) {
			cout << "Login successful!\n" << endl;
			return handler.release();
		} else {
			cout << "Login failed. Try again.\n" << endl;
		}
	}
}


vector<string> split(const string& str, char delimiter) 
{
    vector<string> tokens;
    string token;
    std::stringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}