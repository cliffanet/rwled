

import 'dart:async';
import 'package:flutter/material.dart';

class PlayerElement {
    final String title;
    PlayerElement (this.title);

    Offset pos = Offset(0,0);
    void move(Offset p) {
        pos = p;
    }

    bool mypnt(Offset p) {
        final d = pos-p;
        return (d.dx.abs() < 10) && (d.dy.abs() < 10);
    }

    void paint(Canvas canvas, int tm) {

    }

    static double convertRadiusToSigma(double radius) {
        return radius * 0.57735 + 0.5;
    }
}

class Player {
    static final Player _p = Player._make();
    factory Player() { return _p; }
    Player._make();

    int _tm = 0;
    int _max = 60000;

    final ValueNotifier<int> notify = ValueNotifier(0);
    int get tm => _tm;
    set tm(int v) {
        _tm = v;
        notify.value++;
    }
    int get max => _max;

    get timestr {
        int t = _tm;
        int m = (t / 60000).floor();
        t -= m*60000;
        int s = (t / 1000).floor();
        t -= s*1000;
        int mm = (t / 100).floor();
        return '${m.toString().padLeft(2,'0')}:${s.toString().padLeft(2,'0')}.$mm';
    }

    final _elem = <PlayerElement>[];
    void add(PlayerElement el) {
        el.move(Offset(viewsz.width/2, viewsz.height/2));
        _elem.add(el);
        notify.value++;
        select(_elem.length-1);
    }
    void del(int index) {
        if ((index < 0) || (index >= _elem.length))
            return;
        _elem.removeAt(index);
        notify.value++;
        if (_sel >= _elem.length)
            select(_elem.length-1);
    }
    void delSelected() {
        if ((_sel >= 0) && (_sel < _elem.length))
            del(_sel);
    }
    int get cnt => _elem.length;
    PlayerElement elem(int index) => _elem[index];
    int elByPnt(Offset p) {
        for (int i = _elem.length-1; i >=0; i--)
            if (_elem[i].mypnt(p))
                return i;
        return -1;
    }

    int _sel = -1;
    final ValueNotifier<int> notifySelected = ValueNotifier(0);
    PlayerElement? get selected => (_sel >= 0) && (_sel < _elem.length) ? _elem[_sel] : null;
    void select(int index) {
        if (index < 0) {
            _sel = -1;
            notifySelected.value ++;
        }
        else
        if (index < _elem.length) {
            _sel = index;
            notifySelected.value ++;
        }
    }
    void selectByPnt(Offset p) {
        int i = elByPnt(p);
        if (i>=0) select(i);
    }

    Size viewsz = Size(0,0);
    void paint(Canvas canvas, Size size) {
        viewsz = size;
        _elem.forEach((el) { el.paint(canvas, _tm); });
    }

    Timer ?_play;
    bool get isplay => _play != null;
    void _pnxt() {
        _play = Timer(
            Duration(milliseconds: 97),
            () {
                _tm += 100;
                if (_tm > _max)
                    _tm = _max;
                if (_tm < _max)
                    _pnxt();
                else
                    stop();
                notify.value++;
            }
        );
    }
    void play() {
        if (isplay) return;
        if (_tm >= _max)
            _tm = 0;
        _pnxt();
        notify.value++;
    }
    void stop() {
        if (!isplay) return;
        _play!.cancel();
        _play = null;
        notify.value++;
    }
    void playtgl() {
        if (isplay)
            stop();
        else
            play();
    }

    PlayerElement? _moved;
    Offset? _movec;
    void onPointDown(PointerDownEvent e) {
        int i = elByPnt(e.localPosition);
        if (i<0) return;
        select(i);
        _moved = selected;
        if (_moved == null) return;
        _movec = _moved!.pos-e.localPosition;
        //print("down: $_movec / ${_moved!.pos}");
    }
    void onPointMove(PointerMoveEvent e) {
        if ((_moved == null) || (_movec == null)) return;
        _moved!.pos = _movec! + e.localPosition;
        //print("move: ${_moved!.pos}");
        notify.value++;
    }
    void onPointUp(PointerUpEvent e) {
        _moved=null;
        _movec=null;
    }
}