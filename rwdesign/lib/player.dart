

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/file.dart';

enum LightMode {
    Hidden, Figure, Layer, Led
}

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
        _tm = v > 0 ? v : 0;
        ScenarioMove(_tm);
        notify.value++;
    }
    int get max => _max;
    set max(int v) {
        _max = v > 0 ? v : 0;
        if (_tm > _max)
            tm = _max;
        else
            notify.value++;
    }

    get timestr {
        int t = _tm;
        int m = (t / 60000).floor();
        t -= m*60000;
        int s = (t / 1000).floor();
        t -= s*1000;
        int mm = (t / 100).floor();
        return '${m.toString().padLeft(2,'0')}:${s.toString().padLeft(2,'0')}.$mm';
    }

    Timer ?_play;
    bool get isplay => _play != null;
    void _pnxt() {
        _play = Timer(
            Duration(milliseconds: 8),
            () {
                if (_tm >= _max) {
                    final loop = ScenarioLoop();
                    if ((loop >= 0) && (loop < _max))
                        _tm = loop;
                    else {
                        stop();
                        return;
                    }
                }
                else {
                    _tm += 10;
                    if (_tm > _max)
                        _tm = _max;
                }
                _pnxt();
                ScenarioMove(_tm);
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

    LightMode _lmode = LightMode.Figure;
    LightMode get lmode => _lmode;
    void lmodetgl() {
        switch (_lmode) {
            case LightMode.Hidden:
                _lmode = LightMode.Figure;
                break;
            case LightMode.Figure:
                _lmode = LightMode.Layer;
                break;
            case LightMode.Layer:
                _lmode = LightMode.Led;
                break;
            case LightMode.Led:
            default:
                _lmode = LightMode.Hidden;
        }
        notify.value++;
    }
}
