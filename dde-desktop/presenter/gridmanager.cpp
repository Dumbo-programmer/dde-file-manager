/**
 * Copyright (C) 2016 Deepin Technology Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 **/

#include "gridmanager.h"

#include <QPoint>
#include <QRect>
#include <QDebug>
#include <QStandardPaths>

#include <dfilesystemmodel.h>
#include "apppresenter.h"
#include "dfileservices.h"
#include "../config/config.h"
#include "dabstractfileinfo.h"
#include "screen/screenhelper.h"
#include "interfaces/private/mergeddesktop_common_p.h"

#if 0 //多屏图标自定义配置做修改： older
class GridManagerPrivate
{
public:
    GridManagerPrivate()
    {
        auto settings = Config::instance()->settings();
        settings->beginGroup(Config::groupGeneral);
        positionProfile = settings->value(Config::keyProfile).toString();
        autoArrange = settings->value(Config::keyAutoAlign).toBool();
        autoMerge = settings->value(Config::keyAutoMerge, false).toBool();
        settings->endGroup();

        coordWidth = 0;
        coordHeight = 0;
    }

    inline int cellCount() const
    {
        return  coordHeight * coordWidth;
    }

    inline void clear()
    {
        m_itemGrids.clear();
        m_gridItems.clear();
        m_overlapItems.clear();

        for (int i = 0; i < m_cellStatus.length(); ++i) {
            m_cellStatus[i] = false;
        }
    }

    QStringList rangeItems()
    {
        QStringList sortItems;

        auto inUsePos = m_gridItems.keys();
        qSort(inUsePos.begin(), inUsePos.end(), qQPointLessThanKey);

        for (int index = 0; index < inUsePos.length(); ++index) {
            auto gridPos = inUsePos.value(index);
            auto item = m_gridItems.value(gridPos);
            sortItems << item;
        }

        sortItems << m_overlapItems;
        return sortItems;
    }

    void arrange()
    {
        QStringList sortItems;

        auto inUsePos = m_gridItems.keys();
        qSort(inUsePos.begin(), inUsePos.end(), qQPointLessThanKey);

        for (int index = 0; index < inUsePos.length(); ++index) {
            auto gridPos = inUsePos.value(index);
            auto item = m_gridItems.value(gridPos);
            sortItems << item;
        }

        auto overlapItems = m_overlapItems;

        auto emptyCellCount = cellCount() - sortItems.length();
        for (int i = 0; i < emptyCellCount; ++i) {
            if (!overlapItems.isEmpty()) {
                sortItems << overlapItems.takeFirst();
            }
        }

        clear();

        for (auto item : sortItems) {
            QPoint empty_pos{ takeEmptyPos() };
            add(empty_pos, item);
        }

        m_overlapItems = overlapItems;
    }

    void createProfile()
    {
        m_cellStatus.resize(coordWidth * coordHeight);
        clear();
    }

    void loadProfile(const QList<DAbstractFileInfoPointer> &fileInfoList)
    {
        QMap<QString, int> existItems;

        for (const DAbstractFileInfoPointer &info : fileInfoList) {
            existItems.insert(info->fileUrl().toString(), 0);
        }

        auto settings = Config::instance()->settings();
        settings->beginGroup(positionProfile);
        for (auto &key : settings->allKeys()) {
            auto coords = key.split("_");
            auto x = coords.value(0).toInt();
            auto y = coords.value(1).toInt();
            auto item = settings->value(key).toString();
            if (existItems.contains(item)) {
                QPoint pos{x, y};
                add(pos, item);
                existItems.remove(item);
            }
        }
        settings->endGroup();

        for (auto &item : existItems.keys()) {
            QPoint empty_pos{ takeEmptyPos() };
            add(empty_pos, item);
        }

        if (autoArrange || autoMerge) {
            arrange();
        }
    }

    void loadWithoutProfile(const QList<DAbstractFileInfoPointer> &fileInfoList)
    {
        QMap<QString, int> existItems;

        for (const DAbstractFileInfoPointer &info : fileInfoList) {
            QPoint empty_pos{ takeEmptyPos() };
            add(empty_pos, info->fileUrl().toString());
        }

        if (autoArrange || autoMerge) {
            arrange();
        }
    }

    inline bool isValid(QPoint pos) const
    {
        QRect rect(0, 0, coordWidth, coordHeight);
        return rect.contains(pos);
    }

    inline QPoint overlapPos() const
    {
        return QPoint(coordWidth - 1, coordHeight - 1);
    }

    inline QPoint gridPosAt(int index) const
    {
        auto x = index / coordHeight;
        auto y = index % coordHeight;
        return QPoint(x, y);
    }

    inline int indexOfGridPos(const QPoint &pos) const
    {
        return pos.x() * coordHeight + pos.y();
    }

    inline QPoint emptyPos() const
    {
        for (int i = 0; i < m_cellStatus.size(); ++i) {
            if (!m_cellStatus[i]) {
                return gridPosAt(i);
            }
        }
        return overlapPos();
    }

    inline QPoint takeEmptyPos()
    {
        for (int i = 0; i < m_cellStatus.size(); ++i) {
            if (!m_cellStatus[i]) {
                m_cellStatus[i] = true;
                return gridPosAt(i);
            }
        }
        return overlapPos();
    }

    inline bool add(QPoint pos, const QString &itemId)
    {
        if (itemId.isEmpty()) {
            qCritical() << "add empty item"; // QVector<QString>.value() may retruen an empty QString
            return false;
        } else if (m_itemGrids.contains(itemId)) {
            qCritical() << "add" << itemId  << "failed."
                        << m_itemGrids.value(itemId) << "grid exist item";
            return false;
        }

        if (m_gridItems.contains(pos)) {
            if (pos != overlapPos()) {
                qCritical() << "add" << itemId  << "failed."
                            << pos << "grid exist item" << m_gridItems.value(pos);
                return false;
            } else {
                if (!m_overlapItems.contains(itemId)) {
                    m_overlapItems << itemId;
                }
                return false;
            }
        }

        m_gridItems.insert(pos, itemId);
        m_itemGrids.insert(itemId, pos);
        int index = indexOfGridPos(pos);
        if (m_cellStatus.length() <= index) {
            return false;
        }
        m_cellStatus[index] = true;

        return true;
    }

    QPair<QStringList, QVariantList> generateProfileConfigVariable()
    {
        QStringList keyList;
        QVariantList valueList;
        for (auto pos : m_gridItems.keys()) {
            keyList << positionKey(pos);
            valueList << m_gridItems.value(pos);
        }

        return QPair<QStringList, QVariantList>(keyList, valueList);
    }

    inline void syncProfile()
    {
        QPair<QStringList, QVariantList> kvList = generateProfileConfigVariable();

        if (m_gridItems.size() != m_itemGrids.size()) {
            qCritical() << "data sync failed";
            qCritical() << "-----------------------------";
            qCritical() << m_gridItems << m_itemGrids << kvList.first;
            qCritical() << "-----------------------------";
        }

        if (kvList.first.size() != m_gridItems.size()) {
            qCritical() << "data sync failed";
            qCritical() << "-----------------------------";
            qCritical() << m_gridItems << m_itemGrids << kvList.first;
            qCritical() << "-----------------------------";
        }

        emit Presenter::instance()->removeConfig(positionProfile, "");
        emit Presenter::instance()->setConfigList(positionProfile, kvList.first, kvList.second);
    }

    inline bool remove(QPoint pos, const QString &id)
    {
        m_overlapItems.removeAll(id);
        if (!m_itemGrids.contains(id)) {
            qDebug() << "can not remove" << pos << id;
            return false;
        }

        m_gridItems.remove(pos);
        m_itemGrids.remove(id);

        auto usageIndex = indexOfGridPos(pos);
        m_cellStatus[usageIndex] = false;

        if (!m_overlapItems.isEmpty()
                && (pos == overlapPos())) {
            auto itemId = m_overlapItems.takeFirst();
            add(pos, itemId);
        }
        return true;
    }

    inline void resetGridSize(int w, int h)
    {
        Q_ASSERT(!(coordHeight == h && coordWidth == w));
        coordWidth = w;
        coordHeight = h;

        createProfile();

        auto profile = QString("Position_%1x%2").arg(w).arg(h);
        if (profile != positionProfile) {
            positionProfile = profile;
        }
    }

    inline void changeGridSizeScared(int w, int h)
    {
        Q_ASSERT(!(coordHeight == h && coordWidth == w));

        qDebug() << "old grid size" << coordHeight << coordWidth;
        qDebug() << "new grid size" << w << h;

        auto oldCellCount = coordHeight * coordWidth;
        auto newCellCount = w * h;

        auto allItems = m_itemGrids;
        QVector<int> preferNewIndex;
        QVector<QString> itemIds;

        // record old pos index
        for (int i = 0; i < m_cellStatus.length(); ++i) {
            if (m_cellStatus.value(i)) {
                auto newIndex = i * newCellCount / oldCellCount;
                preferNewIndex.push_back(newIndex);
                itemIds.push_back(m_gridItems.value(gridPosAt(i)));
            }
        }

        coordHeight = h;
        coordWidth = w;

        createProfile();

        auto profile = QString("Position_%1x%2").arg(w).arg(h);
        if (profile != positionProfile) {
            positionProfile = profile;
        }

        for (int i = 0; i < preferNewIndex.length(); ++i) {
            auto index = preferNewIndex.value(i);
            if (m_cellStatus.size() > index && !m_cellStatus.value(index)) {
                QPoint pos{ gridPosAt(index) };
                add(pos, itemIds.value(i));
            } else {
                auto freePos = takeEmptyPos();
                add(freePos, itemIds.value(i));
            }
            allItems.remove(itemIds.value(i));
        }

        // move forward
        for (auto id : allItems.keys()) {
            auto freePos = takeEmptyPos();
            add(freePos, id);
        }
    }

    inline void changeGridSize(int w, int h)
    {
        Q_ASSERT(!(coordHeight == h && coordWidth == w));

//        qDebug() << "old grid size" << coordHeight << coordWidth;
//        qDebug() << "new grid size" << w << h;
        auto oldCellCount = coordHeight * coordWidth;
        auto newCellCount = w * h;

        auto allItems = m_itemGrids;

        auto outCellCount = 0;
        for (int i = 0; i < m_cellStatus.length(); ++i) {
            if (i >= newCellCount && m_cellStatus.value(i)) {
                outCellCount++;
            }
        }

        auto oldCellStatus = this->m_cellStatus;

        // find empty cell count
        auto indexEnd = qMin(oldCellCount, newCellCount);
        auto emptyCellCount = 0;
        for (int i = 0; i < indexEnd; ++i) {
            if (!oldCellStatus.value(i)) {
                emptyCellCount++;
            }
        }

        if (newCellCount > oldCellCount) {
            emptyCellCount += (newCellCount - oldCellCount);
        }

        if (emptyCellCount <= outCellCount + m_overlapItems.length()) {
//            qDebug() << "arrange";
            auto sortItems = rangeItems();
            resetGridSize(w, h);
            for (int i = 0; i < newCellCount; ++i) {
                add(takeEmptyPos(), sortItems.takeFirst());
            }
            m_overlapItems = sortItems;
        } else {
            // find start pos
            auto newEmptyCellCount = emptyCellCount - outCellCount + m_overlapItems.length();
            QVector<int> keepPosIndex;
            QVector<QString> keepItems;

            auto lastEmptyPosIndex = newCellCount;
            for (int i = 0; i < oldCellStatus.length(); ++i) {
                if (oldCellStatus.value(i)) {
                    keepPosIndex.push_back(i);
                    keepItems.push_back(m_gridItems.value(gridPosAt(i)));
                } else {
                    if (newEmptyCellCount <= 0) {
                        lastEmptyPosIndex = i;
                        break;
                    }
                    --newEmptyCellCount;
                }
            }

            QVector<int> nokeepPosIndex;
            QVector<QString> nokeepItems;
            for (int i = lastEmptyPosIndex; i < oldCellStatus.length(); ++i) {
                if (oldCellStatus.value(i)) {
                    nokeepPosIndex.push_back(i);
                    nokeepItems.push_back(m_gridItems.value(gridPosAt(i)));
                }
            }

            auto overlapItems = m_overlapItems;

            resetGridSize(w, h);

            for (int i = 0; i < keepPosIndex.length(); ++i) {
                auto index = keepPosIndex.value(i);
                if (m_cellStatus.size() > index && !m_cellStatus.value(index)) {
                    QPoint pos{ gridPosAt(index) };
                    add(pos, keepItems.value(i));
                }
            }

            for (int i = 0; i < nokeepItems.length(); ++i) {
                QPoint pos{ gridPosAt(lastEmptyPosIndex) };
                add(pos, nokeepItems.value(i));
                lastEmptyPosIndex++;
            }

            // TODO
            for (auto &item : overlapItems) {
                QPoint pos{ takeEmptyPos() };
                add(pos, item);
            }
        }
    }

    bool updateGridSize(int w, int h)
    {
        if (coordHeight == h && coordWidth == w) {
            qWarning() << "profile not changed";
            return false;
        }

        if (0 == coordWidth && 0 == coordHeight) {
            resetGridSize(w, h);
            return false;
        } else {
            qDebug() << "change grid from" << coordWidth << coordHeight
                     << "to" << w << h;
            changeGridSize(w, h);

            // check if we should update grid profile.
            if (autoMerge) {
                return this->autoArrange;
            }

            QPair<QStringList, QVariantList> kvList = generateProfileConfigVariable();

            qDebug() << "updateGridProfile:" << kvList.first.size()
                     << m_gridItems.size() << m_itemGrids.size();

            emit Presenter::instance()->removeConfig(positionProfile, "");
            emit Presenter::instance()->setConfigList(positionProfile, kvList.first, kvList.second);

            return this->autoArrange;
        }
    }


    inline void setWhetherShowHiddenFiles(bool value)noexcept
    {
        m_whetherShowHiddenFiles.store(value, std::memory_order_release);
    }

    inline bool getWhetherShowHiddenFiles()noexcept
    {
        return m_whetherShowHiddenFiles.load(std::memory_order_consume);
    }

public:
    QStringList             m_overlapItems;
    QMap<QPoint, QString>   m_gridItems;
    QMap<QString, QPoint>   m_itemGrids;
    QVector<bool>           m_cellStatus;

    QString                 positionProfile;
    int                     coordWidth;
    int                     coordHeight;

    bool                    autoArrange;
    bool                    autoMerge = false;

    std::atomic<bool>       m_whetherShowHiddenFiles{ false };
};

GridManager::GridManager(): d(new GridManagerPrivate)
{
}

GridManager::~GridManager()
{

}

void GridManager::initProfile(const QList<DAbstractFileInfoPointer> &items)
{
    d->createProfile();
    d->loadProfile(items);
}

// init WITHOUT grid item position data from the config file.
void GridManager::initWithoutProfile(const QList<DAbstractFileInfoPointer> &items)
{
    d->createProfile(); // nothing with profile.
    d->loadWithoutProfile(items);
}

bool GridManager::add(const QString &id)
{
#ifdef QT_DEBUG
    qDebug() << "show hidden files: " << d->getWhetherShowHiddenFiles() << id;
#endif //QT_DEBUG
    DUrl url(id);

    if (!d->getWhetherShowHiddenFiles()) {
        const DAbstractFileInfoPointer info = DFileService::instance()->createFileInfo(nullptr, url);

        if (info->isHidden()) {
            return false;
        }
    }

    if (d->m_itemGrids.contains(id)) {
//        qDebug() << "item exist item" << d->itemGrids.value(id) << id;
        return false;
    }

    QPoint pos{ d->takeEmptyPos() };
    return add(pos, id);
}

bool GridManager::add(QPoint pos, const QString &id)
{
    qDebug() << "add" << pos << id;
    auto ret = d->add(pos, id);
    if (ret && !autoMerge()) {
        d->syncProfile();
    }

    return ret;
}

bool GridManager::move(const QStringList &selecteds, const QString &current, int x, int y)
{
    auto currentPos = d->m_itemGrids.value(current);
    auto destPos = QPoint(x, y);
    auto offset = destPos - currentPos;

    QList<QPoint> originPosList;
    QList<QPoint> destPosList;
    // check dest is empty;
    auto destPosMap = d->m_gridItems;
    auto destUsedGrids = d->m_cellStatus;
    for (auto &id : selecteds) {
        auto oldPos = d->m_itemGrids.value(id);
        originPosList << oldPos;
        destPosMap.remove(oldPos);
        destUsedGrids[d->indexOfGridPos(oldPos)] = false;
        auto destPos = oldPos + offset;
        destPosList << destPos;
    }

    bool conflict = false;
    for (auto pos : destPosList) {
        if (destPosMap.contains(pos) || !d->isValid(pos)) {
            conflict = true;
            break;
        }
    }

    // no need to resize
    if (conflict) {
        auto selectedHeadCount = selecteds.indexOf(current);
        // find free grid before destPos
        auto destIndex = d->indexOfGridPos(destPos);

        QList<int> emptyIndexList;

        for (int  i = 0; i < d->cellCount(); ++i) {
            if (false == destUsedGrids.value(i)) {
                emptyIndexList << i;
            }
        }
        auto destGridHeadCount = emptyIndexList.indexOf(destIndex);

        Q_ASSERT(emptyIndexList.length() >= selecteds.length());

        auto startIndex = emptyIndexList.indexOf(destIndex) - selectedHeadCount;
        if (destGridHeadCount < selectedHeadCount) {
            startIndex = 0;
        }
        auto destTailCount = emptyIndexList.length() - destGridHeadCount;
        auto selectedTailCount = selecteds.length() - selectedHeadCount;
        if (destTailCount <= selectedTailCount) {
            startIndex = emptyIndexList.length() - selecteds.length();
        }

        destPosList.clear();

        startIndex = emptyIndexList.value(startIndex);
        for (int i = startIndex; i < d->cellCount(); ++i) {
            if (false == destUsedGrids.value(i)) {
                destPosList << d->gridPosAt(i);
            }
        }
    }

    for (int i = 0; i < selecteds.length(); ++i) {
        if (contains(selecteds.value(i))) {
            remove(selecteds.value(i));
        }
    }
    for (int i = 0; i < selecteds.length(); ++i) {
        QPoint point{ destPosList.value(i) };
        add(point, selecteds.value(i));
    }

    if (shouldArrange()) {
        reArrange();
    }

    return true;
}

bool GridManager::remove(const QString &id)
{
    auto pos = d->m_itemGrids.value(id);
    return remove(pos, id);
}

bool GridManager::remove(int x, int y, const QString &id)
{
    auto pos = QPoint(x, y);
    return remove(pos, id);
}

bool GridManager::remove(QPoint pos, const QString &id)
{
    auto ret = d->remove(pos, id);
    if (ret && !autoMerge()) {
        d->syncProfile();
    }
    return ret;
}

bool GridManager::clear()
{
    d->createProfile();

    emit Presenter::instance()->removeConfig(d->positionProfile, "");

    return true;
}

QString GridManager::firstItemId()
{
    for (int i = 0; i < d->m_cellStatus.length(); ++i) {
        if (d->m_cellStatus.value(i)) {
            auto pos = d->gridPosAt(i);
            return  itemId(pos);
        }
    }
    return "";
}

QString GridManager::lastItemId()
{
    auto len = d->m_cellStatus.length();
    for (int i = 0; i < len; ++i) {
        if (d->m_cellStatus.value(len - 1 - i)) {
            auto pos = d->gridPosAt(len - 1 - i);
            return  itemId(pos);
        }
    }
    return "";
}

QStringList GridManager::itemIds()
{
    QStringList ids;
    for (int i = 0; i < d->m_cellStatus.length(); ++i) {
        if (d->m_cellStatus.value(i)) {
            auto pos = d->gridPosAt(i);
            ids.append(itemId(pos));
        }
    }
    ids << d->m_overlapItems;
    return ids;
}

bool GridManager::contains(const QString &id)
{
    return d->m_itemGrids.contains(id) || d->m_overlapItems.contains(id);
}

QPoint GridManager::position(const QString &id)
{
    if (!d->m_itemGrids.contains(id)) {
        return d->overlapPos();
    }

    return d->m_itemGrids.value(id);
}

QString GridManager::itemId(int x, int y)
{
    return d->m_gridItems.value(QPoint(x, y));
}

QString GridManager::itemId(QPoint pos)
{
    return d->m_gridItems.value(pos);
}

bool GridManager::isEmpty(int x, int y)
{
    return !d->m_cellStatus.value(d->indexOfGridPos(QPoint(x, y)));
}

const QStringList &GridManager::overlapItems() const
{
    return d->m_overlapItems;
}

bool GridManager::shouldArrange() const
{
    return d->autoArrange || d->autoMerge;
}

bool GridManager::autoArrange() const
{
    return d->autoArrange;
}

bool GridManager::autoMerge() const
{
    return d->autoMerge;
}

void GridManager::toggleArrange()
{
    d->autoArrange = !d->autoArrange;

    if (!d->autoArrange) {
        return;
    }

    reArrange();
}

void GridManager::setAutoMerge(bool enable)
{
    if (d->autoMerge == enable) return;
    d->autoMerge = enable;
}

void GridManager::toggleAutoMerge()
{
    setAutoMerge(!d->autoMerge);
}

void GridManager::reArrange()
{
    d->arrange();

    if (autoMerge()) {
        return;
    }

    QPair<QStringList, QVariantList> kvList = d->generateProfileConfigVariable();

    emit Presenter::instance()->removeConfig(d->positionProfile, "");
    emit Presenter::instance()->setConfigList(d->positionProfile, kvList.first, kvList.second);
}

int GridManager::gridCount() const
{
    return d->coordWidth * d->coordHeight;
}

QPoint GridManager::forwardFindEmpty(QPoint start) const
{
    auto size = gridSize();
    for (int j = start.y(); j < size.height(); ++j) {
        if (GridManager::instance()->isEmpty(start.x(), j)) {
            return QPoint(start.x(), j);
        }
    }

    for (int i = start.x() + 1; i < size.width(); ++i) {
        for (int j = 0; j < size.height(); ++j) {
            if (GridManager::instance()->isEmpty(i, j)) {
                return QPoint(i, j);
            }
        }
    }
    return d->emptyPos();
}

QSize GridManager::gridSize() const
{
    return QSize(d->coordWidth, d->coordHeight);
}

void GridManager::updateGridSize(int w, int h)
{
    if (d->updateGridSize(w, h)) {
        d->arrange();
    }

    emit Presenter::instance()->setConfig(Config::groupGeneral,
                                          Config::keyProfile,
                                          d->positionProfile);
}

GridCore *GridManager::core()
{
    auto core = new GridCore;
    core->overlapItems = d->m_overlapItems;
    core->gridItems = d->m_gridItems;
    core->itemGrids = d->m_itemGrids;
    core->gridStatus = d->m_cellStatus;
    core->coordWidth = d->coordWidth;
    core->coordHeight = d->coordHeight;
    return core;
}

void GridManager::setWhetherShowHiddenFiles(bool value) noexcept
{
    d->setWhetherShowHiddenFiles(value);
}

bool GridManager::getWhetherShowHiddenFiles() noexcept
{
    return d->getWhetherShowHiddenFiles();
}

void GridManager::dump()
{
    for (auto key : d->m_gridItems.keys()) {
        qDebug() << key << d->m_gridItems.value(key);
    }

    for (auto key : d->m_itemGrids.keys()) {
        qDebug() << key << d->m_itemGrids.value(key);
    }
}
#endif
#if 1 ////多屏图标自定义配置做修改： newer
class GridManagerPrivate
{
public:
    GridManagerPrivate()
    {
        auto settings = Config::instance()->settings();
        settings->beginGroup(Config::groupGeneral);
        autoArrange = settings->value(Config::keyAutoAlign).toBool();
        autoMerge = settings->value(Config::keyAutoMerge, false).toBool();
        settings->endGroup();
    }

    inline int cellCount(int screenNum) const
    {
        auto coordInfo = screensCoordInfo.value(screenNum);
        return  coordInfo.second * coordInfo.first;
    }

    void clear()
    {
        m_itemGrids.clear();
        m_gridItems.clear();
        m_overlapItems.clear();
        m_cellStatus.clear();

        for(int i : screenCode()){

            //add empty m_itemGrids
            QMap<QPoint, QString> gridItem;
            m_gridItems.insert(i, gridItem);

            //add empty m_gridItems
            QMap<QString, QPoint> itemGrid;
            m_itemGrids.insert(i, itemGrid);

            m_screenFullStatus.insert(i, false);
        }

        auto coordkeys = screensCoordInfo.keys();
        for (auto key : coordkeys) {
            //add empty coordInfo
            QVector<bool> tempVector;
            auto coordInfo = screensCoordInfo.value(key);
            tempVector.resize(coordInfo.first * coordInfo.second);
            m_cellStatus.insert(key, tempVector);
        }
        auto statusValues = m_cellStatus.values();
        for (auto &oneValue : statusValues) {
            for (int i = 0; i < oneValue.length(); ++i) {
                oneValue[i] = false;
            }
        }
    }

    QStringList rangeItems(int screenNum)
    {
        QStringList sortItems;

        auto inUsePos = m_gridItems.value(screenNum).keys();
        qSort(inUsePos.begin(), inUsePos.end(), qQPointLessThanKey);

        for (int index = 0; index < inUsePos.length(); ++index) {
            auto gridPos = inUsePos.value(index);
            auto item = m_gridItems.value(screenNum).value(gridPos);
            sortItems << item;
        }

        sortItems << m_overlapItems;
        return sortItems;
    }

    void arrange(QStringList sortedItems)
    {
        auto screenOrder = screenCode();
        QTime t;
        t.start();
        qDebug() << "screen count" << screenOrder.size();
        for (int screenNum : screenOrder) {
            qDebug() << "arrange Num" << screenNum << sortedItems.size();
            QMap<QPoint, QString> gridItems;
            QMap<QString, QPoint> itemGrids;
            if (!sortedItems.empty())
            {
                auto cellStatus = m_cellStatus.value(screenNum);
                int coordHeight = screensCoordInfo.value(screenNum).second;
                int i = 0;
                for (; i < cellStatus.size() && !sortedItems.empty(); ++i) {
                   QString item = sortedItems.takeFirst();
                   QPoint pos(i / coordHeight, i % coordHeight);
                   cellStatus[i] = true;
                   gridItems.insert(pos, item);
                   itemGrids.insert(item, pos);
                }
                m_cellStatus.insert(screenNum, cellStatus);
                qDebug() << "screen" << screenNum << "put item:" << i << "cell" << cellStatus.size();
            }

            m_gridItems.insert(screenNum,gridItems);
            m_itemGrids.insert(screenNum,itemGrids);
        }
        qDebug() << "time " << t.elapsed() << "(ms) overlapItems " << sortedItems.size();
        m_overlapItems = sortedItems;
    }

    void arrange()
    {
//        QStringList sortItems;

//        auto inUsePos = m_gridItems.value(screenNum).keys();
//        qSort(inUsePos.begin(), inUsePos.end(), qQPointLessThanKey);

//        for (int index = 0; index < inUsePos.length(); ++index) {
//            auto gridPos = inUsePos.value(index);
//            auto item = m_gridItems.value(screenNum).value(gridPos);
//            sortItems << item;
//        }

//        auto overlapItems = m_overlapItems;

//        auto emptyCellCount = cellCount(screenNum) - sortItems.length();
//        for (int i = 0; i < emptyCellCount; ++i) {
//            if (!overlapItems.isEmpty()) {
//                sortItems << overlapItems.takeFirst();
//            }
//        }

//        clear();

//        for (auto item : sortItems) {
//            QPair<int, QPoint> emptyPosPair{ takeEmptyPos() };
//            add(emptyPosPair.first, emptyPosPair.second, item);
//        }

//        m_overlapItems = overlapItems;
    }

    void createProfile()
    {
        //m_cellStatus.resize(coordWidth * coordHeight);
        if(0 == createProfileTime)
            clear();
        createProfileTime++;
    }

    void loadProfile(const QList<DAbstractFileInfoPointer> &fileInfoList)
    {
        //加载配置文件位置信息，此加载应当加载所有，通过add来将不同屏幕图标信息加载到m_gridItems和m_itemGrids
        QMap<QString, int> existItems;//实际存在的文件
        for (const DAbstractFileInfoPointer &info : fileInfoList) {
            existItems.insert(info->fileUrl().toString(), 0);
        }

        //获取对应屏幕分组信息
//        auto settings = Config::instance()->settings();
//        settings->beginGroup(Config::keyProfile);
//        QStringList screenNumbers = settings->allKeys();
//        QMap<int, QString> screenNumProfiles;
//        for(auto &key : screenNumbers){
//            screenNumProfiles.insert(key.toInt(), settings->value(key).toString());
//        }
//        settings->endGroup();

        QMap<int, QString> screenNumProfiles;
        if(m_bSingleMode){
            screenNumProfiles.insert(1, QString("SinglePosition"));
        }else {
            QList<int> screens = screenCode();
            foreach(int index, screens){
                screenNumProfiles.insert(index, QString("Position_%1").arg(index));
            }
        }
        //屏幕个数与配置文件不符合时直接自动排列
//        if(ScreenMrg->screens().size() != screenNumbers.size()){
//            //调用自动整理接口
//            return;
//        }

        //获取个屏幕分组信息对应的图标信息
        auto settings = Config::instance()->settings();
        for (auto &screenKey : screenNumProfiles.keys()) {
            settings->beginGroup(screenNumProfiles.value(screenKey));
            for (auto &key : settings->allKeys()) {
                auto coords = key.split("_");
                auto x = coords.value(0).toInt();
                auto y = coords.value(1).toInt();
                auto item = settings->value(key).toString();
                if (existItems.contains(item)) {
                    QPoint pos{x, y};
                    add(screenKey, pos, item);
                    existItems.remove(item);
                }
            }
            settings->endGroup();
        }

        for (auto &item : existItems.keys()) {
            QPair<int, QPoint> empty_pos{ takeEmptyPos() };
            add(empty_pos.first,empty_pos.second, item);
        }
    }

    void loadWithoutProfile(const QList<DAbstractFileInfoPointer> &fileInfoList)
    {
        QMap<QString, int> existItems;
        //无profile文件加载时按照屏幕顺序进行方式
        for (const DAbstractFileInfoPointer &info : fileInfoList) {
            QPair<int, QPoint> empty_pos{ takeEmptyPos() };
            add(empty_pos.first, empty_pos.second, info->fileUrl().toString());
        }

        if (autoArrange || autoMerge) {
            arrange();
        }
    }


    inline bool isValid(int screenNum, QPoint pos) const
    {
        auto coordInfo= screensCoordInfo.value(screenNum);
        QRect rect(0, 0, coordInfo.first, coordInfo.second);
        return rect.contains(pos);
    }

    inline QPoint overlapPos(int screenNum) const
    {
        auto coordInfo= screensCoordInfo.value(screenNum);
        return QPoint(coordInfo.first - 1, coordInfo.second - 1);
    }

    inline QPoint gridPosAt(int screenNum, int index) const
    {
        auto coordHeight = screensCoordInfo.value(screenNum).second;
        auto x = index / coordHeight;
        auto y = index % coordHeight;
        return QPoint(x, y);
    }

    inline int indexOfGridPos(int screenNum, const QPoint &pos) const
    {
        //return pos.x() * coordHeight + pos.y();
        auto oneScreenCoord = screensCoordInfo.value(screenNum);
        if(0 == oneScreenCoord.first)
            return 0;
        else {
            return pos.x() * oneScreenCoord.second + pos.y();
        }
    }

    QPair<int, QPoint> emptyPos() const
    {
        //抽出空位屏编号
        int emptyScreenNum = -1;
        for(auto &key : m_screenFullStatus.keys()){
            if(!m_screenFullStatus.value(key)){
                emptyScreenNum = key;
                break;
            }
        }
        //返回空位屏编号
        QPair<int, QPoint> posPair;
        if(-1 != emptyScreenNum){
            auto cellStatus = m_cellStatus.value(emptyScreenNum);
            for (int i = 0; i < cellStatus.size(); ++i) {
                if (!cellStatus[i]) {
                    posPair.first = emptyScreenNum;
                    posPair.second = gridPosAt(emptyScreenNum, i);
                    return posPair;
                }
            }
        }

        posPair.first = m_cellStatus.lastKey();
        posPair.second = overlapPos(posPair.first);
        return  posPair;
    }

    QPair<int, QPoint> takeEmptyPos()
    {
        //兼容单屏幕多屏下获取空位，若无返回重叠位（暂时堆叠在(w-1,h-1)）
        //新的多屏功能下，图标的发展方向的优先级暂时考虑为主屏>编号递增的方式
        //即一个屏满后，下一个优先查看主屏是否还有空缺，若主屏也满了则顺序后延

        //我们存就是按照编号来的，所以直接按照编号遍历即可
        //抽出空位屏编号
        int emptyScreenNum = -1;
        for(auto &key : m_screenFullStatus.keys()){
            if(!m_screenFullStatus.value(key)){
                emptyScreenNum = key;
                break;
            }
        }

        //返回空位屏以及编号
        QPair<int, QPoint> posPair;
        if(-1 != emptyScreenNum){
            auto cellStatus = m_cellStatus.value(emptyScreenNum);
            for (int i = 0; i < cellStatus.size(); ++i) {
                if (!cellStatus[i]) {
                    cellStatus[i] = true;
                    posPair.first = emptyScreenNum;
                    posPair.second = gridPosAt(emptyScreenNum, i);
                    return posPair;
                }
            }
        }

        if(!m_cellStatus.isEmpty()){
            posPair.first = m_cellStatus.lastKey();
            posPair.second = overlapPos(posPair.first);
        }
        return  posPair;
    }

    bool add(int screenNum, QPoint pos, const QString &itemId)
    {
        if (itemId.isEmpty()) {
            qCritical() << "add empty item"; // QVector<QString>.value() may retruen an empty QString
            return false;
        } else if (m_itemGrids.value(screenNum).contains(itemId)) {
            qCritical() << "add" << itemId  << "failed."
                        << m_itemGrids.value(screenNum).value(itemId) << "grid exist item";
            return false;
        }

        if (m_gridItems.value(screenNum).contains(pos)) {
            if (pos != overlapPos(screenNum)) {
                qCritical() << "add" << itemId  << "failed."
                            << pos << "grid exist item in screenNun "<< screenNum << "-" << m_gridItems.value(screenNum).value(pos);
                return false;
            } else {
                if (!m_overlapItems.contains(itemId)) {
                    m_overlapItems << itemId;
                }
                return false;
            }
        }

        if(m_gridItems.end() != m_gridItems.find(screenNum)){
            m_gridItems.find(screenNum)->insert(pos, itemId);
        }
        if(m_itemGrids.end() != m_itemGrids.find(screenNum)){
            m_itemGrids.find(screenNum)->insert(itemId, pos);
        }

//         m_gridItems.find(screenNum)->insert(pos, itemId);
//         m_itemGrids.find(screenNum)->insert(itemId, pos);
//        auto tempGridItemsItor = m_gridItems.find(screenNum);
//        auto tempItemGridsItor = m_itemGrids.find(screenNum);
//        tempGridItemsItor->insert(pos, itemId);
//        tempItemGridsItor->insert(itemId, pos);

        int index = indexOfGridPos(screenNum, pos);
        if (m_cellStatus.value(screenNum).length() <= index) {
            return false;
        }
        if(!m_cellStatus.contains(screenNum))
            return false;
        auto cellStatusItor = m_cellStatus.find(screenNum);
        cellStatusItor.value()[index] = true;
        return true;
    }

    QPair<QStringList, QVariantList> generateProfileConfigVariable(int screenNum)
    {
        //根据屏幕编号获取对应屏幕图标信息
        QStringList keyList;
        QVariantList valueList;
        QMap<QPoint, QString> onceScreenGridItems = m_gridItems.value(screenNum);
        for (auto pos : onceScreenGridItems.keys()) {
            keyList << positionKey(pos);
            valueList << onceScreenGridItems.value(pos);
        }

        return QPair<QStringList, QVariantList>(keyList, valueList);
    }

    void syncProfile(int screenNum)
    {
        QPair<QStringList, QVariantList> kvList = generateProfileConfigVariable(screenNum);

        if (m_gridItems.value(screenNum).size() != m_itemGrids.value(screenNum).size()) {
            qCritical() << "data sync failed";
            qCritical() << "-----------------------------";
            qCritical() << m_gridItems << m_itemGrids << kvList.first;
            qCritical() << "-----------------------------";
        }

        if (kvList.first.size() != m_gridItems.value(screenNum).size()) {
            qCritical() << "data sync failed";
            qCritical() << "-----------------------------";
            qCritical() << m_gridItems << m_itemGrids << kvList.first;
            qCritical() << "-----------------------------";
        }
        //positionProfile需要调整一下，添加上屏幕编号 :Position_%1_%2x%3
        auto screenPositionProfile = positionProfiles.value(screenNum);
        emit Presenter::instance()->removeConfig(screenPositionProfile, "");
        emit Presenter::instance()->setConfigList(screenPositionProfile, kvList.first, kvList.second);
    }

    bool remove(int screenNum, QPoint pos, const QString &id)
    {
        //m_overlapItems 重叠items
        m_overlapItems.removeAll(id);
        if (!m_itemGrids.value(screenNum).contains(id)) {
            qDebug() << "can not remove" << pos << id;
            return false;
        }

        auto tempGridItemsItor = m_gridItems.find(screenNum);
        auto tempItemGridsItor = m_itemGrids.find(screenNum);
        tempGridItemsItor->remove(pos);
        tempItemGridsItor->remove(id);

        auto usageIndex = indexOfGridPos(screenNum, pos);

        if(!m_cellStatus.contains(screenNum))
            return false;
        auto cellStatusItor = m_cellStatus.find(screenNum);
        QVector<bool> cellStatus = cellStatusItor.value();
        if(usageIndex < cellStatus.size()){
            cellStatus[usageIndex] = false;
        }

        if (!m_overlapItems.isEmpty()
                && (pos == overlapPos(screenNum))) {
            auto itemId = m_overlapItems.takeFirst();
            add(screenNum, pos, itemId);
        }
        return true;
    }

    void refreshPositionProfiles(int screenNum)
    {
        //auto profile = QString("Position_%1_%2x%3").arg(screenNum).arg(w).arg(h);
        QString profile;
        if (m_bSingleMode){
            profile = QString("SinglePosition");  //单一桌面模式和扩展桌面模式需要独立保存桌面项目位置配置文件，以便两种模式互相切换时仍然完好如初
        }else {
            foreach(int key, positionProfiles.keys()){
                if (positionProfiles.value(key) == QString("SinglePosition")){
                    positionProfiles[key]= QString("Position_%1").arg(key);
                }
            }
            profile = QString("Position_%1").arg(screenNum);
        }

        if (profile != positionProfiles.value(screenNum)) {
            positionProfiles[screenNum]= profile;
        }
    }

    void resetGridSize(int screenNum, int w, int h)
    {
        if(!screensCoordInfo.contains(screenNum))
            return;

        auto coordInfo = screensCoordInfo.find(screenNum);
        Q_ASSERT(!(coordInfo.value().second == h && coordInfo.value().first == w));
        coordInfo.value().first = w;
        coordInfo.value().second = h;

        createProfile();
        refreshPositionProfiles(screenNum);
    }

    void changeGridSize(int screenNum, int w, int h)
    {
        auto coordInfo = screensCoordInfo.value(screenNum);
        Q_ASSERT(!(coordInfo.second == h && coordInfo.first == w));

//        qDebug() << "old grid size" << coordHeight << coordWidth;
//        qDebug() << "new grid size" << w << h;
        auto oldCellCount = coordInfo.second * coordInfo.first;
        auto newCellCount = w * h;

        auto allItems = m_itemGrids;

        auto outCellCount = 0;
        auto cellStatus = m_cellStatus.value(screenNum);
        for (int i = 0; i < cellStatus.length(); ++i) {
            if (i >= newCellCount && cellStatus.value(i)) {
                outCellCount++;
            }
        }

        auto oldCellStatus = cellStatus;

        // find empty cell count
        auto indexEnd = qMin(oldCellCount, newCellCount);
        auto emptyCellCount = 0;
        for (int i = 0; i < indexEnd; ++i) {
            if (!oldCellStatus.value(i)) {
                emptyCellCount++;
            }
        }

        if (newCellCount > oldCellCount) {
            emptyCellCount += (newCellCount - oldCellCount);
        }

        if (emptyCellCount <= outCellCount + m_overlapItems.length()) {
//            qDebug() << "arrange";
            auto sortItems = rangeItems(screenNum);
            resetGridSize(screenNum, w, h);
            for (int i = 0; i < newCellCount; ++i) {
                add(screenNum, takeEmptyPos().second, sortItems.takeFirst());
            }
            m_overlapItems = sortItems;
        } else {
            // find start pos
            auto newEmptyCellCount = emptyCellCount - outCellCount + m_overlapItems.length();
            QVector<int> keepPosIndex;
            QVector<QString> keepItems;

            auto lastEmptyPosIndex = newCellCount;
            for (int i = 0; i < oldCellStatus.length(); ++i) {
                if (oldCellStatus.value(i)) {
                    keepPosIndex.push_back(i);
                    keepItems.push_back(m_gridItems.value(screenNum).value(gridPosAt(screenNum, i)));
                } else {
                    if (newEmptyCellCount <= 0) {
                        lastEmptyPosIndex = i;
                        break;
                    }
                    --newEmptyCellCount;
                }
            }

            QVector<int> nokeepPosIndex;
            QVector<QString> nokeepItems;
            for (int i = lastEmptyPosIndex; i < oldCellStatus.length(); ++i) {
                if (oldCellStatus.value(i)) {
                    nokeepPosIndex.push_back(i);
                    nokeepItems.push_back(m_gridItems.value(screenNum).value(gridPosAt(screenNum, i)));
                }
            }

            auto overlapItems = m_overlapItems;

            resetGridSize(screenNum, w, h);

            for (int i = 0; i < keepPosIndex.length(); ++i) {
                auto index = keepPosIndex.value(i);
                if (m_cellStatus.size() > index && !cellStatus.value(index)) {
                    QPoint pos{ gridPosAt(screenNum, index) };
                    add(screenNum, pos, keepItems.value(i));
                }
            }

            for (int i = 0; i < nokeepItems.length(); ++i) {
                QPoint pos{ gridPosAt(screenNum, lastEmptyPosIndex) };
                add(screenNum, pos, nokeepItems.value(i));
                lastEmptyPosIndex++;
            }

            // TODO
            for (auto &item : overlapItems) {
                QPair<int, QPoint> posPair{ takeEmptyPos() };
                add(posPair.first,posPair.second, item);
            }
        }
    }

    bool updateGridSize(int screenNum, int w, int h)
    {
#if 0 //older
        if (coordHeight == h && coordWidth == w) {
            qWarning() << "profile not changed";
            return false;
        }

        if (0 == coordWidth && 0 == coordHeight) {
            resetGridSize(w, h);
            return false;
        } else {
            qDebug() << "change grid from" << coordWidth << coordHeight
                     << "to" << w << h;
            changeGridSize(w, h);

            // check if we should update grid profile.
            if (autoMerge) {
                return this->autoArrange;
            }

            QPair<QStringList, QVariantList> kvList = generateProfileConfigVariable(screenNum);

            qDebug() << "updateGridProfile:" << kvList.first.size()
                     << m_gridItems.size() << m_itemGrids.size();

            emit Presenter::instance()->removeConfig(positionProfile, "");
            emit Presenter::instance()->setConfigList(positionProfile, kvList.first, kvList.second);

            return this->autoArrange;
        }
#else

        auto coordInfo = screensCoordInfo.value(screenNum);
        if (coordInfo.second == h && coordInfo.first == w) {
            qWarning() << "profile not changed";
            return false;
        }

        if (0 == coordInfo.first && 0 == coordInfo.second) {
            resetGridSize(screenNum, w, h);
            return false;
        } else {
            qDebug() << "change grid from" << coordInfo.first << coordInfo.second
                     << "to" << w << h;
            changeGridSize(screenNum, w, h);

            // check if we should update grid profile.
            if (autoMerge) {
                return this->autoArrange;
            }
            //syncProfile(screenNum);

            return this->autoArrange;
        }
#endif
    }


    inline void setWhetherShowHiddenFiles(bool value)noexcept
    {
        m_whetherShowHiddenFiles.store(value, std::memory_order_release);
    }

    inline bool getWhetherShowHiddenFiles()noexcept
    {
        return m_whetherShowHiddenFiles.load(std::memory_order_consume);
    }

    inline QList<int> screenCode() const
    {
        QList<int> screenOrder = screensCoordInfo.keys();
        qSort(screenOrder.begin(),screenOrder.end());
        return screenOrder;
    }

    QStringList allItems() const
    {
        QStringList items;
        auto screens = screenCode();
        for (int num : screens){
            auto cells = m_cellStatus.value(num);
            auto gridItems = m_gridItems.value(num);
            for (int i = 0; i < cells.size(); ++i){
                if (cells.value(i)) {
                    QPoint pos = gridPosAt(num,i);
                    const QString &item = gridItems.value(pos);
                    items << item;
                }
            }
        }
        items << m_overlapItems;
        return items;
    }

    void reAutoArrage()
    {
        QStringList items = allItems();
        clear();
        arrange(items);
    }
public:
    QStringList             m_overlapItems;
    QMap<int, QMap<QPoint, QString>> m_gridItems;
    QMap<int, QMap<QString, QPoint>> m_itemGrids;
    //newer
    QMap<int, bool>             m_screenFullStatus;//屏幕图标状态
    QMap<int, QVector<bool>>    m_cellStatus;//<screenNum, QVector<bool>>
    QMap<int,QString>           positionProfiles;
    QMap<int,QPair<int, int>>   screensCoordInfo;//<screenNum,<coordWidth,coordHeight>>
    bool                    autoArrange;
    bool                    autoMerge = false;
    int                     createProfileTime{0};
    std::atomic<bool>       m_whetherShowHiddenFiles{ false };
    bool                    m_bSingleMode = true;
};

GridManager::GridManager(): d(new GridManagerPrivate)
{

}

GridManager::~GridManager()
{

}



DUrl GridManager::getInitRootUrl()
{
    setAutoMerge(d->autoMerge);
    if (d->autoMerge) {
        return DUrl(DFMMD_ROOT MERGEDDESKTOP_FOLDER);
    } else {
        // sa
        QString desktopPath = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();
        DUrl desktopUrl = DUrl::fromLocalFile(desktopPath);

        if (!QDir(desktopPath).exists()) {
            QDir::home().mkpath(desktopPath);
        }

        return desktopUrl;
    }
}

void GridManager::initGridItemsInfos()
{
    DUrl fileUrl = getInitRootUrl();
    d->clear();
    QScopedPointer<DFileSystemModel> tempModel(new DFileSystemModel(nullptr));
    QList<DAbstractFileInfoPointer> infoList = DFileService::instance()->getChildren(this, fileUrl,
                                                                                     QStringList(), tempModel->filters());

    if(GridManager::instance()->autoMerge()){
        initWithoutProfile(infoList);
    }
    else if (GridManager::instance()->autoArrange())
    {
        auto settings = Config::instance()->settings();
        settings->beginGroup(Config::groupGeneral);
        DFileSystemModel::Roles sortRole = (DFileSystemModel::Roles)settings->value(Config::keySortBy).toInt();
        Qt::SortOrder sortOrder = settings->value(Config::keySortOrder).toInt() == Qt::AscendingOrder ?
                    Qt::DescendingOrder : Qt::AscendingOrder;;
        settings->endGroup();

        qDebug() << "auto arrage" << sortRole << sortOrder;
        QModelIndex index = tempModel->setRootUrl(fileUrl);
        DAbstractFileInfoPointer root = tempModel->fileInfo(index);
        QTime t;
        if (root != nullptr){
            DAbstractFileInfo::CompareFunction sortFun = root->compareFunByColumn(sortRole);
            qDebug() << "DAbstractFileInfo::CompareFunction " << (sortFun != nullptr);
            t.start();
            if (sortFun){
                qSort(infoList.begin(), infoList.end(), [sortFun, sortOrder](const DAbstractFileInfoPointer &node1, const DAbstractFileInfoPointer &node2) {
                    return sortFun(node1, node2, sortOrder);
                });
            }
            qDebug() << "sort complete time " <<t.elapsed();
        }
        QStringList list;
        for (const DAbstractFileInfoPointer &df : infoList) {
            list << df->fileUrl().toString();
        }
        qDebug() << "sortefd desktop items num" << list.size() << " time "<< t.elapsed();
        initArrage(list);
    }
    else {
        initProfile(infoList);
    }
}

void GridManager::initProfile(const QList<DAbstractFileInfoPointer> &items)
{
    //初始化Profile,用实际地址的文件去匹配图标位置（自动整理则图标顺延展开，自定义则按照配置文件对应顺序）
    d->createProfile();
    d->loadProfile(items);
}

// init WITHOUT grid item position data from the config file.
void GridManager::initWithoutProfile(const QList<DAbstractFileInfoPointer> &items)
{
    d->createProfile(); // nothing with profile.
    d->loadWithoutProfile(items);
}

void GridManager::initArrage(const QStringList &items)
{
    d->clear();
    d->arrange(items);
    emit sigUpdate();
}

bool GridManager::add(int screenNum, const QString &id)
{
#ifdef QT_DEBUG
    qDebug() << "show hidden files: " << d->getWhetherShowHiddenFiles() << id;
#endif //QT_DEBUG
    DUrl url(id);

    if (!d->getWhetherShowHiddenFiles()) {
        const DAbstractFileInfoPointer info = DFileService::instance()->createFileInfo(nullptr, url);

        if (info->isHidden()) {
            return false;
        }
    }

    if (d->m_itemGrids.value(screenNum).contains(id)) {
//        qDebug() << "item exist item" << d->itemGrids.value(id) << id;
        return false;
    }

    QPair<int, QPoint> posPair{ d->takeEmptyPos() };
    return add(posPair.first, posPair.second, id);
}

bool GridManager::add(int screenNum, QPoint pos, const QString &id)
{
    qDebug() << "add" << pos << id;
    auto ret = d->add(screenNum, pos, id);
    if (ret && !autoMerge()) {
        d->syncProfile(screenNum);
    }

    return ret;
}

bool GridManager::move(int screenNum, const QStringList &selecteds, const QString &current, int x, int y)
{
    auto currentPos = d->m_itemGrids.value(screenNum).value(current);
    auto destPos = QPoint(x, y);
    auto offset = destPos - currentPos;

    QList<QPoint> originPosList;
    QList<QPoint> destPosList;
    // check dest is empty;
    auto destPosMap = d->m_gridItems.value(screenNum);
    auto destUsedGrids = d->m_cellStatus.value(screenNum);
    for (auto &id : selecteds) {
        auto oldPos = d->m_itemGrids.value(screenNum).value(id);
        originPosList << oldPos;
        destPosMap.remove(oldPos);
        destUsedGrids[d->indexOfGridPos(screenNum, oldPos)] = false;
        auto destPos = oldPos + offset;
        destPosList << destPos;
    }

    bool conflict = false;
    for (auto pos : destPosList) {
        if (destPosMap.contains(pos) || !d->isValid(screenNum, pos)) {
            conflict = true;
            break;
        }
    }

    // no need to resize
    if (conflict) {
        auto selectedHeadCount = selecteds.indexOf(current);
        // find free grid before destPos
        auto destIndex = d->indexOfGridPos(screenNum, destPos);

        QList<int> emptyIndexList;

        for (int  i = 0; i < d->cellCount(screenNum); ++i) {
            if (false == destUsedGrids.value(i)) {
                emptyIndexList << i;
            }
        }
        auto destGridHeadCount = emptyIndexList.indexOf(destIndex);

        Q_ASSERT(emptyIndexList.length() >= selecteds.length());

        auto startIndex = emptyIndexList.indexOf(destIndex) - selectedHeadCount;
        if (destGridHeadCount < selectedHeadCount) {
            startIndex = 0;
        }
        auto destTailCount = emptyIndexList.length() - destGridHeadCount;
        auto selectedTailCount = selecteds.length() - selectedHeadCount;
        if (destTailCount <= selectedTailCount) {
            startIndex = emptyIndexList.length() - selecteds.length();
        }

        destPosList.clear();

        startIndex = emptyIndexList.value(startIndex);
        for (int i = startIndex; i < d->cellCount(screenNum); ++i) {
            if (false == destUsedGrids.value(i)) {
                destPosList << d->gridPosAt(screenNum, i);
            }
        }
    }

    for (int i = 0; i < selecteds.length(); ++i) {
        if (contains(screenNum, selecteds.value(i))) {
            remove(screenNum, selecteds.value(i));
        }
    }
    for (int i = 0; i < selecteds.length(); ++i) {
        QPoint point{ destPosList.value(i) };
        add(screenNum, point, selecteds.value(i));
    }

    if (shouldArrange()) {
        reArrange(screenNum);
    }

    return true;
}

bool GridManager::move(int fromScreen, int toScreen, const QStringList &selectedIds, const QString &itemId, int x, int y)
{
    auto currentPos = d->m_itemGrids.value(fromScreen).value(itemId);
    auto destPos = QPoint(x, y);
    auto offset = destPos - currentPos;

    QList<QPoint> originPosList;
    QList<QPoint> destPosList;
    // check dest is empty;
    auto destPosMap = d->m_gridItems.value(fromScreen);
    auto destUsedGrids = d->m_cellStatus.value(fromScreen);
    for (auto &id : selectedIds) {
        auto oldPos = d->m_itemGrids.value(fromScreen).value(id);
        originPosList << oldPos;
        destPosMap.remove(oldPos);
        destUsedGrids[d->indexOfGridPos(fromScreen, oldPos)] = false;
        auto destPos = oldPos + offset;
        destPosList << destPos;
    }

    bool conflict = false;
    for (auto pos : destPosList) {
        if (destPosMap.contains(pos) || !d->isValid(fromScreen, pos)) {
            conflict = true;
            break;
        }
    }

    // no need to resize
    if (conflict) {
        auto selectedHeadCount = selectedIds.indexOf(itemId);
        // find free grid before destPos
        auto destIndex = d->indexOfGridPos(toScreen, destPos);

        QList<int> emptyIndexList;

        for (int  i = 0; i < d->cellCount(toScreen); ++i) {
            if (false == destUsedGrids.value(i)) {
                emptyIndexList << i;
            }
        }
        auto destGridHeadCount = emptyIndexList.indexOf(destIndex);

        Q_ASSERT(emptyIndexList.length() >= selectedIds.length());

        auto startIndex = emptyIndexList.indexOf(destIndex) - selectedHeadCount;
        if (destGridHeadCount < selectedHeadCount) {
            startIndex = 0;
        }
        auto destTailCount = emptyIndexList.length() - destGridHeadCount;
        auto selectedTailCount = selectedIds.length() - selectedHeadCount;
        if (destTailCount <= selectedTailCount) {
            startIndex = emptyIndexList.length() - selectedIds.length();
        }

        destPosList.clear();

        startIndex = emptyIndexList.value(startIndex);
        for (int i = startIndex; i < d->cellCount(toScreen); ++i) {
            if (false == destUsedGrids.value(i)) {
                destPosList << d->gridPosAt(toScreen, i);
            }
        }
    }

    for (int i = 0; i < selectedIds.length(); ++i) {
        if (contains(toScreen, selectedIds.value(i))) {
            remove(toScreen, selectedIds.value(i));
        }
    }
    for (int i = 0; i < selectedIds.length(); ++i) {
        QPoint point{ destPosList.value(i) };
        add(toScreen, point, selectedIds.value(i));
    }

    //多屏下的重新排列需要重新考虑一下设计
//    if (shouldArrange()) {
//        reArrange(screenNum);
//    }

    return true;
}

bool GridManager::remove(int screenNum, const QString &id)
{
    auto pos = d->m_itemGrids.value(screenNum).value(id);
    return remove(screenNum, pos, id);
}

bool GridManager::remove(int screenNum, int x, int y, const QString &id)
{
    auto pos = QPoint(x, y);
    return remove(screenNum, pos, id);
}

bool GridManager::remove(int screenNum, QPoint pos, const QString &id)
{
    auto ret = d->remove(screenNum, pos, id);
    if (ret && !autoMerge()) {
        d->syncProfile(screenNum);
    }
    return ret;
}

bool GridManager::clear()
{
    d->createProfile();
    //此处暂时删除所有即d->positionProfiles,不知对否，后续调整
    emit Presenter::instance()->removeConfigList(Config::keyProfile, QStringList());

    return true;
}

void GridManager::restCoord()
{
    d->screensCoordInfo.clear();
    d->m_gridItems.clear();
    d->m_gridItems.clear();
    d->m_cellStatus.clear();
    d->m_overlapItems.clear();
    d->m_screenFullStatus.clear();
}

void GridManager::addCoord(int screenNum, QPair<int, int> coordInfo)
{
    qDebug() << "add screen" << screenNum << coordInfo;
    if(d->screensCoordInfo.contains(screenNum))
        return;
    //初始化栅格
    d->screensCoordInfo.insert(screenNum, coordInfo);
    if(!d->m_gridItems.contains(screenNum)){
        QMap<QPoint, QString> gridItem;
        d->m_gridItems.insert(screenNum, gridItem);
    }
    if(!d->m_itemGrids.contains(screenNum)){
        QMap<QString, QPoint> itemGrid;
        d->m_itemGrids.insert(screenNum, itemGrid);
    }
//    if(!d->m_cellStatus.contains(screenNum)){
//        QVector<bool> cellStatus;
//        cellStatus.resize(coordInfo.first * coordInfo.second);
//        d->m_cellStatus.insert(screenNum, cellStatus);
//    }
}

QString GridManager::firstItemId(int screenNum)
{
    for (int i = 0; i < d->m_cellStatus.value(screenNum).length(); ++i) {
        if (d->m_cellStatus.value(screenNum).value(i)) {
            auto pos = d->gridPosAt(screenNum, i);
            return  itemId(screenNum, pos);
        }
    }
    return "";
}

QString GridManager::lastItemId(int screenNum)
{
    auto len = d->m_cellStatus.value(screenNum).length();
    for (int i = 0; i < len; ++i) {
        if (d->m_cellStatus.value(screenNum).value(len - 1 - i)) {
            auto pos = d->gridPosAt(screenNum, len - 1 - i);
            return  itemId(screenNum, pos);
        }
    }
    return "";
}

QStringList GridManager::itemIds(int screenNum)
{
    QStringList ids;
    for (int i = 0; i < d->m_cellStatus.value(screenNum).length(); ++i) {
        if (d->m_cellStatus.value(screenNum).value(i)) {
            auto pos = d->gridPosAt(screenNum, i);
            ids.append(itemId(screenNum, pos));
        }
    }
    if (screenNum == d->screenCode().last())
        ids << d->m_overlapItems;
    return ids;
}

bool GridManager::contains(int screebNum, const QString &id)
{
    return d->m_itemGrids.value(screebNum).contains(id) ||
            (d->screenCode().last() == screebNum && d->m_overlapItems.contains(id));
}

QPoint GridManager::position(int screenNum, const QString &id)
{
    if (!d->m_itemGrids.value(screenNum).contains(id)) {
        return d->overlapPos(screenNum);
    }

    return d->m_itemGrids.value(screenNum).value(id);
}

QString GridManager::itemId(int screenNum, int x, int y)
{
    return d->m_gridItems.value(screenNum).value(QPoint(x, y));
}

QString GridManager::itemId(int screenNum, QPoint pos)
{
    return d->m_gridItems.value(screenNum).value(pos);
}

bool GridManager::isEmpty(int screenNum, int x, int y)
{
    auto cellStatus = d->m_cellStatus.value(screenNum);
    return !cellStatus.value(d->indexOfGridPos(screenNum, QPoint(x, y)));
}

QStringList GridManager::overlapItems(int screen) const
{
    if (!d->screenCode().empty() && screen == d->screenCode().last())
        return d->m_overlapItems;

    return QStringList();
}

bool GridManager::shouldArrange() const
{
    return d->autoArrange || d->autoMerge;
}

bool GridManager::autoArrange() const
{
    return d->autoArrange;
}

bool GridManager::autoMerge() const
{
    return d->autoMerge;
}

void GridManager::toggleArrange(int screenNum)
{
    d->autoArrange = !d->autoArrange;

    if (!d->autoArrange) {
        return;
    }

    reArrange(screenNum);
}

void GridManager::setAutoMerge(bool enable)
{
    if (d->autoMerge == enable) return;
    d->autoMerge = enable;
}

void GridManager::toggleAutoMerge()
{
    setAutoMerge(!d->autoMerge);
}

void GridManager::reArrange(int screenNum)
{
    if (autoArrange()){
        d->reAutoArrage();
    }
    else
        d->arrange();

    if (autoMerge()) {
        return;
    }

    d->syncProfile(screenNum);
//    QPair<QStringList, QVariantList> kvList = d->generateProfileConfigVariable(screenNum);
//    auto oneScreenPositionProfile = d->positionProfiles.value(screenNum);

//    emit Presenter::instance()->removeConfig(oneScreenPositionProfile, "");
//    emit Presenter::instance()->setConfigList(oneScreenPositionProfile, kvList.first, kvList.second);
}

int GridManager::gridCount(int screenNum) const
{
    auto coordInfo = d->screensCoordInfo.value(screenNum);
    return coordInfo.first * coordInfo.second;
}

QPair<int, QPoint> GridManager::forwardFindEmpty(int screenNum, QPoint start) const
{
    QPair<int, QPoint> emptyPos;
    auto size = gridSize(screenNum);
    for (int j = start.y(); j < size.height(); ++j) {
        if (GridManager::instance()->isEmpty(screenNum, start.x(), j)) {
            emptyPos.first = screenNum;
            emptyPos.second = QPoint(start.x(), j);
            return emptyPos;
        }
    }

    for (int i = start.x() + 1; i < size.width(); ++i) {
        for (int j = 0; j < size.height(); ++j) {
            if (GridManager::instance()->isEmpty(screenNum, i, j)) {
                emptyPos.first = screenNum;
                emptyPos.second = QPoint(i, j);
                return emptyPos;
            }
        }
    }
    return d->emptyPos();
}

QSize GridManager::gridSize(int screenNum) const
{
    auto coordInfo = d->screensCoordInfo.value(screenNum);
    return QSize(coordInfo.first, coordInfo.second);
}

void GridManager::updateGridSize(int screenNum, int w, int h)
{
    QStringList items = d->allItems();
    if (d->updateGridSize(screenNum, w, h)) {
        //d->arrange(screenNum);
        d->clear();
        d->arrange(items);
        emit sigUpdate();
    }
    //将单个屏幕加载到配置
    //[Profile]
    //1=Position_1_21x9
    emit Presenter::instance()->setConfig(Config::keyProfile,
                                          QString::number(screenNum),
                                          d->positionProfiles.value(screenNum));
}

GridCore *GridManager::core()
{
#if 0
    auto core = new GridCore;
    core->overlapItems = d->m_overlapItems;
    core->gridItems = d->m_gridItems;
    core->itemGrids = d->m_itemGrids;
    core->gridStatus = d->m_cellStatus;
    core->coordWidth = d->coordWidth;
    core->coordHeight = d->coordHeight;
    return core;
#endif
#if 1
    auto core = new GridCore;
    core->overlapItems = d->m_overlapItems;
    core->gridItems = d->m_gridItems;
    core->itemGrids = d->m_itemGrids;
    core->m_cellStatus = d->m_cellStatus;
    core->screensCoordInfo = d->screensCoordInfo;
    return core;
#endif
}

void GridManager::setWhetherShowHiddenFiles(bool value) noexcept
{
    d->setWhetherShowHiddenFiles(value);
}

bool GridManager::getWhetherShowHiddenFiles() noexcept
{
    return d->getWhetherShowHiddenFiles();
}

void GridManager::dump()
{
    for (auto key : d->m_gridItems.keys()) {
        qDebug() << key << d->m_gridItems.value(key);
    }

    for (auto key : d->m_itemGrids.keys()) {
        qDebug() << key << d->m_itemGrids.value(key);
    }
}
void GridManager::setDisplayMode(bool single)
{
    d->m_bSingleMode = single;
}
#endif
