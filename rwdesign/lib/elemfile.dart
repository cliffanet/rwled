import 'dart:async';
import 'dart:convert';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:path/path.dart';
import 'dart:io';

import 'elemtype.dart';
import 'parser/draw.dart';

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

    Future<bool> load(File f) async {
        clear();
        final json = jsonDecode(await f.readAsString());
        if (!(json is Map<String, dynamic>)) return false;
        bool ok = true;
        
        final jdraw = json['draw'];
        if (
                (jdraw != null) && (
                    !(jdraw is List<dynamic>) ||
                    !parseDraw(jdraw, draw)
                )
            )
            ok = false;

        ScenarioNotify.value++;
        return ok;
    }

    void close() {
        developer.log('(${n++}) canceled on close $logs (${_watch.hashCode.toRadixString(16)})');
        _watch?.cancel();
        _removeme();
        clear();
        ScenarioNotify.value++;
    }

    final DrawList draw = [];

    void clear() {
        draw.clear();
        ScenarioNotify.value++;
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

List<ElemFile> Scenario() {
    return _all.toList();
}

ValueNotifier<int> ScenarioNotify = ValueNotifier(0);
