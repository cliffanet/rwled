import 'dart:developer' as developer;

import 'package:flutter/material.dart';
import 'package:rwdesign/player.dart';

class PlacePainter extends CustomPainter {
    final void Function(Size size) ?onSize;
    PlacePainter({ this.onSize });

    @override
    void paint(Canvas canvas, Size size) {
        if (onSize != null) {
            onSize!(size);
        }

        canvas.save();
        //developer.log('tap on: $size');

        Player().paint(canvas, size);
        //canvas.

        /*
        // Рисование точки с инфой
        if (view.posVisible &&
            (view.pos >= 0) && (view.pos < trk.data.length) &&
            trk.data[view.pos].satValid) {
            final ti = trk.data[view.pos];

            final p = view.getPoint(TrkViewPoint.byLogItem(ti, tr).pnt);
            final paintPoint = Paint()
                    ..color = //Colors.orange//
                                trkColor(ti['state'])
                    ..maskFilter = MaskFilter.blur(BlurStyle.normal, convertRadiusToSigma(2));
            canvas.drawCircle(p, 7, paintPoint);

            // info
            final text = TextPainter(
                text: TextSpan(
                    text: '[${view.pos}] ${ti.time}\nверт: ${ti.dscrVert}\nгор: ${ti.dscrHorz}\n${ti.dscrKach}',
                    style: const TextStyle(
                        color: Colors.black,
                        fontSize: 10,
                    )
                ),
                textDirection: TextDirection.ltr,
            );
            text.layout();
            
            // rect
            final rect = RRect.fromRectXY(
                Rect.fromPoints(p+Offset(15, 15), p+Offset(25+text.width, 25+text.height)),
                7, 7
            );
            final paintInfo = Paint()
                    ..color = Colors.white.withAlpha(100)
                    ..maskFilter = MaskFilter.blur(BlurStyle.normal, convertRadiusToSigma(2));
            canvas.drawRRect(rect, paintInfo);

            // paint info
            text.paint(canvas, p+Offset(20, 20));
        }
        */

        canvas.restore();
    }

    @override
    bool shouldRepaint(PlacePainter oldDelegate) => true;
}
