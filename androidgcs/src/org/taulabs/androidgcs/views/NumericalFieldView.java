package org.taulabs.androidgcs.views;
/**
 ******************************************************************************
 * @file       ScrollBarView.java
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2012-2013
 * @brief      A scrollable view
 * @see        The GNU Public License (GPL) Version 3
 *
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

import org.taulabs.androidgcs.util.ObjectFieldMappable;
import org.taulabs.uavtalk.UAVObjectField;

import android.content.Context;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.widget.EditText;
import android.widget.GridLayout;
import android.widget.TextView;

public class NumericalFieldView extends GridLayout implements ObjectFieldMappable {

	private final TextView lbl;
	private final EditText edit;
	private double value;
	private boolean localUpdate = false;

	private Runnable changeListener = null;

	//! This is the constructor used by the SDK for setting it up
	public NumericalFieldView(Context context, AttributeSet attrs) {
		super(context, attrs);

		lbl = new TextView(context);
		lbl.setText("Field: ");
		addView(lbl, new GridLayout.LayoutParams(spec(0), spec(0)));

		edit = new EditText(context);
		edit.setInputType(InputType.TYPE_NUMBER_FLAG_DECIMAL);

		addView(edit, new GridLayout.LayoutParams(spec(0), spec(1)));
		// Update the value when the edit box changes
		edit.addTextChangedListener(new TextWatcher() {

			@Override
			public void afterTextChanged(Editable s) {
				value = Double.parseDouble(s.toString());
				if (changeListener != null && localUpdate == false)
					changeListener.run();
			}

			@Override
			public void beforeTextChanged(CharSequence s, int start, int count,
					int after) {
			}

			@Override
			public void onTextChanged(CharSequence s, int start, int before,
					int count) {
			}
		});

		setPadding(5,5,5,5);

		setMinimumWidth(300);
		setValue(0);
	}

	//! This is the constructor used by the code
	public NumericalFieldView(Context context, AttributeSet attrs, UAVObjectField field, int idx) {
		this(context, attrs);

		lbl.setText(field.getName());
	}


	@Override
	public double getValue() {
		return value;
	}

	//! Set the input type for the editable field
	public void setInputType(int type) {
		edit.setInputType(type);
	}

	@Override
	public void setValue(double val) {
		localUpdate = true;
		value = val;
		edit.setText(Double.toString(value));
		localUpdate = false;
	}

	@Override
	public void setOnChangedListener(Runnable run) {
		changeListener = run;
	}
}
