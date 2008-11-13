/*
 * Dillo Widget
 *
 * Copyright 2005-2007 Sebastian Geerken <sgeerken@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include "fltkcore.hh"

#include <fltk/draw.h>
#include <fltk/run.h>
#include <fltk/events.h>
#include <fltk/Monitor.h>
#include <fltk/utf.h>
#include <stdio.h>

namespace dw {
namespace fltk {

using namespace ::fltk;

/**
 * \todo Distinction between italics and oblique would be nice.
 */

container::typed::HashTable <dw::core::style::FontAttrs,
                             FltkFont> *FltkFont::fontsTable =
   new container::typed::HashTable <dw::core::style::FontAttrs,
                                    FltkFont> (false, false);

FltkFont::FltkFont (core::style::FontAttrs *attrs)
{
   copyAttrs (attrs);

   int fa = 0;
   if(weight >= 500)
      fa |= BOLD;
   if(style != core::style::FONT_STYLE_NORMAL)
      fa  |= ITALIC;

   font = ::fltk::font(name, fa);
   if(font == NULL) {
      fprintf(stderr, "No font '%s', using default sans-serif font.\n", name);
      /*
       * If using xft, fltk::HELVETICA just means sans, fltk::COURIER
       * means mono, and fltk::TIMES means serif.
       */
      font = HELVETICA;
   }      

   setfont(font, size);
   spaceWidth = (int)getwidth(" ");
   int xw, xh;
   measure("x", xw, xh);
   xHeight = xh;
   ascent = (int)getascent();
   descent = (int)getdescent();

   /**
    * \bug The code above does not seem to work, so this workaround.
    */
   xHeight = ascent * 3 / 5;
}

FltkFont::~FltkFont ()
{
   fontsTable->remove (this);
}

FltkFont*
FltkFont::create (core::style::FontAttrs *attrs)
{
   FltkFont *font = fontsTable->get (attrs);

   if (font == NULL) {
      font = new FltkFont (attrs);
      fontsTable->put (font, font);
   }

   return font;
}

container::typed::HashTable <dw::core::style::ColorAttrs,
                             FltkColor>
   *FltkColor::colorsTable =
      new container::typed::HashTable <dw::core::style::ColorAttrs,
                                       FltkColor> (false, false);

FltkColor::FltkColor (int color, core::style::Color::Type type):
   Color (color, type)
{
   this->color = color;
   this->type = type;

   /*
    * fltk/setcolor.cxx:
    * "A Color of zero (fltk::NO_COLOR) will draw black but is
    * ambiguous. It is returned as an error value or to indicate portions
    * of a Style that should be inherited, and it is also used as the
    * default label color for everything so that changing color zero can
    * be used by the -fg switch. You should use fltk::BLACK (56) to get
    * black."
    *
    * i.e., zero only works sometimes.
    */

   if (!(colors[SHADING_NORMAL] = shadeColor (color, SHADING_NORMAL) << 8))
      colors[SHADING_NORMAL] = ::fltk::BLACK;
   if (!(colors[SHADING_INVERSE] = shadeColor (color, SHADING_INVERSE) << 8))
      colors[SHADING_INVERSE] = ::fltk::BLACK;

   if(type == core::style::Color::TYPE_SHADED) {
      if (!(colors[SHADING_DARK] = shadeColor (color, SHADING_DARK) << 8))
         colors[SHADING_DARK] = ::fltk::BLACK;
      if (!(colors[SHADING_LIGHT] = shadeColor (color, SHADING_LIGHT) << 8))
         colors[SHADING_LIGHT] = ::fltk::BLACK;
   }
}

FltkColor::~FltkColor ()
{
   colorsTable->remove (this);
}

FltkColor * FltkColor::create (int col, core::style::Color::Type type)
{
   ColorAttrs attrs(col, type);
   FltkColor *color = colorsTable->get (&attrs);

   if (color == NULL) {
      color = new FltkColor (col, type);
      colorsTable->put (color, color);
   }

   return color; 
}

void FltkView::addFltkWidget (::fltk::Widget *widget,
                              core::Allocation *allocation)
{
}

void FltkView::removeFltkWidget (::fltk::Widget *widget)
{
}

void FltkView::allocateFltkWidget (::fltk::Widget *widget,
                                   core::Allocation *allocation)
{
}

void FltkView::drawFltkWidget (::fltk::Widget *widget, core::Rectangle *area)
{
}


core::ui::LabelButtonResource *
FltkPlatform::FltkResourceFactory::createLabelButtonResource (const char
                                                              *label)
{
   return new ui::FltkLabelButtonResource (platform, label);
}

core::ui::ComplexButtonResource *
FltkPlatform::FltkResourceFactory::createComplexButtonResource (core::Widget
                                                                *widget,
                                                                bool relief)
{
   return new ui::FltkComplexButtonResource (platform, widget, relief);
}

core::ui::ListResource *
FltkPlatform::FltkResourceFactory::createListResource (core::ui
                                                       ::ListResource
                                                       ::SelectionMode
                                                       selectionMode)
{
   return new ui::FltkListResource (platform, selectionMode);
}

core::ui::OptionMenuResource *
FltkPlatform::FltkResourceFactory::createOptionMenuResource ()
{
   return new ui::FltkOptionMenuResource (platform);
}

core::ui::EntryResource *
FltkPlatform::FltkResourceFactory::createEntryResource (int maxLength,
                                                        bool password)
{
   return new ui::FltkEntryResource (platform, maxLength, password);
}

core::ui::MultiLineTextResource *
FltkPlatform::FltkResourceFactory::createMultiLineTextResource (int cols,
                                                                int rows)
{
   return new ui::FltkMultiLineTextResource (platform, cols, rows);
}

core::ui::CheckButtonResource *
FltkPlatform::FltkResourceFactory::createCheckButtonResource (bool activated)
{
   return new ui::FltkCheckButtonResource (platform, activated);
}

core::ui::RadioButtonResource
*FltkPlatform::FltkResourceFactory::createRadioButtonResource
(core::ui::RadioButtonResource *groupedWith, bool activated)
{
   return
      new ui::FltkRadioButtonResource (platform,
                                       (ui::FltkRadioButtonResource*)
                                       groupedWith,
                                       activated);
}

// ----------------------------------------------------------------------

FltkPlatform::FltkPlatform ()
{
   layout = NULL;
   idleQueue = new container::typed::List <IdleFunc> (true);
   idleFuncRunning = false;
   idleFuncId = 0;

   views = new container::typed::List <FltkView> (false);
   resources = new container::typed::List <ui::FltkResource> (false);

   resourceFactory.setPlatform (this);
}

FltkPlatform::~FltkPlatform ()
{
   if(idleFuncRunning)
      remove_idle (generalStaticIdle, (void*)this);
   delete idleQueue;
   delete views;
   delete resources;
}

void FltkPlatform::setLayout (core::Layout *layout)
{
   this->layout = layout;
}


void FltkPlatform::attachView (core::View *view)
{
   views->append ((FltkView*)view);

   for (container::typed::Iterator <ui::FltkResource> it =
           resources->iterator (); it.hasNext (); ) {
      ui::FltkResource *resource = it.getNext ();
      resource->attachView ((FltkView*)view);
   }
}


void FltkPlatform::detachView  (core::View *view)
{
   views->removeRef ((FltkView*)view);

   for (container::typed::Iterator <ui::FltkResource> it =
           resources->iterator (); it.hasNext (); ) {
      ui::FltkResource *resource = it.getNext ();
      resource->detachView ((FltkView*)view);
   }
}


int FltkPlatform::textWidth (core::style::Font *font, const char *text,
                             int len)
{
   FltkFont *ff = (FltkFont*) font;
   setfont (ff->font, ff->size);
   return (int) getwidth (text, len);
}

int FltkPlatform::nextGlyph (const char *text, int idx)
{
   return utf8fwd (&text[idx + 1], text, &text[strlen (text)]) - text;
}

int FltkPlatform::prevGlyph (const char *text, int idx)
{
   return utf8back (&text[idx - 1], text, &text[strlen (text)]) - text;
}

float FltkPlatform::dpiX ()
{
   return ::fltk::Monitor::all ().dpi_x ();
}

float FltkPlatform::dpiY ()
{
   return ::fltk::Monitor::all ().dpi_y ();
}

void FltkPlatform::generalStaticIdle (void *data)
{
   ((FltkPlatform*)data)->generalIdle();
}

void FltkPlatform::generalIdle ()
{
   IdleFunc *idleFunc;

   if (!idleQueue->isEmpty ()) {
      /* Execute the first function in the list. */
      idleFunc = idleQueue->getFirst ();
      (layout->*(idleFunc->func)) ();
  
      /* Remove this function. */
      idleQueue->removeRef(idleFunc);
   }

   if(idleQueue->isEmpty()) {
      idleFuncRunning = false;
      remove_idle (generalStaticIdle, (void*)this);
   }
}

/**
 * \todo Incomplete comments.
 */
int FltkPlatform::addIdle (void (core::Layout::*func) ())
{
   /*
    * Since ... (todo) we have to wrap around fltk_add_idle. There is only one
    * idle function, the passed idle function is put into a queue.
    */
   if(!idleFuncRunning) {
      add_idle (generalStaticIdle, (void*)this);
      idleFuncRunning = true;
   }

   idleFuncId++;

   IdleFunc *idleFunc = new IdleFunc();
   idleFunc->id = idleFuncId;
   idleFunc->func = func;
   idleQueue->append (idleFunc);

   return idleFuncId;
}

void FltkPlatform::removeIdle (int idleId)
{
   bool found;
   container::typed::Iterator <IdleFunc> it;
   IdleFunc *idleFunc;

   for(found = false, it = idleQueue->iterator(); !found && it.hasNext(); ) {
      idleFunc = it.getNext();
      if(idleFunc->id == idleId) {
         idleQueue->removeRef (idleFunc);
         found = true;
      }
   }

   if(idleFuncRunning && idleQueue->isEmpty())
      remove_idle (generalStaticIdle, (void*)this);
}

core::style::Font *FltkPlatform::createFont (core::style::FontAttrs
                                             *attrs,
                                             bool tryEverything)
{
   return FltkFont::create (attrs);
}

core::style::Color *FltkPlatform::createSimpleColor (int color)
{
   return FltkColor::create (color, core::style::Color::TYPE_SIMPLE);
}

core::style::Color *FltkPlatform::createShadedColor (int color)
{
   return FltkColor::create (color, core::style::Color::TYPE_SHADED);
}

void FltkPlatform::copySelection(const char *text)
{
   fltk::copy(text, strlen(text), false);
}

core::Imgbuf *FltkPlatform::createImgbuf (core::Imgbuf::Type type,
                                          int width, int height)
{
   return new FltkImgbuf (type, width, height);
}

core::ui::ResourceFactory *FltkPlatform::getResourceFactory ()
{
   return &resourceFactory;
}


void FltkPlatform::attachResource (ui::FltkResource *resource)
{
   resources->append (resource);

   for (container::typed::Iterator <FltkView> it = views->iterator ();
        it.hasNext (); ) {
      FltkView *view = it.getNext ();
      resource->attachView (view);
   }
}

void FltkPlatform::detachResource (ui::FltkResource *resource)
{
   resources->removeRef (resource);
}

} // namespace fltk
} // namespace dw
