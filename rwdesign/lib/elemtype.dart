

typedef HashItem = Map<String, dynamic>;
typedef HashList = List<HashItem>;

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
