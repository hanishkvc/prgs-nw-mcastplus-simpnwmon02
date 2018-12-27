package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02 Data Recovery Logic
    v20181226IST0231
    HanishKVC, GPL, 2018
 */

import android.util.Log;

import java.io.IOException;
import java.net.ConnectException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;

public class DataRecovery {
    public static final int NWDATA_MAXSIZE = 1600;
    static private final String ATAG = MainActivity.ATAG + "_DR";
    static private final int portSrvr = 1112;
    static private final int portClient = 1113;
    private static final int SOCKTIMEOUT = 500;
    private static final int NWCMDCOMMON = 0xffffff00;
    private static final int PIReqSeqNum = 0xffffff00;
    private static final int URReqSeqNum = 0xffffff02;
    private static final int URAckSeqNum = 0xffffff03;
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
        bbS.putInt(0, PIReqSeqNum);
        for (int i = 0; i < sID.length(); i++) {
            try {
                bbS.putChar(4+i*2, sID.charAt(i));
            } catch (IndexOutOfBoundsException e) {
                bbS.putChar(4+i*2, ' ');
            }
        }
        DatagramPacket pktS = null;
        try {
            pktS = new DatagramPacket(bbS.array(),(4+8*2), InetAddress.getByName(sPeerInitAddr), portSrvr );
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

    public void unicast_recovery() {
        byte [] dataR = new byte[NWDATA_MAXSIZE];
        ByteBuffer bbR = ByteBuffer.wrap(dataR);
        DatagramPacket pktR = new DatagramPacket(dataR, dataR.length);
        byte [] dataS = new byte[NWDATA_MAXSIZE];
        ByteBuffer bbS = ByteBuffer.wrap(dataS);

        while (true) {
            try {
                socket.receive(pktR);
            } catch (SocketTimeoutException e) {
                Log.w(ATAG, "Nw is silent :-(");
                continue;
            } catch (IOException e) {
                Log.e(ATAG, "Waiting for Nw comm:"+e.toString());
                continue;
            }
            InetAddress tPeer = pktR.getAddress();
            if (!tPeer.equals(peer)) {
                Log.w(ATAG, "Got packet for ["+tPeer+"] but peer is ["+peer+"], so ignoring");
                continue;
            }
            int cmd = bbR.getInt(0);
            if ((cmd & NWCMDCOMMON) == NWCMDCOMMON) {
                if (cmd == URReqSeqNum) {
                    Log.i(ATAG, "Got URReqSeqNum");
                    bbS.putInt(0, URAckSeqNum);
                    int numOfRanges = 10;
                    if (MainActivity.lostPackets.list.size() < numOfRanges) {
                        numOfRanges = MainActivity.lostPackets.list.size();
                    }
                    for (int i = 0; i < numOfRanges; i++) {
                        String tData = MainActivity.lostPackets.getStart(i)+"-"+MainActivity.lostPackets.getEnd(i)+"\n";
                        bbS.put(tData.getBytes());
                    }
                    DatagramPacket pktS = null;
                    pktS = new DatagramPacket(bbS.array(),(4+8*2), peer, portSrvr );
                    try {
                        socket.send(pktS);
                        Log.i(ATAG, "URAckSeqNum Sent to " + peer.toString());
                    } catch (IOException e) {
                        Log.d(ATAG, "URAckSeqNum send Failed:"+e.toString());
                    }
                } else {
                    Log.w(ATAG, "Got UnExpected Nw Command");
                }
                continue;
            }
            Log.d(ATAG, "Ignoring Data packet saving for now");
            MainActivity.lostPackets.remove(cmd);
        }
    }

}
