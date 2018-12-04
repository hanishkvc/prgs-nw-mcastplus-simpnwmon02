package india.hanishkvc.simpnwmon02;

/*
    Simple Network Monitor 02
    v20181202IST0409
    HanishKVC, GPL, 2018
 */


import android.os.AsyncTask;
import android.provider.ContactsContract;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import org.w3c.dom.Text;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class MainActivity extends AppCompatActivity {

    private static final int MCASTTIMEOUT = 50;
    private static final int MAXMCASTS = 10;
    EditText etMCastName;
    EditText etMCastGroup;
    EditText etMCastPort;
    EditText etMCastRedDelay;
    EditText etMCastSeqOffset;
    ListView lvMCasts;
    Button btnMCastAdd;
    Button btnStartMon;

    int iNumMCasts = 0;
    String[] sMCName = new String[MAXMCASTS];
    String[] sMCGroup = new String[MAXMCASTS];
    int[] iMCPort = new int[MAXMCASTS];
    int[] iMCDelay = new int[MAXMCASTS];
    int[] iMCRedDelay = new int[MAXMCASTS];
    int[] iMCSeqOffset = new int[MAXMCASTS];

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
            ((TextView) convertView.findViewById(R.id.tvName)).setText(sMCName[position]);
            ((TextView) convertView.findViewById(R.id.tvDelay)).setText("0");
            ((TextView) convertView.findViewById(R.id.tvSeqNum)).setText("0");
            ((TextView) convertView.findViewById(R.id.tvDisjointSeqs)).setText("0");
            ((TextView) convertView.findViewById(R.id.tvOlderSeqs)).setText("0");
            ((TextView) convertView.findViewById(R.id.tvInfo)).setText(sMCGroup[position]+" : "+iMCPort[position]+" , RD: "+iMCRedDelay[position]+" , SeqO: "+iMCSeqOffset[position]);
            return convertView;
        }
    }

    AdapterLVMCasts adapterLVMCasts = new AdapterLVMCasts();

    private class MCastMonitor extends AsyncTask<Void, Void, Void> {

        int iNumMCastsSaved = iNumMCasts;
        MulticastSocket[] socks = new MulticastSocket[MAXMCASTS];
        int[] iSeqNum = new int[MAXMCASTS];
        int[] iDisjointSeqs = new int[MAXMCASTS];
        int[] iOlderSeqs = new int[MAXMCASTS];

        @Override
        protected Void doInBackground(Void... voids) {
            int iRunCnt = 0;
            try {
                for(int i = 0; i < iNumMCastsSaved; i++) {
                    socks[i] = new MulticastSocket(iMCPort[i]);
                    socks[i].joinGroup(InetAddress.getByName(sMCGroup[i]));
                    socks[i].setSoTimeout(MCASTTIMEOUT);
                    iMCDelay[i] = 0;
                    iDisjointSeqs[i] = 0;
                    iOlderSeqs[i] = 0;
                }
            } catch (Exception e) {
                return null;
            }
            byte buf[] = new byte[1600];
            DatagramPacket pkt = new DatagramPacket(buf, buf.length);
            while (!isCancelled()) {
                for(int i = 0; i < iNumMCastsSaved; i++) {
                    try {
                        //TODO: Clear the old content from pkt, as the same pkt is used across channels
                        // as well as across packets of the same channel.
                        socks[i].receive(pkt);
                        iMCDelay[i] = 0;
                        try {
                            ByteBuffer bb = ByteBuffer.wrap(pkt.getData(), iMCSeqOffset[i], 4);
                            bb.order(ByteOrder.LITTLE_ENDIAN);
                            int curSeq = bb.getInt();
                            if (iSeqNum[i] >= curSeq) {
                                iOlderSeqs[i] += 1;
                            } else {
                                if ((curSeq-iSeqNum[i]) != 1) {
                                    iDisjointSeqs[i] += 1;
                                }
                                iSeqNum[i] = curSeq;
                            }
                        } catch (Exception e) {
                            iSeqNum[i] = 987654321;
                        }
                    } catch (SocketTimeoutException e) {
                        iMCDelay[i] += 1;
                    } catch (IOException e) {
                        iMCDelay[i] += 1;
                        break;
                    }
                }
                iRunCnt += 1;
                if (iNumMCasts == 1) {
                    if ((iRunCnt%10) == 0) {
                        publishProgress();
                    }
                } else {
                    publishProgress();
                }
            }

            try {
                for (int i = 0; i < iNumMCastsSaved; i++) {
                    socks[i].leaveGroup(InetAddress.getByName(sMCGroup[i]));
                    socks[i].close();
                }
            } catch (Exception e) {
                return null;
            }
            return null;
        }

        @Override
        protected void onProgressUpdate(Void... values) {
            for (int i = 0; i < iNumMCastsSaved; i++) {
                try {
                    TextView tvTempName = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvName));
                    TextView tvTempDelay = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvDelay));
                    TextView tvSeqNum = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvSeqNum));
                    TextView tvDisjointSeqs = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvDisjointSeqs));
                    TextView tvOlderSeqs = ((TextView)lvMCasts.getChildAt(i).findViewById(R.id.tvOlderSeqs));
                    int tColor = 0x00808080;
                    if (iMCDelay[i] > iMCRedDelay[i]) {
                        tColor = 0x80800000;
                    }
                    tvTempName.setBackgroundColor(tColor);
                    tvTempDelay.setText(Integer.toString(iMCDelay[i]));
                    tvSeqNum.setText(Integer.toString(iSeqNum[i]));
                    tvDisjointSeqs.setText(Integer.toString(iDisjointSeqs[i]));
                    tvOlderSeqs.setText(Integer.toString(iOlderSeqs[i]));
                } catch (Exception e) {
                    Toast.makeText(getApplicationContext(), "WARN: onProgressUpdate: "+e.getMessage(), Toast.LENGTH_LONG).show();
                }
            }
        }

        @Override
        protected void onCancelled(Void aVoid) {
            Toast.makeText(getApplicationContext(),"MonLogic successfully stopped", Toast.LENGTH_SHORT).show();
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            Toast.makeText(getApplicationContext(),"MonLogic failure???", Toast.LENGTH_SHORT).show();
        }
    }

    MCastMonitor taskMon = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        etMCastName = findViewById(R.id.etMCastName);
        etMCastGroup = findViewById(R.id.etMCastGroup);
        etMCastPort = findViewById(R.id.etMCastPort);
        etMCastRedDelay = findViewById(R.id.etMCastRedDelay);
        etMCastSeqOffset = findViewById(R.id.etMCastSeqOffset);
        lvMCasts = findViewById(R.id.lvMCasts);
        lvMCasts.setAdapter(adapterLVMCasts);
        btnMCastAdd = findViewById(R.id.btnMCastAdd);

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
                if (taskMon == null) {
                    taskMon = new MCastMonitor();
                    taskMon.execute();
                    Toast.makeText(getApplicationContext(), "StartMon: "+iNumMCasts, Toast.LENGTH_SHORT).show();
                } else {
                    taskMon.cancel(true);
                    taskMon = null;
                    Toast.makeText(getApplicationContext(), "StopMon", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }
}
