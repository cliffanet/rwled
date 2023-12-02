import 'dart:async';
import 'dart:convert';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:path/path.dart';
import 'package:rwdesign/elemfunc/move.dart';
import 'package:rwdesign/player.dart';
import 'dart:io';

import 'type.dart';
import 'draw.dart';

import 'dart:developer' as developer;

int n=0;
class ElemFile {
    String _n;
    String get logs => '[${hashCode.toRadixString(16)}] $_n';
    StreamSubscription<FileSystemEvent>? _watch;
    int get hashCode => _n.hashCode;

    ElemFile(String fname) : this.byFile(File(fname));
    ElemFile.byFile(File f) :
        _n = f.absolute.path
    {
        developer.log('(${n++}) Opening$logs');
        _makewatch(f);
        load(f);
    }
    ElemFile.empty() : _n = '';

    void _makewatch(File f) {
        developer.log('(${n++}) canceled $logs (${_watch?.hashCode.toRadixString(16)})');
        _watch?.cancel();
        
        _watch = f.watch().listen((event) {
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

                    _n = d.absolute.path;
                    break;
                case FileSystemEvent.modify:
                    load(f);
                    break;
            }
            developer.log('(${n++}) ----------------- file: $event');
        });
                    
        developer.log('(${n++}) maked watcher $logs (${_watch.hashCode.toRadixString(16)})');
    }

    void _removeme() {
        final e = _all.lookup(this);
        if (e == null)
            developer.log('(${n++}) my hash not exists $logs');
        else
        if (identical(e!, this)) {
            final r = _all.remove(this);
            developer.log('(${n++}) removed $logs ($r)');
        }
        else {
            developer.log('(${n++}) exists my hash but not me $logs');
        }
    }

    @override
    bool operator ==(other) => 
        (other is ElemFile) &&
        (other._n == this._n);
    
    // параметры
    bool _hidden = false;
    bool get hidden => _hidden;
    String _name = '';
    String get name => _name;
    int _num = 0;
    int get num => _num;

    // данные
    final draw = ElemDraw();
    final move = ElemMove();
    void _clear() {
        _hidden = false;
        _name = '';
        _num = 0;
        draw.clear();
        move.clear();
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
        
        final jdraw = json['draw'];
        if ((jdraw is String) && (jdraw != '')) {
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

        final jmove = json['move'];
        if ((jmove is String) && (jmove != '')) {
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
        
        Player().max = ScenarioMaxLen();

        ScenarioNotify.value++;
        return ok;
    }

    // данные - сброс
    void close() {
        developer.log('(${n++}) canceled on close $logs (${_watch.hashCode.toRadixString(16)})');
        _watch?.cancel();
        _removeme();
        _clear();
        ScenarioNotify.value++;
    }

    // отрисовка
    void paint(Canvas canvas) {
        draw.paint(
            canvas,
            move.val(ParType.x),
            move.val(ParType.y),
            move.val(ParType.r),
        );
    }
}

final Set<ElemFile> _all = {};
StreamSubscription<FileSystemEvent> ?_wdir;

bool _isjson(File f) => extension(f.path).toLowerCase() == '.json';

void OpenScenarioDir() async {
    String? dir = await FilePicker.platform.getDirectoryPath();
    if (dir == null) {
        return;
    }

    _wdir?.cancel();
    _all.forEach((e) { e.close(); });
    _all.clear();

    final d = Directory(dir);
    final List<FileSystemEntity> entities = d.listSync().toList();
    final Iterable<File> files = entities.whereType<File>().where(
        (f) => _isjson(f)
    );
    developer.log('files: $files');

    void open(File f) {
        final o = ElemFile.byFile(f);
        final e = _all.lookup(o);
        if (e != null) {
            e.close();
            _all.remove(e!);
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
        if (event.type == FileSystemEvent.create) {
            final f = File(event.path);
            if (_isjson(f))
                open(f);
        }
        developer.log('(${n++}) ----------------- dir: $event');
    });
    developer.log('(${n++}) -----------------');
}

void ScenarioMove(int tm) {
    _all.where((s) => !s.hidden).forEach((s) => s.move.tm = tm);
}

void ScenarioPaint(Canvas canvas) {
    _all.where((s) => !s.hidden).forEach((s) => s.paint(canvas));
}

int ScenarioMaxLen() {
    int max = 0;
    _all.where((s) => !s.hidden).forEach(
        (s) { if (max < s.move.tmlen) max = s.move.tmlen; }
    );
    return max;
}

ValueNotifier<int> ScenarioNotify = ValueNotifier(0);
