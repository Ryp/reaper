// The one and only lodepng @cImport.
//
// lodepng stays C++ during coexistence and is compiled from external/lodepng by
// build.zig, so both builds decode PNGs identically.

pub const c = @cImport({
    @cInclude("lodepng.h");
});
