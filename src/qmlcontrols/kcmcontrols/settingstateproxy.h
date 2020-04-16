/*
 *    Copyright 2020 Kevin Ottens <kevin.ottens@enioka.com>
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 */

#ifndef SETTINGSTATEPROXY_H
#define SETTINGSTATEPROXY_H

#include <QObject>
#include <QPointer>

#include <KCoreConfigSkeleton>

/**
 * This element allows to represent in a declarative way the
 * state of a particular setting in a config object.
 *
 * @since 5.70
 */
class SettingStateProxy : public QObject
{
    Q_OBJECT

    /**
     * The config object which will be monitored for setting state changes
     */
    Q_PROPERTY(KCoreConfigSkeleton *configObject READ configObject WRITE setConfigObject NOTIFY configObjectChanged)

    /**
     * The name of the item representing the setting in the config object
     */
    Q_PROPERTY(QString itemName READ itemName WRITE setItemName NOTIFY itemNameChanged)


    /**
     * Indicates if the setting is marked as immutable
     */
    Q_PROPERTY(bool immutable READ isImmutable NOTIFY immutableChanged)


    /**
     * Indicates if the setting differs from its default value
     */
    Q_PROPERTY(bool defaulted READ isDefaulted NOTIFY defaultedChanged)
public:
    using QObject::QObject;

    KCoreConfigSkeleton *configObject() const;
    void setConfigObject(KCoreConfigSkeleton *configObject);

    QString itemName() const;
    void setItemName(const QString &itemName);

    bool isImmutable() const;
    bool isDefaulted() const;

    Q_INVOKABLE void resetToDefault();

Q_SIGNALS:
    void configObjectChanged();
    void itemNameChanged();

    void immutableChanged();
    void defaultedChanged();

private Q_SLOTS:
    void updateState();

private:
    void connectSetting();

    QPointer<KCoreConfigSkeleton> m_configObject;
    QString m_itemName;
    bool m_immutable = false;
    bool m_defaulted = true;
};

#endif
