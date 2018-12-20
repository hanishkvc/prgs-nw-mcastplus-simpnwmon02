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

public class DataHandler {
    private String dataFileName = null;
    private String logFileName = null;
    private FileOutputStream dataFile = null;
    private FileWriter logFile = null;
    private static String ATAG = MainActivity.ATAG+"_DH";

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

}
