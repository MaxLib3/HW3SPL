package bgu.spl.net.impl.stomp;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.impl.data.Database;
import bgu.spl.net.impl.data.LoginStatus;
import bgu.spl.net.srv.Connections;
import bgu.spl.net.srv.ConnectionsImpl;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class StompMessagingProtocolImpl implements StompMessagingProtocol<String> {

    private int connectionId;
    private ConnectionsImpl<String> connections;
    private boolean shouldTerminate = false;
    private String user;
    private ConcurrentHashMap<String, String> subscriptionIdToChannel = new ConcurrentHashMap<>();

    private static Database database = Database.getInstance();
    private static final AtomicInteger msgId = new AtomicInteger(0);

    @Override
    public void start(int connectionId, Connections<String> connections) {
        this.connectionId = connectionId;
        this.connections = (ConnectionsImpl<String>)connections;
    }

    @Override
    public void process(String message) {
        System.out.println("Processing message from connection " + connectionId + ":\n" + message + "\n");
        String[] lines = message.split("\n");
        if (lines.length == 0) return;

        String command = lines[0].trim();
        switch (command) {
            case "CONNECT":
                handleConnect(message);
                break;
            case "SUBSCRIBE":
                handleSubscribe(message);
                break;
            case "UNSUBSCRIBE":
                handleUnsubscribe(message);
                break;
            case "SEND":
                handleSend(message);
                break;
            case "DISCONNECT":
                handleDisconnect(message);
                break;
            default:
                sendError("Unknown command", "The command " + command + " is not recognized", null);
                break;
        }
    }

    @Override
    public boolean shouldTerminate() {
        return shouldTerminate;
    }

    private void handleConnect(String message) {
        
        String version = extractHeader(message, "accept-version");
        String host = extractHeader(message, "host");
        String login = extractHeader(message, "login");
        String passcode = extractHeader(message, "passcode");
        String receipt = extractHeader(message, "receipt");
        if (version == null || host == null || login == null || passcode == null) {
            sendError("Malformed Frame", "Missing headers", receipt);
            return;
        }
        if (!version.equals("1.2")) {
            sendError("Malformed Frame", "Invalid version: " + version, receipt);
            return;
        }
        if (!host.equals("stomp.cs.bgu.ac.il")) {
            sendError("Malformed Frame", "Invalid host: " + host, receipt);
            return;
        }

        LoginStatus status = database.login(connectionId, login, passcode);
        
        if (status == LoginStatus.CLIENT_ALREADY_CONNECTED) {
            sendError("User already logged in", "User " + login + " is already active", receipt);
            return;
        }
        if (status == LoginStatus.ALREADY_LOGGED_IN) {
            sendError("User already logged in", "User " + login + " is already active", receipt);
            return;
        }
        if (status == LoginStatus.WRONG_PASSWORD) {
            sendError("Wrong password", "Password does not match", receipt);
            return;
        }
        
        this.user = login;
        sendFrame("CONNECTED\n" + "version:1.2\n");
        if (receipt != null) {
            sendFrame("RECEIPT\n" + "receipt-id:" + receipt + "\n");
        }
    }

    private void handleSubscribe(String message) {
        String dest = extractHeader(message, "destination");
        String id = extractHeader(message, "id");
        String receipt = extractHeader(message, "receipt");
        if (dest == null || id == null) {
            sendError("Malformed Frame", "Missing destination or id", receipt);
            return;
        }

        System.out.println("Subscribing to " + dest + " with id " + id);
        subscriptionIdToChannel.put(id, dest);
        connections.subscribe(dest, connectionId, id);
        if (receipt != null) {
            sendFrame("RECEIPT\n" + "receipt-id:" + receipt + "\n");
        }
    }

    private void handleUnsubscribe(String message) {
        String subId = extractHeader(message, "id");
        String receipt = extractHeader(message, "receipt");
        if (subId == null) {
            sendError("Malformed Frame", "Missing id", receipt);
            return;
        }

        String channel = subscriptionIdToChannel.remove(subId);
        if (channel != null) {
            connections.unsubscribe(channel, connectionId);
        }

        if (receipt != null) {
            sendFrame("RECEIPT\n" + "receipt-id:" + receipt + "\n");
        }
    }

    private void handleSend(String message) {
        String dest = extractHeader(message, "destination");
        String filename = extractHeader(message, "filename");
        String receipt = extractHeader(message, "receipt");
        if (dest == null) {
            sendError("Malformed Frame", "Missing destination", receipt);
            return;
        }
        if (!subscriptionIdToChannel.containsValue(dest)) {
            sendError("Access Denied", "User not subscribed to topic", receipt);
            return;
        }
        if (filename != null)
            database.trackFileUpload(user, filename, dest);

        String body = "";
        int bodyIdx = message.indexOf("\n\n");
        if (bodyIdx != -1) {
            body = message.substring(bodyIdx + 2);
        }
        String msgFrame = "MESSAGE\n" +
                          "destination:" + dest + "\n" +
                          "message-id:" + msgId.incrementAndGet() + "\n" +
                          "\n" +
                          body;
        connections.send(dest, msgFrame);

        if (receipt != null) {
            sendFrame("RECEIPT\n" + "receipt-id:" + receipt + "\n");
        }
    }

    private void handleDisconnect(String message) {
        String receipt = extractHeader(message, "receipt");
        if (user != null) {
            database.logout(connectionId);
        }

        if (receipt != null) {
            sendFrame("RECEIPT\n" + "receipt-id:" + receipt + "\n");
        }
        connections.disconnect(connectionId);
        shouldTerminate = true;
    }

    private String extractHeader(String msg, String key) {
        Pattern pattern = Pattern.compile(key + ":\\s*(.*)");
        Matcher matcher = pattern.matcher(msg);
        if (matcher.find()) {
            return matcher.group(1).trim();
        }
        return null;
    }

    private void sendFrame(String frameBody) {
        System.out.println("Sending frame to connection " + connectionId + ":\n" + frameBody + "\n\u0000");
        connections.send(connectionId, frameBody + "\n\u0000"); 
    }

    private void sendError(String message, String description, String receiptId) {
        String frame = "ERROR\n" +
                       (receiptId != null ? "receipt-id:" + receiptId + "\n" : "") +
                       "message:" + message + "\n" +
                       "\n" +
                       (description != null ? description : "") + "\n";
        
        sendFrame(frame);
        connections.disconnect(connectionId);
        if (user != null) database.logout(connectionId);
        shouldTerminate = true;
    }
}