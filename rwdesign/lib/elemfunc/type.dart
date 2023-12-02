

import 'dart:ui';

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
