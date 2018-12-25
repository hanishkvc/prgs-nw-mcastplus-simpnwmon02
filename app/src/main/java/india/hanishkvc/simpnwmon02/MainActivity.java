package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02
    v20181223IST2238
    HanishKVC, GPL, 2018
 */


import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Enumeration;
import java.util.concurrent.LinkedBlockingQueue;

public class MainActivity extends AppCompatActivity {

    private static final int MCASTTIMEOUT = 50;
    private static final int MAXMCASTS = 10;
    private static final int PROGRESSUPDATEMOD = (20*5);
    static final String ATAG = "SimpNwMon02";
    private static final int CHANNEL2SAVE = 1;
    EditText etNwInterface;
    EditText etMCastName;
    EditText etMCastGroup;
    EditText etMCastPort;
    EditText etMCastRedDelay;
    EditText etMCastSeqOffset;
    ListView lvMCasts;
    Button btnMCastAdd;
    Button btnStartMon;

    String sNwInterface = null;

    int iNumMCasts = 0;
    String[] sMCName = new String[MAXMCASTS];
    String[] sMCGroup = new String[MAXMCASTS];
    int[] iMCPort = new int[MAXMCASTS];
    int[] iMCDelay = new int[MAXMCASTS];
    int[] iMCRedDelay = new int[MAXMCASTS];
    int[] iMCSeqOffset = new int[MAXMCASTS];

    DataHandler myDH = null;
    private String sExternalBasePath = null;

    private class AdapterLVMCasts extends BaseAdapter {

        @Override
        public int getCount() {
            return iNumMCasts;
        }

        @Override
        public String getItem(int position) {
            if (position >= iNumMCasts)
                return null;
            return sMCName[position];
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            if (position >= iNumMCasts)
                return null;

            if (convertView == null) {
                convertView = getLayoutInflater().inflate(R.layout.listitem_mcast, null);
            }
            if (position == 0) {
                ((TextView) convertView.findViewById(R.id.tvName)).setText("MCName");
            } else {
                ((TextView) convertView.findViewById(R.id.tvName)).setText(sMCName[position]);
            }
            ((TextView) convertView.findViewById(R.id.tvDelay)).setText("DelayCnt");
            ((TextView) convertView.findViewById(R.id.tvSeqNum)).setText("SeqNum");
            ((TextView) convertView.findViewById(R.id.tvPktCnt)).setText("PktCnt");
            ((TextView) convertView.findViewById(R.id.tvDisjointSeqs)).setText("Disjoint\nSeqs");
            ((TextView) convertView.findViewById(R.id.tvDisjointPktCnt)).setText("Disjoint\nPktCnt");
            ((TextView) convertView.findViewById(R.id.tvOlderSeqs)).setText("OlderSeqs");
            if (position == 0) {
                ((TextView) convertView.findViewById(R.id.tvInfo)).setText("MCInfo                ");
            } else {
                ((TextView) convertView.findViewById(R.id.tvInfo)).setText(sMCGroup[position]+" : "+iMCPort[position]+" , RD: "+iMCRedDelay[position]+" , SeqO: "+iMCSeqOffset[position]);
            }
            return convertView;
        }
    }

    AdapterLVMCasts adapterLVMCasts = new AdapterLVMCasts();

    private class MCastMonitor extends AsyncTask<Void, Void, Void> {

        int iNumMCastsSaved = iNumMCasts;
        MulticastSocket[] socks = new MulticastSocket[MAXMCASTS];
        int[] iSeqNum = new int[MAXMCASTS];
        int[] iPktCnt = new int[MAXMCASTS];
        int[] iDisjointSeqs = new int[MAXMCASTS];
        int[] iDisjointPktCnt = new int[MAXMCASTS];
        int[] iOlderSeqs = new int[MAXMCASTS];
        LinkedBlockingQueue theLogQueue = new LinkedBlockingQueue(128);
        Thread theLogTaskThread = new Thread( new LogTaskQ(myDH, theLogQueue));
        Thread theDataThread = new Thread(new Runnable() {
            @Override
            public void run() {
                myDH.SaveDataBufs();
            }
        });

        @Override
        protected Void doInBackground(Void... voids) {
            int iRunCnt = 0;
            try {
                for(int i = 1; i < iNumMCastsSaved; i++) {
                    String stMode;
                    socks[i] = new MulticastSocket(iMCPort[i]);
                    socks[i].joinGroup(InetAddress.getByName(sMCGroup[i]));
                    stMode = "MCast";
                    socks[i].setSoTimeout(MCASTTIMEOUT);
                    iSeqNum[i] = -1;
                    iMCDelay[i] = 0;
                    iPktCnt[i] = 0;
                    iDisjointSeqs[i] = 0;
                    iDisjointPktCnt[i] = 0;
                    iOlderSeqs[i] = 0;
                    Log.i(ATAG, "Opened "+stMode+" socket for "+sMCGroup[i]+":"+iMCPort[i]);
                }
            } catch (Exception e) {
                Log.w(ATAG, "While SettingUp sockets: " + e.toString());
                return null;
            }

            theLogTaskThread.start();
            theDataThread.start();
            byte buf[] = new byte[1600];
            DatagramPacket pkt = new DatagramPacket(buf, buf.length);
            long prevTime = 0;
            while (!isCancelled()) {
                for(int i = 1; i < iNumMCastsSaved; i++) {
                    try {
                        //TODO: Clear the old content from pkt, as the same pkt is used across channels
                        // as well as across packets of the same channel.
                        socks[i].receive(pkt);
                        iMCDelay[i] = 0;
                        iPktCnt[i] += 1;
                        try {
                            ByteBuffer bb = ByteBuffer.wrap(pkt.getData(), iMCSeqOffset[i], 4);
                            bb.order(ByteOrder.LITTLE_ENDIAN);
                            int curSeq = bb.getInt();
                            if (iSeqNum[i] >= curSeq) {
                                iOlderSeqs[i] += 1;
                            } else {
                                if ((curSeq-iSeqNum[i]) != 1) {
                                    iDisjointSeqs[i] += 1;
                                    iDisjointPktCnt[i] += (curSeq - iSeqNum[i] - 1);
                                    if (i == CHANNEL2SAVE) {
                                        log_lostpackets(iSeqNum[i]+1, curSeq-1);
                                    }
                                } else {
                                    if (i == CHANNEL2SAVE) {
                                        myDH.Write2DataBuf(curSeq, pkt.getData());
                                    }
                                }
                                iSeqNum[i] = curSeq;
                            }
                        } catch (Exception e) {
                            iSeqNum[i] = 987654321;
                        }
                    } catch (SocketTimeoutException e) {
                        iMCDelay[i] += 1;
                    } catch (IOException e) {
                        Log.w(ATAG, "In middle of Monitoring: " + e.toString());
                        iMCDelay[i] += 1;
                        break;
                    }
                }
                iRunCnt += 1;
                if (iNumMCastsSaved == 2) {
                    long curTime = System.currentTimeMillis();
                    long deltaTime = curTime - prevTime;
                    if (deltaTime > 5*1000) {
                        publishProgress();
                        prevTime = curTime;
                    }
                } else {
                    publishProgress();
                }
            }

            try {
                for (int i = 1; i < iNumMCastsSaved; i++) {
                    socks[i].leaveGroup(InetAddress.getByName(sMCGroup[i]));
                    socks[i].close();
                }
                Log.i(ATAG, "Closing sockets");
            } catch (Exception e) {
                Log.w(ATAG, "While closing Sockets: " + e.toString());
                return null;
            }
            return null;
        }

        private void log_lostpackets(int iStartSeq, int iEndSeq) {
            int[] seqs = new int[2];
            seqs[0] = iStartSeq;
            seqs[1] = iEndSeq;
            try {
                theLogQueue.put(seqs);
            } catch (InterruptedException e) {
                Log.w(ATAG, "Queueing lost packets info Interrupted: "+e.toString());
            }
        }

        private void log_stop(int i) {
            log_lostpackets(LogTaskQ.STOP_STARTSEQ, LogTaskQ.STOP_ENDSEQ);
            myDH.LogStrLn("SUMMARY:SeqNum:"+iSeqNum[i]);
            myDH.LogStrLn("SUMMARY:PktCnt:"+iPktCnt[i]);
            myDH.LogStrLn("SUMMARY:DisjointPktCnt:"+iDisjointPktCnt[i]);
            myDH.LogStrLn("SUMMARY:OlderSeqs:"+iOlderSeqs[i]);
        }

        private void data_stop() {
            myDH.Write2DataBuf(0,null);
        }

        private void update_ui() {
            for (int i = 1; i < iNumMCastsSaved; i++) {
                try {
                    TextView tvTempName = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvName));
                    TextView tvTempDelay = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvDelay));
                    TextView tvSeqNum = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvSeqNum));
                    TextView tvPktCnt = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvPktCnt));
                    TextView tvDisjointSeqs = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvDisjointSeqs));
                    TextView tvDisjointPktCnt = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvDisjointPktCnt));
                    TextView tvOlderSeqs = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvOlderSeqs));
                    int tColor = 0x00808080;
                    if (iMCDelay[i] > iMCRedDelay[i]) {
                        tColor = 0x80C00000;
                    } else {
                        if (iMCDelay[i] > ((iMCRedDelay[i]*66)/100)) {
                            tColor = 0x80C0C000;
                        }
                    }
                    tvTempName.setBackgroundColor(tColor);
                    tvTempDelay.setText(Integer.toString(iMCDelay[i]));
                    tvSeqNum.setText(Integer.toString(iSeqNum[i]));
                    tvPktCnt.setText(Integer.toString(iPktCnt[i]));
                    tvDisjointSeqs.setText(Integer.toString(iDisjointSeqs[i]));
                    tvDisjointPktCnt.setText(Integer.toString(iDisjointPktCnt[i]));
                    tvOlderSeqs.setText(Integer.toString(iOlderSeqs[i]));
                } catch (Exception e) {
                    Log.w(ATAG, "async-update_ui: "+e.toString());
                    Toast.makeText(getApplicationContext(), "WARN: async-update_ui: "+e.toString(), Toast.LENGTH_LONG).show();
                }
            }
        }

        @Override
        protected void onProgressUpdate(Void... values) {
            update_ui();
        }

        @Override
        protected void onCancelled(Void aVoid) {
            Log.i(ATAG, "AsyncTask onCancelled, MonLogic successfully stopped!!!");
            log_stop(CHANNEL2SAVE);
            data_stop();
            update_ui();
            Toast.makeText(getApplicationContext(),"MonLogic successfully stopped", Toast.LENGTH_SHORT).show();
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            Log.w(ATAG, "AsyncTask onPostExecute, MonLogic failure???");
            log_stop(CHANNEL2SAVE);
            data_stop();
            update_ui();
            Toast.makeText(getApplicationContext(),"MonLogic failure???", Toast.LENGTH_SHORT).show();
        }
    }

    MCastMonitor taskMon = null;

    private void getNwInterfaces() {

        String sNIS = null;
        try {
            Enumeration nis = NetworkInterface.getNetworkInterfaces();
            while (nis.hasMoreElements()) {
                NetworkInterface ni = (NetworkInterface) nis.nextElement();
                String curName = (ni.getName()+"; ");
                if (sNIS == null) {
                    sNIS = curName;
                } else {
                    sNIS += curName;
                }
            }
        } catch (SocketException e) {
            Log.w(ATAG, "getNwInterfaces: " + e.toString());
        }
        etNwInterface.setText(sNIS);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        try {
            sExternalBasePath = getExternalFilesDir(null).getCanonicalPath();
            Log.i(ATAG, "AppSpecificExternalDir: " + sExternalBasePath);
        } catch (IOException e) {
            Log.e(ATAG, "No AppSpecificExternalDir or ???: " + e.toString());
        }

        myDH = new DataHandler(sExternalBasePath + File.separatorChar + "data.bin",
                sExternalBasePath + File.separatorChar + "lost.log");
        myDH.dataSize = 1024; // todo: hardcoded for now
        Log.i(ATAG, "DataHandler is setup");

        etNwInterface = findViewById(R.id.etNwInt);
        getNwInterfaces();
        etMCastName = findViewById(R.id.etMCastName);
        etMCastGroup = findViewById(R.id.etMCastGroup);
        etMCastPort = findViewById(R.id.etMCastPort);
        etMCastRedDelay = findViewById(R.id.etMCastRedDelay);
        etMCastSeqOffset = findViewById(R.id.etMCastSeqOffset);
        lvMCasts = findViewById(R.id.lvMCasts);
        lvMCasts.setAdapter(adapterLVMCasts);
        btnMCastAdd = findViewById(R.id.btnMCastAdd);

        iNumMCasts = 1; // 0th location of MCasts lv is to be treated as header and skipped, so to help with same this is done

        btnMCastAdd.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    sMCName[iNumMCasts] = etMCastName.getText().toString();
                    sMCGroup[iNumMCasts] = etMCastGroup.getText().toString();
                    iMCPort[iNumMCasts] = Integer.parseInt(etMCastPort.getText().toString());
                    iMCRedDelay[iNumMCasts] = Integer.parseInt(etMCastRedDelay.getText().toString());
                    iMCSeqOffset[iNumMCasts] = Integer.parseInt(etMCastSeqOffset.getText().toString());
                    iNumMCasts += 1;
                } catch (Exception e) {
                    Log.w(ATAG, "MCastAdd: Check Values are fine: " + e.toString());
                    Toast.makeText(getApplicationContext(), "Error: Check Values are fine", Toast.LENGTH_LONG).show();
                }
                Toast.makeText(getApplicationContext(),"NumOfItems="+iNumMCasts, Toast.LENGTH_SHORT).show();
                lvMCasts.requestLayout();
            }
        });

        btnStartMon = findViewById(R.id.btnStartMon);
        btnStartMon.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                sNwInterface = etNwInterface.getText().toString();
                if (taskMon == null) {
                    myDH.CloseFiles();
                    try {
                        myDH.OpenFiles();
                    } catch (IOException e) {
                        Toast.makeText(getApplicationContext(), "StartMon: DataHandler OpenFiles FAILED", Toast.LENGTH_LONG).show();
                    }
                    taskMon = new MCastMonitor();
                    taskMon.execute();
                    Log.i(ATAG, "StartMon");
                    Toast.makeText(getApplicationContext(), "StartMon: "+iNumMCasts, Toast.LENGTH_SHORT).show();
                    btnStartMon.setText("StopMon");
                } else {
                    taskMon.cancel(true);
                    taskMon = null;
                    Log.i(ATAG, "StopMon");
                    Toast.makeText(getApplicationContext(), "StopMon", Toast.LENGTH_SHORT).show();
                    btnStartMon.setText("StartMon");
                }
            }
        });
    }

    @Override
    protected void onDestroy() {
        myDH.CloseFiles();
        super.onDestroy();
    }
}
