
import 'package:flutter/material.dart';

import '../elemfunc/type.dart';
import 'circle.dart';
import 'sector.dart';

class LightItem {
    bool get valid => (tmfin > tmbeg) && (tmfin >= tmend) && (tmend >= tmbeg);
    final int tmbeg;
    int tmend = 0, tmfin = 0;

    LightItem(HashItem d) :
        tmbeg = jint(d, 'start') ?? 0
    {
        final tm  = jint(d, 'tm') ?? 0;
        if (tm > 0)
            tmend = tmbeg + tm;
        tmfin = tmend + (jint(d, 'wait') ?? 0);
    }

    LightItem.empty() :
        tmbeg = 0;

    double tmk(int tm) {
        if ((tm < tmbeg) || (tm >= tmfin))
            return -1;
        if (tm >= tmend)
            return 1;
        return (tm-tmbeg) / (tmend-tmbeg);
    }
    bool intm(int tm) => valid && tmk(tm) >= 0;

    void paint(Canvas canvas, int tm) {}

    Color? color(double x, double y, int tm) => null;
}

class Light {
    final List<LightItem> _data = [];
    void clear() {
        _data.clear();
    }
    
    bool load(dynamic s) {
        if (!(s is List<dynamic>)) return false;
        _data.clear();

        bool ok = true;

        s.forEach((d) {
            if (!(d is HashItem)) {
                ok = false;
                return;
            }

            final fig = d['fig'];
            if (!(fig is String)) {
                ok = false;
                return;
            }

            final l =
                fig == 'circle' ?
                    LightCircle(d) :
                fig == 'sector' ?
                    LightSector(d) :
                    LightItem.empty();
            if (!l.valid) {
                ok = false;
                return;
            }

            _data.add(l);
        });

        return ok;
    }

    void paint(Canvas canvas, int tm) {
        canvas.save();
        _data.forEach((l) => l.paint(canvas, tm));
        canvas.restore();
    }

    Color? color(double x, double y, int tm) {
        for (final l in _data.reversed) {
            final col = l.color(x, y, tm);
            if (col != null)
                return col;
        }
        return null;
    }
}
