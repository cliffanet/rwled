import 'dart:developer';

import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/file.dart';
import 'package:rwdesign/player.dart';
import 'package:rwdesign/view/ctrl.dart';
import 'package:rwdesign/painter.dart';

void main() {
    runApp(const MaterialApp(
        title: 'RW-led Design Studio',
        home: DesignApp(),
        debugShowCheckedModeBanner: false
    ));
}

enum MenuCode { Open }

class DesignApp extends StatelessWidget {
    const DesignApp({super.key});

    // This widget is the root of your application.
    @override
    Widget build(BuildContext context) {
        return Material(
            child: Stack(children: [
                ValueListenableBuilder(
                    valueListenable: ScenarioNotify,
                    builder: (context, value, widget) {
                        return CustomPaint(
                            painter: PlacePainter(),
                            size: Size.infinite,
                        );
                    }
                ),
                Positioned(
                    right: 0,
                    top: 0,
                    child: PopupMenuButton<MenuCode>(
                        tooltip: "Меню",
                        onSelected: (MenuCode item) {
                            switch (item) {
                                case MenuCode.Open:
                                    OpenScenarioDir();
                                    break;
                            };
                        },
                        itemBuilder: (context) {
                            return [
                                PopupMenuItem(
                                    value: MenuCode.Open,
                                    child: Row(
                                        children: const [
                                            Icon(Icons.folder, color: Colors.black),
                                            SizedBox(width: 8),
                                            Text('Открыть сценарий'),
                                        ]
                                    ),
                                ),
                            ];
                        }
                    )
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
