package india.hanishkvc.simpnwmon02;

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
        Log.w(ATAG, "LogTaskQueue logic started");
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
        Log.w(ATAG, "LogTaskQueue logic stopping");
    }

    public LogTaskQ(DataHandler oDH, BlockingQueue oQueue) {
        dh = oDH;
        queue = oQueue;
        Log.i(ATAG, "LogTaskQueue logic created");
    }

}
