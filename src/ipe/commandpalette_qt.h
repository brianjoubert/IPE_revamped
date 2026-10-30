// --------------------------------------------------------------------
// Command palette for the Qt frontend of Ipe
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

*/

#ifndef COMMANDPALETTE_QT_H
#define COMMANDPALETTE_QT_H

#include <QAction>
#include <QFrame>
#include <QList>
#include <QPointer>

class QLineEdit;
class QListWidget;
class QMenuBar;

// A searchable list of every command in the menu bar (including ipelets).
class CommandPalette : public QFrame {
public:
    explicit CommandPalette(QWidget * parent);
    void open(QMenuBar * menuBar);

protected:
    bool eventFilter(QObject * obj, QEvent * ev) override;

private:
    struct Entry {
	QPointer<QAction> action;
	QString title;
	QString path;
	QString shortcut;
    };
    void collect(QMenu * menu, const QString & path);
    void refilter();
    void runCurrent();

    QLineEdit * iEdit;
    QListWidget * iList;
    QList<Entry> iEntries;
};

// --------------------------------------------------------------------
#endif
