
import 'package:flutter/material.dart';

import 'type.dart';

typedef DrawItem = Function (Canvas canvas, double x, double y);
typedef DrawList = List<DrawItem>;

double _convertRadiusToSigma(double radius) {
    return radius * 0.57735 + 0.5;
}

Offset? _xy(dynamic s, [String? prefix]) {
    if (!(s is HashItem)) return null;
    final p = prefix ?? '';
    final x = jdouble(s, p+'x');
    final y = jdouble(s, p+'y');
    if ((x == null) && (y == null))
        return null;
    return Offset(x ?? 0, y ?? 0);
}

Offset? _size(dynamic s) {
    if (!(s is HashItem)) return null;
    final w = jdouble(s, 'width');
    if (w == null) return null;
    final h = jdouble(s, 'height');
    if (h == null) return null;
    return Offset(w, h);
}

Path? _path(dynamic s, double x, double y) {
    if (!(s is List<dynamic>)) return null;

    final p = Path();
    bool ok = true;
    int n = 0;
    s.forEach((d) {
        if (!(d is HashItem)) {
            ok = false;
            return;
        }
        final pos = _xy(d);
        if (pos == null) {
            ok = false;
            return;
        }
        if (n == 0) {
            p.moveTo(x+pos.dx, y+pos.dy);
        }
        else {
            final bez = _xy(d, 'qbezier');
            if (bez == null) {
                p.lineTo(x+pos.dx, y+pos.dy);
            }
            else {
                p.quadraticBezierTo(x+pos.dx, y+pos.dy, x+bez.dx, y+bez.dy);
            }
        }
        n++;
    });

    if (!ok) return null;
    
    p.close();
    return p;
}

class ElemDraw {
    final DrawList _data = [];
    void clear() => _data.clear();
    
    bool load(dynamic s) {
        if (!(s is List<dynamic>)) return false;
        clear();

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
                final cen = _xy(d) ?? Offset(0, 0);
                final nob = jbool(d, 'noborder') ?? false;

                switch (d['fig'] as String) {
                    case 'rect':
                        final siz = _size(d);
                        if (siz == null) break;
                        final rnd = _xy(d, 'round') ?? Offset(0, 0);
                    
                        _data.add(
                            (rnd.dx == 0) && (rnd.dy == 0) ?
                            (c, x, y) {
                                final r = Rect.fromCenter(center: Offset(x, y)+cen, width: siz.dx, height: siz.dy);
                                c.drawRect(r, pntF);
                                if (!nob) c.drawRect(r, pntB);
                            } :
                            (c, x, y) {
                                final r = RRect.fromRectXY(
                                    Rect.fromCenter(center: Offset(x, y)+cen, width: siz.dx, height: siz.dy),
                                    rnd.dx, rnd.dy
                                );
                                c.drawRRect(r, pntF);
                                if (!nob) c.drawRRect(r, pntB);
                            }
                        );
                        break;
                    
                    case 'circle':
                        final rad   = jdouble(d, 'radius') ?? 0;
                        final radxy = _xy(d, 'radius') ?? Offset(0, 0);
                        if ((rad == 0) && (radxy.dx == 0) && (radxy.dy == 0)) break;
                    
                        _data.add(
                            (radxy.dx == 0) && (radxy.dy == 0) ?
                            (c, x, y) {
                                final p = Offset(x, y)+cen;
                                c.drawCircle(p, rad!, pntF);
                                if (!nob) c.drawCircle(p, rad!, pntB);
                            } :
                            (c, x, y) {
                                final r = Rect.fromCenter(center: Offset(x, y)+cen, width: radxy.dx, height: radxy.dy);
                                c.drawOval(r, pntF);
                                if (!nob) c.drawOval(r, pntB);
                            }
                        );
                        break;
                    
                    case 'path':
                        _data.add((c, x, y) {
                            final path = _path(d['path'], x, y);
                            if (path == null) return;
                            c.drawPath(path, pntF);
                            if (!nob) c.drawPath(path, pntB);
                        });
                        break;

                    default: ok = false;
                }
            }
        });

        return ok;
    }

    void paint(Canvas canvas, double x, double y) {
        _data.forEach((d) => d(canvas, x, y));
    }
}
