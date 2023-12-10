
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/file.dart';
import 'package:rwdesign/elemfunc/type.dart';
import 'package:rwdesign/player.dart';

double _convertRadiusToSigma(double radius) {
    return radius * 0.57735 + 0.5;
}

class LedItem {
    final int chan, num;
    final double x;
    final double y;
    LedItem(this.chan, this.num, this.x, this.y);

    Offset calc(double nx, double ny, double anr/* тут в радианах */) =>
        Offset(nx, ny) + rotate(x, y, anr);
}

class LedColor {
    final int num;
    final Color col;
    LedColor(this.num, this.col);
}
class LedChan {
    final int chan;
    final List<LedColor> list = [];
    LedChan(this.chan);
}

class ElemLed {
    final List<LedItem> _data = [];
    bool get isEmpty    => _data.isEmpty;
    bool get isNotEmpty => _data.isNotEmpty;
    void clear() {
        _data.clear();
    }
    
    bool load(dynamic s) {
        if (!(s is List<dynamic>)) return false;
        _data.clear();

        bool ok = true;
        int chan = 0;
        int num = 0;

        s.forEach((d) {
            if (!(d is HashItem)) {
                ok = false;
                return;
            }
            final ch = jint(d, 'chan');
            if (ch != null) {
                chan = ch;
                num = 0;
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
                num ++;
                _data.add(LedItem(chan, num, x,y));
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

    void paint(Canvas canvas, double x, double y, double r, [bool colored = false]) {
        final anr = r*pi/180;
        canvas.save();
        if (colored) {
            final tm = Player().tm;
            final p = Paint()
                ..style = PaintingStyle.fill
                ..maskFilter = MaskFilter.blur(BlurStyle.normal, _convertRadiusToSigma(20));
            _data.forEach((d) {
                final pos = d.calc(x, y, anr);
                final col = ScenarioLght(pos.dx, pos.dy, tm);
                if (col == null) return;
                p.color = col;
                canvas.drawCircle(pos, 6, p);
            });
        }
        else {
            final p = Paint()
                ..color = Colors.blue
                ..style = PaintingStyle.fill;
            _data.forEach(
                (d) => canvas.drawCircle(d.calc(x, y, anr), 1, p)
            );
        }
        canvas.restore();
    }

    List<LedChan> info(int tm, double x, double y, double r) {
        final anr = r*pi/180;
        final Map<int,LedChan> all = {};

        _data.forEach((d) {
            if (all[d.chan] == null)
                all[d.chan] = LedChan(d.chan);
            final ch = all[d.chan];
            final pos = d.calc(x, y, anr);
            final col = ScenarioLght(pos.dx, pos.dy, tm) ?? Color(0);
            ch?.list.add(LedColor(d.num, col));
        });

        final list = all.values.toList();
        list.sort(
            (a,b) => a.chan.compareTo( b.chan )
        );
        list.forEach(
            (ch) => ch.list.sort(
                (a,b) => a.num.compareTo(b.num)
            )
        );

        return list;
    }
}
