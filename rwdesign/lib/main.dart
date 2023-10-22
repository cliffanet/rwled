import 'package:flutter/material.dart';
import 'package:rwdesign/view/ctrl.dart';
import 'package:rwdesign/view/title.dart';
import 'package:rwdesign/painter.dart';

void main() {
    runApp(const MaterialApp(
        title: 'RW-led Design Studio',
        home: DesignApp(),
        debugShowCheckedModeBanner: false
    ));
}

class DesignApp extends StatelessWidget {
    const DesignApp({super.key});

    // This widget is the root of your application.
    @override
    Widget build(BuildContext context) {
        return Scaffold(
            appBar: PlaceTitle(),
            body: Stack(children: [
                CustomPaint(
                    painter: PlacePainter(),
                    size: Size.infinite,
                ),
                Positioned(
                    width: MediaQuery.of(context).size.width,
                    bottom: 0,
                    child: PlaceCtrl()
                )
            ])
        );
    }
}
