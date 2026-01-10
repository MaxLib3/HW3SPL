#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

using std::string;
using std::vector;
using std::map;

class StompProtocol {
private:
    string username;
    int subscriptionIdCounter;
    int receiptIdCounter;
    bool shouldTerminate;
    std::mutex mutex;

    // Map: GameName -> SubscriptionID (To know what to Unsubscribe)
    map<string, int> activeSubscriptions;
    // Map: ReceiptID -> Action (To handle receipt responses like "logout")
    map<int, string> receiptActions;

    // Complex State for Summary:
    // GameName -> (UserName -> { GeneralStats, TeamAStats, TeamBStats, EventList })
    struct UserGameStats {
        map<string, string> generalStats;
        map<string, string> teamAStats;
        map<string, string> teamBStats;
        vector<Event> events;
    }; 
    map<string, map<string, UserGameStats>> gameUpdates; 

public:
    StompProtocol();

    // Set the username upon login
    void setUsername(string username);

    // Main Thread Operations
    // Processes keyboard commands and sends frames via handler
    void processKeyboardCommand(const string& commandLine, ConnectionHandler* handler);

    // Listener Thread Operations
    // Processes incoming frames from the server
    bool processServerFrame(const string& frame);

    // Helper Methods
    void sendFrame(ConnectionHandler* handler, string body);
    void updateGameState(string gameName, string user, Event& event);
    string formatEventMessage(const Event& event, string user, string gameName);
};
