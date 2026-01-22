#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <sstream>
#include <fstream>
#include <iostream>
#include <exception>
#include <regex>

using std::cout;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::stringstream;
using std::to_string;

class StompProtocol {
private:
    string username;
    int subIdCounter;
    int receiptIdCounter;
    bool shouldTerminate;
    std::mutex mutex;

    // (gameName, subscriptionID) map
    map<string, int> subscriptions;

    // (receiptID, action description) map
    map<int, string> pendingReceipts;

    // (gameName, (username, gameStats)) map
    struct GameStats {
        map<string, string> generalStats;
        map<string, string> teamAStats;
        map<string, string> teamBStats;
        vector<Event> events;

        GameStats() : generalStats(), teamAStats(), teamBStats(), events() {}
    };
    map<string, map<string, GameStats>> gameUpdates; 

    // Keyboard Command Handlers
    void handleJoin(const string& gameName, ConnectionHandler* handler);
    void handleExit(const string& gameName, ConnectionHandler* handler);
    void handleLogout(ConnectionHandler* handler);
    void handleReport(const string& file, ConnectionHandler* handler);
    void handleSummary(const string& gameName, const string& user, const string& file);

    // Server Frame Handlers
    void handleServerConnected(const vector<string>& lines);
    void handleServerReceipt(const vector<string>& lines);
    void handleServerError(const vector<string>& lines);
    void handleServerMessage(const vector<string>& lines);

    // Helper Methods
    void sendFrame(ConnectionHandler* handler, string body);
    void saveEvent(string gameName, string user, Event& event);
    string buildEventBody(const Event& event, string user, string gameName);
    string trim(const string& str);
    vector<string> split(const string& str, char delimiter);

public:
    StompProtocol();

    void setUsername(string username);
    void processKeyboardCommand(const string& commandLine, ConnectionHandler* handler);
    bool processServerFrame(const string& frame);
};