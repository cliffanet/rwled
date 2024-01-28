

import 'dart:async';
import 'dart:io';
import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:rwdesign/elemfunc/file.dart';
import 'package:rwdesign/elemfunc/move.dart';

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
    void play() {
        if (isplay) return;
        if (_tm >= _max)
            _tm = 0;
        int beg = DateTime.now().millisecondsSinceEpoch;
        
        _play = Timer.periodic(
            Duration(milliseconds: 10),
            (_) {
                _tm = DateTime.now().millisecondsSinceEpoch - beg;
                while (_tm >= _max) {
                    final loop = ScenarioLoop();
                    if ((loop >= 0) && (loop < _max)) {
                        beg += _max-loop;
                        _tm = DateTime.now().millisecondsSinceEpoch - beg;
                    } else {
                        _tm = _max;
                        stop();
                    }
                }
                ScenarioMove(_tm);
                notify.value++;
            }
        );

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

Future<bool> PlayerSave() async {
    String? dir = await FilePicker.platform.getDirectoryPath();
    if (dir == null)
        return false;

    for (final s in ScenarioLed()) {
        final f = File(dir + '/p' + s.num.toString().padLeft(3,'0') + '.led');
        final txt = await f.openWrite();

        txt.writeln("LEDS=" + s.num.toString().padLeft(3,'0'));

        Map<int,List<int>> val = {};

        for (int tm = 0; tm <= s.move.tmlen; tm += 40) {
            s.move.tm = tm;
            final chanall = s.leds.info(tm,
                s.move.val(ParType.x),
                s.move.val(ParType.y),
                s.move.val(ParType.r)
            );
            bool tmprn = false;
            
            for (final ch in chanall) {
                if (!val.containsKey(ch.chan))
                    val[ch.chan] = [];
                final v = val[ch.chan] ?? [];
                bool chprn = false;

                for (final l in ch.list) {
                    while (v.length <= l.num) v.add(0);
                    if (v[l.num] == l.col.value) continue;
                    v[l.num] = l.col.value;

                    if (!tmprn) {
                        txt.writeln("tm=${tm}");
                        tmprn = true;
                    }
                    if (!chprn) {
                        txt.writeln("ch=${ch.chan}");
                        chprn = true;
                    }
                    txt.writeln("led=${l.num},${l.col.value.toRadixString(16).padLeft(8,'0')}");
                }
            }
        }

        final max = Player().max;
        final loop = ScenarioLoop();
        if ((loop >= 0) && (loop < max))
            txt.writeln("LOOP=$loop,$max");

        txt.writeln("END.");

        await txt.close();
    }

    return true;
}
