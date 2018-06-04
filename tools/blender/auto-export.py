#!/usr/bin/env python

import bpy

def main():
    # access the scene
    scene = bpy.context.scene

    # start with empty selection
    bpy.ops.object.select_all(action='DESELECT')

    # loop through all objects in the scene
    for ob in scene.objects:
        # select this object
        scene.objects.active = ob
        ob.select = True

        # if this object is a mesh
        if ob.type == 'MESH':
            bpy.ops.export_scene.obj(filepath=ob.name + '.obj', use_selection=True)

        # deselect this object again
        ob.select = False

if __name__ == '__main__':
    main()
