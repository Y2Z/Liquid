#include <Cocoa/Cocoa.h>

void SetWindowBackgroundColor(long winId, float red, float green, float blue, float alpha)
{
    NSView* view = (NSView*)winId;
    NSWindow* window = [view window];

    // window.titlebarAppearsTransparent = NO;
    // window.hasShadow = NO;
    // [window setAlphaValue:1.0];
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor colorWithCalibratedRed:red green:green blue:blue alpha:alpha]];
}
