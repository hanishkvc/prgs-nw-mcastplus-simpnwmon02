package india.hanishkvc.simpnwmon02;

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

public class MainActivity extends AppCompatActivity {

    EditText etMCastName;
    EditText etMCastGroup;
    EditText etMCastPort;
    ListView lvMCasts;
    Button btnMCastAdd;
    Button btnStartMon;

    int iNumMCasts = 0;
    String[] sMCName = new String[10];
    String[] sMCGroup = new String[10];
    int[] iMCPort = new int[10];
    int[] iMCDelay = new int[10];

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
            ((TextView) convertView.findViewById(R.id.tvInfo)).setText(sMCGroup[position]+":"+iMCPort[position]);
            return convertView;
        }
    }

    AdapterLVMCasts adapterLVMCasts = new AdapterLVMCasts();

    private class MCastMonitor extends AsyncTask<Void, Void, Void> {
        MulticastSocket[] socks = new MulticastSocket[10];
        @Override
        protected Void doInBackground(Void... voids) {
            try {
                for(int i = 0; i < iNumMCasts; i++) {
                    socks[i] = new MulticastSocket(iMCPort[i]);
                    socks[i].joinGroup(InetAddress.getByName(sMCGroup[i]));
                    socks[i].setSoTimeout(50);
                    iMCDelay[i] = 0;
                }
            } catch (Exception e) {
                return null;
            }
            byte buf[] = new byte[1600];
            DatagramPacket pkt = new DatagramPacket(buf, buf.length);
            while (!isCancelled()) {
                for(int i = 0; i < iNumMCasts; i++) {
                    try {
                        socks[i].receive(pkt);
                        iMCDelay[i] = 0;
                    } catch (SocketTimeoutException e) {
                        iMCDelay[i] += 1;
                    } catch (IOException e) {
                        iMCDelay[i] += 1;
                    }
                }
                publishProgress();
            }
            return null;
        }

        @Override
        protected void onProgressUpdate(Void... values) {
            for (int i = 0; i < iNumMCasts; i++) {
                TextView tvTemp = ((TextView)lvMCasts.getChildAt(0).findViewById(R.id.tvName));
                int tColor = 0x00800080;
                if (iMCDelay[i] > 100) {
                    tColor = 0x80000080;
                }
                tvTemp.setBackgroundColor(tColor);
            }
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
        lvMCasts = findViewById(R.id.lvMCasts);
        lvMCasts.setAdapter(adapterLVMCasts);
        btnMCastAdd = findViewById(R.id.btnMCastAdd);

        btnMCastAdd.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                sMCName[iNumMCasts] = etMCastName.getText().toString();
                sMCGroup[iNumMCasts] = etMCastGroup.getText().toString();
                iMCPort[iNumMCasts] = Integer.getInteger(etMCastPort.getText().toString());
                iNumMCasts += 1;
                Toast.makeText(getApplicationContext(),"NumOfItems="+iNumMCasts, Toast.LENGTH_SHORT).show();
            }
        });

        btnStartMon = findViewById(R.id.btnStartMon);
        btnStartMon.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (taskMon == null) {
                    taskMon = new MCastMonitor();
                    taskMon.execute();
                    Toast.makeText(getApplicationContext(), "StartMon", Toast.LENGTH_SHORT).show();
                } else {
                    taskMon.cancel(true);
                    Toast.makeText(getApplicationContext(), "StopMon", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }
}
