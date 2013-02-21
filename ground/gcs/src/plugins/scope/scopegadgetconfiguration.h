/**
 ******************************************************************************
 *
 * @file       scopegadgetconfiguration.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
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

#ifndef SCOPEGADGETCONFIGURATION_H
#define SCOPEGADGETCONFIGURATION_H

#include "scopesconfig.h"
#include "scopes2d/scopes2dconfig.h"
#include "scopes3d/scopes3dconfig.h"
#include "plotdata2d.h"
#include "plotdata3d.h"
#include <coreplugin/iuavgadgetconfiguration.h>

#include <QVector>

#include "qwt/src/qwt_color_map.h"

using namespace Core;



class ScopeGadgetConfiguration : public IUAVGadgetConfiguration
{
    Q_OBJECT
public:
    explicit ScopeGadgetConfiguration(QString classId, QSettings* qSettings = 0, QObject *parent = 0);
    ~ScopeGadgetConfiguration();

    //configuration setter functions
    virtual void clone(ScopesGeneric*){}

    //configurations getter functions
    ScopesGeneric* getScope(){return m_scope;}
    virtual int getScopeType(){}


    virtual PlotDimensions getPlotDimensions(){}

    void saveConfig(QSettings* settings) const; //THIS SEEMS TO BE UNUSED
    IUAVGadgetConfiguration *clone();

private:
    ScopesGeneric *m_scope;


};

#endif // SCOPEGADGETCONFIGURATION_H
