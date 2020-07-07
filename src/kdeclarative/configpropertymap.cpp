/*
 *   Copyright 2013 Marco Martin <notmart@gmail.com>
 *   Copyright 2020 David Edmundson <davidedmundson@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "configpropertymap.h"

#include <QJSValue>
#include <QPointer>
#include <KCoreConfigSkeleton>

#include <functional>

namespace KDeclarative {

class ConfigPropertyMapPrivate {
public:
    ConfigPropertyMapPrivate(ConfigPropertyMap *map)
        : q(map)
    {
    }

    enum LoadConfigOption {
        DontEmitValueChanged,
        EmitValueChanged
    };

    void loadConfig(LoadConfigOption option);
    void writeConfig();
    void writeConfigValue(const QString &key, const QVariant &value);

    ConfigPropertyMap *q;
    QPointer<KCoreConfigSkeleton> config;
    bool updatingConfigValue = false;
    bool autosave = true;
    bool notify = false;
};

ConfigPropertyMap::ConfigPropertyMap(KCoreConfigSkeleton *config, QObject *parent)
    : QQmlPropertyMap(this, parent),
      d(new ConfigPropertyMapPrivate(this))
{
    d->config = config;

    // Reload the config only if the change signal has *not* been emitted by ourselves updating the config
    connect(config, &KCoreConfigSkeleton::configChanged, this, [this] () {
        if (!d->updatingConfigValue) {
            d->loadConfig(ConfigPropertyMapPrivate::EmitValueChanged);
        }
    });
    connect(this, &ConfigPropertyMap::valueChanged, this,
            [this](const QString &key, const QVariant &value){d->writeConfigValue(key, value);});

    d->loadConfig(ConfigPropertyMapPrivate::DontEmitValueChanged);
}

ConfigPropertyMap::~ConfigPropertyMap()
{
    if (d->autosave) {
        d->writeConfig();
    }
    delete d;
}

bool KDeclarative::ConfigPropertyMap::isAutosave() const
{
    return d->autosave;
}

void ConfigPropertyMap::setAutosave(bool autosave)
{
    d->autosave = autosave;
}

bool ConfigPropertyMap::isNotify() const
{
    return d->notify;
}

void ConfigPropertyMap::setNotify(bool notify)
{
    d->notify = notify;
}

QVariant ConfigPropertyMap::updateValue(const QString &key, const QVariant &input)
{
    Q_UNUSED(key);
    if (input.userType() == qMetaTypeId<QJSValue>()) {
        return input.value<QJSValue>().toVariant();
    }
    return input;
}

bool ConfigPropertyMap::isImmutable(const QString &key) const
{
    KConfigSkeletonItem *item = d->config.data()->findItem(key);
    if (item) {
        return item->isImmutable();
    }

    return false;
}

void ConfigPropertyMapPrivate::loadConfig(ConfigPropertyMapPrivate::LoadConfigOption option)
{
    if (!config) {
        return;
    }

    const auto &items = config.data()->items();
    for (KConfigSkeletonItem *item : items) {
        q->insert(item->key(), item->property());
        if (option == EmitValueChanged) {
            emit q->valueChanged(item->key(), item->property());
        }
    }
}

void ConfigPropertyMapPrivate::writeConfig()
{
    if (!config) {
        return;
    }

    const auto lstItems = config.data()->items();
    for (KConfigSkeletonItem *item : lstItems) {
        item->setWriteFlags(notify ? KConfigBase::Notify : KConfigBase::Normal);
        item->setProperty(q->value(item->key()));
    }

    if (autosave) {
        updatingConfigValue = true;
        config.data()->save();
        updatingConfigValue = false;
    }
}

void ConfigPropertyMapPrivate::writeConfigValue(const QString &key, const QVariant &value)
{
    KConfigSkeletonItem *item = config.data()->findItem(key);
    if (item) {
        updatingConfigValue = true;
        item->setWriteFlags(notify ? KConfigBase::Notify : KConfigBase::Normal);
        item->setProperty(value);
        if (autosave) {
            config.data()->save();
            //why read? read will update KConfigSkeletonItem::mLoadedValue,
            //allowing a write operation to be performed next time
            config.data()->read();
        }
        updatingConfigValue = false;
    }
}

}

#include "moc_configpropertymap.cpp"
