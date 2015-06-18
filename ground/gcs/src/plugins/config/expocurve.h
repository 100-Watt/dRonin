
/**
 ******************************************************************************
 * @file       expocurve.h
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2012-2015
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup ConfigPlugin Config Plugin
 * @{
 * @brief Visualize the expo seettings
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

#ifndef EXPOCURVE_H
#define EXPOCURVE_H


#include <QWidget>

#include "qwt/src/qwt.h"
#include "qwt/src/qwt_plot.h"
#include "qwt/src/qwt_plot_curve.h"
#include "qwt/src/qwt_scale_draw.h"
#include "qwt/src/qwt_scale_widget.h"
#include "qwt/src/qwt_plot_grid.h"
#include "qwt/src/qwt_legend.h"
#include "qwt/src/qwt_legend_label.h"
#include "qwt/src/qwt_plot_marker.h"
#include "qwt/src/qwt_symbol.h"
#include <QMutexLocker>

class ExpoCurve : public QwtPlot
{
    Q_OBJECT
public:
    explicit ExpoCurve(QWidget *parent = 0);

    typedef struct ExpoPlotElements
    {
      QwtPlotCurve Curve;
      QwtPlotMarker Mark;
      QwtPlotMarker Mark_;
      QwtPlotCurve Curve2;
      QwtPlotMarker Mark2;
      QwtPlotMarker Mark2_;
    } ExpoPlotElements_t;

    //! Set label for the stick channels
    void init(int lbl_mode,int horizon_transistion,int roll_value,int pitch_value,int yaw_value,int roll_max,int pitch_max,int yaw_max,int roll_max2,int pitch_max2,int yaw_max2);

    //! Show expo data for one of the stick channels
    void plotData(int value, int max, ExpoPlotElements_t &plot_elements, int mode);

public slots:

    //! Show expo data for roll
    void plotDataRoll(double value, int max, int mode);

    //! Show expo data for pitch
    void plotDataPitch(double value, int max, int mode);

    //! Show expo data for yaw
    void plotDataYaw(double value, int max, int mode);

    //! En-/Disable a expo curve
    void showCurve(const QVariant & itemInfo, bool on, int index);

signals:

public slots:

private:

    int STEPS;
    int HorizonTransition;
    int CurveCnt;
    double *x_data;
    double *y_data;

    ExpoPlotElements_t rollElements;
    ExpoPlotElements_t pitchElements;
    ExpoPlotElements_t yawElements;

    QMutex mutex;

    //! Inverse expo function
    double invers_expo3(double y,int g);
};

#endif // EXPOCURVE_H
