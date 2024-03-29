import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:path/path.dart';
import 'package:rwdesign/light/light.dart';

import 'draw.dart';
import 'led.dart';
import 'move.dart';
import '../player.dart';

import 'dart:developer' as developer;

int n=0;
class ElemFile {
    String _fpath;
    String get logs => '[${hashCode.toRadixString(16)}] $_fpath';
    StreamSubscription<FileSystemEvent>? _watch;
    int get hashCode => _fpath.hashCode;

    ElemFile._byFile(File f) :
        _fpath = f.absolute.path
    {
        developer.log('(${n++}) Opening$logs');
        _makewatch(f);
    }
    static Future<ElemFile> byFile(File f) async {
        final e = ElemFile._byFile(f);
        await e.load(f);
        return e;
    }
    ElemFile.empty() : _fpath = '';

    void _makewatch(File f) {
        if (_watch != null) {
            developer.log('(${n++}) canceled $logs (${_watch!.hashCode.toRadixString(16)})');
            _watch!.cancel();
        }
        
        _watch = f.watch().listen((event) {
            developer.log('(${n++}) ----------------- file: $event');
            switch (event.type) {
                case FileSystemEvent.delete:
                    developer.log('(${n++}) delete-event $logs');
                    close();
                    break;
                case FileSystemEvent.move:
                    final e = event as FileSystemMoveEvent;
                    developer.log('(${n++}) move-event $logs => ${e.destination}');

                    final d = File(e.destination ?? '');
                    if (!_isjson(d))
                        close();

                    _fpath = d.absolute.path;
                    break;
                case FileSystemEvent.modify:
                    load(f);
                    break;
            }
        });
                    
        developer.log('(${n++}) maked watcher $logs (${_watch.hashCode.toRadixString(16)})');
    }

    void _removeme() {
        final e = _all.lookup(this);
        if (e == null)
            developer.log('(${n++}) my hash not exists $logs');
        else
        if (identical(e, this)) {
            final r = _all.remove(this);
            developer.log('(${n++}) removed $logs ($r)');
        }
        else {
            developer.log('(${n++}) exists my hash but not me $logs');
        }
        _clearunam();
    }

    @override
    bool operator ==(other) => 
        (other is ElemFile) &&
        (other._fpath == this._fpath);
    
    // параметры
    bool _hidden = false;
    bool get hidden => _hidden;
    String _name = '';
    String get name => _name;
    int _num = 0;
    int get num => _num;

    int _tm = 0;
    set tm(int tm) {
        _tm = tm;
        move.tm = tm;
    }

    // зависимость от других файлов
    final unam = Set<String>();
    void _clearunam() {
        _unam.forEach((name, list) {
            list.removeWhere((e) {
                if (!identical(e, this))
                    return false;
                developer.log('(${n++}) removed $logs from unam: $name');
                return true;
            });
            if (list.isEmpty)
                developer.log('(${n++}) cleared unam: $name');
        });
        _unam.removeWhere((_, list) => list.isEmpty);
        // чистим локальный список
        unam.clear();
    }
    void _addunam(String name) {
        if (unam.contains(name)) return;
        unam.add(name);
        if (!_unam.containsKey(name))
            _unam[name] = [];
        _unam[name]!.add(this);
        developer.log('(${n++}) add $logs to unam: $name');
    }

    // данные
    int  _loop = -1;
    int get loop => _loop;
    final draw = ElemDraw();
    final leds = ElemLed();
    final move = ElemMove();
    final lght = Light();
    void _clear() {
        _hidden = false;
        _name = '';
        _num = 0;
        _loop = -1;
        _clearunam();
        draw.clear();
        leds.clear();
        move.clear();
        lght.clear();
    }

    // данные - загрузка
    Future<bool> load(File f) async {
        _clear();
        final json = jsonDecode(await f.readAsString());
        if (!(json is Map<String, dynamic>)) return false;
        bool ok = true;

        if (json['hidden'] is bool)
            _hidden = json['hidden'];
        if (json['name'] is String)
            _name = json['name'];
        if (json['num'] is int) {
            _num = json['num'];
            draw.num = _num;
        }

        if (json['loop'] is int)
            _loop = json['loop'];
        
        final jdraw = json['draw'];
        if ((jdraw is String) && (jdraw != '')) {
            _addunam(jdraw);
            final f = _all.firstWhere((f) => f.name == jdraw, orElse: () => ElemFile.empty());
            if (f.name == jdraw)
                draw.clone(f.draw);
            else
                ok = false;
        }
        else
        if (
                (jdraw != null) && (
                    !(jdraw is List<dynamic>) ||
                    !draw.load(jdraw)
                )
            )
            ok = false;
        
        final jleds = json['leds'];
        if ((jleds is String) && (jleds != '')) {
            _addunam(jleds);
            final f = _all.firstWhere((f) => f.name == jleds, orElse: () => ElemFile.empty());
            if (f.name == jleds)
                leds.clone(f.leds);
            else
                ok = false;
        }
        else
        if (
                (jleds != null) && (
                    !(jleds is List<dynamic>) ||
                    !leds.load(jleds)
                )
            )
            ok = false;

        final jmove = json['move'];
        if ((jmove is String) && (jmove != '')) {
            _addunam(jmove);
            final f = _all.firstWhere((f) => f.name == jmove, orElse: () => ElemFile.empty());
            if (f.name == jmove)
                move.clone(f.move);
            else
                ok = false;
        }
        else
        if (
                (jmove != null) && (
                    !(jmove is List<dynamic>) ||
                    !move.load(jmove)
                )
            )
            ok = false;

        final jlght = json['light'];
        if (
                (jlght != null) && (
                    !(jlght is List<dynamic>) ||
                    !lght.load(jlght)
                )
            )
            ok = false;
        
        Player().max = ScenarioMaxLen();

        developer.log('(${n++}) loaded $logs');

        if (name.isNotEmpty) {
            final list = (_unam[name] ?? []).where((e) => !identical(e, this)).toList();
            list.forEach((e) => e.reload());
        }

        ScenarioNotify.value++;
        return ok;
    }

    // данные - сброс
    void close([bool noremove = false]) {
        developer.log('(${n++}) canceled on close $logs (${_watch.hashCode.toRadixString(16)})');
        _watch?.cancel();
        if (!noremove) _removeme();
        _clear();
        ScenarioNotify.value++;
    }

    Future<bool> reload() async {
        //close();
        if (_fpath.isEmpty) return false;
        final f = File(_fpath);
        developer.log('(${n++}) reloading $logs');
        return await load(f);
    }

    // отрисовка
    void paint(Canvas canvas) {
        final x = move.val(ParType.x);
        final y = move.val(ParType.y);
        final r = move.val(ParType.r);
        final m = Player().lmode;
        draw.paint(canvas, x, y, r);
        if (m == LightMode.Led)
            leds.paint(canvas, x, y, r, true);
        else
        if (m != LightMode.Hidden)
            leds.paint(canvas, x, y, r);
    }
}

final Set<ElemFile> _all = {};
final Map<String,List<ElemFile>> _unam = {};
StreamSubscription<FileSystemEvent> ?_wdir;

bool _isjson(File f) => extension(f.path).toLowerCase() == '.json';

void OpenScenarioDir() async {
    String? dir = await FilePicker.platform.getDirectoryPath();
    if (dir == null) {
        return;
    }

    _wdir?.cancel();
    _all.forEach((e) { e.close(true); });
    _all.clear();
    _unam.clear();

    final d = Directory(dir);
    final List<FileSystemEntity> entities = d.listSync().toList()
        ..sort((a, b) => a.path.compareTo(b.path));
    final Iterable<File> files = entities.whereType<File>().where(
        (f) => _isjson(f)
    );
    developer.log('files: $files');

    void open(File f) async {
        final o = await ElemFile.byFile(f);
        if (!o.hidden)
            o.tm = Player().tm;
        // добавляем в _all
        final e = _all.lookup(o);
        if (e != null) {
            e.close();
            _all.remove(e);
            developer.log('(${n++}) removed other my by open ${o.logs}');
        }
        else 
            developer.log('(${n++}) other opened not founded ${o.logs}');
        _all.add(o);
        developer.log('(${n++}) added ${o.logs}');
    }
    
    for (final f in files)
        open(f);

    _wdir = File(dir).watch().listen((event) {
        developer.log('(${n++}) ----------------- dir: $event');
        if (event.type == FileSystemEvent.create) {
            final f = File(event.path);
            if (_isjson(f))
                open(f);
        }
    });
    developer.log('(${n++}) -----------------');
}

void ScenarioMove(int tm) {
    _all.where((s) => !s.hidden).forEach((s) => s.tm = tm);
}

int ScenarioMaxLen() {
    int max = 0;
    _all.where((s) => !s.hidden).forEach(
        (s) { if (max < s.move.tmlen) max = s.move.tmlen; }
    );
    return max;
}

int ScenarioLoop() {
    int l = -1;
    _all.where((s) => !s.hidden).forEach(
        (s) { if (l < s.loop) l = s.loop; }
    );
    return l;
}

Color? ScenarioLght(double x, double y, int tm) {
    for (final s in _all.toList().reversed) {
        final col = s.lght.color(x, y, tm);
        if (col != null)
            return col;
    }
    return null;
}

class PersonLed {
    final int num;
    final ElemMove  move;
    final ElemLed   leds;
    PersonLed(this.num, this.move, this.leds);
}
List<PersonLed> ScenarioLed() {
    return _all
        .where((s) => !s.hidden && s.leds.isNotEmpty)
        .map((d) => PersonLed(d.num, d.move.copy(), d.leds))
        .toList();
}

ValueNotifier<int> ScenarioNotify = ValueNotifier(0);


double _convertRadiusToSigma(double radius) {
    return radius * 0.57735 + 0.5;
}
class ScenarioPainter extends CustomPainter {
    ScenarioPainter();

    @override
    void paint(Canvas canvas, Size size) {
        if (Player().lmode == LightMode.Figure)
            _all
                .where((s) => !s.hidden)
                .forEach((s) => s.lght.paint(canvas, Player().tm));
        
        _all
            .where((s) => !s.hidden)
            .forEach((s) => s.paint(canvas));
        
        if (Player().lmode == LightMode.Layer) {
            canvas.save();
            final lghtall = _all.toList().reversed;
            final tm = Player().tm;
            final p = Paint()
                ..style = PaintingStyle.fill
                ..maskFilter = MaskFilter.blur(BlurStyle.normal, _convertRadiusToSigma(20));

            for (double x = 0; x < size.width; x += 10)
                for (double y = 0; y < size.height; y += 10)
                    for (final s in lghtall) {
                        final col = s.lght.color(x, y, tm);
                        if (col == null)
                            continue;
                        p.color = col;
                        canvas.drawCircle(Offset(x, y), 5, p);
                        break;
                    }
            
            canvas.restore();
        }
    }

    @override
    bool shouldRepaint(ScenarioPainter oldDelegate) => true;
}
