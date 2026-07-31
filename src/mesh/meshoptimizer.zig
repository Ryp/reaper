// The one and only meshoptimizer @cImport.
//
// meshoptimizer stays C++ during coexistence and is compiled from
// external/meshoptimizer by build.zig, so both builds get identical meshlets.

pub const c = @cImport({
    @cInclude("meshoptimizer.h");
});
