package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02 Data Handler
    v20181227IST18XY
    HanishKVC, GPL, 19XY
 */


import android.util.Log;

import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.util.concurrent.LinkedBlockingQueue;

public class DataHandler {
    static final int STOP_SAVEDATABUFS = 9999;
    private static final String LINETERM = "\n";
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
    //private Semaphore sem = new Semaphore(NUMOFDATABUFS-BUF2LOCKDELTA,true);
    private int iWrite2Buf = 0;
    private int iSaveBuf = 0;
    private LinkedBlockingQueue dataQueue = new LinkedBlockingQueue(NUMOFDATABUFS);

    public DataHandler(String sDataFile, String sLogFile) {
        dataFileName = sDataFile;
        logFileName = sLogFile;
    }

    public void OpenFiles() throws IOException {
        try {
            dataFile = new FileOutputStream(dataFileName);
            logFile = new FileWriter(logFileName);
            Log.i(ATAG, "Opened Data & Log Files");
        } catch (Exception e) {
            Log.e(ATAG, "While Opening Files: " + e.toString());
            throw new IOException("Failed to open DataHandler files: " + e.toString());
        }
    }

    public void CloseFiles() {
        try {
            if (dataFile != null) {
                dataFile.close();
                dataFile = null;
                Log.i(ATAG, "Closed Data File");
            }
            if (logFile != null) {
                logFile.close();
                logFile = null;
                Log.i(ATAG, "Closed Log File");
            }
        } catch (IOException e) {
            Log.e(ATAG, "While Closing Files: " + e.toString());
        }
    }

    public String getLogFileName() {
        return logFileName;
    }

    public void LogLostPacketsEx(int StartSeq, int EndSeq) {
        synchronized (logFile) {
            for (int i = StartSeq; i <= EndSeq; i++) {
                try {
                    logFile.write(i+LINETERM);
                } catch (IOException e) {
                    Log.e(ATAG, "While Logging: " + e.toString());
                }
            }
        }
    }

    public void LogLostPackets(int StartSeq, int EndSeq) {
        try {
            synchronized (logFile) {
                logFile.write(StartSeq + "-" + EndSeq + LINETERM);
            }
        } catch (IOException e) {
            Log.e(ATAG, "While Logging: " + e.toString());
        }
    }

    public void LogStr(String str) {
        try {
            synchronized (logFile) {
                logFile.write(str);
                logFile.flush();
            }
        } catch (IOException e) {
            Log.e(ATAG, "While Logging: " + e.toString());
        }
    }

    public void LogStrLn(String str) {
        LogStr(str+LINETERM);
        Log.i(ATAG,str);
    }

    public void Write2DataBuf(int theBlockId, byte[] theData) {
        try {
            Log.i(ATAG, "Writing2DataBuf: " + iWrite2Buf);
            if (theData == null) {
                Log.i(ATAG, "Writing2DataBuf: Its A STOP");
                dataQueue.put(STOP_SAVEDATABUFS);
                return;
            }
            System.arraycopy(theData, 0, dataBuf, iWrite2Buf*DATABUFSIZE, dataSize);
            blockId[iWrite2Buf] = theBlockId;
            dataQueue.put(iWrite2Buf);
            iWrite2Buf += 1;
            if (iWrite2Buf >= NUMOFDATABUFS) {
                iWrite2Buf = 0;
            }
        } catch (InterruptedException e) {
            Log.e(ATAG, "Write2DataBuf: Interrupted queueing data index: " + e.toString());
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
            Log.e(ATAG, "While saving Data[" + iData + "]: " + e.toString());
        }
    }

    public void SaveDataBufs() {
        Log.i(ATAG, "SaveDataBufs: logic started");
        while (true) {
            try {
                int iTemp = (int) dataQueue.take();
                if (iTemp == STOP_SAVEDATABUFS) {
                    break;
                }
                if (iTemp != iSaveBuf) {
                    Log.d(ATAG, "SaveDataBufs: Mismatch wrt DataBuf index between Producer "+iTemp+" and Consumer "+iSaveBuf);
                }
                SaveDataBuf(iSaveBuf);
                iSaveBuf += 1;
                if (iSaveBuf >= NUMOFDATABUFS) {
                    iSaveBuf = 0;
                }
            } catch (InterruptedException e) {
                Log.e(ATAG, "SaveDataBufs: Interrupted: " + e.toString());
            }
        }
        Log.i(ATAG, "SaveDataBufs: logic stopping");
    }

}
