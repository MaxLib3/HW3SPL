package bgu.spl.net.impl.stomp;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;

public class StompMessagingProtocolImpl<T> implements StompMessagingProtocol<String> {

    private boolean shouldTerminate = false;
    private int connectionId;
    private Connections<String> connections;
    private static Map<String, String> users = new ConcurrentHashMap<>(); //username to password 
    private static Map<Integer, String> connectionIdToUser = new ConcurrentHashMap<>(); // connectionId to username
    private static Map<String, Integer> SubscriptiontoChannelname =  new ConcurrentHashMap<>(); // Subscription id to channel name
    private static int messageId = 0;

    @Override
    public void start(int connectionId, Connections<String> connections) {
        // TODO Auto-generated method stub
        this.connectionId = connectionId;
        this.connections = connections;        
    }

    @Override
    public void process(String message) {
        // TODO Auto-generated method stub     
        int i = message.indexOf("\n");
        String command = message.substring(0, i);
        String body = message.substring(i+1);
        switch (command) {
            case "CONNECT":
                this.connect(body);
                break;
            case "SEND":
                this.send(body);
                break;
            case "SUBSCRIBE":
                this.subscribe(body);
                break;
            case "UNSUBSCRIBE":
                this.unsubscribe(body);
                break;
            case "DISCONNECT":
                this.disconnect(body);
                break;
            default:
                //need to send an error to the client
                throw new IllegalArgumentException("Unexpected value: " + command);
        }

    }

    @Override
    public boolean shouldTerminate() {
        // TODO Auto-generated method stub
        return shouldTerminate;
    }

    private void connect (String body) {
        Pattern header = Pattern.compile("accept-version:(.*)\n");
        Pattern header1 = Pattern.compile("host:(.*)\n");
        Pattern header2 = Pattern.compile("passcode:(.*)\n");
        Pattern header3 = Pattern.compile("login:(.*)\n");
        Pattern header4 = Pattern.compile("receipt-id:(.*)\n");
        Matcher matcher = header.matcher(body);
        Matcher matcher1 = header1.matcher(body);
        Matcher matcher2 = header2.matcher(body);
        Matcher matcher3 = header3.matcher(body);
        if (!matcher.find()) {
            this.error("Missing accept-version header");
            return;
        }
        if (!matcher1.find()) {
            this.error("Missing host header");
            return;
        }
        if(!matcher2.find()) {
            this.error("Missing passcode header");
            return;
        }
        if(!matcher3.find()) {
            this.error("Missing login header");
            return;
        }
        String version = matcher.group(1);
        String host = matcher1.group(1);
        if (!version.equals("1.2")) {
            this.error("Unsupported version");
            return;
        }
        if(!host.equals("stomp.cs.bgu.ac.il")) {
            this.error("Wrong host");
            return;
        }
        String username = matcher3.group(1);
        String passcode = matcher2.group(1);
        if (users.containsKey(username)) {
            if (!users.get(username).equals(passcode)) {
                this.error("Wrong password");
                return;
            }
            if (connectionIdToUser.containsValue(username)) {
                this.error("User already logged in");
                return;
            }
        } else {
            users.put(username, passcode);
        }
        connectionIdToUser.put(this.connectionId, username);

        String response = "CONNECTED\n" +
                          "version:1.2\n\n" +
                          "\u0000";
        connections.send(this.connectionId, response);

        Matcher matcher4 = header4.matcher(body);
        if (matcher4.find()) {
            String receiptId = matcher4.group(1);
            String receiptResponse = "RECEIPT\n" +
                                     "receipt-id:" + receiptId + "\n\n" +
                                     "\u0000";
            connections.send(this.connectionId, receiptResponse);
        }
    }

    private void send (String body) {
        Pattern header = Pattern.compile("destination:(.*)\n\n(.*)\n");
        Pattern header1 = Pattern.compile("destination:(.*)\nreceipt-id:.*\n" + "(.*)\n");
        Pattern header4 = Pattern.compile("receipt-id:(.*)\n");

        Matcher matcher = header.matcher(body);
        Matcher matcher1 = header1.matcher(body);
        if (!matcher.find() && !matcher1.find()) {
            this.error("Missing destination header");
            return;
        }
        String destination;
        String content;
        if (matcher1.find()) {
            destination = matcher1.group(1);
            content = matcher1.group(2);
        } else {
            destination = matcher.group(1);
            content = matcher.group(2);
        }
        ArrayList<Integer> subscribers = new ArrayList<>();
        for(Map.Entry<String, Integer> entry : SubscriptiontoChannelname.entrySet()) {
            if (entry.getKey().equals(destination)) {
                subscribers.add(entry.getValue());
            }
        }
        for (Integer subId : subscribers) {
            String message = "MESSAGE\n" +
                            "subscription:" + subId + "\n" +
                            "message-id:" + this.messageId + "\n" +
                            "destination:" + destination + "\n\n" +
                            content + "\n" +
                            "\u0000";
            connections.send(this.connectionId, message);
        }
        this.messageId += 1;


        Matcher matcher4 = header4.matcher(body);
        if (matcher4.find()) {
            String receiptId = matcher4.group(1);
            String receiptResponse = "RECEIPT\n" +
                                     "receipt-id:" + receiptId + "\n\n" +
                                     "\u0000";
            connections.send(this.connectionId, receiptResponse);
        }
    }

    private void subscribe (String body) {
        Pattern header = Pattern.compile("destination:(.*)\n");
        Pattern header1 = Pattern.compile("id:(.*)\n");
        Pattern header4 = Pattern.compile("receipt-id:(.*)\n");
        Matcher matcher = header.matcher(body);
        if (!matcher.find()) {
            this.error("Missing destination header");
            return;
        }
        Matcher matcher1 = header1.matcher(body);
        if (!matcher1.find()) {
            this.error("Missing id header");
            return;
        }
        String destination = matcher.group(1);
        String id = matcher1.group(1);
        int subId = Integer.parseInt(id);
        SubscriptiontoChannelname.put(destination, subId);


        Matcher matcher4 = header4.matcher(body);
        if (matcher4.find()) {
            String receiptId = matcher4.group(1);
            String receiptResponse = "RECEIPT\n" +
                                     "receipt-id:" + receiptId + "\n\n" +
                                     "\u0000";
            connections.send(this.connectionId, receiptResponse);
        }

    }

    private void unsubscribe (String body) {
        Pattern header4 = Pattern.compile("receipt-id:(.*)\n");
        Pattern header1 = Pattern.compile("id:(.*)\n");
        Matcher matcher1 = header1.matcher(body);
        if (!matcher1.find()) {
            this.error("Missing id header");
            return;
        }
        String id = matcher1.group(1);
        int subId = Integer.parseInt(id);
        this.SubscriptiontoChannelname.values().removeIf(value -> value.equals(subId));

        Matcher matcher4 = header4.matcher(body);
        if (matcher4.find()) {
            String receiptId = matcher4.group(1);
            String receiptResponse = "RECEIPT\n" +
                                     "receipt-id:" + receiptId + "\n\n" +
                                     "\u0000";
            connections.send(this.connectionId, receiptResponse);
        }

    }

    private void disconnect (String body) {
        Pattern header4 = Pattern.compile("receipt-id:(.*)\n");
        this.connectionIdToUser.remove(this.connectionId);
        this.shouldTerminate = true;
        
        Matcher matcher4 = header4.matcher(body);
        if (matcher4.find()) {
            String receiptId = matcher4.group(1);
            String receiptResponse = "RECEIPT\n" +
                                     "receipt-id:" + receiptId + "\n\n" +
                                     "\u0000";
            connections.send(this.connectionId, receiptResponse);
        }
    }


    private void error (String errorMessage) {
        String response = "ERROR\n" +
                          "message:" + errorMessage + "\n\n" +
                          "\u0000";
        connections.send(this.connectionId, response);
        this.shouldTerminate = true;
    }
    
}
