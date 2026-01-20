#include "../include/StompProtocol.h"
#include <regex.h>

StompProtocol::StompProtocol() : 
    subIdCounter(0), receiptIdCounter(0), shouldTerminate(false) {}

void StompProtocol::setUsername(string username) {
    this->username = username;
}

void StompProtocol::sendFrame(ConnectionHandler* handler, string frame) {
    cout << "Sending frame to server:\n" << frame << std::endl;
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
                   "id:" + to_string(subId) + "\n"
                   "receipt:" + to_string(receiptId) + "\n"
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
                   "id:" + to_string(subId) + "\n"
                   "receipt:" + to_string(receiptId) + "\n"
                   "\n"
                   "\0";
    sendFrame(handler, frame);
}

void StompProtocol::handleLogout(ConnectionHandler* handler) {
    std::lock_guard<std::mutex> lock(mutex);
    int receiptId = receiptIdCounter++;
    pendingReceipts[receiptId] = "DISCONNECT";

    string frame = "DISCONNECT\n"
                   "receipt:" + to_string(receiptId) + "\n"
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

    std::sort(data.events.begin(), data.events.end(), [](const Event& a, const Event& b) {
        return a.get_time() < b.get_time();
    });

    string gameName = data.team_a_name + "_" + data.team_b_name;
    for (Event& event : data.events) 
    {
        string body = buildEventBody(event, this->username, gameName);
        string frame = "SEND\n"
                       "destination:/" + gameName + "\n"
                       "filename:" + file + "\n"
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
    std::sort(stats.events.begin(), stats.events.end(), [](const Event& a, const Event& b) {
        return a.get_time() < b.get_time();
    });

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
    char spliter = '\n';
    vector<string> result = split(frame, spliter);
    if (result.size() == 0) return false;

    string command = trim(result[0]);
    if (command.length() == 0) return true;

    cout << "Received frame from server: " << frame << endl;
    if (command == "CONNECTED") {
        result.erase(result.begin());
        handleServerConnected(result);
    } 
    else if (command == "ERROR") {
        result.erase(result.begin());
        handleServerError(result);
        return false;
    } 
    else if (command == "MESSAGE") {
        result.erase(result.begin());
        handleServerMessage(result);
    } 
    else if (command == "RECEIPT") {
        result.erase(result.begin());
        handleServerReceipt(result);
        if (shouldTerminate) return false; 
    } 
    else {
        return false; 
    }
    return true;
}

string StompProtocol::trim(const string& str){
    int firstindex = 0;
    bool found = false;
    for (int i = 0; i < str.length() && !found; i++)
    {
        if (str[i] != ' ' && str[i] != '\n' && str[i] != '\r' && str[i] != '\t'){
            firstindex = i;
            found = true;
        }
    }
    found = false;
    string midstr(""); 
    midstr = str.substr(firstindex);
    int lastindex = str.length() - 1;
    for (int i = str.length() - 1; i >= 0 && !found; i--)
    {
        if (str[i] != ' ' && str[i] != '\n' && str[i] != '\r' && str[i] != '\t'){
            lastindex = i;
            found = true;
        }
    }
    string retval = midstr.substr(0, lastindex + 1);
    return retval;
}

vector<string> StompProtocol::split(const string& str, char delimiter) {
    vector<string> tokens;
    string line("");
    for (char c : str) {
        if (c == delimiter) {
            tokens.push_back(line);
            line = "";
        } else {
            line += c;
        }
    }
    tokens.push_back(line);
    return tokens;
}

void StompProtocol::handleServerConnected(const vector<string>& lines) {
    std::regex versionRegex("version:\\s*([0-9\\.]+)");
    for (const string& line : lines) {
        std::smatch match;
        if (std::regex_search(line, match, versionRegex)) {
            string version = match[1];
            version = trim(version);
            if (version == "1.2")
            {
                cout << "Login successful" << endl;
            }
        }
    }
}

void StompProtocol::handleServerReceipt(const vector<string>& lines) {
    std::lock_guard<std::mutex> lock(mutex);
    std::regex receiptRegex("receipt-id:\\s*(\\d+)");
    for (const string& line : lines) {
        std::smatch match;
        if (std::regex_search(line, match, receiptRegex)) {
            int receiptId = std::stoi(match[1]);
            if (pendingReceipts.find(receiptId) != pendingReceipts.end()) {
                string action = pendingReceipts[receiptId];
                if (action == "DISCONNECT") {
                    shouldTerminate = true;
                }
                pendingReceipts.erase(receiptId);
            }
        }
    }
}

void StompProtocol::handleServerError(const vector<string>& lines) {
    string errorMsg = "Error from server:";
    for (const string& line : lines) {
        errorMsg += "\n" + line;
    }
    cout << errorMsg << endl;
    shouldTerminate = true;
}

void StompProtocol::handleServerMessage(const vector<string>& lines) {
    string gameName = "";
    bool readingBody = false;

    // Needed parameters for event
    string user = "";
    string teamA = "";
    string teamB = "";
    string eventName = "";
    int time = 0;
    string description = "";
    std::map<string, string> generalUpdates;
    std::map<string, string> teamAUpdates;
    std::map<string, string> teamBUpdates;
    
    string currentSection = "";
    for (const string& rawLine : lines) {
        if (!readingBody && trim(rawLine).empty()) {
            readingBody = true;
            continue;
        }

        if (!readingBody) {
            // Header parse
            string line = trim(rawLine);
            if (line.find("destination:") == 0) {
                gameName = trim(line.substr(12));
                if (!gameName.empty() && gameName[0] == '/') {
                    gameName = gameName.substr(1);
                }
            }
        } 
        else {
            // Body parse
            string line = rawLine;
            if (!line.empty() && line.back() == '\0') line.pop_back();
            string trimmed = trim(line);

            // Detect Sections
            if (trimmed == "general game updates:") { 
                currentSection = "general"; 
                continue; 
            }
            else if (trimmed == "team a updates:") { 
                currentSection = "teamA"; 
                continue; 
            }
            else if (trimmed == "team b updates:") { 
                currentSection = "teamB"; 
                continue; 
            }
            else if (trimmed == "description:") { 
                currentSection = "description"; 
                continue; 
            }

            // Parse content based on current section
            if (currentSection == "") {
                // We are in the standard fields (user, time, event name, etc.)
                size_t colon = line.find(':');
                if (colon != string::npos) {
                    string key = trim(line.substr(0, colon));
                    string val = trim(line.substr(colon + 1));

                    if (key == "user") user = val;
                    else if (key == "team a") teamA = val;
                    else if (key == "team b") teamB = val;
                    else if (key == "event name") eventName = val;
                    else if (key == "time") time = std::stoi(val);
                }
            }
            else if (currentSection == "description") {
                description += line + "\n";
            }
            else {
                // We are in one of the update maps (general/teamA/teamB)
                size_t colon = line.find(':');
                if (colon != string::npos) {
                    string key = trim(line.substr(0, colon));
                    string val = trim(line.substr(colon + 1));

                    if (currentSection == "general") generalUpdates[key] = val;
                    else if (currentSection == "teamA") teamAUpdates[key] = val;
                    else if (currentSection == "teamB") teamBUpdates[key] = val;
                }
            }
        }
    }

    if (!user.empty() && !gameName.empty()) {
        Event event(teamA, teamB, eventName, time, 
                    generalUpdates, teamAUpdates, teamBUpdates, 
                    description);    
        saveEvent(gameName, user, event);
        cout << "Received update for " << gameName << " from " << user << endl;
    }
}