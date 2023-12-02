
import 'dart:math';

import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/type.dart';
import 'package:rwdesign/light/circle.dart';

class LightItem {
    bool valid = false;
    int tmbeg = 0, tmend = 0; 

    LightItem(HashItem d) {
        final beg = jint(d, 'start');
        final tm  = jint(d, 'tm') ?? 0;
        if ((beg == null) || (tm <= 0))
            return;
        
        tmbeg = beg;
        tmend = beg + tm;

        valid = true;
    }

    LightItem.empty();

    double tmk(int tm) {
        if ((tm < tmbeg) || (tm >= tmend))
            return -1;
        return (tm-tmbeg) / (tmend-tmbeg);
    }
    bool intm(int tm) => tmk(tm) >= 0;

    void paint(Canvas canvas, int tm) {}
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
}
