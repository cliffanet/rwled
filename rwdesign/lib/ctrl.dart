import 'package:flutter/material.dart';

Widget PlaceCtrl() {
    return Row(children: [
        FloatingActionButton(
            heroTag: 'play',
            onPressed: () {
            },
            child: Icon(Icons.play_arrow)//Icon(_play == null ? Icons.play_arrow : Icons.stop),
        ),
        Expanded(
            child: Slider(
                value: 10,
                max: 100,
                onChanged: (double value) {
                },
            ),
        ),
    ]);
}