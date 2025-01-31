/*
    SPDX-FileCopyrightText: 2020 David Redondo <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2014 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 1998 Mark Donohoe <donohoe@kde.org>
    SPDX-FileCopyrightText: 2001 Ellis Whitehead <ellis@kde.org>
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "keysequencehelper.h"

#include <QDebug>
#include <QHash>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickRenderControl>
#include <QQuickWindow>

#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardShortcut>

#if !defined(Q_OS_WIN) && !defined(Q_OS_DARWIN)
#include <KGlobalAccel>
#include <KGlobalShortcutInfo>
#endif

class KeySequenceHelperPrivate
{
public:
    KeySequenceHelperPrivate(KeySequenceHelper *qq);

    /**
     * Conflicts the key sequence @a seq with a current standard
     * shortcut?
     */
    bool conflictWithStandardShortcuts(const QKeySequence &seq);

    /**
     * Conflicts the key sequence @a seq with a current global
     * shortcut?
     */
    bool conflictWithGlobalShortcuts(const QKeySequence &seq);

    /**
     * Get permission to steal the shortcut @seq from the standard shortcut @a std.
     */
    bool stealStandardShortcut(KStandardShortcut::StandardShortcut std, const QKeySequence &seq);

    bool checkAgainstStandardShortcuts() const
    {
        return checkAgainstShortcutTypes & KeySequenceHelper::StandardShortcuts;
    }

    bool checkAgainstGlobalShortcuts() const
    {
        return checkAgainstShortcutTypes & KeySequenceHelper::GlobalShortcuts;
    }

    // members
    KeySequenceHelper *const q;

    //! Check the key sequence against KStandardShortcut::find()
    KeySequenceHelper::ShortcutTypes checkAgainstShortcutTypes;
};

KeySequenceHelperPrivate::KeySequenceHelperPrivate(KeySequenceHelper *qq)
    : q(qq)
    , checkAgainstShortcutTypes(KeySequenceHelper::StandardShortcuts | KeySequenceHelper::GlobalShortcuts)
{
}

KeySequenceHelper::KeySequenceHelper(QObject *parent)
    : KKeySequenceRecorder(nullptr, parent)
    , d(new KeySequenceHelperPrivate(this))
{
}

KeySequenceHelper::~KeySequenceHelper()
{
    delete d;
}

bool KeySequenceHelper::isKeySequenceAvailable(const QKeySequence &keySequence) const
{
    if (keySequence.isEmpty()) {
        return true;
    }
    bool conflict = false;
    if (d->checkAgainstShortcutTypes.testFlag(GlobalShortcuts)) {
        conflict |= d->conflictWithGlobalShortcuts(keySequence);
    }
    if (d->checkAgainstShortcutTypes.testFlag(StandardShortcuts)) {
        conflict |= d->conflictWithStandardShortcuts(keySequence);
    }
    return !conflict;
}

KeySequenceHelper::ShortcutTypes KeySequenceHelper::checkAgainstShortcutTypes()
{
    return d->checkAgainstShortcutTypes;
}

void KeySequenceHelper::setCheckAgainstShortcutTypes(KeySequenceHelper::ShortcutTypes types)
{
    if (d->checkAgainstShortcutTypes != types) {
        d->checkAgainstShortcutTypes = types;
    }
    Q_EMIT checkAgainstShortcutTypesChanged();
}

bool KeySequenceHelperPrivate::conflictWithGlobalShortcuts(const QKeySequence &keySequence)
{
#ifdef Q_OS_WIN
    // on windows F12 is reserved by the debugger at all times, so we can't use it for a global shortcut
    if (KeySequenceHelper::GlobalShortcuts && keySequence.toString().contains(QLatin1String("F12"))) {
        QString title = i18n("Reserved Shortcut");
        QString message = i18n(
            "The F12 key is reserved on Windows, so cannot be used for a global shortcut.\n"
            "Please choose another one.");

        KMessageBox::error(nullptr, message, title);
    }
    return false;
#elif !defined(Q_OS_DARWIN)
    if (!(checkAgainstShortcutTypes & KeySequenceHelper::GlobalShortcuts)) {
        return false;
    }

    // Global shortcuts are on key+modifier shortcuts. They can clash with a multi key shortcut.
    QList<KGlobalShortcutInfo> others;
    QList<KGlobalShortcutInfo> shadow;
    QList<KGlobalShortcutInfo> shadowed;
    if (!KGlobalAccel::isGlobalShortcutAvailable(keySequence, QString())) {
        others << KGlobalAccel::globalShortcutsByKey(keySequence);

        // look for shortcuts shadowing
        shadow << KGlobalAccel::globalShortcutsByKey(keySequence, KGlobalAccel::MatchType::Shadows);
        shadowed << KGlobalAccel::globalShortcutsByKey(keySequence, KGlobalAccel::MatchType::Shadowed);
    }

    if (!shadow.isEmpty() || !shadowed.isEmpty()) {
        QString title = i18n("Global Shortcut Shadowing");
        QString message;
        if (!shadowed.isEmpty()) {
            message += i18n("The '%1' key combination is shadowed by following global actions:\n").arg(keySequence.toString());
            for (const KGlobalShortcutInfo &info : std::as_const(shadowed)) {
                message += i18n("Action '%1' in context '%2'\n").arg(info.friendlyName(), info.contextFriendlyName());
            }
        }
        if (!shadow.isEmpty()) {
            message += i18n("The '%1' key combination shadows following global actions:\n").arg(keySequence.toString());
            for (const KGlobalShortcutInfo &info : std::as_const(shadow)) {
                message += i18n("Action '%1' in context '%2'\n").arg(info.friendlyName(), info.contextFriendlyName());
            }
        }

        KMessageBox::error(nullptr, message, title);
        return true;
    }

    if (!others.isEmpty() && !KGlobalAccel::promptStealShortcutSystemwide(nullptr, others, keySequence)) {
        return true;
    }

    // The user approved stealing the shortcut. We have to steal
    // it immediately because KAction::setGlobalShortcut() refuses
    // to set a global shortcut that is already used. There is no
    // error it just silently fails. So be nice because this is
    // most likely the first action that is done in the slot
    // listening to keySequenceChanged().
    KGlobalAccel::stealShortcutSystemwide(keySequence);
    return false;
#else
    return false;
#endif
}

bool KeySequenceHelperPrivate::conflictWithStandardShortcuts(const QKeySequence &keySequence)
{
    if (!checkAgainstStandardShortcuts()) {
        return false;
    }

    KStandardShortcut::StandardShortcut ssc = KStandardShortcut::find(keySequence);
    if (ssc != KStandardShortcut::AccelNone && !stealStandardShortcut(ssc, keySequence)) {
        return true;
    }
    return false;
}

bool KeySequenceHelperPrivate::stealStandardShortcut(KStandardShortcut::StandardShortcut std, const QKeySequence &seq)
{
    QString title = i18n("Conflict with Standard Application Shortcut");
    QString message = i18n(
        "The '%1' key combination is also used for the standard action "
        "\"%2\" that some applications use.\n"
        "Do you really want to use it as a global shortcut as well?",
        seq.toString(QKeySequence::NativeText),
        KStandardShortcut::label(std));

    if (KMessageBox::warningContinueCancel(nullptr, message, title, KGuiItem(i18n("Reassign"))) != KMessageBox::Continue) {
        return false;
    }
    return true;
}

QKeySequence KeySequenceHelper::fromString(const QString &str)
{
    return QKeySequence::fromString(str, QKeySequence::NativeText);
}

bool KeySequenceHelper::keySequenceIsEmpty(const QKeySequence &keySequence)
{
    return keySequence.isEmpty();
}

QString KeySequenceHelper::keySequenceNativeText(const QKeySequence &keySequence)
{
    return keySequence.toString(QKeySequence::NativeText);
}

QWindow *KeySequenceHelper::renderWindow(QQuickWindow *quickWindow)
{
    QWindow *renderWindow = QQuickRenderControl::renderWindowFor(quickWindow);
    auto window = renderWindow ? renderWindow : quickWindow;
    // If we have CppOwnership, set it explicitly to prevent the engine taking ownership of the window
    // and crashing on teardown
    if (QQmlEngine::objectOwnership(window) == QQmlEngine::CppOwnership) {
        QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    }
    return window;
}

#include "moc_keysequencehelper.cpp"
