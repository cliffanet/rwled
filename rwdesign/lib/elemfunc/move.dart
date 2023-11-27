
import 'package:rwdesign/elemfunc/type.dart';

enum ParType { x, y, r }

class ElemPos {
    ElemPos(this.tm, this.p);
    final int tm;
    final Map<ParType, double> p;
}

typedef MoveList = List<ElemPos>;

class ParBeg {
    ParBeg(this.i, this.tm, this.val);
    ParBeg.zero() : i=0, tm=0, val=0;
    final int i;
    final int tm;
    final double val;
}

class ParEnd extends ParBeg {
    ParEnd(super.i, super.tm, super.val);
    ParEnd.byBeg(ParBeg beg) : super(beg.i, beg.tm, beg.val) {}
}

class ElemMove {
    final  Map<ParType, ParBeg> _beg = {};
    final  Map<ParType, ParEnd> _end = {};
    final  Map<ParType, double> _val = {};
    double val(ParType t) => _val[t] ?? 0;

    int _tm = 0, _tmlen = 0;
    int get tm => _tm;
    int get tmlen => _tmlen;
    set tm(int tm) {
        _tm = tm < _tmlen ? tm : _tmlen;

        // Для каждого параметра удалим текущий интервал,
        // tcли мы вышли за его пределы
        ParType.values.forEach((t) {
            var b = _beg[t];
            var e = _end[t];

            if ((b == null) || (b.tm > _tm) ||
                (e == null) || (e.tm <= _tm)) {
                // есть необходимость заного найти текущий диапазон
                _beg.remove(t);
                _end.remove(t);

                // начало поиска будет: либо с end-позиции, либо с самого начала
                int i =
                    (e != null) && (e.tm <= _tm) &&
                    (e.i >= 0) && (e.i < _data.length) ? e.i : 0;
                for (; i < _data.length; i++) {
                    final d = _data[i];
                    if (!d.p.containsKey(t))
                        continue;
                    if (d.tm <= _tm)
                        _beg[t] = ParBeg(i, d.tm, d.p[t]!);
                    else {
                        _end[t] = ParEnd(i, d.tm, d.p[t]!);
                        break;
                    }
                }
                b = _beg[t];
                e = _end[t];
                if (b == null) {
                    b = ParBeg(-1, 0, e != null ? e.val : 0);
                    _beg[t] = b;
                }
                if (e == null) {
                    e = ParEnd(-1, _tmlen+1, b.val);
                    _end[t] = e;
                }
            }

            // вычисляем текущее значение, исходя из диапазона
            _val[t] = e.tm > b.tm ?
                (e.val - b.val) *
                (_tm-b.tm) / (e.tm-b.tm) 
                + b.val :
                b.val;
        });
    }

    final MoveList _data = [];
    void clear() {
        _data.clear();
        _beg.clear();
        _end.clear();
        _val.clear();
    }

    bool load(dynamic s) {
        if (!(s is List<dynamic>)) return false;
        clear();

        bool ok = true;
        int tmc = 0;
        Map<ParType, double> lst = {};
        s.forEach((d) {
            if (!(d is HashItem)) {
                ok = false;
                return;
            }

            final tm1 = jint(d, 'tm');
            if (tm1 != null)
                tmc += tm1;

            final Map<ParType, double> p = {};
            ParType.values.forEach((t) {
                if (
                        (d.containsKey('all') && (d['all'] == null)) ||
                        (d.containsKey(t.name)&& (d[t.name] == null))
                    ) {
                    final v = lst[t];
                    if (v != null) 
                        p[t] = v;
                }
                else {
                    final v = jdouble(d, t.name);
                    if (v == null) return;
                    p[t] = v;
                    lst[t] = v;
                }
            });
            if (p.isNotEmpty)
                _data.add(ElemPos(tmc, p));

        });
        _tmlen = tmc;
        tm = tm;

        return ok;
    }
}
