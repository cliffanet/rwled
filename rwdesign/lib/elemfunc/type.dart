

import 'dart:ui';

import 'package:flutter/material.dart';

typedef HashItem = Map<String, dynamic>;
typedef HashList = List<HashItem>;

int? jint(dynamic s, String key) {
    if (!(s is HashItem)) return null;
    final v = s[key];
    if (v is int)
        return v;
    return null;
}

double? jdouble(dynamic s, String key) {
    if (!(s is HashItem)) return null;
    final v = s[key];
    if (v is int)
        return v.toDouble();
    if (v is double)
        return v;
    return null;
}

bool? jbool(dynamic s, String key) {
    if (!(s is HashItem)) return null;
    final v = s[key];
    return v is bool ? v : null;
}

Offset? jxy(dynamic s, [String? prefix]) {
    if (!(s is HashItem)) return null;
    final p = prefix ?? '';
    final x = jdouble(s, p+'x');
    final y = jdouble(s, p+'y');
    if ((x == null) && (y == null))
        return null;
    return Offset(x ?? 0, y ?? 0);
}

Color? jcolor(dynamic s, [String key = 'color']) {
    if (!(s is HashItem)) return null;
    var v = s[key];
    if (!(v is String)) return null;
    while (v.length < 6) v = '0' + v;
    while (v.length < 8) v = 'f' + v;
    return Color(int.parse(v, radix: 16));
}

Color coldiff(Color beg, Color end, double k) {
    return Color.fromRGBO(
        ((end.red   - beg.red)      * k + beg.red   ).round(),
        ((end.green - beg.green)    * k + beg.green ).round(),
        ((end.blue  - beg.blue)     * k + beg.blue  ).round(),
        (end.opacity- beg.opacity)  * k + beg.opacity
    );
}
