
import 'dart:math';

import 'package:flutter/material.dart';

import '../elemfunc/type.dart';
import 'light.dart';

class LParamSector {
    final Offset cen;
    final double r, ang1, ang2;
    final Color  col1, col2;

    LParamSector(HashItem d, [LParamSector? beg]) :
        cen = jxy(d)                ?? beg?.cen     ?? Offset.zero,
        r   = jdouble(d, 'r')       ?? beg?.r       ?? 100,
        ang1= jdouble(d, 'ang1')    ?? beg?.ang1    ?? 0,
        ang2= jdouble(d, 'ang2')    ?? beg?.ang2    ?? 180,
        col1= jcolor(d, 'color1')   ?? jcolor(d, 'color') ?? beg?.col1 ?? Colors.black,
        col2= jcolor(d, 'color2')   ?? jcolor(d, 'color') ?? beg?.col2 ?? jcolor(d, 'color1') ?? Colors.black;
    
    LParamSector.zero() :
        cen = Offset.zero,
        r   = 0,
        ang1= 0,
        ang2= 0,
        col1= Colors.black,
        col2= Colors.black;
}

class LightSector extends LightItem {
    LParamSector
        beg = LParamSector.zero(),
        end = LParamSector.zero();
    
    LightSector(HashItem d) : super(d) {
        beg = LParamSector(d['beg']);
        end = LParamSector(d['end'] ?? d['beg'], beg);
    }

    @override
    void paint(Canvas canvas, int tm) {
        final k = tmk(tm);
        if (k < 0) return;

        // Вычисление всех параметров с учётом движения
        // от beg к end - по tm/k
        final   cen = posdiff(beg.cen,  end.cen,    k);
        final   r   = dbldiff(beg.r,    end.r,      k);
        double  a1  = dbldiff(beg.ang1, end.ang1,   k);
        double  a2  = dbldiff(beg.ang2, end.ang2,   k);
        Color   c1  = coldiff(beg.col1, end.col1,   k);
        Color   c2  = coldiff(beg.col2, end.col2,   k);
        if (a1 > a2)
            // будем двигаться по углу от a1 к a2,
            // поэтому д.б.: a1 <= a2
            (a1, a2, c1, c2) = (a2, a1, c2, c1);

        final alen = a2-a1;
        for (double ax = 0; ax <= alen; ax += 1) {
            // вычисляем по циклу - внешнюю точку и её цвет
            final xy2 = rotate(0, -r, (a1+ax) * pi/180);
            final p = Paint()
                // цвет теперь у нас между c1 и c2
                ..color = coldiff(c1, c2, ax/alen)
                ..style = PaintingStyle.fill;

            canvas.drawLine(cen, cen+xy2, p);
        }
    }
}