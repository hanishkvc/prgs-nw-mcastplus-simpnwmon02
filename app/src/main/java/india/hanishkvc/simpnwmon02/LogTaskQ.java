package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02 LogTask Queue based Runnable
    This needs to be called from a new thread, so that the caller doesnt block.
    However here the Caller needs to create a single Thread and Inturn also create a BlockingQueue
    which will be used to pass data from the caller to this logic.
    v20181220IST1530
    HanishKVC, GPL, 2018
 */

import android.util.Log;

import java.util.concurrent.BlockingQueue;

public class LogTaskQ implements Runnable {
    final static String ATAG = MainActivity.ATAG+"LTQ";
    final static int STOP_STARTSEQ = 4321;
    final static int STOP_ENDSEQ = 1234;
    private BlockingQueue queue;
    private DataHandler dh;

    @Override
    public void run() {
        int[] seqs;
        Log.i(ATAG, "LogTaskQueue logic started");
        while (true) {
            try {
                seqs = (int[]) queue.take();
                if ((seqs[0] == STOP_STARTSEQ) && (seqs[1] == STOP_ENDSEQ)) {
                    break;
                }
                dh.LogLostPackets(seqs[0], seqs[1]);
            } catch (InterruptedException e) {
                Log.w(ATAG, "LogTaskQueue interrupted: " + e.toString());
            }
        }
        Log.i(ATAG, "LogTaskQueue logic stopping");
    }

    public LogTaskQ(DataHandler oDH, BlockingQueue oQueue) {
        dh = oDH;
        queue = oQueue;
        Log.i(ATAG, "LogTaskQueue logic created");
    }

}
