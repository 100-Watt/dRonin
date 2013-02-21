/**
 ******************************************************************************
 *
 * @file       scopes2d.h
 * @author     Tau Labs, http://www.taulabs.org Copyright (C) 2013.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup ScopePlugin Scope Gadget Plugin
 * @{
 * @brief The scope Gadget, graphically plots the states of UAVObjects
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

#ifndef SCOPES2D_H
#define SCOPES2D_H

#include "scopesconfig.h"
#include "plotdata2d.h"

#include <coreplugin/iuavgadgetconfiguration.h>
#include "scopegadgetwidget.h"

// This struct holds the configuration for individual 2D data sources
struct Plot2dCurveConfiguration
{
    QString uavObjectName;
    QString uavFieldName;
    int yScalePower; //This is the power to which each value must be raised
    QRgb color;
    int yMeanSamples;
    QString mathFunction;
    double yMinimum;
    double yMaximum;
};

/**
 * @brief The HistogramScope class The histogram scope has a variable sized list of
 * data sources
 */
class Scopes2d : public ScopesGeneric
{
    Q_OBJECT
public:
    virtual void saveConfiguration(QSettings *qSettings) = 0;
    virtual PlotDimensions getPlotDimensions() {return PLOT2D;}
    virtual int getScopeType(){} //TODO: Fix this. It should return the true value, not HISTOGRAM
    virtual QList<Plot2dCurveConfiguration*> getDataSourceConfigs(){}
    virtual void loadConfiguration(ScopeGadgetWidget **scopeGadgetWidget){}
    Plot2dType getPlot2dType(){return m_plot2dType;}
    virtual void setScopeType(Plot2dType val){m_plot2dType = val;}
   virtual void clone(ScopesGeneric *){}

protected:
    PlotDimensions m_plotDimensions;
    Plot2dType m_plot2dType; //The type of 2d plot
private:
};

#endif // SCOPES2D_H
