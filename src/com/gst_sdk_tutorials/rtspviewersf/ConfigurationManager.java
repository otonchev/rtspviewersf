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

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;

import java.util.ArrayList;
import java.util.HashMap;

import com.gst_sdk_tutorials.rtspviewersf.R;

import android.view.View.OnClickListener;

public class ConfigurationManager extends ListActivity {
    
    private ArrayList<HashMap<String, String>> listEntries = null;
    private AccessoriesAdapter mgrAdapter;
    
    static final String STATE_ENTRIES = "savedArray";
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.confmanager);
        
        if (listEntries == null) {
            if (savedInstanceState != null) {
                listEntries = (ArrayList<HashMap<String,String>>) savedInstanceState.getSerializable(STATE_ENTRIES);
            } else {
                listEntries = new ArrayList<HashMap<String, String>>();
        
                //home camera
                HashMap<String, String> item = new HashMap<String, String>();
                item.put("uri", "rtsp://192.168.0.90/axis-media/media.amp");
                item.put("name", "Camera " + 0);
                item.put("user", "root");
                item.put("pass", "pass");
                listEntries.add(item);
                
                //store camera
                item = new HashMap<String, String>();
                item.put("uri", "rtsph://94.190.242.153/axis-media/media.amp");
                item.put("name", "Camera " + 1);
                item.put("user", "root");
                item.put("pass", "pass");
                listEntries.add(item);
            }
        }
        
        mgrAdapter = new AccessoriesAdapter();
        setListAdapter(mgrAdapter);
        
        Button add = (Button) this.findViewById(R.id.mgrButtonNew);
        add.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                startAddAlertDialog(-1);
            }
        });
        
        Button cancel = (Button) this.findViewById(R.id.mgrButtonCancel);
        cancel.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                setResult(RESULT_CANCELED);
                finish();
            }
        });
    }
    
    @Override
    public void onPause() {
        SharedPreferences sharedPreferences = getPreferences(Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPreferences.edit();
        
        super.onPause();
        
        editor.putInt("entriesSize", listEntries.size());
        for (int i=0; i<listEntries.size(); i++) {
            HashMap<String, String> item = listEntries.get(i);
            editor.putString("uri" + i, item.get("uri"));
            editor.putString("name" + i, item.get("name"));
            editor.putString("user" + i, item.get("user"));
            editor.putString("pass" + i, item.get("pass"));
        }
        editor.commit();
    }

    @Override
    public void onResume() {
        int size;
        SharedPreferences sharedPreferences = getPreferences(Context.MODE_PRIVATE);
        
        super.onResume();

        size = sharedPreferences.getInt("entriesSize", 0);
        if (size == 0)
            return;
        
        listEntries = new ArrayList<HashMap<String, String>>();
        
        for (int i=0; i<size; i++) {
            String uri;
            String name;
            String user;
            String pass;
            HashMap<String, String> item = new HashMap<String, String>();
            
            uri = sharedPreferences.getString("uri"+i, null);
            name = sharedPreferences.getString("name"+i, null);
            user = sharedPreferences.getString("user"+i, null);
            pass = sharedPreferences.getString("pass"+i, null);
            
            item.put("uri", uri);
            item.put("name", name);
            item.put("user", user);
            item.put("pass", pass);
            listEntries.add(item);
        }
    }
    
    @Override
    protected void onSaveInstanceState(Bundle savedInstanceState) {
        super.onSaveInstanceState(savedInstanceState);
        savedInstanceState.putSerializable(STATE_ENTRIES, listEntries);
        super.onSaveInstanceState(savedInstanceState);
    }
    
    private static class AccessoriesViewHolder {
        public TextView content;
    }

    private class AccessoriesAdapter extends BaseAdapter {

        @Override
        public int getCount() {
            return listEntries.size();
        }

        @Override
        public String getItem(int position) {
            HashMap<String, String> item = listEntries.get(position);
            return item.get("name");
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            AccessoriesViewHolder holder;

            if (convertView == null) {
                convertView = getLayoutInflater().inflate(R.layout.confmanagerrow, parent, false);

                holder = new AccessoriesViewHolder();
                holder.content = (TextView) convertView.findViewById(R.id.mgrrowtext);

                ((ImageButton) convertView.findViewById(R.id.button_delete_entry)).setOnClickListener(mDeleteButtonClickListener);
                ((ImageButton) convertView.findViewById(R.id.button_edit_entry)).setOnClickListener(mEditButtonClickListener);
                ((TextView) convertView.findViewById(R.id.mgrrowtext)).setOnClickListener(mViewTextViewClickListener);
                
                convertView.setTag(holder);
            } else {
                holder = (AccessoriesViewHolder) convertView.getTag();
            }

            holder.content.setText(getItem(position));

            return convertView;
        }
        
        private OnClickListener mDeleteButtonClickListener = new OnClickListener() {
            @Override
            public void onClick(View v) {
                final int position = getListView().getPositionForView(v);
                listEntries.remove(position);
                mgrAdapter.notifyDataSetChanged();
            }
        };
        
        private OnClickListener mEditButtonClickListener = new OnClickListener() {
            @Override
            public void onClick(View v) {
                final int position = getListView().getPositionForView(v);
                startAddAlertDialog(position);
            }
        };
        
        private OnClickListener mViewTextViewClickListener = new OnClickListener() {
            @Override
            public void onClick(View v) {
                final int position = getListView().getPositionForView(v);
                HashMap<String, String> item = listEntries.get(position);
                Intent output = getIntent();
                
                PlayerConfiguration config = new PlayerConfiguration();
                config.setUri(item.get("uri"));
                config.setUser(item.get("user"));
                config.setPass(item.get("pass"));
                config.setName(item.get("name"));
                
                output.putExtra("config", config);
                setResult(RESULT_OK, output);
                finish();
            }
        };
    }
    
    private void startAddAlertDialog(int position)
    {   
        LayoutInflater li = LayoutInflater.from(this);
        View manageEntry = li.inflate(R.layout.manageentry, null);
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
     
        final EditText namev = (EditText) manageEntry.findViewById(R.id.EditText_Name);
        final EditText uriv = (EditText) manageEntry.findViewById(R.id.EditText_Uri);
        final EditText userv = (EditText) manageEntry.findViewById(R.id.EditText_User);
        final EditText passv = (EditText) manageEntry.findViewById(R.id.EditText_Pass);
        final int selected_position = position;
        
        builder.setView(manageEntry);
        
        if (position == -1) {
            builder.setTitle("Add new URI");
        } else {
            HashMap<String, String> item = listEntries.get(selected_position);
            
            builder.setTitle("Edit URI");
            namev.setText(item.get("name"));
            uriv.setText(item.get("uri"));
            userv.setText(item.get("user"));
            passv.setText(item.get("pass"));
        }

        builder.setPositiveButton("OK", new DialogInterface.OnClickListener() {

            @Override
            public void onClick(DialogInterface dialog, int which) {
                String uri = uriv.getText().toString();
                String name = namev.getText().toString();
                String user = userv.getText().toString();
                String pass = passv.getText().toString();

                if (!uri.isEmpty() && (uri.startsWith("rtsp://") ||
                        uri.startsWith("rtspt://") || uri.startsWith("rtsph://"))) {
                    if (selected_position == -1) {
                        HashMap<String, String> item = new HashMap<String, String>();
                        item.put("uri", uri);
                        item.put("name", name);
                        item.put("user", user);
                        item.put("pass", pass);
                        listEntries.add(item);
                        mgrAdapter.notifyDataSetChanged();
                    } else {
                        HashMap<String, String> item = listEntries.get(selected_position);
                        item.put("uri", uri);
                        item.put("name", name);
                        item.put("user", user);
                        item.put("pass", pass);
                        mgrAdapter.notifyDataSetChanged();
                    }
                } else {
                    Toast.makeText(ConfigurationManager.this, "Invalid URI, accepted format is rtsp[t|h]://IP/path",
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