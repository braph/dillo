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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "core.hh"
#include "../lout/debug.hh"

#include <stdio.h>

namespace dw {
namespace core {
namespace ui {

using namespace lout;
using namespace lout::object;

int Embed::CLASS_ID = -1;

Embed::Embed(Resource *resource)
{
   DBG_OBJ_CREATE ("dw::core::ui::Embed");
   registerName ("dw::core::ui::Embed", &CLASS_ID);
   this->resource = resource;
   resource->setEmbed (this);
   DBG_OBJ_ASSOC_CHILD (resource);
}

Embed::~Embed()
{
   delete resource;
   DBG_OBJ_DELETE ();
}

void Embed::sizeRequestImpl (Requisition *requisition)
{
   resource->sizeRequest (requisition);
}

void Embed::getExtremesImpl (Extremes *extremes)
{
   resource->getExtremes (extremes);
   correctExtremes (extremes);
}

void Embed::sizeAllocateImpl (Allocation *allocation)
{
   resource->sizeAllocate (allocation);
}

int Embed::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   return resource->getAvailWidthOfChild (child, forceValue);
}

int Embed::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   return resource->getAvailHeightOfChild (child, forceValue);
}

void Embed::correctRequisitionOfChild (Widget *child,
                                       Requisition *requisition,
                                       void (*splitHeightFun) (int, int*, int*))
{
   resource->correctRequisitionOfChild (child, requisition, splitHeightFun);
}

void Embed::correctExtremesOfChild (Widget *child, Extremes *extremes)
{
   resource->correctExtremesOfChild (child, extremes);
}

void Embed::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");
   // Nothing to do (as long as all resources return empty iterators).
   DBG_OBJ_LEAVE ();
}

void Embed::enterNotifyImpl (core::EventCrossing *event)
{
   resource->emitEnter();
   Widget::enterNotifyImpl(event);
}

void Embed::leaveNotifyImpl (core::EventCrossing *event)
{
   resource->emitLeave();
   Widget::leaveNotifyImpl(event);
}

bool Embed::buttonPressImpl (core::EventButton *event)
{
   bool handled;

   if (event->button == 3) {
      resource->emitClicked(event);
      handled = true;
   } else {
      handled = false;
   }
   return handled;
}

void Embed::setDisplayed (bool displayed)
{
   resource->setDisplayed (displayed);
}

void Embed::setEnabled (bool enabled)
{
   resource->setEnabled (enabled);
}

void Embed::draw (View *view, Rectangle *area)
{
   drawWidgetBox (view, area, false);
   resource->draw (view, area);
}

Iterator *Embed::iterator (Content::Type mask, bool atEnd)
{
   return resource->iterator (mask, atEnd);
}

void Embed::setStyle (style::Style *style)
{
   resource->setStyle (style);
   Widget::setStyle (style);
}

// ----------------------------------------------------------------------

bool Resource::ActivateEmitter::emitToReceiver (lout::signal::Receiver
                                                *receiver,
                                                int signalNo,
                                                int argc, Object **argv)
{
   ActivateReceiver *ar = (ActivateReceiver*)receiver;
   Resource *res = (Resource*)((Pointer*)argv[0])->getValue ();

   switch (signalNo) {
   case 0:
      ar->activate (res);
      break;
   case 1:
      ar->enter (res);
      break;
   case 2:
      ar->leave (res);
      break;
   default:
      misc::assertNotReached ();
   }
   return false;
}

void Resource::ActivateEmitter::emitActivate (Resource *resource)
{
   Pointer p (resource);
   Object *argv[1] = { &p };
   emitVoid (0, 1, argv);
}

void Resource::ActivateEmitter::emitEnter (Resource *resource)
{
   Pointer p (resource);
   Object *argv[1] = { &p };
   emitVoid (1, 1, argv);
}

void Resource::ActivateEmitter::emitLeave (Resource *resource)
{
   Pointer p (resource);
   Object *argv[1] = { &p };
   emitVoid (2, 1, argv);
}

// ----------------------------------------------------------------------

Resource::~Resource ()
{
   DBG_OBJ_DELETE ();
}

void Resource::setEmbed (Embed *embed)
{
   this->embed = embed;
}

void Resource::getExtremes (Extremes *extremes)
{
   DBG_OBJ_ENTER0 ("resize", 0, "getExtremes");

   /* Simply return the requisition width */
   Requisition requisition;
   sizeRequest (&requisition);
   extremes->minWidth = extremes->maxWidth = requisition.width;

   DBG_OBJ_MSGF ("resize", 1, "result: %d / %d",
                 extremes->minWidth, extremes->maxWidth);
   DBG_OBJ_LEAVE ();
}

void Resource::sizeAllocate (Allocation *allocation)
{
}

int Resource::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   // Only used when the resource contains other dillo widgets.
   misc::assertNotReached ();
   return 0;
}

int Resource::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   // Only used when the resource contains other dillo widgets.
   misc::assertNotReached ();
   return 0;
}

void Resource::correctRequisitionOfChild (Widget *child,
                                          Requisition *requisition,
                                          void (*splitHeightFun) (int, int*,
                                                                  int*))
{
   // Only used when the resource contains other dillo widgets.
   misc::assertNotReached ();
}

void Resource::correctExtremesOfChild (Widget *child, Extremes *extremes)
{
   // Only used when the resource contains other dillo widgets.
   misc::assertNotReached ();
}

void Resource::setDisplayed (bool displayed)
{
}

void Resource::draw (View *view, Rectangle *area)
{
}

void Resource::setStyle (style::Style *style)
{
}

void Resource::emitEnter ()
{
   activateEmitter.emitEnter(this);
}

void Resource::emitLeave ()
{
   activateEmitter.emitLeave(this);
}

bool Resource::ClickedEmitter::emitToReceiver(lout::signal::Receiver *receiver,
                                              int signalNo, int argc,
                                              Object **argv)
{
   ((ClickedReceiver*)receiver)
      ->clicked ((Resource*)((Pointer*)argv[0])->getValue (),
                 (EventButton*)((Pointer*)argv[1])->getValue());
   return false;
}

void Resource::ClickedEmitter::emitClicked (Resource *resource,
                                            EventButton *event)
{
   Pointer p1 (resource);
   Pointer p2 (event);
   Object *argv[2] = { &p1, &p2 };
   emitVoid (0, 2, argv);
}

// ----------------------------------------------------------------------

Iterator *LabelButtonResource::iterator (Content::Type mask, bool atEnd)
{
   /** \todo Perhaps in brackets? */
   // return new TextIterator (getEmbed (), mask, atEnd, getLabel ());
   /** \bug Not implemented. */
   return new EmptyIterator (getEmbed (), mask, atEnd);
}

// ----------------------------------------------------------------------

void ComplexButtonResource::LayoutReceiver::resizeQueued (bool extremesChanged)
{
   DBG_OBJ_ENTER ("resize", 0, "LayoutReceiver/resizeQueued", "%s",
                  extremesChanged ? "true" : "false");
   resource->queueResize (extremesChanged);
   DBG_OBJ_LEAVE ();
}

ComplexButtonResource::ComplexButtonResource ()
{
   DBG_OBJ_CREATE ("dw::core::ui::ComplexButtonResource");
   layout = NULL;
   layoutReceiver.resource = this;
   click_x = click_y = -1;
}

void ComplexButtonResource::init (Widget *widget)
{
   childWidget = widget;

   layout = new Layout (createPlatform ());
   setLayout (layout);
   DBG_OBJ_ASSOC_CHILD (layout);
   layout->setWidget (widget);
   layout->connect (&layoutReceiver);

   if (getEmbed ())
      childWidget->setQuasiParent (getEmbed ());
}

void ComplexButtonResource::setEmbed (Embed *embed)
{
   ButtonResource::setEmbed (embed);

   if (childWidget)
      childWidget->setQuasiParent (getEmbed ());
}

ComplexButtonResource::~ComplexButtonResource ()
{
   delete layout;
   DBG_OBJ_DELETE ();
}

void ComplexButtonResource::sizeRequest (Requisition *requisition)
{
   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequest");

   Requisition widgetRequisition;
   childWidget->sizeRequest (&widgetRequisition);
   requisition->width = widgetRequisition.width + 2 * reliefXThickness ();
   requisition->ascent = widgetRequisition.ascent + reliefYThickness ();
   requisition->descent = widgetRequisition.descent + reliefYThickness ();

   DBG_OBJ_MSGF ("resize", 1, "result: %d * (%d + %d)",
                 requisition->width, requisition->ascent, requisition->descent);
   DBG_OBJ_LEAVE ();
}

void ComplexButtonResource::getExtremes (Extremes *extremes)
{
   DBG_OBJ_ENTER0 ("resize", 0, "getExtremes");

   Extremes widgetExtremes;
   childWidget->getExtremes (&widgetExtremes);
   extremes->minWidth = widgetExtremes.minWidth + 2 * reliefXThickness ();
   extremes->maxWidth = widgetExtremes.maxWidth + 2 * reliefXThickness ();

   DBG_OBJ_MSGF ("resize", 1, "result: %d / %d",
                 extremes->minWidth, extremes->maxWidth);
   DBG_OBJ_LEAVE ();
}

void ComplexButtonResource::sizeAllocate (Allocation *allocation)
{
}

int ComplexButtonResource::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   int embedWidth = getEmbed()->getAvailWidth (forceValue);
   if (embedWidth == -1)
      return -1;
   else
      return misc::max (embedWidth - 2 * reliefXThickness (), 0);
}

int ComplexButtonResource::getAvailHeightOfChild (Widget *child,
                                                  bool forceValue)
{
   int embedHeight = getEmbed()->getAvailHeight (forceValue);
   if (embedHeight == -1)
      return -1;
   else
      return misc::max (embedHeight - 2 * reliefYThickness (), 0);
}

void ComplexButtonResource::correctRequisitionOfChild (Widget *child,
                                                       Requisition *requisition,
                                                       void (*splitHeightFun)
                                                       (int, int*, int*))
{
   // Similar to Widget::correctRequisitionOfChild, but for percentage
   // the relief has to be considered.
   
   if (style::isPerLength (child->getStyle()->width)) {
      int availWidth = getEmbed()->getAvailHeight (false);
      if (availWidth != -1) {
         int baseWidth = misc::max (availWidth
                                    - getEmbed()->boxDiffWidth ()
                                    - 2 * reliefXThickness (),
                                    0);
         requisition->width =
            child->applyPerWidth (baseWidth, child->getStyle()->width);
      }
   } else
      getEmbed()->correctReqWidthOfChildNoRec (child, requisition);

   // TODO Percentage heights are ignored again.
   getEmbed()->correctReqHeightOfChildNoRec (child, requisition,
                                             splitHeightFun);

}

void ComplexButtonResource::correctExtremesOfChild (Widget *child,
                                                    Extremes *extremes)
{
   // Similar to Widget::correctExtremesOfChild, but for percentage
   // the relief has to be considered.
   
   if (style::isPerLength (child->getStyle()->width)) {
      int availWidth = getEmbed()->getAvailHeight (false);
      if (availWidth != -1) {
         int baseWidth = misc::max (availWidth
                                    - getEmbed()->boxDiffWidth ()
                                    - 2 * reliefXThickness (),
                                    0);
         extremes->minWidth = extremes->maxWidth =
            child->applyPerWidth (baseWidth, child->getStyle()->width);
      }
   } else
      getEmbed()->correctExtremesOfChildNoRec (child, extremes);
}

Iterator *ComplexButtonResource::iterator (Content::Type mask, bool atEnd)
{
   /**
    * \bug Implementation.
    * This is a bit more complicated: We have two layouts here.
    */
   return new EmptyIterator (getEmbed (), mask, atEnd);
}

// ----------------------------------------------------------------------

Iterator *TextResource::iterator (Content::Type mask, bool atEnd)
{
   // return new TextIterator (getEmbed (), mask, atEnd, getText ());
   /** \bug Not implemented. */
   return new EmptyIterator (getEmbed (), mask, atEnd);
}

// ----------------------------------------------------------------------

Iterator *CheckButtonResource::iterator (Content::Type mask, bool atEnd)
{
   //return new TextIterator (getEmbed (), mask, atEnd,
   //                         isActivated () ? "[X]" : "[ ]");
   /** \bug Not implemented. */
   return new EmptyIterator (getEmbed (), mask, atEnd);
}

// ----------------------------------------------------------------------

RadioButtonResource::GroupIterator::~GroupIterator ()
{
}

Iterator *RadioButtonResource::iterator (Content::Type mask, bool atEnd)
{
   //return new TextIterator (getEmbed (), mask, atEnd,
   //                         isActivated () ? "(*)" : "( )");
   /** \bug Not implemented. */
   return new EmptyIterator (getEmbed (), mask, atEnd);
}

} // namespace ui
} // namespace core
} // namespace dw

