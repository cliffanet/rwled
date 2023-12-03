
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/type.dart';

class LedItem {
    final double x;
    final double y;
    LedItem(this.x, this.y);

    Offset calc(double nx, double ny, double anr/* тут в радианах */) =>
        Offset(nx, ny) + rotate(x, y, anr);
}

class ElemLed {
    final List<LedItem> _data = [];
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
            final beg = jxy(d) ?? Offset(0, 0);
            final ang = jdouble(d, 'ang') ?? 0;
            final anr = ang*pi/180;
            final cnt = jint(d, 'count') ?? 20;
            final len = jdouble(d, 'length') ?? 50;
            
            // интервал смещения по x
            final intx = len*sin(anr) / cnt;
            final inty = len*cos(anr) / cnt * (-1); // -1: наша O-Y направлена вниз

            double x = beg.dx;
            double y = beg.dy;
            for (int n = 0; n < cnt; n++) {
                _data.add(LedItem(x,y));
                x += intx;
                y += inty;
            }
        });

        return ok;
    }

    void clone(ElemLed orig) {
        _data.clear();
        _data.addAll(orig._data);
    }

    void paint(Canvas canvas, double x, double y, double r) {
        final anr = r*pi/180;
        final p = Paint()
            ..color = Colors.blue
            ..style = PaintingStyle.fill;
        canvas.save();
        _data.forEach((d) => canvas.drawCircle(d.calc(x, y, anr), 1, p));
        canvas.restore();
    }
}
