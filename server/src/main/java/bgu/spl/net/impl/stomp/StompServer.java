package bgu.spl.net.impl.stomp;

import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Invalid number of arguments");
            return;
        }

        int port = Integer.parseInt(args[0]);
        String serverType = args[1];

        if (serverType.equals("tpc")) {
            Server.threadPerClient(
                    port,
                    () -> new StompMessagingProtocolImpl(),
                    StompEncoderDecoder::new
            ).serve();
        } else if (serverType.equals("reactor")) {
            Server.reactor(
                    Runtime.getRuntime().availableProcessors(),
                    port,
                    () -> new StompMessagingProtocolImpl(),
                    StompEncoderDecoder::new
            ).serve();
        } else {
            System.out.println("Unknown server type: " + serverType);
        }
    }
}
