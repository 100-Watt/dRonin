/**
 ******************************************************************************
 * @file       ObjectViewer.java
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2012-2013
 * @brief      Fragment to show the value of a UAVO
 * @see        The GNU Public License (GPL) Version 3
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
package org.taulabs.androidgcs.fragments;

import org.taulabs.androidgcs.ObjectBrowser;
import org.taulabs.androidgcs.R;
import org.taulabs.uavtalk.UAVObject;
import org.taulabs.uavtalk.UAVObjectManager;

import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

public class ObjectViewer extends ObjectManagerFragment {

	static final String TAG = "ObjectViewer";
	String objectName;
	long objectID;
	long instID;
	UAVObject object;

	@Override
	public void setArguments(Bundle b) {
		objectName = b.getString("org.taulabs.androidgcs.ObjectName");
		objectID = b.getLong("org.taulabs.androidgcs.ObjectId");
		instID = b.getLong("org.taulabs.androidgcs.InstId");
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		return inflater.inflate(R.layout.object_viewer, container, false);
	}

	@Override
	public void onConnected(UAVObjectManager objMngr) {
		super.onConnected(objMngr);

		object = objMngr.getObject(objectID, instID);
		if (object == null) {
			Log.d(TAG, "Object not found:" + objectID);
			return;
		}
		
		registerObjectUpdates(object);
		objectUpdated(object);
	}
	
	/**
	 * Because on connected and on resume can happen in any order
	 * this must attempt to update the text too.
	 */
	public void onResume() {
		super.onResume();
		
		((ObjectBrowser) getActivity()).attachObjectView();
		
		if (object != null)
			objectUpdated(object);
	}
	
	/**
	 * Called whenever any objects subscribed to via registerObjects
	 */
	public void objectUpdated(UAVObject obj) {
		if (getActivity() != null) {
			TextView text = (TextView) getActivity().findViewById(R.id.object_info_view);
			if (text != null)
				text.setText(obj.toStringData());
		} else {
			Log.d(TAG, "Update without activity");
		}
	}
	
	@Override
	protected String getDebugTag() {
		return TAG;
	}

}
