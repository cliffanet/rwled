import 'dart:ui';
import 'package:flutter/material.dart';
import 'package:rwdesign/player.dart';

int n = 0;

Widget PlaceCtrl() {
    final p = Player();

    return ValueListenableBuilder(
        valueListenable: p.notify,
        builder: (context, value, widget) {
            final row = <Widget>[];

            row.add(
                Row(children: [
                    FloatingActionButton(
                        heroTag: 'addelem',
                        onPressed: () => p.add(PlayerElement('Elem ${n++}')),
                        child: Icon(Icons.add),
                    ),
                ])
            );

            for (int i = p.cnt-1; i >= 0; i--)
                row.add(
                    Row(children: [
                        SizedBox(
                            width: 110,
                            child: Row(children: [
                                FloatingActionButton.small(
                                    heroTag: 'elem-${i}',
                                    onPressed: () => p.del(i),
                                    child: Icon(Icons.remove),
                                ),
                                Text(p.elem(i).title)
                            ])
                        )
                    ])
                );
                
            return Column(children: [
                ...row,
                Row(children: [
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
                                p.tm = value.floor();
                            },
                            onChangeStart: (v) => p.stop(),
                        ),
                    ),
                ])
            ]);
        }
    );
}
