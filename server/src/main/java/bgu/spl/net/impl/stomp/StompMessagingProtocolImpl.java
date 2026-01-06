package bgu.spl.net.impl.stomp;
import bgu.spl.net.api.StompMessagingProtocol;
import bgu.spl.net.srv.Connections;

public class StompMessagingProtocolImpl<T> implements StompMessagingProtocol<String> {

    @Override
    public void start(int connectionId, Connections<String> connections) {
        // TODO Auto-generated method stub
        
    }

    @Override
    public void process(String message) {
        // TODO Auto-generated method stub
        
    }

    @Override
    public boolean shouldTerminate() {
        // TODO Auto-generated method stub
        return false;
    }
    
}
