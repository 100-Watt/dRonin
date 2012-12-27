/**
 ******************************************************************************
 * @file       DefaultHwSettingsWidget.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @author     PhoenixPilot, http://github.com/PhoenixPilot, Copyright (C) 2012
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup ConfigPlugin Config Plugin
 * @{
 * @brief Placeholder for attitude panel until board is connected.
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
#include "defaulthwsettingswidget.h"
#include "ui_defaultattitude.h"
#include "hwfieldselector.h"
#include <QMutexLocker>
#include <QErrorMessage>
#include <QDebug>

DefaultHwSettingsWidget::DefaultHwSettingsWidget(QWidget *parent) :
        ConfigTaskWidget(parent),
        ui(new Ui_defaulthwsettings),
        hwSettingsObject("HwFreedom")
{
    ui->setupUi(this);
    fieldWidgets.clear();
    updateFields();
}

DefaultHwSettingsWidget::~DefaultHwSettingsWidget()
{
    delete ui;
}

/**
 * @brief DefaultHwSettingsWidget::updateFields Update the list of fields
 * on the UI
 */
void DefaultHwSettingsWidget::updateFields()
{
    QLayout *layout = ui->portSettingsFrame->layout();
    for (int i = 0; i < fieldWidgets.size(); i++)
        layout->removeWidget(fieldWidgets[i]);
    fieldWidgets.clear();

    UAVObject *settings = getObjectManager()->getObject(hwSettingsObject);
    QList <UAVObjectField*> fields = settings->getFields();
    for (int i = 0; i < fields.size(); i++) {
        HwFieldSelector *sel = new HwFieldSelector(this);
        layout->addWidget(sel);
        sel->setUavoField(fields[i]);
        fieldWidgets.append(sel);
    }
}
