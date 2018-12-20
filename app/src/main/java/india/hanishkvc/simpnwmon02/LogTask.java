package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02 Simple LogTask Runnable
    This needs to be called from a new thread, so that the caller doesnt block.
    The caller will have to call it with a new thread runnable each time.
    v20181220IST1530
    HanishKVC, GPL, 2018
 */

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
