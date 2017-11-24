#!/usr/bin/env python3

import sys
import os
import clang.cindex

from clang.cindex import Cursor, CursorKind, TranslationUnit

def find_child(parent: Cursor, kind: CursorKind, spelling: str):
    for node in parent.get_children():
        if kind == node.kind and spelling == node.spelling:
            return node

def debug_cursor(node: Cursor):
    print(node.kind, node.spelling, node.displayname)

def print_all_of_kind(parent: Cursor, kind: CursorKind):
    for node in parent.get_children():
        #if kind == node.kind:
        debug_cursor(node)
        print_all_of_kind(node, kind)

def get_main(translation_unit: TranslationUnit) -> Cursor:
    return find_child(translation_unit.cursor, CursorKind.FUNCTION_DECL, 'main')

def dump(cursor: Cursor):
    #line_count = cursor.get_tokens[0].location.line
    line_count = 1
    for t in cursor.get_tokens():
        current_line = t.location.line
        if current_line > line_count:
            for _ in range(current_line - line_count):
                print('')
            line_count = current_line
        print(t.spelling, end=' ')
    print('')

def get_translation_unit(compilation_database_dir: str, cpp_path: str) -> TranslationUnit:
    compilation_database = clang.cindex.CompilationDatabase.fromDirectory(compilation_database_dir)
    compilation_commands = compilation_database.getCompileCommands(cpp_path)

    if compilation_commands:
        compilation_arguments = list(compilation_commands[0].arguments)
        hacked_arguments = compilation_arguments[:-4] # BIGASS HACK
    else:
        hacked_arguments = []

    index = clang.cindex.Index.create()
    translation_unit = index.parse(cpp_path, hacked_arguments)
#    for error in translation_unit.diagnostics:
#        print(error)
    return translation_unit

def test(args):
    database_dir = os.path.realpath(args[0])
    target = os.path.realpath(args[1])

    print(database_dir, target)

    translation_unit = get_translation_unit(database_dir, target)
    #node = get_main(translation_unit)
    #print(node.spelling)

    print_all_of_kind(translation_unit.cursor, CursorKind.CLASS_DECL)

    #dump(translation_unit.cursor)

if __name__ == "__main__":
    test(sys.argv[1:])
