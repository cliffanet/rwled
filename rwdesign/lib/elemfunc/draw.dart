
import 'dart:math';

import 'package:flutter/material.dart';

import 'type.dart';

typedef DrawItem = Function (ElemDraw e, Canvas canvas);
typedef DrawList = List<DrawItem>;

double _convertRadiusToSigma(double radius) {
    return radius * 0.57735 + 0.5;
}

Offset? _size(dynamic s) {
    if (!(s is HashItem)) return null;
    final w = jdouble(s, 'width');
    if (w == null) return null;
    final h = jdouble(s, 'height');
    if (h == null) return null;
    return Offset(w, h);
}

Path? _path(dynamic s) {
    if (!(s is List<dynamic>)) return null;

    final p = Path();
    bool ok = true;
    int n = 0;
    s.forEach((d) {
        if (!(d is HashItem)) {
            ok = false;
            return;
        }
        final pos = jxy(d);
        if (pos == null) {
            ok = false;
            return;
        }
        if (n == 0) {
            p.moveTo(pos.dx, pos.dy);
        }
        else {
            final bez = jxy(d, 'qbezier');
            if (bez == null) {
                p.lineTo(pos.dx, pos.dy);
            }
            else {
                p.quadraticBezierTo(pos.dx, pos.dy, bez.dx, bez.dy);
            }
        }
        n++;
    });

    if (!ok) return null;
    
    p.close();
    return p;
}

class ElemDraw {
    int num = 0;
    final DrawList _data = [];
    void clear() {
        num = 0;
        _data.clear();
    }
    
    bool load(dynamic s) {
        if (!(s is List<dynamic>)) return false;
        _data.clear();

        bool ok = true;
        final pntF = Paint()
                ..color = Colors.grey
                ..style = PaintingStyle.fill;
        final pntB = Paint()
                ..color = Colors.black
                ..style = PaintingStyle.stroke
                ..maskFilter = MaskFilter.blur(BlurStyle.inner, _convertRadiusToSigma(1));

        s.forEach((d) {
            if (!(d is HashItem)) {
                ok = false;
                return;
            }
            if (d['fig'] is String) {
                final cen = jxy(d) ?? Offset(0, 0);
                final nob = jbool(d, 'noborder') ?? false;

                switch (d['fig'] as String) {
                    case 'num':
                        final fsize = jdouble(d, 'fontsize') ?? 12;
                        final width = jdouble(d, 'width');
                        _data.add((e, c) {
                            // Перенос формирования TextPainter в рисовальщик
                            // необходим для корректной работы this.num
                            final text = TextPainter(
                                text: TextSpan(
                                    text: e.num.toString(),
                                    style: TextStyle(
                                        color: Colors.black,
                                        fontSize: fsize,
                                    )
                                ),
                                textDirection: TextDirection.ltr
                            );
                            text.layout();
                            text.paint(c, cen-Offset(text.width/2, text.height/2));
                        });
                        break;
                    case 'rect':
                        final siz = _size(d);
                        if (siz == null) break;
                        final rnd = jxy(d, 'round') ?? Offset(0, 0);
                        final r = Rect.fromCenter(center: cen, width: siz.dx, height: siz.dy);
                        if ((rnd.dx == 0) && (rnd.dy == 0)) {
                            _data.add((e, c) => c.drawRect(r, pntF));
                            if (!nob)
                                _data.add((e, c) => c.drawRect(r, pntB));
                        }
                        else {
                            final rr = RRect.fromRectXY(r, rnd.dx, rnd.dy);
                            _data.add((e, c) => c.drawRRect(rr, pntF));
                            if (!nob)
                                _data.add((e, c) => c.drawRRect(rr, pntB));
                        }
                        break;
                    
                    case 'circle':
                        final rad   = jdouble(d, 'radius') ?? 0;
                        final radxy = jxy(d, 'radius') ?? Offset(0, 0);
                        if (rad > 0) {
                            _data.add((e, c) => c.drawCircle(cen, rad, pntF));
                            if (!nob)
                                _data.add((e, c) => c.drawCircle(cen, rad, pntB));
                        }
                        else
                        if ((radxy.dx > 0) && (radxy.dy > 0)) {
                            final r = Rect.fromCenter(center: cen, width: radxy.dx, height: radxy.dy);
                            _data.add((e, c) => c.drawOval(r, pntF));
                            if (!nob)
                                _data.add((e, c) => c.drawOval(r, pntB));
                        }
                        break;
                    
                    case 'path':
                        final path = _path(d['path']);
                        if (path != null) {
                            _data.add((e, c) => c.drawPath(path, pntF));
                            if (!nob)
                                _data.add((e, c) => c.drawPath(path, pntB));
                        }
                        break;

                    default: ok = false;
                }
            }
        });

        return ok;
    }

    void clone(ElemDraw orig) {
        _data.clear();
        _data.addAll(orig._data);
    }

    void paint(Canvas canvas, double x, double y, double r) {
        final ang = r*pi/180;
        canvas.save();
        canvas.rotate(ang);
        canvas.translate(
            // translate он будет делать под углом, заданым предыдущим rotate(),
            // поэтому координаты перемещения надо тоже провернуть,
            // но в обратную сторону
            x * cos(-ang) - y * sin(-ang),
            x * sin(-ang) + y * cos(-ang)
        );
        _data.forEach((d) => d(this, canvas));
        canvas.restore();
    }
}
