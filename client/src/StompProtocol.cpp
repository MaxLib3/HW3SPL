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
                   "receipt:" + to_string(receiptId) + "\n\n\0";
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
                   "receipt:" + to_string(receiptId) + "\n\n\0";
    sendFrame(handler, frame);
}

void StompProtocol::handleLogout(ConnectionHandler* handler) {
    std::lock_guard<std::mutex> lock(mutex);
    int receiptId = receiptIdCounter++;
    pendingReceipts[receiptId] = "DISCONNECT";

    string frame = "DISCONNECT\n"
                   "receipt:" + to_string(receiptId) + "\n\n\0";
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

    std::sort(data.events.begin(), data.events.end(), [](const Event& e1, const Event& e2) {
        bool is_pre_ht_1 = false;
        const auto& updates1 = e1.get_game_updates();
        if (updates1.find("before halftime") != updates1.end()) {
            if (updates1.at("before halftime") == "true") {
                is_pre_ht_1 = true;
            }
        }

        bool is_pre_ht_2 = false;
        const auto& updates2 = e2.get_game_updates();
        if (updates2.find("before halftime") != updates2.end()) {
            if (updates2.at("before halftime") == "true") {
                is_pre_ht_2 = true;
            }
        }

        if (is_pre_ht_1 != is_pre_ht_2) {
            return is_pre_ht_1; 
        }
        return e1.get_time() < e2.get_time();
    });

    string gameName = data.team_a_name + "_" + data.team_b_name;
    bool firstSend = true;
    for (Event& event : data.events) 
    {
        string body = buildEventBody(event, this->username, gameName);
        string frame = "";
        if (firstSend)
            frame = "SEND\n"
                        "destination:/" + gameName + "\n"
                        "filename:" + file + "\n\n" + body + "\n\0";
        else
            frame = "SEND\n"
                        "destination:/" + gameName + "\n"
                        "\n" + body + "\n\0";
        sendFrame(handler, frame);
        firstSend = false;
    }
}

void StompProtocol::saveEvent(string gameName, string user, Event& event) {
    std::lock_guard<std::mutex> lock(mutex);
    GameStats& stats = gameUpdates[gameName][user];
    stats.events.push_back(event);

    std::sort(stats.events.begin(), stats.events.end(), [](const Event& e1, const Event& e2) {
        bool is_pre_ht_1 = false;
        const auto& updates1 = e1.get_game_updates();
        if (updates1.find("before halftime") != updates1.end()) {
            if (updates1.at("before halftime") == "true") {
                is_pre_ht_1 = true;
            }
        }

        bool is_pre_ht_2 = false;
        const auto& updates2 = e2.get_game_updates();
        if (updates2.find("before halftime") != updates2.end()) {
            if (updates2.at("before halftime") == "true") {
                is_pre_ht_2 = true;
            }
        }

        if (is_pre_ht_1 != is_pre_ht_2) {
            return is_pre_ht_1; 
        }
        return e1.get_time() < e2.get_time();
    });

    for (auto& pair : event.get_game_updates()) 
        stats.generalStats[pair.first] = pair.second;
    for (auto& pair : event.get_team_a_updates()) 
        stats.teamAStats[pair.first] = pair.second;
    for (auto& pair : event.get_team_b_updates()) 
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
    for (auto& pair : event.get_game_updates()) 
        ss << "\t" << pair.first << ":" << pair.second << "\n";
    ss << "team a updates:\n";
    for (auto& pair : event.get_team_a_updates()) 
        ss << "\t" << pair.first << ":" << pair.second << "\n";
    ss << "team b updates:\n";
    for (auto& pair : event.get_team_b_updates())
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

    GameStats& gs = gameUpdates[gameName][user];
    std::ofstream f(file);
    if (!f) {
        cout << "Error: Could not open file " << file << endl;
        return;
    }

    string tA = "Team A";
    string tB = "Team B";
    if (gs.events.size() > 0) {
        tA = gs.events[0].get_team_a_name();
        tB = gs.events[0].get_team_b_name();
    }

    f << tA << " vs " << tB << "\n";
    f << "Game stats:\n";
    f << "General stats:\n";
    for (auto& p : gs.generalStats) {
        f << p.first << ": " << p.second << "\n";
    }
    f << tA << " stats:\n";
    for (auto& p : gs.teamAStats) {
        f << p.first << ": " << p.second << "\n";
    }
    f << tB << " stats:\n";
    for (auto& p : gs.teamBStats) {
        f << p.first << ": " << p.second << "\n";
    }
    f << "Game event reports:\n";
    for (int i = 0; i < gs.events.size(); i++) {
        f << gs.events[i].get_time() << " - " << gs.events[i].get_name() << ":\n\n";
        f << gs.events[i].get_description() << "\n\n";
    }
    f.close();
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
    string game_name = "";
    bool in_body = false;

    // Needed parameters for event
    string user_name = "";
    string team_a_name = "";
    string team_b_name = "";
    string event_name = "";
    int time_point = 0;
    string desc = "";
    std::map<string, string> gen_updates;
    std::map<string, string> team_a_updates;
    std::map<string, string> team_b_updates;
    
    string section = "";
    for (const string& raw : lines) {
        if (!in_body && trim(raw).empty()) {
            in_body = true;
            continue;
        }

        if (!in_body) {
            string h = trim(raw);
            if (h.find("destination:") == 0) {
                game_name = trim(h.substr(12));
                if (!game_name.empty() && game_name[0] == '/') {
                    game_name = game_name.substr(1);
                }
            }
        } 
        else {
            string line_content = raw;
            if (!line_content.empty() && line_content.back() == '\0') line_content.pop_back();
            string trimmed_line = trim(line_content);

            if (trimmed_line == "general game updates:") { 
                section = "general"; 
                continue; 
            }
            else if (trimmed_line == "team a updates:") { 
                section = "teamA"; 
                continue; 
            }
            else if (trimmed_line == "team b updates:") { 
                section = "teamB"; 
                continue; 
            }
            else if (trimmed_line == "description:") { 
                section = "description"; 
                continue; 
            }

            // Parse content of current section
            if (section == "") {
                // Update fields user, time, event name, team a, team b
                int split_idx = line_content.find(':');
                if (split_idx != string::npos) {
                    string key_part = trim(line_content.substr(0, split_idx));
                    string val_part = trim(line_content.substr(split_idx + 1));

                    if (key_part == "user") user_name = val_part;
                    else if (key_part == "team a") team_a_name = val_part;
                    else if (key_part == "team b") team_b_name = val_part;
                    else if (key_part == "event name") event_name = val_part;
                    else if (key_part == "time") time_point = std::stoi(val_part);
                }
            }
            else if (section == "description") {
                desc += line_content + "\n";
            }
            else {
                // Update maps of general/teamA/teamB
                size_t split_idx = line_content.find(':');
                if (split_idx != string::npos) {
                    string key_part = trim(line_content.substr(0, split_idx));
                    string val_part = trim(line_content.substr(split_idx + 1));

                    if (section == "general") gen_updates[key_part] = val_part;
                    else if (section == "teamA") team_a_updates[key_part] = val_part;
                    else if (section == "teamB") team_b_updates[key_part] = val_part;
                }
            }
        }
    }

    if (!user_name.empty() && !game_name.empty()) {
        Event event(team_a_name, team_b_name, event_name, time_point, 
                    gen_updates, team_a_updates, team_b_updates, 
                    desc);    
        saveEvent(game_name, user_name, event);
        cout << "Received update for " << game_name << " from " << user_name << endl;
    }
}