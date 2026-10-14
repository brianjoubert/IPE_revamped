// --------------------------------------------------------------------
// Modern theme support for the Qt frontend of Ipe
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

*/

#ifndef THEME_QT_H
#define THEME_QT_H

#include <QColor>
#include <QIcon>
#include <QString>

namespace ipetheme {

enum class Mode { System, Light, Dark };

// Colors of the current theme
struct Colors {
    QColor window;     // main window / panels
    QColor surface;    // toolbars, docks
    QColor base;       // inputs, lists
    QColor raised;     // buttons, combos
    QColor border;
    QColor text;
    QColor textMuted;
    QColor accent;
    QColor accentText;
    QColor hover;
    QColor canvas; // area around the paper
    bool dark;
};

// Must be called once after QApplication has been created.
void initialize();

Mode mode();
void setMode(Mode m);  // applies and persists
bool isDark();
const Colors & colors();

// Small icons painted with QPainter (independent of icons.ipe)
// names: "search", "sun", "moon", "system"
QIcon glyph(const QString & name);

QString modeName(Mode m);

} // namespace ipetheme

// --------------------------------------------------------------------
#endif
