
import 'package:flutter/material.dart';

import '../elemfunc/type.dart';
import 'light.dart';

class LParamCircle {
    final Offset cen;
    final double r;
    final Color  col;

    LParamCircle(HashItem d, [LParamCircle? beg]) :
        cen = jxy(d)            ?? (beg != null ? beg.cen   : Offset.zero),
        r   = jdouble(d, 'r')   ?? (beg != null ? beg.r     : 100),
        col = jcolor(d)         ?? (beg != null ? beg.col   : Colors.black);
    
    LParamCircle.zero() :
        cen = Offset.zero,
        r   = 0,
        col = Colors.black;
}

class LightCircle extends LightItem {
    LParamCircle
        beg = LParamCircle.zero(),
        end = LParamCircle.zero();
    
    LightCircle(HashItem d) : super(d) {
        beg = LParamCircle(d['beg']);
        end = LParamCircle(d['end'], beg);
    }

    @override
    void paint(Canvas canvas, int tm) {
        final k = tmk(tm);
        if (k < 0) return;

        final p = Paint()
            ..color = coldiff(beg.col, end.col, k)
            ..style = PaintingStyle.fill;
        final cen = (end.cen-beg.cen)*k+beg.cen;
        final r   = (end.r  -beg.r)  *k+beg.r;
        canvas.drawCircle(cen, r, p);
    }
}