package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02 Data Handler
    v20181220IST1040
    HanishKVC, GPL, 2018
 */


import android.util.Log;

import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.Semaphore;

public class DataHandler {
    private String dataFileName = null;
    private String logFileName = null;
    private FileOutputStream dataFile = null;
    private FileWriter logFile = null;
    private static String ATAG = MainActivity.ATAG+"_DH";
    public static final int NUMOFDATABUFS = 64;
    public static final int BUF2LOCKDELTA=3;
    public static final int DATABUFSIZE = 2048;
    private byte [] dataBuf = new byte[NUMOFDATABUFS*DATABUFSIZE];
    private int [] blockId = new int[NUMOFDATABUFS];
    public int dataSize = 1024;
    private Semaphore sem = new Semaphore(NUMOFDATABUFS-BUF2LOCKDELTA,true);
    private int iWrite2Buf = 0;
    private int iSaveBuf = 0;
    private LinkedBlockingQueue dataQueue = new LinkedBlockingQueue(NUMOFDATABUFS);

    public DataHandler(String sDataFile, String sLogFile) throws IOException {
        dataFileName = sDataFile;
        logFileName = sLogFile;

        try {
            dataFile = new FileOutputStream(dataFileName);
            logFile = new FileWriter(logFileName);
        } catch (Exception e) {
            Log.e(ATAG, "While Opening Files: " + e.toString());
            throw new IOException("Failed to open DataHandler files: " + e.toString());
        }
    }

    public void LogLostPacketsEx(int StartSeq, int EndSeq) {
        synchronized (logFile) {
            for (int i = StartSeq; i <= EndSeq; i++) {
                try {
                    logFile.write(i+"\n");
                } catch (IOException e) {
                    Log.e(ATAG, "While Logging: " + e.toString());
                }
            }
        }
    }

    public void LogLostPackets(int StartSeq, int EndSeq) {
        try {
            synchronized (logFile) {
                logFile.write(StartSeq + "-" + EndSeq + "\n");
            }
        } catch (IOException e) {
            Log.e(ATAG, "While Logging: " + e.toString());
        }
    }

    public void Write2DataBuf(int theBlockId, byte[] theData) {
        try {
            Log.i(ATAG, "Writing2DataBuf: " + iWrite2Buf);
            System.arraycopy(theData, 0, dataBuf, iWrite2Buf*DATABUFSIZE, dataSize);
            blockId[iWrite2Buf] = theBlockId;
            dataQueue.put(iWrite2Buf);
            iWrite2Buf += 1;
            if (iWrite2Buf >= NUMOFDATABUFS) {
                iWrite2Buf = 0;
            }
            sem.acquire();
        } catch (InterruptedException e) {
            Log.e(ATAG, "Write2DataBuf: Interrupted trying to acquire sem: " + e.toString());
        }
    }

    public void SaveDataBuf(int iData) {
        try {
            Log.i(ATAG, "Saving DataBuf: " + iData);
            synchronized (dataFile) {
                dataFile.getChannel().position(blockId[iData]*dataSize);
                dataFile.write(dataBuf, iData*DATABUFSIZE, dataSize);
            }
        } catch (IOException e) {
            Log.e(ATAG, "While writing Data[" + iData + "]: " + e.toString());
        }
    }

    public void SaveDataBufs() {
        while (true) {
            try {
                int iTemp = (int) dataQueue.take();
                if (iTemp != iSaveBuf) {
                    Log.d(ATAG, "SaveDataBufs: Mismatch wrt DataBuf index between Producer "+iTemp+" and Consumer "+iSaveBuf);
                }
                SaveDataBuf(iSaveBuf);
                iSaveBuf += 1;
                if (iSaveBuf >= NUMOFDATABUFS) {
                    iSaveBuf = 0;
                }
                sem.release();
            } catch (InterruptedException e) {
                Log.e(ATAG, "SaveDataBufs: Interrupted: " + e.toString());
            }
        }
    }

}
