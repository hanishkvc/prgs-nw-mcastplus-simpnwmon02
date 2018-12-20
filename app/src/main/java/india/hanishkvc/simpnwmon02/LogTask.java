package india.hanishkvc.simpnwmon02;

public class LogTask implements Runnable {
    int startSeq;
    int endSeq;
    DataHandler dh;

    @Override
    public void run() {
        dh.LogLostPackets(startSeq, endSeq);
    }

    public LogTask(DataHandler oDH, int iStartSeq, int iEndSeq) {
        startSeq = iStartSeq;
        endSeq = iEndSeq;
        dh = oDH;
    }

}
