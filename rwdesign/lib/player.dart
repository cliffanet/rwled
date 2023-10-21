

import 'dart:async';
import 'dart:ffi';
import 'package:flutter/material.dart';

class PlayerElement {
    final String title;
    PlayerElement (this.title);

    int _tm = 0;
    int get tm => _tm;
    set tm(int v) => _tm = v;
}

class Player {
    static final Player _p = Player._make();
    factory Player() { return _p; }
    Player._make();

    final ValueNotifier<int> _notify = ValueNotifier(0);
    int _tm = 0;
    int _max = 60000;

    ValueNotifier<int> get notify => _notify;
    int get tm => _tm;
    set tm(int v) {
        _tm = v;
        _notify.value++;
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
        _elem.add(el);
        _notify.value++;
    }
    void del(int index) {
        if ((index < 0) || (index >= _elem.length))
            return;
        _elem.removeAt(index);
        _notify.value++;
    }
    int get cnt => _elem.length;
    PlayerElement elem(int index) => _elem[index];

    Timer ?_play;
    bool get isplay => _play != null;
    void _pnxt() {
        _play = Timer(
            Duration(milliseconds: 100),
            () {
                _tm += 100;
                if (_tm > _max)
                    _tm = _max;
                if (_tm < _max)
                    _pnxt();
                else
                    stop();
                _notify.value++;
            }
        );
    }
    void play() {
        if (isplay) return;
        if (_tm >= _max)
            _tm = 0;
        _pnxt();
        _notify.value++;
    }
    void stop() {
        if (!isplay) return;
        _play!.cancel();
        _play = null;
        _notify.value++;
    }
    void playtgl() {
        if (isplay)
            stop();
        else
            play();
    }
}