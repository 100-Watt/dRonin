/**
 ******************************************************************************
 *
 * @file       spectrogramscope.cpp
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

#include "spectrogramdata.h"
#include "scopes3d/spectrogramscopeconfig.h"
#include "coreplugin/icore.h"
#include "coreplugin/connectionmanager.h"


SpectrogramScope::SpectrogramScope()
{
    m_refreshInterval = 50; //TODO: This should not be set here. Probably should come from a define somewhere.
    yAxisUnits = "";
    timeHorizon = 60;
    samplingFrequency = 100;
    windowWidth = 64;
    zMaximum = 120;
}


SpectrogramScope::SpectrogramScope(QSettings *qSettings) //TODO: Understand where to put m_refreshInterval default values
{
    timeHorizon = qSettings->value("timeHorizon").toDouble();
    samplingFrequency = qSettings->value("samplingFrequency").toDouble();
    windowWidth       = qSettings->value("windowWidth").toInt();
    zMaximum = qSettings->value("zMaximum").toDouble();

    int plot3dCurveCount = qSettings->value("dataSourceCount").toInt();

    for(int i = 0; i < plot3dCurveCount; i++){
        Plot3dCurveConfiguration *plotCurveConf = new Plot3dCurveConfiguration();

        qSettings->beginGroup(QString("spectrogramDataSource") + QString().number(i));

        plotCurveConf->uavObjectName = qSettings->value("uavObject").toString();
        plotCurveConf->uavFieldName  = qSettings->value("uavField").toString();
        plotCurveConf->color         = qSettings->value("color").value<QRgb>();
        plotCurveConf->yScalePower   = qSettings->value("yScalePower").toInt();
        plotCurveConf->mathFunction  = qSettings->value("mathFunction").toString();
        plotCurveConf->yMeanSamples  = qSettings->value("yMeanSamples").toInt();

        plotCurveConf->yMinimum = qSettings->value("yMinimum").toDouble();
        plotCurveConf->yMaximum = qSettings->value("yMaximum").toDouble();

        //Stop reading XML block
        qSettings->endGroup();

        m_spectrogramSourceConfigs.append(plotCurveConf);
    }
}


SpectrogramScope::SpectrogramScope(Ui::ScopeGadgetOptionsPage *options_page)
{
    bool parseOK = false;

    windowWidth = options_page->sbSpectrogramWidth->value();
    samplingFrequency = options_page->sbSpectrogramFrequency->value();
    timeHorizon = options_page->sbSpectrogramTimeHorizon->value();
    zMaximum = options_page->spnMaxSpectrogramZ->value();


    Plot3dCurveConfiguration* newPlotCurveConfigs = new Plot3dCurveConfiguration();
    newPlotCurveConfigs->uavObjectName = options_page->cmbUAVObjectsSpectrogram->currentText();
    newPlotCurveConfigs->uavFieldName  = options_page->cmbUavoFieldSpectrogram->currentText();
    newPlotCurveConfigs->yScalePower   = options_page->sbSpectrogramDataMultiplier->value();
    newPlotCurveConfigs->yMeanSamples  = options_page->spnMeanSamplesSpectrogram->value();
    newPlotCurveConfigs->mathFunction  = options_page->cmbMathFunctionSpectrogram->currentText();

    QVariant varColor = (int)QColor(options_page->btnColorSpectrogram->text()).rgb();
    int rgb = varColor.toInt(&parseOK);
    if(!parseOK)
        newPlotCurveConfigs->color = QColor(Qt::red).rgb();
    else
        newPlotCurveConfigs->color = (QRgb) rgb;

    m_spectrogramSourceConfigs.append(newPlotCurveConfigs);

}

SpectrogramScope::~SpectrogramScope()
{

}


ScopesGeneric* SpectrogramScope::cloneScope(ScopesGeneric *originalScope)
{
    SpectrogramScope *originalHistogramScope = (SpectrogramScope*) originalScope;
    SpectrogramScope *cloneObj = new SpectrogramScope();

    cloneObj->timeHorizon = originalHistogramScope->timeHorizon;

    int plotCurveCount = originalHistogramScope->m_spectrogramSourceConfigs.size();

    for (int i = 0; i < plotCurveCount; i++){
        Plot3dCurveConfiguration *currentPlotCurveConf = originalHistogramScope->m_spectrogramSourceConfigs.at(i);
        Plot3dCurveConfiguration *newSpectrogramConf     = new Plot3dCurveConfiguration();

        newSpectrogramConf->uavObjectName = currentPlotCurveConf->uavObjectName;
        newSpectrogramConf->uavFieldName  = currentPlotCurveConf->uavFieldName;
        newSpectrogramConf->color         = currentPlotCurveConf->color;
        newSpectrogramConf->yScalePower   = currentPlotCurveConf->yScalePower;
        newSpectrogramConf->yMeanSamples  = currentPlotCurveConf->yMeanSamples;
        newSpectrogramConf->mathFunction  = currentPlotCurveConf->mathFunction;

        newSpectrogramConf->yMinimum = currentPlotCurveConf->yMinimum;
        newSpectrogramConf->yMaximum = currentPlotCurveConf->yMaximum;

        cloneObj->m_spectrogramSourceConfigs.append(newSpectrogramConf);
    }

    return cloneObj;
}


void SpectrogramScope::saveConfiguration(QSettings* qSettings)
{
    //Start writing new XML block
    qSettings->beginGroup(QString("plot3d"));

    int plot3dCurveCount = m_spectrogramSourceConfigs.size();

    qSettings->setValue("plot3dType", SPECTROGRAM);
    qSettings->setValue("dataSourceCount", plot3dCurveCount);

    qSettings->setValue("samplingFrequency", samplingFrequency);
    qSettings->setValue("timeHorizon", timeHorizon);
    qSettings->setValue("windowWidth", windowWidth);
    qSettings->setValue("zMaximum",  zMaximum);

    for(int i = 0; i < plot3dCurveCount; i++){
        Plot3dCurveConfiguration *plotCurveConf = m_spectrogramSourceConfigs.at(i);

        //Start new XML block
        qSettings->beginGroup(QString("spectrogramDataSource") + QString().number(i));

        qSettings->setValue("uavObject",  plotCurveConf->uavObjectName);
        qSettings->setValue("uavField",  plotCurveConf->uavFieldName);
        qSettings->setValue("colormap",  plotCurveConf->color);

        //Stop writing XML block
        qSettings->endGroup();
    }

    //Stop writing XML block
    qSettings->endGroup();
}


/**
 * @brief SpectrogramScope::replaceHistogramSource Replaces the list of histogram data sources
 * @param histogramSourceConfigs
 */
void SpectrogramScope::replaceSpectrogramDataSource(QList<Plot3dCurveConfiguration*> spectrogramSourceConfigs)
{
    m_spectrogramSourceConfigs.clear();
    m_spectrogramSourceConfigs.append(spectrogramSourceConfigs);
}


/**
 * @brief SpectrogramScope::loadConfiguration loads the plot configuration into the scope gadget widget
 * @param scopeGadgetWidget
 */
void SpectrogramScope::loadConfiguration(ScopeGadgetWidget **scopeGadgetWidget)
{
    (*scopeGadgetWidget)->setupSpectrogramPlot(this);
    (*scopeGadgetWidget)->setRefreshInterval(m_refreshInterval);

    //There should be only one spectrogram per plot //TODO: Change this to handle multiple spectrograms
    if ( m_spectrogramSourceConfigs.length() != 1)
        return;

    Plot3dCurveConfiguration* spectrogramSourceConfigs = m_spectrogramSourceConfigs.front();
    QString uavObjectName = spectrogramSourceConfigs->uavObjectName;
    QString uavFieldName = spectrogramSourceConfigs->uavFieldName;
    int scaleOrderFactor = spectrogramSourceConfigs->yScalePower;
    int meanSamples = spectrogramSourceConfigs->yMeanSamples;
    QString mathFunction = spectrogramSourceConfigs->mathFunction;

    // Get and store the units
    units = getUavObjectFieldUnits(uavObjectName, uavFieldName);

//    // Create the Qwt waterfall plot
//    (*scopeGadgetWidget)->addWaterfallPlot(
//                uavObjectName,
//                uavFieldName,
//                scale,
//                mean,
//                mathFunction,
//                timeHorizon,
//                samplingFrequency,
//                windowWidth,
//                zMaximum
//                );

    {
        SpectrogramData* spectrogramData = new SpectrogramData(uavObjectName, uavFieldName, samplingFrequency, windowWidth, timeHorizon);

        spectrogramData->setXMinimum(0);
        spectrogramData->setXMaximum(samplingFrequency/2);
        spectrogramData->setYMinimum(0);
        spectrogramData->setYMaximum(timeHorizon);
        spectrogramData->setZMaximum(zMaximum);
        spectrogramData->setScalePower(scaleOrderFactor);
        spectrogramData->setMeanSamples(meanSamples);
        spectrogramData->setMathFunction(mathFunction);

        //Generate the waterfall name
        QString waterfallName = (spectrogramData->getUavoName()) + "." + (spectrogramData->getUavoFieldName());
        if(spectrogramData->getHaveSubFieldFlag())
            waterfallName = waterfallName.append("." + spectrogramData->getUavoSubFieldName());

        //Get the uav object
        ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
        UAVObjectManager *objManager = pm->getObject<UAVObjectManager>();
        UAVDataObject* obj = dynamic_cast<UAVDataObject*>(objManager->getObject((spectrogramData->getUavoName())));
        if(!obj) {
            qDebug() << "Object " << spectrogramData->getUavoName() << " is missing";
            return;
        }

        //Get the units
        QString units = getUavObjectFieldUnits(spectrogramData->getUavoName(), spectrogramData->getUavoFieldName());

        //Generate name with scaling factor appeneded
        QString waterfallNameScaled;
        if(scaleOrderFactor == 0)
            waterfallNameScaled = waterfallName + "(" + units + ")";
        else
            waterfallNameScaled = waterfallName + "(x10^" + QString::number(scaleOrderFactor) + " " + units + ")";

        //Create the waterfall plot
        QwtPlotSpectrogram* plotSpectrogram = new QwtPlotSpectrogram(waterfallNameScaled);
        plotSpectrogram->setRenderThreadCount( 0 ); // use system specific thread count
        plotSpectrogram->setRenderHint(QwtPlotItem::RenderAntialiased);
        plotSpectrogram->setColorMap(new ColorMap() );

        // Initial raster data
        spectrogramData->rasterData = new QwtMatrixRasterData();

        QDateTime NOW = QDateTime::currentDateTime(); //TODO: This should show UAVO time and not system time
        for ( uint i = 0; i < timeHorizon; i++ ){
            spectrogramData->timeDataHistory->append(NOW.toTime_t() + NOW.time().msec() / 1000.0 + i);
        }

        if (((double) windowWidth) * timeHorizon < (double) 10000000.0 * sizeof(spectrogramData->zDataHistory->front())){ //Don't exceed 10MB for memory
            for ( uint i = 0; i < windowWidth*timeHorizon; i++ ){
                spectrogramData->zDataHistory->append(0);
            }
        }
        else{
            qDebug() << "For some reason, we're trying to allocate a gigantic spectrogram. This probably represents a problem in the configuration file. TimeHorizion: "<< timeHorizon << ", windowWidth: "<< windowWidth;
            Q_ASSERT(0);
            return;
        }

        int numColumns = windowWidth;
        spectrogramData->rasterData->setValueMatrix( *(spectrogramData->zDataHistory), numColumns );

        //Set the ranges for the plot
        spectrogramData->rasterData->setInterval( Qt::XAxis, QwtInterval(spectrogramData->getXMinimum(), spectrogramData->getXMaximum()));
        spectrogramData->rasterData->setInterval( Qt::YAxis, QwtInterval(spectrogramData->getYMinimum(), spectrogramData->getYMaximum()));
        spectrogramData->rasterData->setInterval( Qt::ZAxis, QwtInterval(0, zMaximum));

        //Set up colorbar on right axis
        spectrogramData->rightAxis = (*scopeGadgetWidget)->axisWidget( QwtPlot::yRight );
        spectrogramData->rightAxis->setTitle( "Intensity" );
        spectrogramData->rightAxis->setColorBarEnabled( true );
        spectrogramData->rightAxis->setColorMap( QwtInterval(0, zMaximum), new ColorMap() );
        (*scopeGadgetWidget)->setAxisScale( QwtPlot::yRight, 0, zMaximum);
        (*scopeGadgetWidget)->enableAxis( QwtPlot::yRight );



        plotSpectrogram->setData(spectrogramData->rasterData);

        plotSpectrogram->attach((*scopeGadgetWidget));
        spectrogramData->spectrogram = plotSpectrogram;

        //Keep the curve details for later
        m_curves3dData.insert(waterfallNameScaled, spectrogramData);

        //Link to the new signal data only if this UAVObject has not been connected yet
        if (!(*scopeGadgetWidget)->m_connectedUAVObjects.contains(obj->getName())) {
            (*scopeGadgetWidget)->m_connectedUAVObjects.append(obj->getName());
            connect(obj, SIGNAL(objectUpdated(UAVObject*)), (*scopeGadgetWidget), SLOT(uavObjectReceived(UAVObject*)));
        }

    }
    mutex.lock();
    (*scopeGadgetWidget)->replot();
    mutex.unlock();
}



void SpectrogramScope::setGuiConfiguration(Ui::ScopeGadgetOptionsPage *options_page)
{
    //Set the tab widget to 3D
    options_page->tabWidget2d3d->setCurrentWidget(options_page->tabPlot3d);

    //Set the plot type
    options_page->cmb3dPlotType->setCurrentIndex(options_page->cmb3dPlotType->findData(getScopeType()));

    options_page->sbSpectrogramTimeHorizon->setValue(timeHorizon);
    options_page->sbSpectrogramFrequency->setValue(samplingFrequency);
    options_page->spnMaxSpectrogramZ->setValue(zMaximum);

    foreach (Plot3dCurveConfiguration* plot3dData,  m_spectrogramSourceConfigs) {
        int uavoIdx= options_page->cmbUAVObjectsSpectrogram->findText(plot3dData->uavObjectName);
        options_page->cmbUAVObjectsSpectrogram->setCurrentIndex(uavoIdx);
//        on_cmbUAVObjectsSpectrogram_currentIndexChanged(plot3dData->uavObjectName);
        options_page->sbSpectrogramWidth->setValue(windowWidth);

        int uavoFieldIdx= options_page->cmbUavoFieldSpectrogram->findText(plot3dData->uavFieldName);
        options_page->cmbUavoFieldSpectrogram->setCurrentIndex(uavoFieldIdx);
    }

}


void SpectrogramScope::preparePlot(ScopeGadgetWidget *scopeGadgetWidget)
{
    scopeGadgetWidget->setMinimumSize(64, 64);
    scopeGadgetWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    scopeGadgetWidget->setCanvasBackground(QColor(64, 64, 64));

    //Remove grid lines
    scopeGadgetWidget->m_grid->enableX( false );
    scopeGadgetWidget->m_grid->enableY( false );
    scopeGadgetWidget->m_grid->enableXMin( false );
    scopeGadgetWidget->m_grid->enableYMin( false );
    scopeGadgetWidget->m_grid->setMajPen(QPen(Qt::gray, 0, Qt::DashLine));
    scopeGadgetWidget->m_grid->setMinPen(QPen(Qt::lightGray, 0, Qt::DotLine));
    scopeGadgetWidget->m_grid->setPen(QPen(Qt::darkGray, 1, Qt::DotLine));
    scopeGadgetWidget->m_grid->attach(scopeGadgetWidget);

    // Only start the timer if we are already connected
    Core::ConnectionManager *cm = Core::ICore::instance()->connectionManager();
    if (cm->getCurrentConnection() && scopeGadgetWidget->replotTimer)
    {
        if (!scopeGadgetWidget->replotTimer->isActive())
            scopeGadgetWidget->replotTimer->start(m_refreshInterval);
        else
            scopeGadgetWidget->replotTimer->setInterval(m_refreshInterval);
    }

    // Spectrograms use autoscale
    scopeGadgetWidget->setAxisAutoScale(QwtPlot::xBottom);
    scopeGadgetWidget->setAxisAutoScale(QwtPlot::yLeft);

}


void SpectrogramScope::plotNewData(ScopeGadgetWidget *scopeGadgetWidget)
{
    foreach(Plot3dData* plot3dData, m_curves3dData.values())
    {
        plot3dData->removeStaleData();
        // Load spectrogram parameters
        SpectrogramData *spectrogramData = (SpectrogramData*) plot3dData;

        // Check for new data
        if (spectrogramData->readAndResetUpdatedFlag() == true){
            // Plot new data
            spectrogramData->rasterData->setValueMatrix(*plot3dData->zDataHistory, spectrogramData->windowWidth);

            // Check autoscale. (For some reason, QwtSpectrogram doesn't support autoscale)
            if (plot3dData->getZMaximum() == 0){
                double newVal = spectrogramData->readAndResetAutoscaleValue();
                if (newVal != 0){
                    spectrogramData->rightAxis->setColorMap( QwtInterval(0, newVal), new ColorMap() );
                    scopeGadgetWidget->setAxisScale( QwtPlot::yRight, 0, newVal);
                }
            }
        }
    }
}


void SpectrogramScope::clearPlots()
{
    foreach(Plot3dData* plot3dData, m_curves3dData.values()) {
        SpectrogramData* spectrogramData = (SpectrogramData*) plot3dData;
        spectrogramData->spectrogram->detach();

        // Don't delete raster data, this is done by the spectrogram's destructor
        /* delete plot3dData->rasterData; */

        // Delete spectrogram (also deletes raster data)
        delete spectrogramData->spectrogram;
        delete spectrogramData;
    }

    // Clear the data
    m_curves3dData.clear();
}


void SpectrogramScope::uavObjectReceived(UAVObject* obj)
{
    foreach(Plot3dData* plot3dData, m_curves3dData.values()) {
        bool ret = plot3dData->append(obj);
        if (ret)
            plot3dData->setUpdatedFlagToTrue();
    }
}
