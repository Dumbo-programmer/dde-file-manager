/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -c IntrospectableInterface -p introspectable_interface introspectable.xml
 *
 * qdbusxml2cpp is Copyright (C) 2016 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#ifndef INTROSPECTABLE_INTERFACE_H
#define INTROSPECTABLE_INTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface org.freedesktop.DBus.Introspectable
 */
class IntrospectableInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "org.freedesktop.DBus.Introspectable"; }

public:
    IntrospectableInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = nullptr);

    ~IntrospectableInterface();

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<QString> InterfaceName()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("InterfaceName"), argumentList);
    }

    inline QDBusPendingReply<QString> Introspect()
    {
        QList<QVariant> argumentList;
        return asyncCallWithArgumentList(QStringLiteral("Introspect"), argumentList);
    }

Q_SIGNALS: // SIGNALS
};

namespace org {
  namespace freedesktop {
    namespace DBus {
      typedef ::IntrospectableInterface Introspectable;
    }
  }
}
#endif
