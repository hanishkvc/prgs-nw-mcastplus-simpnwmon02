package india.hanishkvc.simpnwmon02;

import android.util.Log;

import java.util.concurrent.BlockingQueue;

public class LogTaskQ implements Runnable {
    final static String ATAG = MainActivity.ATAG+"LTQ";
    private BlockingQueue queue;
    private DataHandler dh;

    @Override
    public void run() {
        int[] seqs;
        while (true) {
            try {
                seqs = (int[]) queue.take();
                dh.LogLostPackets(seqs[0], seqs[1]);
            } catch (InterruptedException e) {
                Log.w(ATAG, "LogTaskQueue interrupted: " + e.toString());
            }
        }
    }

    public LogTaskQ(DataHandler oDH, BlockingQueue oQueue) {
        dh = oDH;
        queue = oQueue;
        Log.i(ATAG, "LogTaskQueue created");
    }

}
