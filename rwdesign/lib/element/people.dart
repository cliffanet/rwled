import 'package:flutter/material.dart';
import 'package:rwdesign/player.dart';

class People extends PlayerElement {
    static int n = 0;
    People() : super('Участник ${++n}');

    @override
    bool mypnt(Offset p) {
        final d = pos-p;
        return (d.dx.abs() < 25) && (d.dy >= -50) && (d.dy < 28);
    }

    @override
    void paint(Canvas canvas, int tm) {
        final pntF = Paint()
                ..color = Colors.grey
                ..style = PaintingStyle.fill;
                //..maskFilter = MaskFilter.blur(BlurStyle.normal, PlayerElement.convertRadiusToSigma(2));
        final pntB = Paint()
                ..color = Colors.black
                ..style = PaintingStyle.stroke;
        
        canvas.drawRect(Rect.fromCenter(center: pos+Offset(0,22), width: 20, height: 16), pntF);

        Path path = Path()
            ..moveTo(pos.dx+10, pos.dy-13)
            ..quadraticBezierTo(pos.dx+15, pos.dy-15, pos.dx+20, pos.dy-33)
            ..quadraticBezierTo(pos.dx+25, pos.dy-37, pos.dx+30, pos.dy-33)
            ..quadraticBezierTo(pos.dx+28, pos.dy-25, pos.dx+17, pos.dy-13)
            ..quadraticBezierTo(pos.dx+13, pos.dy-10, pos.dx+13, pos.dy+13)
            ..lineTo(pos.dx-13, pos.dy+13)
            ..quadraticBezierTo(pos.dx-13, pos.dy-10, pos.dx-17, pos.dy-13)
            ..quadraticBezierTo(pos.dx-28, pos.dy-25, pos.dx-30, pos.dy-33)
            ..quadraticBezierTo(pos.dx-25, pos.dy-37, pos.dx-20, pos.dy-33)
            ..quadraticBezierTo(pos.dx-15, pos.dy-15, pos.dx-10, pos.dy-13)
            ..close();
        canvas.drawPath(path, pntF);
        canvas.drawPath(path, pntB);

        canvas.drawRRect(RRect.fromRectXY(Rect.fromCenter(center: pos, width: 20, height: 26), 3, 3), pntF);
        canvas.drawRRect(RRect.fromRectXY(Rect.fromCenter(center: pos, width: 20, height: 26), 3, 3), pntB);
        canvas.drawCircle(pos-Offset(0,23), 10, pntF);
        canvas.drawCircle(pos-Offset(0,23), 10, pntB);

        canvas.drawOval(Rect.fromCenter(center: pos+Offset(-12, 32), width: 22, height: 36), pntF);
        canvas.drawOval(Rect.fromCenter(center: pos+Offset(-12, 32), width: 22, height: 36), pntB);
        canvas.drawOval(Rect.fromCenter(center: pos+Offset(12, 32),  width: 22, height: 36), pntF);
        canvas.drawOval(Rect.fromCenter(center: pos+Offset(12, 32),  width: 22, height: 36), pntB);
    }
}
