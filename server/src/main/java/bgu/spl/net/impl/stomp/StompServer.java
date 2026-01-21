package bgu.spl.net.impl.stomp;

import bgu.spl.net.impl.data.Database;
import bgu.spl.net.srv.Server;

public class StompServer {

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Invalid number of arguments");
            return;
        }

        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("\nServer is shutting down... Printing Report:");
            Database.getInstance().printReport();
        }));

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
