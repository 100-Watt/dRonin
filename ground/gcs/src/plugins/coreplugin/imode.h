/**
 ******************************************************************************
 *
 * @file       imode.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 *             Parts by Nokia Corporation (qt-info@nokia.com) Copyright (C) 2009.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup CorePlugin Core Plugin
 * @{
 * @brief The Core GCS plugin
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
 * with this program; if not, see <http://www.gnu.org/licenses/>
 */

#ifndef IMODE_H
#define IMODE_H

#include "icontext.h"

#include <coreplugin/core_global.h>

QT_BEGIN_NAMESPACE
class QIcon;
QT_END_NAMESPACE

namespace Core {

class CORE_EXPORT IMode : public IContext
{
    Q_OBJECT
public:
    IMode(QObject *parent = 0)
        : IContext(parent)
    {
    }
    virtual ~IMode() {}

    virtual QString name() const = 0;
    virtual QIcon icon() const = 0;
    virtual int priority() const = 0;
    virtual void setPriority(int priority) = 0;
    virtual const char *uniqueModeName() const = 0;
};

} // namespace Core

#endif // IMODE_H
