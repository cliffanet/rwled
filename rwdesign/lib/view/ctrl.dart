import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:rwdesign/player.dart';

int n = 0;

Widget PlaceCtrl() {
    final p = Player();

    return ValueListenableBuilder(
        valueListenable: p.notify,
        builder: (context, value, widget) {
            return Row(children: [
                SizedBox(
                    width: 115,
                    child: Row(children: [
                        FloatingActionButton(
                            heroTag: 'play',
                            onPressed: p.playtgl,
                            child: Icon(p.isplay ? Icons.stop : Icons.play_arrow),
                        ),
                        Expanded(
                            child: Text(
                                p.timestr,
                                textAlign: TextAlign.right,
                                style: TextStyle(fontFeatures: [FontFeature.tabularFigures()])
                            )
                        )
                    ])
                ),
                Expanded(
                    child: Slider(
                        value: p.tm.toDouble(),
                        //divisions: 10,
                        max: p.max.toDouble(),
                        onChanged: (double value) {
                            p.tm = (value / 100).round() * 100;
                        },
                        onChangeStart: (v) => p.stop(),
                    ),
                ),
                FloatingActionButton(
                    heroTag: 'lmode',
                    onPressed: p.lmodetgl,
                    child: Icon(
                        p.lmode == LightMode.Hidden ?
                            Icons.hide_image :
                        p.lmode == LightMode.Figure ?
                            Icons.pentagon :
                        p.lmode == LightMode.Layer ?
                            Icons.layers :
                        p.lmode == LightMode.Led ?
                            Icons.light_mode :
                            Icons.device_unknown
                    ),
                    tooltip: p.lmode.toString()
                ),
            ]);
        }
    );
}
