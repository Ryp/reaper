import gdb.printing

class uvec2Printer(object):
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'uvec2({}, {})'.format(self.val['x'], self.val['y'])

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("reaper_pp")
    pp.add_printer('uvec2', '^vec2<.*>$', uvec2Printer)
    return pp

gdb.printing.register_pretty_printer(gdb.current_objfile(),
                                     build_pretty_printer())
