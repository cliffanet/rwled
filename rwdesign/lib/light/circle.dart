
import 'dart:math';

import 'package:flutter/material.dart';

import '../elemfunc/type.dart';
import 'light.dart';

class LParamCircle {
    final Offset cen;
    final double r;
    final Color  col;

    LParamCircle(HashItem d, [LParamCircle? beg]) :
        cen = jxy(d)            ?? beg?.cen ?? Offset.zero,
        r   = jdouble(d, 'r')   ?? beg?.r   ?? 100,
        col = jcolor(d)         ?? beg?.col ?? Colors.black;
    
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
        end = LParamCircle(d['end'] ?? d['beg'], beg);
    }

    @override
    void paint(Canvas canvas, int tm) {
        final k = tmk(tm);
        if (k < 0) return;

        final p = Paint()
            ..color = coldiff(beg.col, end.col, k)
            ..style = PaintingStyle.fill;
        final cen = posdiff(beg.cen,   end.cen, k);
        final r   = dbldiff(beg.r,     end.r,   k);
        canvas.drawCircle(cen, r, p);
    }
    
    @override
    Color? color(double x, double y, int tm) {
        final k = tmk(tm);
        if (k < 0) return null;

        final cen = posdiff(beg.cen,   end.cen, k);
        final r   = dbldiff(beg.r,     end.r,   k);
        final r1 = sqrt(pow(cen.dx-x, 2) + pow(cen.dy-y, 2));
        return r1 <= r ? coldiff(beg.col, end.col, k) : null;
    }
}