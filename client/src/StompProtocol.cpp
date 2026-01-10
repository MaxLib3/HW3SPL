#include "StompProtocol.h"

StompProtocol::StompProtocol() : 
    subscriptionIdCounter(0), receiptIdCounter(0), shouldTerminate(false) {}

void StompProtocol::setUsername(string username) {
    this->username = username;
}

void StompProtocol::sendFrame(ConnectionHandler* handler, string frame) {
    if (!handler->sendFrameAscii(frame, '\0')) {
        cout << "Error: Connection lost while sending frame" << endl;
        shouldTerminate = true;
    }
}