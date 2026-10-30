// --------------------------------------------------------------------
// Gradient editor dialog for the Qt frontend of Ipe
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    Lua interface:

      spec = ipeui.gradientDialog(parent, spec)

    'spec' is a table
      { type = "linear" | "radial",
	angle = degrees,                 -- linear
	cx, cy = 0..1, radius = > 0,     -- radial: center and size (relative)
	fx, fy = -1..1,                  -- radial: focus offset (relative to radius)
	extend = boolean,
	target = "fill" | "stroke",
	stops = { { offset = 0..1, color = { r, g, b }, opacity = 0..1 }, ... } }

    Returns the edited table, or nil if the dialog was canceled.

*/

#include "ipeui_common.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDial>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Stop {
    double offset;
    QColor color; // alpha holds the opacity
};

struct Spec {
    bool radial = false;
    double angle = 0.0;
    double cx = 0.5, cy = 0.5, radius = 1.0, fx = 0.0, fy = 0.0;
    bool extend = true;
    bool stroke = false;
    std::vector<Stop> stops;
};

void sortStops(std::vector<Stop> & stops) {
    std::stable_sort(stops.begin(), stops.end(),
		     [](const Stop & a, const Stop & b) { return a.offset < b.offset; });
}

QGradientStops qtStops(const std::vector<Stop> & stops) {
    QGradientStops qs;
    for (const Stop & s : stops) qs.push_back({std::clamp(s.offset, 0.0, 1.0), s.color});
    return qs;
}

QColor colorAt(const std::vector<Stop> & stops, double t) {
    if (stops.empty()) return Qt::black;
    if (t <= stops.front().offset) return stops.front().color;
    for (size_t i = 1; i < stops.size(); ++i) {
	if (t <= stops[i].offset) {
	    const Stop & a = stops[i - 1];
	    const Stop & b = stops[i];
	    double u = (b.offset > a.offset) ? (t - a.offset) / (b.offset - a.offset) : 0.0;
	    auto mix = [u](double x, double y) { return x + u * (y - x); };
	    return QColor::fromRgbF(mix(a.color.redF(), b.color.redF()),
				    mix(a.color.greenF(), b.color.greenF()),
				    mix(a.color.blueF(), b.color.blueF()),
				    mix(a.color.alphaF(), b.color.alphaF()));
	}
    }
    return stops.back().color;
}

void drawChecker(QPainter & p, const QRectF & r, int cell = 6) {
    p.save();
    p.setClipRect(r, Qt::IntersectClip);
    p.fillRect(r, QColor(255, 255, 255));
    QColor c(204, 204, 204);
    for (int y = 0; y * cell < r.height(); ++y)
	for (int x = 0; x * cell < r.width(); ++x)
	    if ((x + y) % 2) p.fillRect(QRectF(r.x() + x * cell, r.y() + y * cell, cell, cell), c);
    p.restore();
}

// The brush for a rectangle, using exactly the same geometry as gradient.lua
QBrush gradientBrush(const Spec & s, const QRectF & r) {
    double w = r.width(), h = r.height();
    if (!s.radial) {
	double a = s.angle * kPi / 180.0;
	double dx = std::cos(a), dy = std::sin(a);
	double L = 0.5 * (std::fabs(w * dx) + std::fabs(h * dy));
	QPointF c = r.center();
	// screen y points down, Ipe y points up
	QPointF d(L * dx, -L * dy);
	QLinearGradient g(c - d, c + d);
	g.setStops(qtStops(s.stops));
	g.setSpread(QGradient::PadSpread);
	return QBrush(g);
    } else {
	double R = s.radius * 0.5 * std::max(w, h);
	QPointF p(r.left() + s.cx * w, r.top() + (1.0 - s.cy) * h);
	QPointF f(p.x() + s.fx * R, p.y() - s.fy * R);
	QRadialGradient g(p, std::max(R, 0.01), f);
	g.setStops(qtStops(s.stops));
	g.setSpread(QGradient::PadSpread);
	return QBrush(g);
    }
}

// --------------------------------------------------------------------

class Preview : public QWidget {
public:
    explicit Preview(Spec * s)
	: iSpec(s) {
	setMinimumSize(200, 140);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *) override {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF r = QRectF(rect()).adjusted(12, 12, -12, -12);
	QPainterPath path;
	if (iSpec->radial) {
	    double side = std::min(r.width(), r.height());
	    QRectF sq(r.center().x() - side / 2, r.center().y() - side / 2, side, side);
	    path.addEllipse(sq);
	    r = sq;
	} else
	    path.addRoundedRect(r, 10, 10);
	p.save();
	p.setClipPath(path);
	drawChecker(p, r, 8);
	p.fillPath(path, gradientBrush(*iSpec, r));
	p.restore();
	QPen pen(palette().color(QPalette::Mid), 1.0);
	p.setPen(pen);
	p.drawPath(path);
    }

private:
    Spec * iSpec;
};

// --------------------------------------------------------------------

class StopBar : public QWidget {
public:
    StopBar(Spec * s, std::function<void()> changed, std::function<void()> editColor)
	: iSpec(s)
	, iChanged(std::move(changed))
	, iEditColor(std::move(editColor)) {
	setMinimumHeight(54);
	setMinimumWidth(260);
	setFocusPolicy(Qt::StrongFocus);
	setToolTip("Click the bar to add a color stop, drag a marker to move it,\n"
		   "double-click a marker to change its color, drag it away to remove it.");
    }

    int selected() const { return iSelected; }
    void setSelected(int i) {
	iSelected = i;
	update();
    }

protected:
    QRectF barRect() const { return QRectF(10, 4, width() - 20, 24); }
    double xOf(double offset) const {
	QRectF b = barRect();
	return b.left() + offset * b.width();
    }
    int markerAt(QPointF pos) const {
	for (int i = int(iSpec->stops.size()) - 1; i >= 0; --i) {
	    double x = xOf(iSpec->stops[i].offset);
	    if (std::fabs(pos.x() - x) <= 7 && pos.y() >= 26 && pos.y() <= 50) return i;
	}
	return -1;
    }

    void paintEvent(QPaintEvent *) override {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF b = barRect();
	QPainterPath bar;
	bar.addRoundedRect(b, 5, 5);
	p.save();
	p.setClipPath(bar);
	drawChecker(p, b, 6);
	QLinearGradient g(b.topLeft(), b.topRight());
	g.setStops(qtStops(iSpec->stops));
	p.fillPath(bar, g);
	p.restore();
	p.setPen(QPen(palette().color(QPalette::Mid), 1));
	p.drawPath(bar);

	for (int i = 0; i < int(iSpec->stops.size()); ++i) {
	    const Stop & s = iSpec->stops[i];
	    double x = xOf(s.offset);
	    QPainterPath m;
	    m.moveTo(x, 29);
	    m.lineTo(x + 7, 36);
	    m.lineTo(x + 7, 48);
	    m.lineTo(x - 7, 48);
	    m.lineTo(x - 7, 36);
	    m.closeSubpath();
	    bool sel = (i == iSelected);
	    p.setPen(QPen(sel ? palette().color(QPalette::Highlight)
			      : palette().color(QPalette::Mid),
			  sel ? 2.5 : 1.2));
	    p.setBrush(palette().color(QPalette::Base));
	    p.drawPath(m);
	    QRectF sw(x - 4.5, 37.5, 9, 8);
	    drawChecker(p, sw, 3);
	    p.fillRect(sw, s.color);
	}
    }

    void mousePressEvent(QMouseEvent * ev) override {
	QPointF pos = ev->position();
	int m = markerAt(pos);
	if (m >= 0) {
	    iSelected = m;
	    iDragging = true;
	} else if (barRect().adjusted(0, -4, 0, 4).contains(pos)) {
	    // add a new stop with the interpolated color
	    QRectF b = barRect();
	    double t = std::clamp((pos.x() - b.left()) / b.width(), 0.0, 1.0);
	    QColor c = colorAt(iSpec->stops, t);
	    iSpec->stops.push_back({t, c});
	    sortStops(iSpec->stops);
	    for (int i = 0; i < int(iSpec->stops.size()); ++i)
		if (iSpec->stops[i].offset == t) iSelected = i;
	    iDragging = true;
	}
	iChanged();
	update();
    }

    void mouseMoveEvent(QMouseEvent * ev) override {
	if (!iDragging || iSelected < 0) return;
	QRectF b = barRect();
	double t = std::clamp((ev->position().x() - b.left()) / b.width(), 0.0, 1.0);
	bool away = ev->position().y() > height() + 25 || ev->position().y() < -25;
	iRemovePending = away && iSpec->stops.size() > 2;
	setCursor(iRemovePending ? Qt::ForbiddenCursor : Qt::ArrowCursor);
	Stop moved = iSpec->stops[iSelected];
	moved.offset = t;
	iSpec->stops.erase(iSpec->stops.begin() + iSelected);
	auto it = std::upper_bound(
	    iSpec->stops.begin(), iSpec->stops.end(), t,
	    [](double v, const Stop & s) { return v < s.offset; });
	iSelected = int(it - iSpec->stops.begin());
	iSpec->stops.insert(it, moved);
	iChanged();
	update();
    }

    void mouseReleaseEvent(QMouseEvent *) override {
	if (iRemovePending && iSelected >= 0 && iSpec->stops.size() > 2) {
	    iSpec->stops.erase(iSpec->stops.begin() + iSelected);
	    iSelected = std::min(iSelected, int(iSpec->stops.size()) - 1);
	    iChanged();
	}
	iRemovePending = false;
	iDragging = false;
	unsetCursor();
	update();
    }

    void mouseDoubleClickEvent(QMouseEvent * ev) override {
	if (markerAt(ev->position()) >= 0) iEditColor();
    }

    void keyPressEvent(QKeyEvent * ev) override {
	if ((ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Backspace)
	    && iSelected >= 0 && iSpec->stops.size() > 2) {
	    iSpec->stops.erase(iSpec->stops.begin() + iSelected);
	    iSelected = std::min(iSelected, int(iSpec->stops.size()) - 1);
	    iChanged();
	    update();
	} else
	    QWidget::keyPressEvent(ev);
    }

private:
    Spec * iSpec;
    std::function<void()> iChanged;
    std::function<void()> iEditColor;
    int iSelected = 0;
    bool iDragging = false;
    bool iRemovePending = false;
};

// --------------------------------------------------------------------

struct Preset {
    const char * name;
    bool radial;
    double fx, fy;
    std::vector<std::pair<double, unsigned>> stops; // offset, 0xAARRGGBB
};

const std::vector<Preset> & presets() {
    static const std::vector<Preset> p = {
	{"Sunset", false, 0, 0, {{0, 0xffff5e62}, {0.5, 0xffff9966}, {1, 0xffffe29f}}},
	{"Ocean", false, 0, 0, {{0, 0xff0b3d91}, {0.55, 0xff1e88e5}, {1, 0xff9be7ff}}},
	{"Forest", false, 0, 0, {{0, 0xff134e2a}, {1, 0xffa8e063}}},
	{"Fire", false, 0, 0, {{0, 0xff7a0000}, {0.45, 0xffff3d00}, {1, 0xffffd600}}},
	{"Lavender", false, 0, 0, {{0, 0xff6a3093}, {1, 0xffd9a7c7}}},
	{"Steel", false, 0, 0, {{0, 0xff434343}, {0.5, 0xffd7d7d7}, {1, 0xff606060}}},
	{"Grayscale", false, 0, 0, {{0, 0xff000000}, {1, 0xffffffff}}},
	{"Rainbow",
	 false,
	 0,
	 0,
	 {{0, 0xffe53935},
	  {0.2, 0xffffb300},
	  {0.4, 0xfffdd835},
	  {0.6, 0xff43a047},
	  {0.8, 0xff1e88e5},
	  {1, 0xff8e24aa}}},
	{"Fade out", false, 0, 0, {{0, 0xff1e88e5}, {1, 0x001e88e5}}},
	{"Glass ball", true, -0.35, 0.4, {{0, 0xffffffff}, {0.25, 0xff7fb2ff}, {1, 0xff0b3d91}}},
	{"Red sphere", true, -0.35, 0.4, {{0, 0xffffe0e0}, {0.3, 0xffff5252}, {1, 0xff6d0000}}},
	{"Spotlight", true, 0, 0, {{0, 0xffffffff}, {1, 0x00ffffff}}},
    };
    return p;
}

std::vector<Stop> presetStops(const Preset & pr) {
    std::vector<Stop> v;
    for (auto & [o, argb] : pr.stops)
	v.push_back({o, QColor::fromRgba(argb)});
    return v;
}

QIcon presetIcon(const Preset & pr) {
    QPixmap pm(56, 28);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r(1, 1, 54, 26);
    QPainterPath path;
    if (pr.radial) {
	path.addEllipse(QRectF(15, 1, 26, 26));
	r = QRectF(15, 1, 26, 26);
    } else
	path.addRoundedRect(r, 5, 5);
    p.setClipPath(path);
    drawChecker(p, r, 4);
    Spec s;
    s.radial = pr.radial;
    s.fx = pr.fx;
    s.fy = pr.fy;
    s.stops = presetStops(pr);
    p.fillPath(path, gradientBrush(s, r));
    p.end();
    return QIcon(pm);
}

QPixmap swatch(const QColor & c, int w = 34, int h = 18) {
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QRectF r(0, 0, w, h);
    drawChecker(p, r, 4);
    p.fillRect(r, c);
    p.setPen(QColor(0, 0, 0, 90));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    return pm;
}

// --------------------------------------------------------------------

double getNumberField(lua_State * L, int t, const char * key, double deflt) {
    lua_getfield(L, t, key);
    double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : deflt;
    lua_pop(L, 1);
    return v;
}

Spec readSpec(lua_State * L, int t) {
    Spec s;
    lua_getfield(L, t, "type");
    s.radial = lua_isstring(L, -1) && QString(lua_tostring(L, -1)) == "radial";
    lua_pop(L, 1);
    s.angle = getNumberField(L, t, "angle", 0.0);
    s.cx = getNumberField(L, t, "cx", 0.5);
    s.cy = getNumberField(L, t, "cy", 0.5);
    s.radius = getNumberField(L, t, "radius", 1.0);
    s.fx = getNumberField(L, t, "fx", 0.0);
    s.fy = getNumberField(L, t, "fy", 0.0);
    lua_getfield(L, t, "extend");
    s.extend = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, t, "target");
    s.stroke = lua_isstring(L, -1) && QString(lua_tostring(L, -1)) == "stroke";
    lua_pop(L, 1);
    lua_getfield(L, t, "stops");
    if (lua_istable(L, -1)) {
	int n = int(lua_rawlen(L, -1));
	for (int i = 1; i <= n; ++i) {
	    lua_rawgeti(L, -1, i);
	    if (lua_istable(L, -1)) {
		int st = lua_gettop(L);
		double off = getNumberField(L, st, "offset", 0.0);
		double op = getNumberField(L, st, "opacity", 1.0);
		double r = 0, g = 0, b = 0;
		lua_getfield(L, st, "color");
		if (lua_istable(L, -1)) {
		    int ct = lua_gettop(L);
		    r = getNumberField(L, ct, "r", 0.0);
		    g = getNumberField(L, ct, "g", 0.0);
		    b = getNumberField(L, ct, "b", 0.0);
		}
		lua_pop(L, 1);
		s.stops.push_back({std::clamp(off, 0.0, 1.0),
				   QColor::fromRgbF(std::clamp(r, 0.0, 1.0),
						    std::clamp(g, 0.0, 1.0),
						    std::clamp(b, 0.0, 1.0),
						    std::clamp(op, 0.0, 1.0))});
	    }
	    lua_pop(L, 1);
	}
    }
    lua_pop(L, 1);
    if (s.stops.size() < 2) {
	s.stops = {{0.0, QColor(30, 136, 229)}, {1.0, QColor(255, 255, 255)}};
    }
    sortStops(s.stops);
    return s;
}

void setNumberField(lua_State * L, const char * key, double v) {
    // round to 4 decimals to keep the XML tidy
    lua_pushnumber(L, std::round(v * 10000.0) / 10000.0);
    lua_setfield(L, -2, key);
}

void pushSpec(lua_State * L, const Spec & s) {
    lua_createtable(L, 0, 10);
    lua_pushstring(L, s.radial ? "radial" : "linear");
    lua_setfield(L, -2, "type");
    setNumberField(L, "angle", s.angle);
    setNumberField(L, "cx", s.cx);
    setNumberField(L, "cy", s.cy);
    setNumberField(L, "radius", s.radius);
    setNumberField(L, "fx", s.fx);
    setNumberField(L, "fy", s.fy);
    lua_pushboolean(L, s.extend);
    lua_setfield(L, -2, "extend");
    lua_pushstring(L, s.stroke ? "stroke" : "fill");
    lua_setfield(L, -2, "target");
    lua_createtable(L, int(s.stops.size()), 0);
    for (int i = 0; i < int(s.stops.size()); ++i) {
	const Stop & st = s.stops[i];
	lua_createtable(L, 0, 3);
	setNumberField(L, "offset", st.offset);
	setNumberField(L, "opacity", st.color.alphaF());
	lua_createtable(L, 0, 3);
	setNumberField(L, "r", st.color.redF());
	setNumberField(L, "g", st.color.greenF());
	setNumberField(L, "b", st.color.blueF());
	lua_setfield(L, -2, "color");
	lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "stops");
}

} // namespace

// --------------------------------------------------------------------

int ipeui_gradientDialog(lua_State * L) {
    QWidget * parent = check_winid(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    Spec spec = readSpec(L, 2);
    bool hasStroke = true;
    lua_getfield(L, 2, "allow_stroke");
    if (!lua_isnil(L, -1)) hasStroke = lua_toboolean(L, -1);
    lua_pop(L, 1);

    QDialog dlg(parent);
    dlg.setWindowTitle("Gradient");
    dlg.setModal(true);

    auto * outer = new QHBoxLayout(&dlg);
    outer->setSpacing(16);

    // ---------- left: preview + presets ----------
    auto * left = new QVBoxLayout;
    auto * preview = new Preview(&spec);
    left->addWidget(preview, 1);
    auto * presetLabel = new QLabel("Presets");
    presetLabel->setObjectName("SectionLabel");
    left->addWidget(presetLabel);
    auto * presetGrid = new QGridLayout;
    presetGrid->setSpacing(4);
    left->addLayout(presetGrid);
    outer->addLayout(left, 1);

    // ---------- right: controls ----------
    auto * right = new QVBoxLayout;
    outer->addLayout(right, 1);

    // type
    auto * typeRow = new QHBoxLayout;
    auto * linearBtn = new QToolButton;
    linearBtn->setText("Linear");
    linearBtn->setCheckable(true);
    auto * radialBtn = new QToolButton;
    radialBtn->setText("Radial");
    radialBtn->setCheckable(true);
    auto * typeGroup = new QButtonGroup(&dlg);
    typeGroup->setExclusive(true);
    typeGroup->addButton(linearBtn, 0);
    typeGroup->addButton(radialBtn, 1);
    for (auto * b : {linearBtn, radialBtn}) {
	b->setToolButtonStyle(Qt::ToolButtonTextOnly);
	b->setMinimumWidth(80);
    }
    typeRow->addWidget(linearBtn);
    typeRow->addWidget(radialBtn);
    typeRow->addStretch(1);
    auto * reverseBtn = new QPushButton("Reverse");
    reverseBtn->setToolTip("Reverse the order of the colors");
    typeRow->addWidget(reverseBtn);
    right->addLayout(typeRow);

    // color stops
    auto * stopsBox = new QGroupBox("Colors");
    auto * stopsLo = new QVBoxLayout(stopsBox);
    StopBar * bar = nullptr;
    auto * colorBtn = new QPushButton;
    auto * posSpin = new QDoubleSpinBox;
    auto * opacitySlider = new QSlider(Qt::Horizontal);
    auto * opacityLabel = new QLabel;
    auto * removeBtn = new QPushButton("Remove");

    // the update machinery
    bool updating = false;
    std::function<void()> refreshStopControls;
    auto changed = [&]() {
	preview->update();
	if (bar) bar->update();
	if (refreshStopControls) refreshStopControls();
    };
    auto editColor = [&]() {
	int i = bar->selected();
	if (i < 0 || i >= int(spec.stops.size())) return;
	QColor c = QColorDialog::getColor(spec.stops[i].color, &dlg, "Stop color",
					  QColorDialog::ShowAlphaChannel);
	if (c.isValid()) {
	    spec.stops[i].color = c;
	    changed();
	}
    };
    bar = new StopBar(&spec, changed, editColor);
    stopsLo->addWidget(bar);

    auto * stopForm = new QGridLayout;
    stopForm->setHorizontalSpacing(8);
    stopForm->addWidget(new QLabel("Color"), 0, 0);
    colorBtn->setToolTip("Change the color of the selected stop");
    colorBtn->setIconSize(QSize(34, 18));
    stopForm->addWidget(colorBtn, 0, 1);
    stopForm->addWidget(new QLabel("Position"), 0, 2);
    posSpin->setRange(0, 100);
    posSpin->setDecimals(1);
    posSpin->setSuffix(" %");
    stopForm->addWidget(posSpin, 0, 3);
    stopForm->addWidget(new QLabel("Opacity"), 1, 0);
    opacitySlider->setRange(0, 100);
    stopForm->addWidget(opacitySlider, 1, 1, 1, 2);
    opacityLabel->setMinimumWidth(40);
    stopForm->addWidget(opacityLabel, 1, 3);
    removeBtn->setToolTip("Remove the selected stop (or press Delete)");
    stopForm->addWidget(removeBtn, 2, 3);
    stopsLo->addLayout(stopForm);
    right->addWidget(stopsBox);

    refreshStopControls = [&]() {
	updating = true;
	int i = bar->selected();
	bool ok = (i >= 0 && i < int(spec.stops.size()));
	colorBtn->setEnabled(ok);
	posSpin->setEnabled(ok);
	opacitySlider->setEnabled(ok);
	removeBtn->setEnabled(ok && spec.stops.size() > 2);
	if (ok) {
	    const Stop & s = spec.stops[i];
	    colorBtn->setIcon(QIcon(swatch(s.color)));
	    colorBtn->setText(s.color.name().toUpper());
	    posSpin->setValue(s.offset * 100.0);
	    opacitySlider->setValue(int(std::round(s.color.alphaF() * 100)));
	    opacityLabel->setText(QString("%1 %").arg(opacitySlider->value()));
	}
	updating = false;
    };

    QObject::connect(colorBtn, &QPushButton::clicked, &dlg, editColor);
    QObject::connect(posSpin, &QDoubleSpinBox::valueChanged, &dlg, [&](double v) {
	if (updating) return;
	int i = bar->selected();
	if (i < 0) return;
	Stop s = spec.stops[i];
	s.offset = v / 100.0;
	spec.stops.erase(spec.stops.begin() + i);
	auto it = std::upper_bound(spec.stops.begin(), spec.stops.end(), s.offset,
				   [](double x, const Stop & st) { return x < st.offset; });
	bar->setSelected(int(it - spec.stops.begin()));
	spec.stops.insert(it, s);
	changed();
    });
    QObject::connect(opacitySlider, &QSlider::valueChanged, &dlg, [&](int v) {
	if (updating) return;
	int i = bar->selected();
	if (i < 0) return;
	spec.stops[i].color.setAlphaF(v / 100.0);
	changed();
    });
    QObject::connect(removeBtn, &QPushButton::clicked, &dlg, [&]() {
	int i = bar->selected();
	if (i < 0 || spec.stops.size() <= 2) return;
	spec.stops.erase(spec.stops.begin() + i);
	bar->setSelected(std::min(i, int(spec.stops.size()) - 1));
	changed();
    });
    QObject::connect(reverseBtn, &QPushButton::clicked, &dlg, [&]() {
	for (Stop & s : spec.stops) s.offset = 1.0 - s.offset;
	std::reverse(spec.stops.begin(), spec.stops.end());
	bar->setSelected(int(spec.stops.size()) - 1 - bar->selected());
	changed();
    });

    // geometry
    auto * geoStack = new QStackedWidget;
    // linear page
    auto * linPage = new QWidget;
    auto * linLo = new QHBoxLayout(linPage);
    linLo->setContentsMargins(0, 0, 0, 0);
    auto * dial = new QDial;
    dial->setRange(0, 359);
    dial->setWrapping(true);
    dial->setNotchesVisible(true);
    dial->setFixedSize(64, 64);
    auto * angleSpin = new QSpinBox;
    angleSpin->setRange(-360, 360);
    angleSpin->setSuffix("°");
    angleSpin->setWrapping(true);
    auto * angleForm = new QFormLayout;
    angleForm->addRow("Angle", angleSpin);
    auto * quick = new QHBoxLayout;
    for (int a : {0, 45, 90, 135, 180, 270}) {
	auto * qb = new QToolButton;
	qb->setText(QString("%1°").arg(a));
	quick->addWidget(qb);
	QObject::connect(qb, &QToolButton::clicked, &dlg, [angleSpin, a]() {
	    angleSpin->setValue(a);
	});
    }
    auto * linRight = new QVBoxLayout;
    linRight->addLayout(angleForm);
    linRight->addLayout(quick);
    linLo->addWidget(dial);
    linLo->addLayout(linRight, 1);
    geoStack->addWidget(linPage);
    // radial page
    auto * radPage = new QWidget;
    auto * radForm = new QGridLayout(radPage);
    radForm->setContentsMargins(0, 0, 0, 0);
    auto mkSpin = [](double lo, double hi) {
	auto * sp = new QSpinBox;
	sp->setRange(int(lo), int(hi));
	sp->setSuffix(" %");
	sp->setSingleStep(5);
	return sp;
    };
    auto * cxSpin = mkSpin(-100, 200);
    auto * cySpin = mkSpin(-100, 200);
    auto * rSpin = mkSpin(5, 400);
    auto * fxSpin = mkSpin(-95, 95);
    auto * fySpin = mkSpin(-95, 95);
    radForm->addWidget(new QLabel("Center"), 0, 0);
    radForm->addWidget(cxSpin, 0, 1);
    radForm->addWidget(cySpin, 0, 2);
    radForm->addWidget(new QLabel("Size"), 1, 0);
    radForm->addWidget(rSpin, 1, 1);
    radForm->addWidget(new QLabel("Highlight"), 2, 0);
    radForm->addWidget(fxSpin, 2, 1);
    radForm->addWidget(fySpin, 2, 2);
    cxSpin->setToolTip("Horizontal position of the center (0% = left, 100% = right)");
    cySpin->setToolTip("Vertical position of the center (0% = bottom, 100% = top)");
    rSpin->setToolTip("Radius relative to half the size of the object");
    fxSpin->setToolTip("Horizontal offset of the highlight (focus) from the center");
    fySpin->setToolTip("Vertical offset of the highlight (focus) from the center");
    geoStack->addWidget(radPage);

    auto * geoBox = new QGroupBox("Direction");
    auto * geoLo = new QVBoxLayout(geoBox);
    geoLo->addWidget(geoStack);
    right->addWidget(geoBox);

    // options
    auto * extendCheck = new QCheckBox("Extend colors beyond the ends");
    right->addWidget(extendCheck);
    auto * targetRow = new QHBoxLayout;
    targetRow->addWidget(new QLabel("Apply to"));
    auto * fillRadio = new QRadioButton("Fill");
    auto * strokeRadio = new QRadioButton("Outline");
    strokeRadio->setEnabled(hasStroke);
    targetRow->addWidget(fillRadio);
    targetRow->addWidget(strokeRadio);
    targetRow->addStretch(1);
    right->addLayout(targetRow);
    right->addStretch(1);

    auto * buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText("Apply");
    right->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // geometry sync
    auto syncGeometryControls = [&]() {
	updating = true;
	(spec.radial ? radialBtn : linearBtn)->setChecked(true);
	geoStack->setCurrentIndex(spec.radial ? 1 : 0);
	int a = int(std::lround(spec.angle));
	angleSpin->setValue(a);
	dial->setValue(((90 - a) % 360 + 360) % 360); // dial 0 is at the top
	cxSpin->setValue(int(std::lround(spec.cx * 100)));
	cySpin->setValue(int(std::lround(spec.cy * 100)));
	rSpin->setValue(int(std::lround(spec.radius * 100)));
	fxSpin->setValue(int(std::lround(spec.fx * 100)));
	fySpin->setValue(int(std::lround(spec.fy * 100)));
	extendCheck->setChecked(spec.extend);
	(spec.stroke ? strokeRadio : fillRadio)->setChecked(true);
	updating = false;
    };
    QObject::connect(typeGroup, &QButtonGroup::idClicked, &dlg, [&](int id) {
	spec.radial = (id == 1);
	syncGeometryControls();
	changed();
    });
    QObject::connect(angleSpin, &QSpinBox::valueChanged, &dlg, [&](int v) {
	if (updating) return;
	spec.angle = v;
	syncGeometryControls();
	changed();
    });
    QObject::connect(dial, &QDial::valueChanged, &dlg, [&](int v) {
	if (updating) return;
	int a = ((90 - v) % 360 + 360) % 360;
	if (a > 180) a -= 360;
	spec.angle = a;
	syncGeometryControls();
	changed();
    });
    auto bindSpin = [&](QSpinBox * sp, double * field) {
	QObject::connect(sp, &QSpinBox::valueChanged, &dlg, [&, field](int v) {
	    if (updating) return;
	    *field = v / 100.0;
	    changed();
	});
    };
    bindSpin(cxSpin, &spec.cx);
    bindSpin(cySpin, &spec.cy);
    bindSpin(rSpin, &spec.radius);
    bindSpin(fxSpin, &spec.fx);
    bindSpin(fySpin, &spec.fy);
    QObject::connect(extendCheck, &QCheckBox::toggled, &dlg,
		     [&](bool on) { spec.extend = on; });
    QObject::connect(strokeRadio, &QRadioButton::toggled, &dlg,
		     [&](bool on) { spec.stroke = on; });

    // presets
    const auto & ps = presets();
    for (int i = 0; i < int(ps.size()); ++i) {
	auto * b = new QToolButton;
	b->setIcon(presetIcon(ps[i]));
	b->setIconSize(QSize(56, 28));
	b->setToolTip(ps[i].name);
	b->setAutoRaise(true);
	presetGrid->addWidget(b, i / 4, i % 4);
	QObject::connect(b, &QToolButton::clicked, &dlg, [&, i]() {
	    const Preset & pr = presets()[i];
	    spec.stops = presetStops(pr);
	    spec.radial = pr.radial;
	    if (pr.radial) {
		spec.fx = pr.fx;
		spec.fy = pr.fy;
		spec.cx = spec.cy = 0.5;
		spec.radius = 1.0;
	    }
	    bar->setSelected(0);
	    syncGeometryControls();
	    changed();
	});
    }

    syncGeometryControls();
    refreshStopControls();
    dlg.resize(760, 460);

    int result = dlg.exec();
    if (parent) parent->activateWindow();
    if (result != QDialog::Accepted) return 0;
    sortStops(spec.stops);
    pushSpec(L, spec);
    return 1;
}

// --------------------------------------------------------------------
