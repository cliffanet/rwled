import 'package:confirm_dialog/confirm_dialog.dart';
import 'package:flutter/material.dart';
import 'package:rwdesign/player.dart';

enum MenuCode { Add, Remove }

int n = 0;

PreferredSizeWidget PlaceTitle() {
    final p = Player();

    return AppBar(
        title: ValueListenableBuilder(
            valueListenable: p.notifySelected,
            builder: (_, __, ___) {
                if (p.selected == null)
                    return Text('<пусто>');
                return Text(p.selected!.title);
            }
        ),
        actions: [
            ValueListenableBuilder(
                valueListenable: p.notifySelected,
                builder: (context, __, ___) {
                    final del = p.selected != null ?
                        [
                            PopupMenuDivider(),
                            PopupMenuItem(
                                value: MenuCode.Remove,
                                child: Row(
                                    children: const [
                                        Icon(Icons.remove, color: Colors.black),
                                        SizedBox(width: 8),
                                        Text('Удалить'),
                                    ]
                                ),
                            ),
                        ] : [];
                    
                    return PopupMenuButton<MenuCode>(
                        tooltip: "Меню",
                        onSelected: (MenuCode item) async {
                            switch (item) {
                                case MenuCode.Add:
                                    p.add(PlayerElement('Elem ${n++}'));
                                    break;
                                case MenuCode.Remove:
                                    if (p.selected == null)
                                        break;
                                    if (await confirm(context, content: Text('Удаление: ${p.selected!.title}')))
                                        p.delSelected();
                                    break;
                            };
                        },
                        itemBuilder: (context) {
                            return [
                                PopupMenuItem(
                                    value: MenuCode.Add,
                                    child: Row(
                                        children: const [
                                            Icon(Icons.add, color: Colors.black),
                                            SizedBox(width: 8),
                                            Text('Добавить'),
                                        ]
                                    ),
                                ),
                                ...del
                            ];
                        }
                    );
                }
            )
        ]
    );
}