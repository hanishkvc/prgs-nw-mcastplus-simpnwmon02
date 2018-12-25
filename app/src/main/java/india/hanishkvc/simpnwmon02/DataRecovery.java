package india.hanishkvc.simpnwmon02;

import android.util.Log;

import java.io.IOException;
import java.net.ConnectException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;

public class DataRecovery {
    static private final String ATAG = MainActivity.ATAG + "_DR";
    static private final int portSrvr = 1112;
    static private final int portClient = 1113;
    private static final int SOCKTIMEOUT = 500;
    private static final int SSN_META = 0xffffffff;
    static  public String sID = "AD44";
    private DatagramSocket socket = null;
    public InetAddress peer = null;
    public String sPeerInitAddr = "255.255.255.255";

    public void init() {
        try {
            socket = new DatagramSocket(portClient);
            socket.setBroadcast(true);
            socket.setSoTimeout(SOCKTIMEOUT);
            Log.i(ATAG, "DataRecovery Init done");
        } catch (SocketException e) {
            Log.e(ATAG, "Failed to create socket:"+e.toString());
        }
    }

    public void presence_info() throws ConnectException {
        byte [] dataR = new byte[32];
        DatagramPacket pktR = new DatagramPacket(dataR, dataR.length);
        byte [] dataS = new byte[32];
        ByteBuffer bbS = ByteBuffer.wrap(dataS);
        bbS.putInt(0,SSN_META);
        for (int i = 0; i < sID.length(); i++) {
            bbS.putChar(4+i, sID.charAt(i));
        }
        DatagramPacket pktS = null;
        try {
            pktS = new DatagramPacket(bbS.array(),16, InetAddress.getByName(sPeerInitAddr), portSrvr );
            Log.i(ATAG, "PeerInitSearchAddr: "+sPeerInitAddr+":"+portSrvr);
        } catch (UnknownHostException e) {
            Log.d(ATAG, "Failed to create broadcast pkt:"+e.toString());
        }
        for (int i = 0; i < 10; i++) {
            Log.i(ATAG, "PI find peer: try "+i);
            try {
                socket.send(pktS);
                socket.receive(pktR);
                peer = pktR.getAddress();
                Log.i(ATAG, "PI found peer: " + peer.toString());
                break;
            } catch (IOException e) {
                Log.d(ATAG, "Failed PI comm:"+e.toString());
            }
        }
        if (peer == null) {
            throw new ConnectException("Didnt find peer using PI");
        }
    }
}
