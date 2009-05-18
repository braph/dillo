#include <fltk/Window.h>
#include <fltk/x11.h>
#include <fltk/x.h>

#include "xembed.hh"

using namespace fltk;
// TODO; Implement proper XEMBED support;
// http://standards.freedesktop.org/xembed-spec/xembed-spec-latest.html
void Xembed::embed (unsigned long xid) {
#if USE_X11
   fltk::Widget *r = resizable();
   // WORKAROUND: Avoid jumping windows with tiling window managers (e.g. dwm)
   resizable(NULL);
   fltk::Window::show();
   fltk::Widget::hide();
   resizable(r);
   XReparentWindow (fltk::xdisplay, fltk::xid(this), xid, 0, 0);
#endif
}
