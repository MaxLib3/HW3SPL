#include "StompProtocol.h"

StompProtocol::StompProtocol() : 
    subIdCounter(0), receiptIdCounter(0), shouldTerminate(false) {}

void StompProtocol::setUsername(string username) {
    this->username = username;
}

void StompProtocol::sendFrame(ConnectionHandler* handler, string frame) {
    if (!handler->sendFrameAscii(frame, '\0')) {
        cout << "Error: Connection lost while sending frame" << endl;
        shouldTerminate = true;
    }
}

// Keyboard Command Processing
void StompProtocol::processKeyboardCommand(const string& commandLine, ConnectionHandler* handler) {
    stringstream ss(commandLine);
    string command;
    ss >> command;

    if (command == "join") {
        string gameName;
        ss >> gameName;
        handleJoin(gameName, handler);
    } 
    else if (command == "exit") {
        string gameName;
        ss >> gameName;
        handleExit(gameName, handler);
    }
    else if (command == "logout") {
        handleLogout(handler);
    }
    else if (command == "report") {
        string file;
        ss >> file;
        handleReport(file, handler);
    }
    else if (command == "summary") {
        string gameName, user, file;
        ss >> gameName >> user >> file;
        handleSummary(gameName, user, file);
    }
    else {
        cout << "Invalid command!" << endl;
    }
}

void StompProtocol::handleJoin(const string& gameName, ConnectionHandler* handler) {
    std::lock_guard<std::mutex> lock(mutex);
    int subId = subIdCounter++;
    int receiptId = receiptIdCounter++;
    
    subscriptions[gameName] = subId;
    pendingReceipts[receiptId] = "Joined channel " + gameName;

    string frame = "SUBSCRIBE\n"
                   "destination:/" + gameName + "\n"
                   "id:" + subId + "\n"
                   "receipt:" + receiptId + "\n"
                   "\n"
                   "\0";
    sendFrame(handler, frame);
}

void StompProtocol::handleExit(const string& gameName, ConnectionHandler* handler) {
    std::lock_guard<std::mutex> lock(mutex); 
    if (subscriptions.find(gameName) == subscriptions.end()) {
        cout << "Error: Not subscribed to " << gameName << endl;
        return;
    }

    int subId = subscriptions[gameName];
    int receiptId = receiptIdCounter++;
    pendingReceipts[receiptId] = "Exited channel " + gameName;
    subscriptions.erase(gameName);

    string frame = "UNSUBSCRIBE\n"
                   "id:" + subId + "\n"
                   "receipt:" + receiptId + "\n"
                   "\n"
                   "\0";
    sendFrame(handler, frame);
}

void StompProtocol::handleLogout(ConnectionHandler* handler) {
    std::lock_guard<std::mutex> lock(mutex);
    int receiptId = receiptIdCounter++;
    pendingReceipts[receiptId] = "DISCONNECT";

    string frame = "DISCONNECT\n"
                   "receipt:" + receiptId + "\n"
                   "\n"
                   "\0";
    sendFrame(handler, frame);
}

void StompProtocol::handleReport(const string& file, ConnectionHandler* handler) {
    names_and_events data;
    try {
        data = parseEventsFile(file);
    } catch (std::exception& e) {
        cout << "Error: Failed to parse file " << file << endl;
        return;
    }

    string gameName = data.team_a_name + "_" + data.team_b_name;
    for (Event& event : data.events) 
    {
        saveEvent(gameName, this.username, event);
        string body = buildEventBody(event, this.username, gameName);
        string frame = "SEND\n"
                       "destination:/" + gameName + "\n"
                       "\n" + 
                       body + "\n"
                       "\0";
        sendFrame(handler, frame);
    }
}

void StompProtocol::saveEvent(string gameName, string user, Event& event) {
    std::lock_guard<std::mutex> lock(mutex);
    GameStats& stats = gameUpdates[gameName][user];
    stats.events.push_back(event);
    for (auto const& pair : event.get_game_updates()) 
        stats.generalStats[pair.first] = pair.second;
    for (auto const& pair : event.get_team_a_updates()) 
        stats.teamAStats[pair.first] = pair.second;
    for (auto const& pair : event.get_team_b_updates()) 
        stats.teamBStats[pair.first] = pair.second;
}

string StompProtocol::buildEventBody(const Event& event, string user, string gameName) {
    stringstream ss;
    ss << "user: " << user << "\n";
    ss << "team a: " << event.get_team_a_name() << "\n";
    ss << "team b: " << event.get_team_b_name() << "\n";
    ss << "event name: " << event.get_name() << "\n";
    ss << "time: " << event.get_time() << "\n";
    
    ss << "general game updates:\n";
    for (auto const& pair : event.get_game_updates()) 
        ss << "\t" << pair.first << ":" << pair.second << "\n";

    ss << "team a updates:\n";
    for (auto const& pair : event.get_team_a_updates()) 
        ss << "\t" << pair.first << ":" << pair.second << "\n";

    ss << "team b updates:\n";
    for (auto const& pair : event.get_team_b_updates())
        ss << "\t" << pair.first << ":" << pair.second << "\n";

    ss << "description:\n" << event.get_description() << "\n";  
    return ss.str();
}

void StompProtocol::handleSummary(const string& gameName, const string& user, const string& file) {
    std::lock_guard<std::mutex> lock(mutex);
    if (gameUpdates.find(gameName) == gameUpdates.end() || 
        gameUpdates[gameName].find(user) == gameUpdates[gameName].end()) {
        cout << "Error: No data found for game " << gameName << " user " << user << endl;
        return;
    }

    const GameStats& stats = gameUpdates[gameName][user];   
    std::ofstream outfile(file);
    if (!outfile.is_open()) {
        cout << "Error: Could not open file " << file << endl;
        return;
    }

    string teamA = "Team A"; 
    string teamB = "Team B"; 
    if (!stats.events.empty()) {
        teamA = stats.events[0].get_team_a_name();
        teamB = stats.events[0].get_team_b_name();
    }
    
    outfile << teamA << " vs " << teamB << "\n";
    outfile << "Game stats:\n";
    outfile << "General stats:\n";
    for (auto const& pair : stats.generalStats) 
        outfile << pair.first << ": " << pair.second << "\n";

    outfile << teamA << " stats:\n";
    for (auto const& pair : stats.teamAStats) 
        outfile << pair.first << ": " << pair.second << "\n";
    
    outfile << teamB << " stats:\n";
    for (auto const& pair : stats.teamBStats) 
        outfile << pair.first << ": " << pair.second << "\n";
        
    outfile << "Game event reports:\n";
    for (const Event& e : stats.events) {
        outfile << e.get_time() << " - " << e.get_name() << ":\n\n";
        outfile << e.get_description() << "\n\n";
    }
    outfile.close();
}

// Server Frame Processing
bool StompProtocol::processServerFrame(const string& frame) {

}

void StompProtocol::handleServerConnected(const string& frame) {

}

void StompProtocol::handleServerReceipt(const string& frame) {

}

void StompProtocol::handleServerError(const string& frame) {

}

void StompProtocol::handleServerMessage(const string& frame) {

}