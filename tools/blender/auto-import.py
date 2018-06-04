#!/usr/bin/env python

import sys

import bpy

def import_obj(file_loc):
    # Import OBJ file
    imported_object = bpy.ops.import_scene.obj(filepath=file_loc)

    # Scale for better usability
    uniform_scale = 0.06
    obj_object = bpy.context.selected_objects[0]
    obj_object.scale = (uniform_scale, uniform_scale, uniform_scale)

def main(args):
    # Hide splash screen
    bpy.context.user_preferences.view.show_splash = False

    # Create empty scene
    bpy.ops.scene.new(type='EMPTY')

    # Import new object into it
    for file_to_import in args:
        import_obj(file_to_import)

if __name__ == '__main__':
    try:
        # Detect command-line arguments
        args_index = sys.argv.index('--') + 1
        main(sys.argv[args_index:])
    except ValueError:
        # Or pass none if not found
        main([])
