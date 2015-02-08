/**
 ******************************************************************************
 *
 * @file       rfmbindwizard.h
 * @author     Tau Labs, http://taulabs.org, Copyright (C) 2014
 * @see        The GNU Public License (GPL) Version 3
 *
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup RfmBindWizard Setup Wizard
 * @{
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

#ifndef RFMBINDWIZARD_H
#define RFMBINDWIZARD_H

#include <QWizard>
#include <coreplugin/icore.h>
#include <coreplugin/iboardtype.h>
#include <coreplugin/connectionmanager.h>

class RfmBindWizard : public QWizard {
    Q_OBJECT

public:
    RfmBindWizard(QWidget *parent = 0);
    int nextId() const;

    // gettor and accessor for coordinator ID
    void setCoordID(quint32 id) { m_coordID = id; }
    quint32 getCoordID() const { return m_coordID; }

    // gettor and accessor for the max BPS
    void setMaxBps(quint32 bps) { m_maxBps = bps; }
    quint32 getMaxBps() const { return m_maxBps; }

    // gettor and accessor for rf power
    void setMaxRfPower(float rfPower) { m_maxRfPower = rfPower; }
    float getMaxRfPower() const { return m_maxRfPower; }

    Core::ConnectionManager *getConnectionManager()
    {
        if (!m_connectionManager) {
            m_connectionManager = Core::ICore::instance()->connectionManager();
            Q_ASSERT(m_connectionManager);
        }
        return m_connectionManager;
    }

private slots:
    void customBackClicked();
    void pageChanged(int currId);

private:

    /**
     * This wizard will do the following steps:
     * 1. give an introduction
     * 2. ask the user to plug in the coordinator board
     *    - detect the coordinator device id
     *    - set this board to TelemCoord, CoordID = 0
     *    - save the hardware configuration
     * 3. ask the user to plug in the coordinated board
     *    - set board to Telem, set CoordID to the previous value
     *    - save the settings
     * 4. inform the user they should get a bind when both boards powered
     */
    enum { PAGE_START, PAGE_CONFIGURE, PAGE_COORDINATOR, PAGE_COORDINATED, PAGE_END };

    bool m_ppm;
    quint32 m_maxBps;
    float m_maxRfPower;
    quint32 m_coordID;

    void createPages();

    bool m_back;

    Core::ConnectionManager *m_connectionManager;
};

#endif // RFMBINDWIZARD_H
