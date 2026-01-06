package bgu.spl.net.srv;

public class ConnectionsImpl<T> implements Connections<T> {

    @Override
    public boolean send(int connectionId, T msg) {
        //IMPLEMENT IF NEEDED
        return false;
    }

    @Override
    public void send(String channel, T msg) {
        //IMPLEMENT IF NEEDED
    }

    @Override
    public void disconnect(int connectionId) {
        //IMPLEMENT IF NEEDED
    }
    
}
