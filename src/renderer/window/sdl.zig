// The one and only SDL3 @cImport.
//
// Every other file goes through this module so that all SDL types are the same
// Zig types (@cImport results are only identical when the include set matches).

pub const c = @cImport({
    @cInclude("SDL3/SDL.h");
    @cInclude("SDL3/SDL_vulkan.h");
});
