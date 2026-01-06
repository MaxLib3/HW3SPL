package bgu.spl.net.srv;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public class ConnectionsImpl<T> implements Connections<T> {
    
    private final ConcurrentHashMap<Integer, ConnectionHandler<T>> connections;
    private final ConcurrentHashMap<String, CopyOnWriteArrayList<Integer>> channels;

    public ConnectionsImpl() {
        this.connections = new ConcurrentHashMap<>();
        this.channels = new ConcurrentHashMap<>();
    }

    @Override
    public boolean send(int connectionId, T msg) {
        ConnectionHandler<T> handler = connections.get(connectionId);
        if (handler != null) {
            handler.send(msg);
            return true;
        }
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        CopyOnWriteArrayList<Integer> subscribers = channels.get(channel);
        if (subscribers != null) {
            for (Integer connectionId : subscribers) {
                send(connectionId, msg);
            }
        }
    }

    @Override
    public void disconnect(int connectionId) {
        connections.remove(connectionId);
        for (CopyOnWriteArrayList<Integer> subscribers : channels.values()) {
            subscribers.remove(Integer.valueOf(connectionId));
        }
    }

    public void connect(int connectionId, ConnectionHandler<T> handler) {
        connections.put(connectionId, handler);
    }

    public void subscribe(String channel, int connectionId) {
        channels.computeIfAbsent(channel, k -> new CopyOnWriteArrayList<>()).add(connectionId);
    }

    public void unsubscribe(String channel, int connectionId) {
        CopyOnWriteArrayList<Integer> subscribers = channels.get(channel);
        if (subscribers != null) {
            subscribers.remove(Integer.valueOf(connectionId));
        }
    }
    
}
