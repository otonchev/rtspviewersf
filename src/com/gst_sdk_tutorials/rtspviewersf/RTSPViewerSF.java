/*
 * Copyright (C) 2014 Ognyan Tonchev <otonchev at gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
package com.gst_sdk_tutorials.rtspviewersf;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.PowerManager;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;
import android.widget.Toast;
import android.app.AlertDialog;
import android.widget.EditText;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.view.*;

import com.gst_sdk_tutorials.rtspviewersf.R;
import org.freedesktop.gstreamer.GStreamer;

public class RTSPViewerSF extends Activity implements SurfaceHolder.Callback, OnSeekBarChangeListener {

    private static final int numPlayers = 2;

    /* default for Axis cameras */
    private static final String defaultMediaUri = "rtsp://192.168.0.90/axis-media/media.amp";
    private static final String defaultMediaUser = "root";
    private static final String defaultMediaPass = "pass";

    private static final String mediaRTSPUriFormat = "rtsp[t|h]://IP/path[?options]";

    private native long nativePlayerCreate();        // Initialize native code, build pipeline, etc
    private native void nativePlayerFinalize(long data);   // Destroy pipeline and shutdown native code
    private native void nativeSetUri(long data, String uri, String user, String pass); // Set the URI of the media to play
    private native void nativePlay(long data);       // Set pipeline to PLAYING
    private native void nativeSetPosition(long data, int milliseconds); // Seek to the indicated position, in milliseconds
    private native void nativePause(long data);      // Set pipeline to PAUSED
    private native void nativeReady(long data);      // Set pipeline to READY
    private static native boolean nativeLayerInit(); // Initialize native class: cache Method IDs for callbacks
    private native void nativeSurfaceInit(long data, Object surface); // A new surface is available
    private native void nativeSurfaceFinalize(long data); // Surface about to be destroyed

    private long native_custom_data[];      // Native code will store the player here

    private boolean is_playing_desired[];   // Whether the user asked to go to PLAYING
    private int position[];                 // Current position, reported by native code
    private int duration[];                 // Current clip duration, reported by native code
    private int desired_position[];         // Position where the users wants to seek to
    private PlayerConfiguration playerConfigs[];              // URI of the clip being played
    private String state[];
    private boolean is_full_screen;

    private int active_player;

    private PowerManager.WakeLock wake_lock;

    private Boolean isOrientationLandscape() {
        return (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE);
    }
    
    public RTSPViewerSF () {
    	native_custom_data = new long[numPlayers];
    	playerConfigs = new PlayerConfiguration[numPlayers];
    	is_playing_desired = new boolean[numPlayers];
    	position = new int[numPlayers];
    	duration = new int[numPlayers];
    	desired_position = new int[numPlayers];
    	state = new String[numPlayers];

    	for (int i = 0; i < numPlayers; i++) {
    	    playerConfigs[i] = new PlayerConfiguration();
    	    playerConfigs[i].setUri(defaultMediaUri);
    	    is_playing_desired[i] = false;
    	    position[i] = 0;
    	    duration[i] = 0;
    	    desired_position[i] = 0;

            Log.i ("Constructed", "  playing:" + is_playing_desired[i] + " position:" + position[i] +
                   " duration: " + duration[i] + " uri: " + playerConfigs[i].getUri());
    	}
    	
    	if (!nativeLayerInit())
    	    throw new RuntimeException("Failed to initialize Native layer(not all necessary interface methods implemeted?)");
    }
    
    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        // Initialize GStreamer and warn if it fails
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish(); 
            return;
        }

        setContentView(R.layout.main);

        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wake_lock = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK, "RTSP Viewer");
        wake_lock.setReferenceCounted(false);

        ImageButton play = (ImageButton) this.findViewById(R.id.button_play);
        play.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                is_playing_desired[active_player] = true;
                wake_lock.acquire();
                nativePlay(native_custom_data[active_player]);
            }
        });

        ImageButton pause = (ImageButton) this.findViewById(R.id.button_pause);
        pause.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                is_playing_desired[active_player] = false;
                wake_lock.release();
                nativePause(native_custom_data[active_player]);
            }
        });
        
        ImageButton stop = (ImageButton) this.findViewById(R.id.button_stop);
        stop.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                is_playing_desired[active_player] = false;
                wake_lock.release();
                nativeReady(native_custom_data[active_player]);
            }
        });

        ImageButton select = (ImageButton) this.findViewById(R.id.button_select);
        select.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	if (++active_player == numPlayers)
            	    active_player = 0;
            		
            	for (int i = 0; i < numPlayers; i++) {
            	    setState(i);
            	}
	        if (!isOrientationLandscape()) {
	            for (int i = 0; i < numPlayers; i++) {
                        SurfaceView sv = findSurfaceViewByPlayerId(i);
                        TextView tv = findTextViewByPlayerId(i);
                        SeekBar sb = findSeekBarByPlayerId (i);
                        TextView st = findSeekTextViewByPlayerId(i);

                        if (sv == null || tv == null || sb == null || st == null)
                            break;

                        if (i == active_player) {
                            sv.setVisibility(View.VISIBLE);
                            tv.setVisibility(View.VISIBLE);
                            sb.setVisibility(View.VISIBLE);
                            st.setVisibility(View.VISIBLE);
                        } else {
                            sv.setVisibility(View.INVISIBLE);
                            tv.setVisibility(View.INVISIBLE);
                            sb.setVisibility(View.INVISIBLE);
                            st.setVisibility(View.INVISIBLE);
                        }
                    }
                }
            	
            	Toast.makeText(RTSPViewerSF.this, getHumanizedPlayerId(active_player), Toast.LENGTH_SHORT).show();
            }
        });
        
        ImageButton list = (ImageButton) this.findViewById(R.id.button_list);
        list.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Intent intent = new Intent(getApplicationContext(), ConfigurationManager.class);
                startActivityForResult(intent, 100);
            }
        });
        
        ImageButton fullscreen = (ImageButton) this.findViewById(R.id.button_full);
        fullscreen.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                setFullscreen();
                is_full_screen = true;
            }
        });

        //configure our surface views
        for (int i = 0; i < numPlayers; i++) {
            SurfaceView sv = findSurfaceViewByPlayerId(i);
            TextView tv = findTextViewByPlayerId(i);
            SeekBar sb = findSeekBarByPlayerId (i);
            TextView st = findSeekTextViewByPlayerId (i);
            SurfaceHolder sh;
            
            if (sv == null || tv == null || sb == null || st == null)
                break;
        	
            if (i != active_player && !isOrientationLandscape()) {
            	sv.setVisibility(View.INVISIBLE);
                tv.setVisibility(View.INVISIBLE);
                sb.setVisibility(View.INVISIBLE);
                st.setVisibility(View.INVISIBLE);
            }
        	
            sb.setOnSeekBarChangeListener(this);

            sh = sv.getHolder();
            sh.addCallback(this);

            sv.setOnClickListener(new OnClickListener() {
                public void onClick(View v) {
                    if (is_full_screen) {
                        unsetFullscreen();
                        is_full_screen = false;
                    } else
                        startUriAlertDialog();
                }
            });
        }

        // Retrieve our previous state, or initialize it to default values
        if (savedInstanceState != null) {
            for (int i = 0; i < numPlayers; i++) {
            	position[i] = savedInstanceState.getInt("position" + i);
            	duration[i] = savedInstanceState.getInt("duration" + i);
            	is_playing_desired[i] = savedInstanceState.getBoolean("playing" + i);
            	playerConfigs[i].setUri(savedInstanceState.getString("mediaUri" + i));
                playerConfigs[i].setUser(savedInstanceState.getString("mediaUser" + i));
                playerConfigs[i].setPass(savedInstanceState.getString("mediaPass" + i));
                playerConfigs[i].setName(savedInstanceState.getString("mediaName" + i));
                
                Log.d ("GStreamer", "  playing:" + is_playing_desired[i] + " position:" + position[i] +
                        " duration: " + duration[i] + " uri: " + playerConfigs[i].getUri());
            }

            Log.i ("GStreamer", "Activity created with saved state:");
        } else {            
            SharedPreferences sharedPreferences = getPreferences(Context.MODE_PRIVATE);
            
            for (int i=0; i < numPlayers; i++) {
                String uri;
                String name;
                String user;
                String pass;
                
                uri = sharedPreferences.getString("uri" + i, null);
                name = sharedPreferences.getString("name" + i, null);
                user = sharedPreferences.getString("user" + i, null);
                pass = sharedPreferences.getString("pass" + i, null);
                
                if (uri != null) {
                    playerConfigs[i].setUri(uri);
                    playerConfigs[i].setName(name);
                    playerConfigs[i].setUser(user);
                    playerConfigs[i].setPass(pass);
		    Log.d("GStreamer", "Retrieving configuration from shared preferences: " +
                            playerConfigs[i].getUri());
                }
            }
            
            Log.i ("GStreamer", "Activity created with no saved state:");
        }

        // Start with disabled buttons, until native code is initialized
        this.findViewById(R.id.button_play).setEnabled(false);
        this.findViewById(R.id.button_pause).setEnabled(false);
        this.findViewById(R.id.button_stop).setEnabled(false);
        
        for (int i = 0; i < numPlayers; i++) {
            native_custom_data[i] = nativePlayerCreate ();
        }
    }
    
    private int findPlayerIdByPlayerData (long data) {
    	for (int i = 0; i < numPlayers; i++)
    	    if (native_custom_data[i] == data)
                return i;
    	return -1;
    }
        
    private TextView findTextViewByPlayerId(int playerid) {
    	String textviewID = "textview_protocol_" + playerid;
    	int resID = getResources().getIdentifier(textviewID, "id", getPackageName());
    	TextView tv = (TextView) RTSPViewerSF.this.findViewById(resID);
    	return tv;
    }
    
    private SurfaceView findSurfaceViewByPlayerId(int playerid) {
    	String surfaceviewID = "surface_video_" + playerid;
    	int resID = getResources().getIdentifier(surfaceviewID, "id", getPackageName());
    	SurfaceView sv = (SurfaceView) RTSPViewerSF.this.findViewById(resID);
    	return sv;
    }
    
    private SeekBar findSeekBarByPlayerId(int playerid) {
    	String seekbarID = "seek_bar_" + playerid;
    	int resID = getResources().getIdentifier(seekbarID, "id", getPackageName());
    	SeekBar sb = (SeekBar) RTSPViewerSF.this.findViewById(resID);
    	return sb;
    }
    
    private int findPlayerIdBySeekBar(SeekBar sb) {
    	for (int i = 0; i < numPlayers; i++) {
    	    String seekbarID = "seek_bar_" + i;
            int resID = getResources().getIdentifier(seekbarID, "id", getPackageName());
    	    SeekBar tmp = (SeekBar) RTSPViewerSF.this.findViewById(resID);
    	    if (sb == tmp)
    	        return i;
    	}
    	return 0;
    }
    
    private TextView findSeekTextViewByPlayerId(int playerid) {
    	String textviewID = "textview_time_" + playerid;
    	int resID = getResources().getIdentifier(textviewID, "id", getPackageName());
    	TextView tv = (TextView) RTSPViewerSF.this.findViewById(resID);
    	return tv;
    }
    
    private String getHumanizedPlayerId(int player_id) {
    	String name;
    	
    	name = playerConfigs[player_id].getName();
        if (name == null)
            name = "Player " + player_id;
        
        return name;
    }
    
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode == RESULT_CANCELED) {
            return;
        }
        switch (requestCode) {
            case 100: {
                playerConfigs[active_player] = (PlayerConfiguration) data.getSerializableExtra("config");
                Log.d ("GStreamer", "Got new configuration: " + playerConfigs[active_player].getUri());
                setMediaUri(active_player, playerConfigs[active_player]);
                Toast.makeText(RTSPViewerSF.this, "Selected URI for Player " + active_player, Toast.LENGTH_SHORT).show();
                setState(active_player);
                break;
            }
        }
    }

    protected void onSaveInstanceState (Bundle outState) {
        for (int i = 0; i < numPlayers; i++) {
            outState.putInt("position" + i, position[i]);
            outState.putInt("duration" + i, duration[i]);
            outState.putBoolean("playing" + i, is_playing_desired[i]);
            outState.putString("mediaUri" + i, playerConfigs[i].getUri());
            outState.putString("mediaUser" + i, playerConfigs[i].getUser());
            outState.putString("mediaPass" + i, playerConfigs[i].getPass());
            outState.putString("mediaName" + i, playerConfigs[i].getName());
            
            Log.d ("GStreamer", "Saving state, playing:" + is_playing_desired[i] + " position:" + position[i] +
                    " duration: " + duration[i] + " uri: " + playerConfigs[i].getUri());
        }
    }

    protected void onDestroy() {
    	
        SharedPreferences sharedPreferences = getPreferences(Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        
        for (int i=0; i < numPlayers; i++) {
            editor.putString("name" + i, playerConfigs[i].getName());
            editor.putString("uri" + i, playerConfigs[i].getUri());
            editor.putString("user" + i, playerConfigs[i].getUser());
            editor.putString("pass" + i, playerConfigs[i].getPass());
            Log.d("GStreamer", "Storing configuration to shared preferences: " + playerConfigs[i].getUri());
        }
        editor.commit();
        
    	for (int i = 0; i < numPlayers; i++) {
	    nativePlayerFinalize(native_custom_data[i]);
    	    native_custom_data[i] = 0x0;
    	}
        if (wake_lock.isHeld())
            wake_lock.release();
        super.onDestroy();
    }
    
    private String getPlayerTitle (int player_id) {
        final String protocol;
        final String player;
        final String uri = playerConfigs[player_id].getUri();
        final String message;
        
        if (uri.startsWith("rtsp://")) {
            protocol = "RTP/UDP";
        } else if (uri.startsWith("rtspt://")) {
            protocol = "RTP/RTSP";
        } else if (uri.startsWith("rtsph://")) {
            protocol = "RTP/RTSP/HTTP";
        } else {
            protocol = "";
        }
        
        player = getHumanizedPlayerId(player_id);
        
        if (active_player == player_id)
            message = state[player_id] + " " + protocol + " " + player + " *";
        else
            message = state[player_id] + " " + protocol + " " + player;
        
        return message;
    }

    private void setState (final int player_id) {
        final TextView tv;
        
        tv = findTextViewByPlayerId(player_id);
        
        tv.setText(getPlayerTitle (player_id));
    }
    
    // Called from native code.
    private void nativeStateChanged(long data, final String state) {
        final TextView tv;
        final String message;
        int player_id = findPlayerIdByPlayerData(data);
        
        this.state[player_id] = state;
        
        tv = findTextViewByPlayerId(player_id);

        message = getPlayerTitle (player_id);
        
        runOnUiThread (new Runnable() {
          public void run() {
            tv.setText(message);
          }
        });
    }
    
    // Called from native code.
    private void nativeErrorOccured(long data, String message) {
    	final String ui_message;
    	int player_id = findPlayerIdByPlayerData(data);
    	
    	ui_message = "Player " + player_id + ":" + message;
    	
        runOnUiThread (new Runnable() {
            public void run() {
                Toast.makeText(RTSPViewerSF.this, ui_message, Toast.LENGTH_SHORT).show();
            }
        });
    }

    // Set the URI to play, and record whether it is a local or remote file
    private void setMediaUri(int player, PlayerConfiguration conf) {
        nativeSetUri (native_custom_data[player], conf.getUri(), conf.getUser(), conf.getPass());
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void nativeGStreamerInitialized (long data) {
    	int player_id = findPlayerIdByPlayerData (data);
    	
        Log.i ("GStreamer", "GStreamer initialized:");
        Log.i ("GStreamer", "  playing:" + is_playing_desired[player_id] + " position:" + position[player_id] +
        		" uri: " + playerConfigs[player_id].getUri() + " player id: " + player_id);

        // Restore previous playing state
        setMediaUri (player_id, playerConfigs[player_id]);
        nativeSetPosition (native_custom_data[player_id], position[player_id]);
        if (is_playing_desired[player_id]) {
            nativePlay(native_custom_data[player_id]);
            wake_lock.acquire();
        } else {
            wake_lock.release();
        }

        // Re-enable buttons, now that GStreamer is initialized
        final Activity activity = this;
        runOnUiThread(new Runnable() {
            public void run() {
                activity.findViewById(R.id.button_play).setEnabled(true);
                //FIXME: pause does not work very well
                //activity.findViewById(R.id.button_pause).setEnabled(true);
                activity.findViewById(R.id.button_stop).setEnabled(true);
            }
        });
    }

    // The text widget acts as an slave for the seek bar, so it reflects what the seek bar shows, whether
    // it is an actual pipeline position or the position the user is currently dragging to.
    private void updateTimeWidget (int player_id) {
        TextView tv = findSeekTextViewByPlayerId (player_id);
        SeekBar sb = findSeekBarByPlayerId (player_id);
        int pos = sb.getProgress();

        SimpleDateFormat df = new SimpleDateFormat("HH:mm:ss");
        df.setTimeZone(TimeZone.getTimeZone("UTC"));
        String message = df.format(new Date (pos)) + " / " + df.format(new Date (duration[player_id]));
        tv.setText(message);
    }

    // Called from native code
    private void nativePositionUpdated(long data, final int position, final int duration) {
        final int player_id = findPlayerIdByPlayerData (data);
        final SeekBar sb = findSeekBarByPlayerId (player_id);

        // Ignore position messages from the pipeline if the seek bar is being dragged
        if (sb.isPressed()) return;

        runOnUiThread (new Runnable() {
          public void run() {
            sb.setMax(duration);
            sb.setProgress(position);
            updateTimeWidget(player_id);
            sb.setEnabled(duration != 0);
          }
        });
        this.position[player_id] = position;
        this.duration[player_id] = duration;
    }

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("mediaplayer");
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width,
            int height) {
        Log.d("GStreamer", "Surface changed to format " + format + " width "
                + width + " height " + height);
        
        for (int i = 0; i < numPlayers; i++) {
            String surfaceID = "surface_video_" + i;
            int resID = getResources().getIdentifier(surfaceID, "id", getPackageName());
            SurfaceView sv = ((SurfaceView) this.findViewById(resID));
            SurfaceHolder sh;

            if (sv == null)
                break;

            sh = sv.getHolder();
            if (sh == holder) {
                nativeSurfaceInit (native_custom_data[i], holder.getSurface());
            }
        }
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface created: " + holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface destroyed");
        
        for (int i = 0; i < numPlayers; i++) {
            String surfaceID = "surface_video_" + i;
            int resID = getResources().getIdentifier(surfaceID, "id", getPackageName());
            SurfaceView sv = ((SurfaceView) this.findViewById(resID));
            SurfaceHolder sh;

            if (sv == null)
                break;

            sh = sv.getHolder();
            if (sh == holder) {
                nativeSurfaceFinalize (native_custom_data[i]);
            }
        }
    }

    // Called from native code when the size of the media changes or is first detected.
    // Inform the video surface about the new size and recalculate the layout.
    private void nativeMediaSizeChanged (long data, int width, int height) {
    	int player_id = findPlayerIdByPlayerData (data);
    	
        Log.i ("GStreamer", "Media size changed to " + width + "x" + height);
        
        final GStreamerSurfaceView gsv = (GStreamerSurfaceView) findSurfaceViewByPlayerId (player_id);
        gsv.media_width = width;
        gsv.media_height = height;
        runOnUiThread(new Runnable() {
            public void run() {
                gsv.requestLayout();
            }
        });
    }

    // The Seek Bar thumb has moved, either because the user dragged it or we have called setProgress()
    public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
        if (fromUser == false) return;
        int player_id = findPlayerIdBySeekBar (sb);
        desired_position[player_id] = progress;
        updateTimeWidget(player_id);
    }

    // The user started dragging the Seek Bar thumb
    public void onStartTrackingTouch(SeekBar sb) {
        nativePause(native_custom_data[active_player]);
    }

    // The user released the Seek Bar thumb
    public void onStopTrackingTouch(SeekBar sb) {
        nativeSetPosition(native_custom_data[active_player], desired_position[active_player]);
        if (is_playing_desired[active_player])
            nativePlay(native_custom_data[active_player]);
    }
    
    private void setFullscreen()
    {
        ImageButton play = (ImageButton) this.findViewById(R.id.button_play);
        ImageButton pause = (ImageButton) this.findViewById(R.id.button_pause);
        ImageButton stop = (ImageButton) this.findViewById(R.id.button_stop);
        ImageButton select = (ImageButton) this.findViewById(R.id.button_select);
        ImageButton list = (ImageButton) this.findViewById(R.id.button_list);
        ImageButton fullscreen = (ImageButton) this.findViewById(R.id.button_full);
        
        play.setVisibility(View.INVISIBLE);
        pause.setVisibility(View.INVISIBLE);
        stop.setVisibility(View.INVISIBLE);
        select.setVisibility(View.INVISIBLE);
        list.setVisibility(View.INVISIBLE);
        fullscreen.setVisibility(View.INVISIBLE);
        
    	for (int i = 0; i < numPlayers; i++) {
    	    TextView tv = findTextViewByPlayerId(i);
    	    SeekBar sb = findSeekBarByPlayerId (i);
            TextView time = findSeekTextViewByPlayerId (i);
            
    	    if (tv == null || sb == null || time == null)
    	        break;
    	    tv.setVisibility(View.INVISIBLE);
    	    time.setVisibility(View.INVISIBLE);
    	    sb.setVisibility(View.INVISIBLE);
    	}
        
        this.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        this.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
    }
    
    private void unsetFullscreen()
    {
        ImageButton play = (ImageButton) this.findViewById(R.id.button_play);
        ImageButton pause = (ImageButton) this.findViewById(R.id.button_pause);
        ImageButton stop = (ImageButton) this.findViewById(R.id.button_stop);
        ImageButton select = (ImageButton) this.findViewById(R.id.button_select);
        ImageButton list = (ImageButton) this.findViewById(R.id.button_list);
        ImageButton fullscreen = (ImageButton) this.findViewById(R.id.button_full);
        
        play.setVisibility(View.VISIBLE);
        pause.setVisibility(View.VISIBLE);
        stop.setVisibility(View.VISIBLE);
        select.setVisibility(View.VISIBLE);
        list.setVisibility(View.VISIBLE);
        fullscreen.setVisibility(View.VISIBLE);
        
    	for (int i = 0; i < numPlayers; i++) {
    	    TextView tv = findTextViewByPlayerId(i);
    	    SeekBar sb = findSeekBarByPlayerId (i);
            TextView time = findSeekTextViewByPlayerId (i);
            
    	    if (tv == null || sb == null || time == null)
    	        break;

    	    if (active_player == i || isOrientationLandscape ()) {
    	        tv.setVisibility(View.VISIBLE);
    	        time.setVisibility(View.VISIBLE);
    	        sb.setVisibility(View.VISIBLE);
    	    }
    	}
        
        this.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        this.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
    }
    
    private void startUriAlertDialog()
    {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Enter URI");

        final EditText input_uri = new EditText(this);
        final EditText input_user = new EditText(this);
        final EditText input_pass = new EditText(this);
        
        LinearLayout linearLayout = new LinearLayout(this);
        linearLayout.setOrientation(LinearLayout.VERTICAL);
        linearLayout.addView(input_uri);
        linearLayout.addView(input_user);
        linearLayout.addView(input_pass);

        input_uri.setInputType(InputType.TYPE_CLASS_TEXT);
        input_uri.setText(playerConfigs[active_player].getUri());
        input_uri.setHint(mediaRTSPUriFormat);
        input_user.setInputType(InputType.TYPE_CLASS_TEXT);
        input_user.setText(playerConfigs[active_player].getUser());
        input_user.setHint("user");
        input_pass.setInputType(InputType.TYPE_TEXT_VARIATION_PASSWORD);
        input_pass.setTransformationMethod(PasswordTransformationMethod.getInstance());
        input_pass.setText(playerConfigs[active_player].getPass());
        input_pass.setHint("passwd");
        builder.setView(linearLayout);
        
        builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {

            @Override
            public void onClick(DialogInterface dialog, int which) {
                String uri = input_uri.getText().toString();
                String user = input_user.getText().toString();
                String pass = input_pass.getText().toString();
                if (!uri.isEmpty() && (uri.startsWith("rtsp://") ||
                    uri.startsWith("rtspt://") || uri.startsWith("rtsph://"))) {
                    playerConfigs[active_player].setUri(uri);
                    playerConfigs[active_player].setUser(user);
                    playerConfigs[active_player].setPass(pass);
                    position[active_player] = 0;

                    Log.d("GStreamer", "New configuration from alert dialog: " + playerConfigs[active_player].getUri());

                    setMediaUri(active_player, playerConfigs[active_player]);
                } else {
                    Toast.makeText(RTSPViewerSF.this, "Invalid URI, accepted format is " + mediaRTSPUriFormat,
                            Toast.LENGTH_SHORT).show();
                }
            }
        });
        builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {

            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.cancel();
            }
        });

        builder.show();
    }
}
