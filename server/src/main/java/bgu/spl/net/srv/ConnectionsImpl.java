package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;

public class ConnectionsImpl<T> implements Connections<T> {

    // (connectionID, ConnectionHandler) for all active connections
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> activeConnections;
    
    // (channelName, (connectionID -> subscriptionID)) for all channels and their subscribers
    private final ConcurrentHashMap<String, ConcurrentHashMap<Integer, String>> channels;

    public ConnectionsImpl() {
        this.activeConnections = new ConcurrentHashMap<>();
        this.channels = new ConcurrentHashMap<>();
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = activeConnections.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    @SuppressWarnings("unchecked")
    public void send(String channel, T msg) {
        ConcurrentHashMap<Integer, String> subscribers = channels.get(channel);
        if (subscribers != null) {
            for (Integer connectionId : subscribers.keySet()) 
            {
                String subscriptionId = subscribers.get(connectionId);
                String frame = (String) msg;
                frame = frame.replaceFirst("MESSAGE\n", "MESSAGE\nsubscription:" + subscriptionId + "\n");           
                send(connectionId, (T)frame);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        activeConnections.remove(connectionId);
        for (ConcurrentHashMap<Integer, String> subscribers : channels.values()) {
            subscribers.remove(connectionId);
        }
    }

    public void connect(int connectionId, ConnectionHandler<T> handler) {
        activeConnections.put(connectionId, handler);
    }

    public void subscribe(String channel, int connectionId, String subscriptionId) {
        channels.computeIfAbsent(channel, k -> new ConcurrentHashMap<>())
                .put(connectionId, subscriptionId);
        System.out.println("Current subscribers to " + channel + ": " + channels.get(channel).size());
    }

    public void unsubscribe(String channel, int connectionId) {
        ConcurrentHashMap<Integer, String> subscribers = channels.get(channel);
        if (subscribers != null) {
            subscribers.remove(connectionId);
        }
        System.out.println("Current subscribers to " + channel + ": " + channels.get(channel).size());
    }
}