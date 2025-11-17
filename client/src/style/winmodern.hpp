#pragma once
#include <QWidget>

#ifdef Q_OS_WINDOWS
#include <QOperatingSystemVersion>
#include <QWindow>
#endif

inline void ApplyWin11Style(QWidget *w)
{
#ifdef Q_OS_WINDOWS
    if (!w) return;

    // Only apply on Windows 11+
    if (QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows11) {
        if (auto win = w->windowHandle()) {
            // Enables Windows 11 rounded corners + Mica/Acrylic depending on theme
            win->setProperty("WindowsAccentColor", true);
            win->setProperty("WindowsEnableMica", true);
        }

        // Optional: remove old Windows 7 borders
        w->setWindowFlags(w->windowFlags() | Qt::FramelessWindowHint);
        w->setAttribute(Qt::WA_TranslucentBackground, true);
    }
#endif
}
