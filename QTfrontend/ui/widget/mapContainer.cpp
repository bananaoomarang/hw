/*
 * Hedgewars, a free turn based strategy game
 * Copyright (c) 2006-2012 Igor Ulyanov <iulyanov@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <QBitmap>
#include <QBuffer>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QStringListModel>
#include <QTextStream>
#include <QUuid>
#include <QVBoxLayout>

#include "hwconsts.h"
#include "mapContainer.h"
#include "themeprompt.h"
#include "seedprompt.h"
#include "igbox.h"
#include "HWApplication.h"
#include "ThemeModel.h"



HWMapContainer::HWMapContainer(QWidget * parent) :
    QWidget(parent),
    mainLayout(this),
    pMap(0),
    mapgen(MAPGEN_REGULAR),
    m_previewSize(256, 128)
{
    // don't show preview anything until first show event
    m_previewEnabled = false;
    m_missionsViewSetup = false;
    m_staticViewSetup = false;
    m_script = QString();
    m_prevMapFeatureSize = 12;
    m_mapFeatureSize = 12;

    hhSmall.load(":/res/hh_small.png");
    hhLimit = 18;
    templateFilter = 0;
    m_master = true;

    linearGrad = QLinearGradient(QPoint(128, 0), QPoint(128, 128));
    linearGrad.setColorAt(1, QColor(0, 0, 192));
    linearGrad.setColorAt(0, QColor(66, 115, 225));

    mainLayout.setContentsMargins(HWApplication::style()->pixelMetric(QStyle::PM_LayoutLeftMargin),
                                  10,
                                  HWApplication::style()->pixelMetric(QStyle::PM_LayoutRightMargin),
                                  HWApplication::style()->pixelMetric(QStyle::PM_LayoutBottomMargin));

    m_staticMapModel = DataManager::instance().staticMapModel();
    m_missionMapModel = DataManager::instance().missionMapModel();
    m_themeModel = DataManager::instance().themeModel();

    /* Layouts */

    QWidget * topWidget = new QWidget();
    QHBoxLayout * topLayout = new QHBoxLayout(topWidget);
    topWidget->setContentsMargins(0, 0, 0, 0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout * twoColumnLayout = new QHBoxLayout();
    QVBoxLayout * leftLayout = new QVBoxLayout();
    QVBoxLayout * rightLayout = new QVBoxLayout();
    twoColumnLayout->addLayout(leftLayout, 0);
    twoColumnLayout->addStretch(1);
    twoColumnLayout->addLayout(rightLayout, 0);
    QVBoxLayout * drawnControls = new QVBoxLayout();

    /* Map type combobox */

    topLayout->setSpacing(10);
    topLayout->addWidget(new QLabel(tr("Map type:")), 0);
    cType = new QComboBox(this);
    topLayout->addWidget(cType, 1);
    cType->insertItem(0, tr("Image map"), MapModel::StaticMap);
    cType->insertItem(1, tr("Mission map"), MapModel::MissionMap);
    cType->insertItem(2, tr("Hand-drawn"), MapModel::HandDrawnMap);
    cType->insertItem(3, tr("Randomly generated"), MapModel::GeneratedMap);
    cType->insertItem(4, tr("Random maze"), MapModel::GeneratedMaze);
    cType->insertItem(5, tr("Random perlin"), MapModel::GeneratedPerlin);
    connect(cType, SIGNAL(currentIndexChanged(int)), this, SLOT(mapTypeChanged(int)));
    m_childWidgets << cType;

    /* Randomize button */

    topLayout->addStretch(1);
    const QIcon& lp = QIcon(":/res/dice.png");
    QSize sz = lp.actualSize(QSize(65535, 65535));
    btnRandomize = new QPushButton();
    btnRandomize->setText(tr("Random"));
    btnRandomize->setIcon(lp);
    btnRandomize->setFixedHeight(30);
    btnRandomize->setIconSize(sz);
    btnRandomize->setFlat(true);
    btnRandomize->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(btnRandomize, SIGNAL(clicked()), this, SLOT(setRandomMap()));
    m_childWidgets << btnRandomize;
    btnRandomize->setStyleSheet("padding: 5px;");
    btnRandomize->setFixedHeight(cType->height());
    topLayout->addWidget(btnRandomize, 1);

    /* Seed button */

    btnSeed = new QPushButton(parentWidget()->parentWidget());
    btnSeed->setText(tr("Seed"));
    btnSeed->setStyleSheet("padding: 5px;");
    btnSeed->setFixedHeight(cType->height());
    connect(btnSeed, SIGNAL(clicked()), this, SLOT(showSeedPrompt()));
    topLayout->addWidget(btnSeed, 0);

    /* Map preview label */

    QLabel * lblMapPreviewText = new QLabel(this);
    lblMapPreviewText->setText(tr("Map preview:"));
    leftLayout->addWidget(lblMapPreviewText, 0);

    /* Map Preview */

    mapPreview = new QPushButton(this);
    mapPreview->setObjectName("mapPreview");
    mapPreview->setFlat(true);
    mapPreview->setFixedSize(256 + 6, 128 + 6);
    mapPreview->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(mapPreview, 0);
    connect(mapPreview, SIGNAL(clicked()), this, SLOT(previewClicked()));

    /* Bottom-Left layout */

    QVBoxLayout * bottomLeftLayout = new QVBoxLayout();
    leftLayout->addLayout(bottomLeftLayout, 1);

    /* Map list label */

    lblMapList = new QLabel(this);
    rightLayout->addWidget(lblMapList, 0);

    /* Static maps list */

    staticMapList = new QListView(this);
    rightLayout->addWidget(staticMapList, 1);
    m_childWidgets << staticMapList;

    /* Mission maps list */

    missionMapList = new QListView(this);
    rightLayout->addWidget(missionMapList, 1);
    m_childWidgets << missionMapList;

    /* Map load and edit buttons */

    drawnControls->addStretch(1);

    btnLoadMap = new QPushButton(tr("Load map drawing"));
    btnLoadMap->setStyleSheet("padding: 20px;");
    drawnControls->addWidget(btnLoadMap, 0);
    m_childWidgets << btnLoadMap;
    connect(btnLoadMap, SIGNAL(clicked()), this, SLOT(loadDrawing()));

    btnEditMap = new QPushButton(tr("Edit map drawing"));
    btnEditMap->setStyleSheet("padding: 20px;");
    drawnControls->addWidget(btnEditMap, 0);
    m_childWidgets << btnEditMap;
    connect(btnEditMap, SIGNAL(clicked()), this, SIGNAL(drawMapRequested()));

    drawnControls->addStretch(1);

    rightLayout->addLayout(drawnControls);

    /* Generator style list */

    generationStyles = new QListWidget(this);
    new QListWidgetItem(tr("All"), generationStyles);
    new QListWidgetItem(tr("Small"), generationStyles);
    new QListWidgetItem(tr("Medium"), generationStyles);
    new QListWidgetItem(tr("Large"), generationStyles);
    new QListWidgetItem(tr("Cavern"), generationStyles);
    new QListWidgetItem(tr("Wacky"), generationStyles);
    connect(generationStyles, SIGNAL(currentRowChanged(int)), this, SLOT(setTemplateFilter(int)));
    m_childWidgets << generationStyles;
    rightLayout->addWidget(generationStyles, 1);

    /* Maze style list */

    mazeStyles = new QListWidget(this);
    new QListWidgetItem(tr("Small tunnels"), mazeStyles);
    new QListWidgetItem(tr("Medium tunnels"), mazeStyles);
    new QListWidgetItem(tr("Large tunnels"), mazeStyles);
    new QListWidgetItem(tr("Small islands"), mazeStyles);
    new QListWidgetItem(tr("Medium islands"), mazeStyles);
    new QListWidgetItem(tr("Large islands"), mazeStyles);
    connect(mazeStyles, SIGNAL(currentRowChanged(int)), this, SLOT(setMazeSize(int)));
    m_childWidgets << mazeStyles;
    rightLayout->addWidget(mazeStyles, 1);

    mapFeatureSize = new QSlider(Qt::Horizontal, this);
    mapFeatureSize->setObjectName("mapFeatureSize");
    //mapFeatureSize->setTickPosition(QSlider::TicksBelow);
    mapFeatureSize->setMaximum(25);
    mapFeatureSize->setMinimum(1);
    //mapFeatureSize->setFixedWidth(259);
    mapFeatureSize->setValue(m_mapFeatureSize);
    mapFeatureSize->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    bottomLeftLayout->addWidget(mapFeatureSize, 0);
    connect(mapFeatureSize, SIGNAL(valueChanged(int)), this, SLOT(setFeatureSize(int)));
    m_childWidgets << mapFeatureSize;

    /* Mission description */

    lblDesc = new QLabel();
    lblDesc->setWordWrap(true);
    lblDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    lblDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    lblDesc->setStyleSheet("font: 10px;");
    bottomLeftLayout->addWidget(lblDesc, 100);

    /* Add stretch above theme button */

    bottomLeftLayout->addStretch(1);

    /* Theme chooser */

    btnTheme = new QPushButton(this);
    btnTheme->setFlat(true);
    btnTheme->setIconSize(QSize(30, 30));
    btnTheme->setFixedHeight(30);
    connect(btnTheme, SIGNAL(clicked()), this, SLOT(showThemePrompt()));
    m_childWidgets << btnTheme;
    bottomLeftLayout->addWidget(btnTheme, 0);

    /* Add everything to main layout */

    mainLayout.addWidget(topWidget, 0);
    mainLayout.addLayout(twoColumnLayout, 1);

    /* Set defaults */

    setRandomSeed();
    setMazeSize(0);
    setTemplateFilter(0);
    staticMapChanged(m_staticMapModel->index(0, 0));
    missionMapChanged(m_missionMapModel->index(0, 0));
    changeMapType(MapModel::GeneratedMap);
}

void HWMapContainer::setImage(const QPixmap &newImage)
{
    addInfoToPreview(newImage);
    pMap = 0;

    cType->setEnabled(isMaster());
}

void HWMapContainer::setHHLimit(int newHHLimit)
{
    hhLimit = newHHLimit;
}

// Should this add text to identify map size?
void HWMapContainer::addInfoToPreview(const QPixmap &image)
{
    QPixmap finalImage = QPixmap(image.size());
//finalImage.fill(QColor(0, 0, 0, 0));

    QPainter p(&finalImage);
    p.fillRect(finalImage.rect(), linearGrad);
    p.drawPixmap(finalImage.rect(), image);
    //p.setPen(QColor(0xf4,0x9e,0xe9));
    p.setPen(QColor(0xff,0xcc,0x00));
    p.setBrush(QColor(0, 0, 0));
    p.drawRect(finalImage.rect().width() - hhSmall.rect().width() - 28, 3, 40, 20);
    p.setFont(QFont("MS Shell Dlg", 10));
    QString text = (hhLimit > 0) ? QString::number(hhLimit) : "?";
    p.drawText(finalImage.rect().width() - hhSmall.rect().width() - 14 - (hhLimit > 9 ? 10 : 0), 18, text);
    p.drawPixmap(finalImage.rect().width() - hhSmall.rect().width() - 5, 5, hhSmall.rect().width(), hhSmall.rect().height(), hhSmall);

    // Shrink, crop, and center preview image
    /*QPixmap centered(QSize(m_previewSize.width() - 6, m_previewSize.height() - 6));
    QPainter pc(&centered);
    pc.fillRect(centered.rect(), linearGrad);
    pc.drawPixmap(-3, -3, finalImage);*/

    mapPreview->setIcon(QIcon(finalImage));
    mapPreview->setIconSize(finalImage.size());
}

void HWMapContainer::askForGeneratedPreview()
{
    pMap = new HWMap(this);
    connect(pMap, SIGNAL(ImageReceived(QPixmap)), this, SLOT(setImage(QPixmap)));
    connect(pMap, SIGNAL(HHLimitReceived(int)), this, SLOT(setHHLimit(int)));
    connect(pMap, SIGNAL(destroyed(QObject *)), this, SLOT(onPreviewMapDestroyed(QObject *)));
    pMap->getImage(m_seed,
                   getTemplateFilter(),
                   get_mapgen(),
                   getMazeSize(),
                   getDrawnMapData(),
                   m_script,
		           m_mapFeatureSize
                  );

    setHHLimit(0);

    const QPixmap waitIcon(":/res/iconTime.png");

    QPixmap waitImage(m_previewSize);
    QPainter p(&waitImage);

    p.fillRect(waitImage.rect(), linearGrad);
    int x = (waitImage.width() - waitIcon.width()) / 2;
    int y = (waitImage.height() - waitIcon.height()) / 2;
    p.drawPixmap(QPoint(x, y), waitIcon);

    addInfoToPreview(waitImage);

    cType->setEnabled(false);
}

void HWMapContainer::previewClicked()
{
    if (isMaster()) // should only perform these if master, but disabling the button when not, causes an unattractive preview.
        switch (m_mapInfo.type)
        {
            case MapModel::HandDrawnMap:
                emit drawMapRequested();
                break;
            default:
                setRandomMap();
                break;
        }
}

QString HWMapContainer::getCurrentSeed() const
{
    return m_seed;
}

QString HWMapContainer::getCurrentMap() const
{
    switch (m_mapInfo.type)
    {
        case MapModel::StaticMap:
        case MapModel::MissionMap:
            return m_curMap;
        default:
            return QString();
    }
}

QString HWMapContainer::getCurrentTheme() const
{
    return(m_theme);
}

bool HWMapContainer::getCurrentIsMission() const
{
    return(m_mapInfo.type == MapModel::MissionMap);
}

int HWMapContainer::getCurrentHHLimit() const
{
    return hhLimit;
}

QString HWMapContainer::getCurrentScheme() const
{
    return(m_mapInfo.scheme);
}

QString HWMapContainer::getCurrentWeapons() const
{
    return(m_mapInfo.weapons);
}

quint32 HWMapContainer::getTemplateFilter() const
{
    return generationStyles->currentRow();
}

quint32 HWMapContainer::getFeatureSize() const
{
    return m_mapFeatureSize;
}

void HWMapContainer::resizeEvent ( QResizeEvent * event )
{
    Q_UNUSED(event);
}

void HWMapContainer::intSetSeed(const QString & seed)
{
    m_seed = seed;
}

void HWMapContainer::setSeed(const QString & seed)
{
    intSetSeed(seed);
    if ((m_mapInfo.type == MapModel::GeneratedMap)
            || (m_mapInfo.type == MapModel::GeneratedMaze)
            || (m_mapInfo.type == MapModel::GeneratedPerlin))
        updatePreview();
}

void HWMapContainer::setScript(const QString & script)
{
    m_script = script;
    if ((m_mapInfo.type == MapModel::GeneratedMap)
            || (m_mapInfo.type == MapModel::GeneratedMaze)
            || (m_mapInfo.type == MapModel::GeneratedPerlin)
            || (m_mapInfo.type == MapModel::HandDrawnMap))
        updatePreview();
}

void HWMapContainer::intSetMap(const QString & map)
{
    if (map == "+rnd+")
    {
        //changeMapType(MapModel::GeneratedMap);
    }
    else if (map == "+maze+")
    {
        //changeMapType(MapModel::GeneratedMaze);
    }
    else if (map == "+perlin+")
    {
        //changeMapType(MapModel::GeneratedPerlin);
    }
    else if (map == "+drawn+")
    {
        //changeMapType(MapModel::HandDrawnMap);
    }
    else if (m_staticMapModel->mapExists(map))
    {
        changeMapType(MapModel::StaticMap, m_staticMapModel->index(m_staticMapModel->findMap(map), 0));
    }
    else if (m_missionMapModel->mapExists(map))
    {
        changeMapType(MapModel::MissionMap, m_missionMapModel->index(m_missionMapModel->findMap(map), 0));
    } else
    {
        qDebug() << "HWMapContainer::intSetMap: Map doesn't exist: " << map;
    }
}

void HWMapContainer::setMap(const QString & map)
{
    if ((m_mapInfo.type == MapModel::Invalid) || (map != m_mapInfo.name))
        intSetMap(map);
}

void HWMapContainer::setTheme(const QString & theme)
{
    QModelIndexList mdl = m_themeModel->match(m_themeModel->index(0), ThemeModel::ActualNameRole, theme);

    if(mdl.size())
        updateTheme(mdl.at(0));
    else
        intSetIconlessTheme(theme);
}

void HWMapContainer::setRandomMap()
{
    if (!m_master) return;

    setRandomSeed();
    switch(m_mapInfo.type)
    {
        case MapModel::GeneratedMap:
        case MapModel::GeneratedMaze:
        case MapModel::GeneratedPerlin:
            setRandomTheme();
            break;
        case MapModel::MissionMap:
            missionMapChanged(m_missionMapModel->index(rand() % m_missionMapModel->rowCount(), 0));
            break;
        case MapModel::StaticMap:
            staticMapChanged(m_staticMapModel->index(rand() % m_staticMapModel->rowCount(), 0));
            break;
        default:
            break;
    }
}

void HWMapContainer::setRandomSeed()
{
    setSeed(QUuid::createUuid().toString());
    emit seedChanged(m_seed);
}

void HWMapContainer::setRandomTheme()
{
    if(!m_themeModel->rowCount()) return;
    quint32 themeNum = rand() % m_themeModel->rowCount();
    updateTheme(m_themeModel->index(themeNum));
    emit themeChanged(m_theme);
}

void HWMapContainer::intSetTemplateFilter(int filter)
{
    generationStyles->setCurrentRow(filter);
    emit newTemplateFilter(filter);
}

void HWMapContainer::setTemplateFilter(int filter)
{
    intSetTemplateFilter(filter);
    if (m_mapInfo.type == MapModel::GeneratedMap)
        updatePreview();
}

MapGenerator HWMapContainer::get_mapgen(void) const
{
    return mapgen;
}

int HWMapContainer::getMazeSize(void) const
{
    return mazeStyles->currentRow();
}

void HWMapContainer::intSetMazeSize(int size)
{
    mazeStyles->setCurrentRow(size);
    emit mazeSizeChanged(size);
}

void HWMapContainer::setMazeSize(int size)
{
    intSetMazeSize(size);
    if (m_mapInfo.type == MapModel::GeneratedMaze)
        updatePreview();
}

void HWMapContainer::intSetMapgen(MapGenerator m)
{
    if (mapgen != m)
    {
        mapgen = m;

        bool f = false;
        switch (m)
        {
            case MAPGEN_REGULAR:
                m_mapInfo.type = MapModel::GeneratedMap;
                f = true;
                break;
            case MAPGEN_MAZE:
                m_mapInfo.type = MapModel::GeneratedMaze;
                f = true;
                break;
            case MAPGEN_PERLIN:
                m_mapInfo.type = MapModel::GeneratedPerlin;
                f = true;
                break;
            case MAPGEN_DRAWN:
                m_mapInfo.type = MapModel::HandDrawnMap;
                f = true;
                break;
            case MAPGEN_MAP:
                switch (m_mapInfo.type)
                {
                    case MapModel::GeneratedMap:
                    case MapModel::GeneratedMaze:
                    case MapModel::GeneratedPerlin:
                    case MapModel::HandDrawnMap:
                        m_mapInfo.type = MapModel::Invalid;
                    default:
                        break;
                }
                break;
        }

        if(f)
            changeMapType(m_mapInfo.type, QModelIndex());
    }
}

void HWMapContainer::setMapgen(MapGenerator m)
{
    intSetMapgen(m);
    if(m != MAPGEN_MAP)
        updatePreview();
}

void HWMapContainer::setDrawnMapData(const QByteArray & ar)
{
    drawMapScene.decode(ar);
    updatePreview();
}

QByteArray HWMapContainer::getDrawnMapData()
{
    return drawMapScene.encode();
}

void HWMapContainer::setNewSeed(const QString & newSeed)
{
    setSeed(newSeed);
    emit seedChanged(newSeed);
}

DrawMapScene * HWMapContainer::getDrawMapScene()
{
    return &drawMapScene;
}

void HWMapContainer::mapDrawingFinished()
{
    emit drawnMapChanged(getDrawnMapData());

    updatePreview();
}

void HWMapContainer::showEvent(QShowEvent * event)
{
    if (!m_previewEnabled) {
        m_previewEnabled = true;
        setRandomTheme();
        updatePreview();
    }
    QWidget::showEvent(event);
}

void HWMapContainer::updatePreview()
{
    // abort if the widget isn't supposed to show anything yet
    if (!m_previewEnabled)
        return;

    if (pMap)
    {
        disconnect(pMap, 0, this, SLOT(setImage(const QPixmap)));
        disconnect(pMap, 0, this, SLOT(setHHLimit(int)));
        pMap = 0;
    }

    QPixmap failIcon;

    switch(m_mapInfo.type)
    {
        case MapModel::Invalid:
            failIcon = QPixmap(":/res/btnDisabled.png");
            mapPreview->setIcon(QIcon(failIcon));
            mapPreview->setIconSize(failIcon.size());
            break;
        case MapModel::GeneratedMap:
        case MapModel::GeneratedMaze:
        case MapModel::GeneratedPerlin:
        case MapModel::HandDrawnMap:
            askForGeneratedPreview();
            break;
        default:
            QPixmap mapImage;
            bool success = mapImage.load("physfs://Maps/" + m_mapInfo.name + "/preview.png");

            if(!success)
            {
                mapPreview->setIcon(QIcon());
                return;
            }

            hhLimit = m_mapInfo.limit;
            addInfoToPreview(mapImage);
    }
}

void HWMapContainer::setAllMapParameters(const QString &map, MapGenerator m, int mazesize, const QString &seed, int tmpl, int featureSize)
{
    intSetMapgen(m);
    intSetMazeSize(mazesize);
    intSetSeed(seed);
    intSetTemplateFilter(tmpl);
    // this one last because it will refresh the preview
    intSetMap(map);
    intSetMazeSize(mazesize);
    intSetFeatureSize(featureSize);
}

void HWMapContainer::updateModelViews()
{
    // restore theme selection
    // do this before map selection restore, because map may overwrite theme
    if (!m_theme.isEmpty())
    {
        QModelIndexList mdl = m_themeModel->match(m_themeModel->index(0), Qt::DisplayRole, m_theme);
        if (mdl.size() > 0)
            updateTheme(mdl.at(0));
        else
            setRandomTheme();
    }

    // restore map selection
    if (!m_curMap.isEmpty())
        intSetMap(m_curMap);
    else
        updatePreview();
}


void HWMapContainer::onPreviewMapDestroyed(QObject * map)
{
    if (map == pMap)
        pMap = 0;
}

void HWMapContainer::mapTypeChanged(int index)
{
    changeMapType((MapModel::MapType)cType->itemData(index).toInt());
}

void HWMapContainer::changeMapType(MapModel::MapType type, const QModelIndex & newMap)
{
    staticMapList->hide();
    missionMapList->hide();
    lblMapList->hide();
    generationStyles->hide();
    mazeStyles->hide();
    lblDesc->hide();
    btnLoadMap->hide();
    btnEditMap->hide();
    mapFeatureSize->show();

    switch (type)
    {
        case MapModel::GeneratedMap:
            mapgen = MAPGEN_REGULAR;
            setMapInfo(MapModel::MapInfoRandom);
            lblMapList->setText(tr("Map size:"));
            lblMapList->show();
            generationStyles->show();
            break;
        case MapModel::GeneratedMaze:
            mapgen = MAPGEN_MAZE;
            setMapInfo(MapModel::MapInfoMaze);
            lblMapList->setText(tr("Maze style:"));
            lblMapList->show();
            mazeStyles->show();
            break;
        case MapModel::GeneratedPerlin:
            mapgen = MAPGEN_PERLIN;
            setMapInfo(MapModel::MapInfoPerlin);
            lblMapList->setText(tr("Style:"));
            lblMapList->show();
            mazeStyles->show();
            break;
        case MapModel::HandDrawnMap:
            mapgen = MAPGEN_DRAWN;
            setMapInfo(MapModel::MapInfoDrawn);
            btnLoadMap->show();
            mapFeatureSize->hide();
            btnEditMap->show();
            break;
        case MapModel::MissionMap:
            setupMissionMapsView();
            mapgen = MAPGEN_MAP;
            missionMapChanged(newMap.isValid() ? newMap : missionMapList->currentIndex());
            lblMapList->setText(tr("Mission:"));
            lblMapList->show();
            missionMapList->show();
            mapFeatureSize->hide();
            lblDesc->setText(m_mapInfo.desc);
            lblDesc->show();
            emit mapChanged(m_curMap);
            break;
        case MapModel::StaticMap:
            setupStaticMapsView();
            mapgen = MAPGEN_MAP;
            staticMapChanged(newMap.isValid() ? newMap : staticMapList->currentIndex());
            lblMapList->setText(tr("Map:"));
            lblMapList->show();
            mapFeatureSize->hide();
            staticMapList->show();
            emit mapChanged(m_curMap);
            break;
        default:
            break;
    }

    // Update theme button size
    updateThemeButtonSize();

    // Update cType combobox
    for (int i = 0; i < cType->count(); i++)
    {
        if ((MapModel::MapType)cType->itemData(i).toInt() == type)
        {
            cType->setCurrentIndex(i);
            break;
        }
    }

    repaint();

    emit mapgenChanged(mapgen);
}

void HWMapContainer::intSetFeatureSize(int val)
{
    mapFeatureSize->setValue(val);    
    emit mapFeatureSizeChanged(val);
}
void HWMapContainer::setFeatureSize(int val)
{
    m_mapFeatureSize = val;
    intSetFeatureSize(val);
    //m_mapFeatureSize = val>>2<<2;
    //if (qAbs(m_prevMapFeatureSize-m_mapFeatureSize) > 4)
    {
        m_prevMapFeatureSize = m_mapFeatureSize;
        updatePreview();
    }
}

// unused because I needed the space for the slider
void HWMapContainer::updateThemeButtonSize()
{
    if (m_mapInfo.type != MapModel::StaticMap && m_mapInfo.type != MapModel::HandDrawnMap)
    {
        btnTheme->setIconSize(QSize(30, 30));
        btnTheme->setFixedHeight(30);
    }
    else
    {
        QSize iconSize = btnTheme->icon().actualSize(QSize(65535, 65535));
        btnTheme->setFixedHeight(64);
        btnTheme->setIconSize(iconSize);
    }

    repaint();
}

void HWMapContainer::showThemePrompt()
{
    ThemePrompt prompt(m_themeID, this);
    int theme = prompt.exec() - 1; // Since 0 means canceled, so all indexes are +1'd
    if (theme < 0) return;

    QModelIndex current = m_themeModel->index(theme, 0);
    updateTheme(current);
    emit themeChanged(m_theme);
}

void HWMapContainer::updateTheme(const QModelIndex & current)
{
    m_theme = selectedTheme = current.data(ThemeModel::ActualNameRole).toString();
    m_themeID = current.row();
    QIcon icon = qVariantValue<QIcon>(current.data(Qt::DecorationRole));
    //QSize iconSize = icon.actualSize(QSize(65535, 65535));
    //btnTheme->setFixedHeight(64);
    //btnTheme->setIconSize(iconSize);
    btnTheme->setIcon(icon);
    btnTheme->setText(tr("Theme: %1").arg(current.data(Qt::DisplayRole).toString()));
    updateThemeButtonSize();
}

void HWMapContainer::staticMapChanged(const QModelIndex & map, const QModelIndex & old)
{
    mapChanged(map, 0, old);
}

void HWMapContainer::missionMapChanged(const QModelIndex & map, const QModelIndex & old)
{
    mapChanged(map, 1, old);
}

// Type: 0 = static, 1 = mission
void HWMapContainer::mapChanged(const QModelIndex & map, int type, const QModelIndex & old)
{
    QListView * mapList;

    if (type == 0)      mapList = staticMapList;
    else if (type == 1) mapList = missionMapList;
    else                return;

    // Make sure it is a valid index
    if (!map.isValid())
    {
        if (old.isValid())
        {
            mapList->setCurrentIndex(old);
            mapList->scrollTo(old);
        }
        else
        {
            m_mapInfo.type = MapModel::Invalid;
            updatePreview();
        }

        return;
    }

    // If map changed, update list selection
    if (mapList->currentIndex() != map)
    {
        mapList->setCurrentIndex(map);
        mapList->scrollTo(map);
    }

    Q_ASSERT(map.data(Qt::UserRole + 1).canConvert<MapModel::MapInfo>()); // Houston, we have a problem.
    setMapInfo(map.data(Qt::UserRole + 1).value<MapModel::MapInfo>());
}

void HWMapContainer::setMapInfo(MapModel::MapInfo mapInfo)
{
    m_mapInfo = mapInfo;
    m_curMap = m_mapInfo.name;

    // the map has no pre-defined theme, so let's use the selected one
    if (m_mapInfo.theme.isEmpty())
    {
        if (!selectedTheme.isEmpty())
        {
            setTheme(selectedTheme);
            emit themeChanged(selectedTheme);
        }
    }
    else
    {
        setTheme(m_mapInfo.theme);
        emit themeChanged(m_mapInfo.theme);
    }

    lblDesc->setText(mapInfo.desc);

    updatePreview();
    emit mapChanged(m_curMap);
}

void HWMapContainer::loadDrawing()
{
    QString fileName = QFileDialog::getOpenFileName(NULL, tr("Load drawn map"), ".", tr("Drawn Maps") + " (*.hwmap);;" + tr("All files") + " (*)");

    if(fileName.isEmpty()) return;

    QFile f(fileName);

    if(!f.open(QIODevice::ReadOnly))
    {
        QMessageBox errorMsg(parentWidget());
        errorMsg.setIcon(QMessageBox::Warning);
        errorMsg.setWindowTitle(QMessageBox::tr("File error"));
        errorMsg.setText(QMessageBox::tr("Cannot open '%1' for reading").arg(fileName));
        errorMsg.setWindowModality(Qt::WindowModal);
        errorMsg.exec();
    }
    else
    {
        drawMapScene.decode(qUncompress(QByteArray::fromBase64(f.readAll())));
        mapDrawingFinished();
    }
}

void HWMapContainer::showSeedPrompt()
{
    SeedPrompt prompt(parentWidget()->parentWidget(), getCurrentSeed(), isMaster());
    connect(&prompt, SIGNAL(seedSelected(const QString &)), this, SLOT(setNewSeed(const QString &)));
    prompt.exec();
}

bool HWMapContainer::isMaster()
{
    return m_master;
}

void HWMapContainer::setMaster(bool master)
{
    if (master == m_master) return;
    m_master = master;

    foreach (QWidget *widget, m_childWidgets)
        widget->setEnabled(master);
}

void HWMapContainer::intSetIconlessTheme(const QString & name)
{
    if (name.isEmpty()) return;

    m_theme = name;
    btnTheme->setIcon(QIcon());
    btnTheme->setText(tr("Theme: %1").arg(name));
}

void HWMapContainer::setupMissionMapsView()
{
    if(m_missionsViewSetup) return;
    m_missionsViewSetup = true;

    m_missionMapModel->loadMaps();
    missionMapList->setModel(m_missionMapModel);
    missionMapList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QItemSelectionModel * missionSelectionModel = missionMapList->selectionModel();
    connect(missionSelectionModel,
            SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex &)),
            this,
            SLOT(missionMapChanged(const QModelIndex &, const QModelIndex &)));
    missionSelectionModel->setCurrentIndex(m_missionMapModel->index(0, 0), QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);
}

void HWMapContainer::setupStaticMapsView()
{
    if(m_staticViewSetup) return;
    m_staticViewSetup = true;

    m_staticMapModel->loadMaps();
    staticMapList->setModel(m_staticMapModel);
    staticMapList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QItemSelectionModel * staticSelectionModel = staticMapList->selectionModel();
    connect(staticSelectionModel,
            SIGNAL(currentRowChanged(const QModelIndex &, const QModelIndex &)),
            this,
            SLOT(staticMapChanged(const QModelIndex &, const QModelIndex &)));
    staticSelectionModel->setCurrentIndex(m_staticMapModel->index(0, 0), QItemSelectionModel::Clear | QItemSelectionModel::SelectCurrent);
}
