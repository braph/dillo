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

#include "../lout/msg.h"
#include "../lout/debug.hh"

using namespace lout;
using namespace lout::object;

namespace dw {
namespace core {

// ----------------------------------------------------------------------

bool Widget::WidgetImgRenderer::readyToDraw ()
{
   return widget->wasAllocated ();
}

void Widget::WidgetImgRenderer::getBgArea (int *x, int *y, int *width,
                                           int *height)
{
   widget->getPaddingArea (x, y, width, height);
}

void Widget::WidgetImgRenderer::getRefArea (int *xRef, int *yRef, int *widthRef,
                                            int *heightRef)
{
   widget->getPaddingArea (xRef, yRef, widthRef, heightRef);
}

style::Style *Widget::WidgetImgRenderer::getStyle ()
{
   return widget->getStyle ();
}

void Widget::WidgetImgRenderer::draw (int x, int y, int width, int height)
{
   widget->queueDrawArea (x - widget->allocation.x, y - widget->allocation.y,
                          width, height);
}

// ----------------------------------------------------------------------

int Widget::CLASS_ID = -1;

Widget::Widget ()
{
   DBG_OBJ_CREATE ("dw::core::Widget");
   registerName ("dw::core::Widget", &CLASS_ID);

   flags = (Flags)(NEEDS_RESIZE | EXTREMES_CHANGED);
   parent = generator = container = NULL;
   DBG_OBJ_SET_PTR ("container", container);

   layout = NULL;

   allocation.x = -1;
   allocation.y = -1;
   allocation.width = 1;
   allocation.ascent = 1;
   allocation.descent = 0;

   extraSpace.top = extraSpace.right = extraSpace.bottom = extraSpace.left = 0;
   
   style = NULL;
   bgColor = NULL;
   buttonSensitive = true;
   buttonSensitiveSet = false;

   deleteCallbackData = NULL;
   deleteCallbackFunc = NULL;

   widgetImgRenderer = NULL;
}

Widget::~Widget ()
{
   if (deleteCallbackFunc)
      deleteCallbackFunc (deleteCallbackData);

   if (widgetImgRenderer) {
      if (style && style->backgroundImage)
         style->backgroundImage->removeExternalImgRenderer (widgetImgRenderer);
      delete widgetImgRenderer;
   }

   if (style)
      style->unref ();

   if (parent)
      parent->removeChild (this);
   else if (layout)
      layout->removeWidget ();

   DBG_OBJ_DELETE ();
}


/**
 * \brief Calculates the intersection of widget->allocation and area, returned
 *    in intersection (in widget coordinates!).
 *
 * Typically used by containers when
 * drawing their children. Returns whether intersection is not empty.
 */
bool Widget::intersects (Rectangle *area, Rectangle *intersection)
{
   Rectangle parentArea, childArea;

   parentArea = *area;
   parentArea.x += parent->allocation.x;
   parentArea.y += parent->allocation.y;

   childArea.x = allocation.x;
   childArea.y = allocation.y;
   childArea.width = allocation.width;
   childArea.height = getHeight ();

   if (parentArea.intersectsWith (&childArea, intersection)) {
      intersection->x -= allocation.x;
      intersection->y -= allocation.y;
      return true;
   } else
      return false;
}

void Widget::setParent (Widget *parent)
{
   this->parent = parent;
   layout = parent->layout;

   if (!buttonSensitiveSet)
      buttonSensitive = parent->buttonSensitive;

   DBG_OBJ_ASSOC_PARENT (parent);
   //printf ("The %s %p becomes a child of the %s %p\n",
   //        getClassName(), this, parent->getClassName(), parent);

   // Determine the container. Currently rather simple; will become
   // more complicated when absolute and fixed positions are
   // supported.
   container = NULL;
   for (Widget *widget = getParent (); widget != NULL && container == NULL;
        widget = widget->getParent())
      if (widget->isPossibleContainer ())
         container = widget;
   // If there is no possible container widget, there is
   // (surprisingly!) also no container (i. e. the viewport is
   // used). Does not occur in dillo, where the toplevel widget is a
   // Textblock.
   DBG_OBJ_SET_PTR ("container", container);

   notifySetParent();
}

void Widget::queueDrawArea (int x, int y, int width, int height)
{
   /** \todo Maybe only the intersection? */

   DBG_OBJ_ENTER ("draw", 0, "queueDrawArea", "%d, %d, %d, %d",
                  x, y, width, height);

   _MSG("Widget::queueDrawArea alloc(%d %d %d %d) wid(%d %d %d %d)\n",
       allocation.x, allocation.y,
       allocation.width, allocation.ascent + allocation.descent,
       x, y, width, height);
   if (layout)
      layout->queueDraw (x + allocation.x, y + allocation.y, width, height);

   DBG_OBJ_LEAVE ();
}

/**
 * \brief This method should be called, when a widget changes its size.
 *
 * A "fast" queueResize will ignore the anchestors, and furthermore
 * not trigger the idle function. Used only within
 * viewportSizeChanged, and not available outside Layout and Widget.
 */
void Widget::queueResize (int ref, bool extremesChanged, bool fast)
{
   DBG_OBJ_ENTER ("resize", 0, "queueResize", "%d, %s, %s",
                  ref, extremesChanged ? "true" : "false",
                  fast ? "true" : "false");

   // queueResize() can be called recursively; calls are queued, so
   // that actualQueueResize() is clean.

   if (queueResizeEntered ()) {
      DBG_OBJ_MSG ("resize", 1, "put into queue");
      layout->queueQueueResizeList->put (new Layout::QueueResizeItem
                                         (this, ref, extremesChanged, fast));
   } else {
      actualQueueResize (ref, extremesChanged, fast);
      
      while (layout != NULL && layout->queueQueueResizeList->size () > 0) {
         Layout::QueueResizeItem *item = layout->queueQueueResizeList->get (0);
         DBG_OBJ_MSGF ("resize", 1, "taken out of queue queue (size = %d)",
                      layout->queueQueueResizeList->size ());
         item->widget->actualQueueResize (item->ref, item->extremesChanged,
                                          item->fast);
         layout->queueQueueResizeList->remove (0); // hopefully not too large
      }
   }

   DBG_OBJ_LEAVE ();
}

void Widget::actualQueueResize (int ref, bool extremesChanged, bool fast)
{
   assert (!queueResizeEntered ());
   
   DBG_OBJ_ENTER ("resize", 0, "actualQueueResize", "%d, %s, %s",
                  ref, extremesChanged ? "true" : "false",
                  fast ? "true" : "false");

   enterQueueResize ();

   Widget *widget2, *child;

   Flags resizeFlag, extremesFlag;

   if (layout) {
      // If RESIZE_QUEUED is set, this widget is already in the list.
      if (!resizeQueued ())
         layout->queueResizeList->put (this);

      resizeFlag = RESIZE_QUEUED;
      extremesFlag = EXTREMES_QUEUED;
   } else {
      resizeFlag = NEEDS_RESIZE;
      extremesFlag = EXTREMES_CHANGED;
   }

   setFlags (resizeFlag);
   setFlags (ALLOCATE_QUEUED);
   markSizeChange (ref);
      
   if (extremesChanged) {
      setFlags (extremesFlag);
      markExtremesChange (ref);
   }

   if (!fast) {
      for (widget2 = parent, child = this; widget2;
           child = widget2, widget2 = widget2->parent) {      
         if (layout && !widget2->resizeQueued ())
            layout->queueResizeList->put (widget2);
         
         widget2->setFlags (resizeFlag);
         widget2->markSizeChange (child->parentRef);
         widget2->setFlags (ALLOCATE_QUEUED);
         
         if (extremesChanged) {
            widget2->setFlags (extremesFlag);
            widget2->markExtremesChange (child->parentRef);
         }
      }

      if (layout)
         layout->queueResize (extremesChanged);
   }

   leaveQueueResize ();

   DBG_OBJ_LEAVE ();
}

void Widget::containerSizeChanged ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChanged");

   // If there is a container widget (not the viewport), which has not
   // changed its size (which can be determined by the respective
   // flags: this method is called recursively), this widget will
   // neither change its size. Also, the recursive iteration can be
   // stopped, since the children of this widget will
   if (container == NULL || 
       container->needsResize () || container->resizeQueued () ||
       container->extremesChanged () || container->extremesQueued ()) {
      // Viewport (container == NULL) or container widget has changed
      // its size.
      if (affectedByContainerSizeChange ())
         queueResizeFast (0, true);

      // Even if *this* widget is not affected, children may be, so
      // iterate over children.
      containerSizeChangedForChildren ();
   }

   DBG_OBJ_LEAVE ();
}

bool Widget::affectedByContainerSizeChange ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "affectedByContainerSizeChange");

   bool ret;

   // This standard implementation is suitable for all widgets which
   // call correctRequisition() and correctExtremes(), even in the way
   // how Textblock and Image do (see comments there). Has to be kept
   // in sync.

   if (container == NULL) {
      if (style::isAbsLength (getStyle()->width) &&
          style::isAbsLength (getStyle()->height))
         // Both absolute, i. e. fixed: no dependency.
         ret = false;
      else if (style::isPerLength (getStyle()->width) ||
               style::isPerLength (getStyle()->height)) {
         // Any percentage: certainly dependenant.
         ret = true;
      } else
         // One or both is "auto": depends ...
         ret =
            (getStyle()->width == style::LENGTH_AUTO ?
             usesAvailWidth () : false) ||
            (getStyle()->height == style::LENGTH_AUTO ?
             usesAvailHeight () : false);
   } else
      ret = container->affectsSizeChangeContainerChild (this);

   DBG_OBJ_MSGF ("resize", 1, "=> %s", ret ? "true" : "false");
   DBG_OBJ_LEAVE ();
   return ret;
}

bool Widget::affectsSizeChangeContainerChild (Widget *child)
{
   DBG_OBJ_ENTER ("resize", 0, "affectsSizeChangeContainerChild", "%p", child);

   bool ret;

   // From the point of view of the container. This standard
   // implementation should be suitable for most (if not all)
   // containers.

   if (style::isAbsLength (child->getStyle()->width) &&
       style::isAbsLength (child->getStyle()->height))
      // Both absolute, i. e. fixed: no dependency.
      ret = false;
   else if (style::isPerLength (child->getStyle()->width) ||
            style::isPerLength (child->getStyle()->height)) {
      // Any percentage: certainly dependenant.
      ret = true;
   } else
      // One or both is "auto": depends ...
      ret =
         (child->getStyle()->width == style::LENGTH_AUTO ?
          child->usesAvailWidth () : false) ||
         (child->getStyle()->height == style::LENGTH_AUTO ?
          child->usesAvailHeight () : false);

   DBG_OBJ_MSGF ("resize", 1, "=> %s", ret ? "true" : "false");
   DBG_OBJ_LEAVE ();
   return ret;  
}

void Widget::containerSizeChangedForChildren ()
{
   DBG_OBJ_ENTER0 ("resize", 0, "containerSizeChangedForChildren");

   // Working, but inefficient standard implementation.
   Iterator *it = iterator ((Content::Type)(Content::WIDGET_IN_FLOW |
                                            Content::WIDGET_OOF_CONT),
                            false);
   while (it->next ())
      it->getContent()->widget->containerSizeChanged ();
   it->unref ();

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Must be implemengted by a method returning true, when
 *    getAvailWidth() is called.
 */
bool Widget::usesAvailWidth ()
{
   return false;
}

/**
 * \brief Must be implemengted by a method returning true, when
 *    getAvailHeight() is called.
 */
bool Widget::usesAvailHeight ()
{
   return false;
}

/**
 *  \brief This method is a wrapper for Widget::sizeRequestImpl(); it calls
 *     the latter only when needed.
 */
void Widget::sizeRequest (Requisition *requisition)
{
   assert (!queueResizeEntered ());

   DBG_OBJ_ENTER0 ("resize", 0, "sizeRequest");

   enterSizeRequest ();

   //printf ("The %stop-level %s %p with parentRef = %d: needsResize: %s, "
   //        "resizeQueued = %s\n",
   //        parent ? "non-" : "", getClassName(), this, parentRef,
   //        needsResize () ? "true" : "false",
   //        resizeQueued () ? "true" : "false");

   if (resizeQueued ()) {
      // This method is called outside of Layout::resizeIdle.
      setFlags (NEEDS_RESIZE);
      unsetFlags (RESIZE_QUEUED);
      // The widget is not taken out of Layout::queueResizeList, since
      // other *_QUEUED flags may still be set and processed in
      // Layout::resizeIdle.
   }

   if (needsResize ()) {
      /** \todo Check requisition == &(this->requisition) and do what? */
      sizeRequestImpl (requisition);
      this->requisition = *requisition;
      unsetFlags (NEEDS_RESIZE);

      DBG_OBJ_SET_NUM ("requisition.width", requisition->width);
      DBG_OBJ_SET_NUM ("requisition.ascent", requisition->ascent);
      DBG_OBJ_SET_NUM ("requisition.descent", requisition->descent);
   } else
      *requisition = this->requisition;

   //printf ("   ==> Result: %d x (%d + %d)\n",
   //        requisition->width, requisition->ascent, requisition->descent);

   leaveSizeRequest ();

   DBG_OBJ_LEAVE ();
}

/**
 * Return available width including margin/border/padding
 * (extraSpace?), not only the content width.
 */
int Widget::getAvailWidth (bool forceValue)
{
   // TODO Correct by extremes?

   DBG_OBJ_ENTER ("resize", 0, "getAvailWidth", "%s",
                  forceValue ? "true" : "false");

   int width;

   if (parent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      // TODO Consider nested layouts (e. g. <button>).
      if (style::isAbsLength (getStyle()->width)) {
         DBG_OBJ_MSGF ("resize", 1, "absolute width: %dpx",
                       style::absLengthVal (getStyle()->width));
         width = style::absLengthVal (getStyle()->width) + boxDiffWidth ();
      } else {
         int viewportWidth =
            layout->viewportWidth - (layout->canvasHeightGreater ?
                                     layout->vScrollbarThickness : 0);
         if (style::isPerLength (getStyle()->width)) {
            DBG_OBJ_MSGF ("resize", 1, "percentage width: %g%%",
                          100 * style::perLengthVal_useThisOnlyForDebugging
                          (getStyle()->width));
            width = applyPerWidth (viewportWidth, getStyle()->width);
         } else {
            DBG_OBJ_MSG ("resize", 1, "no specification");
            width = viewportWidth;
         }

      }
      DBG_OBJ_MSG_END ();
   } else {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      width = parent->getAvailWidthOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();

   return width;
}

/**
 * Return available height including margin/border/padding
 * (extraSpace?), not only the content height.
 */
int Widget::getAvailHeight (bool forceValue)
{
   // TODO Correct by ... not extremes, but ...? (Height extremes?)

   DBG_OBJ_ENTER ("resize", 0, "getAvailHeight", "%s",
                  forceValue ? "true" : "false");

   int height;

   if (parent == NULL) {
      DBG_OBJ_MSG ("resize", 1, "no parent, regarding viewport");
      DBG_OBJ_MSG_START ();

      // TODO Consider nested layouts (e. g. <button>).
      if (style::isAbsLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                       style::absLengthVal (getStyle()->height));
         height = style::absLengthVal (getStyle()->height) + boxDiffHeight ();
      } else if (style::isPerLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                       100 * style::perLengthVal_useThisOnlyForDebugging
                       (getStyle()->height));
         // Notice that here -- unlike getAvailWidth() --
         // layout->hScrollbarThickness is not considered here;
         // something like canvasWidthGreater (analogue to
         // canvasHeightGreater) would be complicated and lead to
         // possibly contradictory self-references.
         height = applyPerHeight (layout->viewportHeight, getStyle()->height);
      } else {
         DBG_OBJ_MSG ("resize", 1, "no specification");
         height = layout->viewportHeight;
      }

      DBG_OBJ_MSG_END ();
   } else {
      DBG_OBJ_MSG ("resize", 1, "delegated to parent");
      DBG_OBJ_MSG_START ();
      height = container->getAvailHeightOfChild (this, forceValue);
      DBG_OBJ_MSG_END ();
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", height);
   DBG_OBJ_LEAVE ();

   return height;
}

void Widget::correctRequisition (Requisition *requisition,
                                 void (*splitHeightFun) (int, int *, int *))
{
   // TODO Correct by extremes?

   DBG_OBJ_ENTER ("resize", 0, "correctRequisition", "%d * (%d + %d), ...",
                  requisition->width, requisition->ascent,
                  requisition->descent);

   if (container == NULL) {
      if (style::isAbsLength (getStyle()->width)) {
         DBG_OBJ_MSGF ("resize", 1, "absolute width: %dpx",
                       style::absLengthVal (getStyle()->width));
         requisition->width =
            style::absLengthVal (getStyle()->width) + boxDiffWidth ();
      } else if (style::isPerLength (getStyle()->width)) {
         DBG_OBJ_MSGF ("resize", 1, "percentage width: %g%%",
                       100 * style::perLengthVal_useThisOnlyForDebugging
                                (getStyle()->width));
         int viewportWidth =
            layout->viewportWidth - (layout->canvasHeightGreater ?
                                     layout->vScrollbarThickness : 0);
         requisition->width = applyPerWidth (viewportWidth, getStyle()->width);
      }

      // TODO Perhaps split first, then add box ascent and descent.
      if (style::isAbsLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                       style::absLengthVal (getStyle()->height));
         splitHeightFun (style::absLengthVal (getStyle()->height)
                         + boxDiffHeight (),
                         &requisition->ascent, &requisition->descent);
      } else if (style::isPerLength (getStyle()->height)) {
         DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                       100 * style::perLengthVal_useThisOnlyForDebugging
                                (getStyle()->height));
#if 0
         // TODO Percentage heights are somewhat more complicated. Has
         // to be clarified.

         // For layout->viewportHeight, see comment in getAvailHeight().
         splitHeightFun (applyPerHeight (layout->viewportHeight,
                                         getStyle()->height),
                         &requisition->ascent, &requisition->descent);
#endif
      }
   } else
      container->correctRequisitionOfChild (this, requisition, splitHeightFun);

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent,
                 requisition->descent);
   DBG_OBJ_LEAVE ();  
}

void Widget::correctExtremes (Extremes *extremes)
{
   // TODO Extremes only corrected?

   DBG_OBJ_ENTER ("resize", 0, "correctExtremes", "%d (%d) / %d (%d)",
                  extremes->minWidth, extremes->minWidthIntrinsic,
                  extremes->maxWidth, extremes->maxWidthIntrinsic);

   if (container == NULL) {
      if (style::isAbsLength (getStyle()->width))
         extremes->minWidth = extremes->maxWidth =
            style::absLengthVal (getStyle()->width) + boxDiffWidth ();
      else if (style::isPerLength (getStyle()->width)) {
         int viewportWidth =
            layout->viewportWidth - (layout->canvasHeightGreater ?
                                     layout->vScrollbarThickness : 0);
         extremes->minWidth = extremes->maxWidth =
            applyPerWidth (viewportWidth, getStyle()->width);
      }
   } else
      container->correctExtremesOfChild (this, extremes);

   DBG_OBJ_MSGF ("resize", 1, "=> %d / %d",
                 extremes->minWidth, extremes->maxWidth);
   DBG_OBJ_LEAVE ();
}

/**
 * \brief Wrapper for Widget::getExtremesImpl().
 */
void Widget::getExtremes (Extremes *extremes)
{
   assert (!queueResizeEntered ());

   DBG_OBJ_ENTER0 ("resize", 0, "getExtremes");
   DBG_OBJ_MSG_START ();

   enterGetExtremes ();

   if (extremesQueued ()) {
      // This method is called outside of Layout::resizeIdle.
      setFlags (EXTREMES_CHANGED);
      unsetFlags (EXTREMES_QUEUED);
      // The widget is not taken out of Layout::queueResizeList, since
      // other *_QUEUED flags may still be set and processed in
      // Layout::resizeIdle.
   }

   if (extremesChanged ()) {
      // For backward compatibility (part 1/2):
      extremes->minWidthIntrinsic = extremes->maxWidthIntrinsic = -1;

      getExtremesImpl (extremes);

      // For backward compatibility (part 2/2):
      if (extremes->minWidthIntrinsic == -1)
         extremes->minWidthIntrinsic = extremes->minWidth;
      if (extremes->maxWidthIntrinsic == -1)
         extremes->maxWidthIntrinsic = extremes->maxWidth;

      this->extremes = *extremes;
      unsetFlags (EXTREMES_CHANGED);

      DBG_OBJ_SET_NUM ("extremes.minWidth", extremes->minWidth);
      DBG_OBJ_SET_NUM ("extremes.minWidthIntrinsic",
                       extremes->minWidthIntrinsic);
      DBG_OBJ_SET_NUM ("extremes.maxWidth", extremes->maxWidth);
      DBG_OBJ_SET_NUM ("extremes.maxWidthIntrinsic",
                       extremes->maxWidthIntrinsic);
   } else
      *extremes = this->extremes;

   leaveGetExtremes ();

   DBG_OBJ_LEAVE ();
}

/**
 * \brief Wrapper for Widget::sizeAllocateImpl, calls the latter only when
 *    needed.
 */
void Widget::sizeAllocate (Allocation *allocation)
{
   assert (!queueResizeEntered ());
   assert (!sizeRequestEntered ());
   assert (!getExtremesEntered ());
   assert (resizeIdleEntered ());

   DBG_OBJ_ENTER ("resize", 0, "sizeAllocate", "%d, %d; %d * (%d + %d)",
                  allocation->x, allocation->y, allocation->width,
                  allocation->ascent, allocation->descent);

   DBG_OBJ_MSGF ("resize", 1,
                 "old allocation (%d, %d; %d * (%d + %d)); needsAllocate: %s",
                 this->allocation.x, this->allocation.y, this->allocation.width,
                 this->allocation.ascent, this->allocation.descent,
                 needsAllocate () ? "true" : "false");

   enterSizeAllocate ();

   /*printf ("The %stop-level %s %p is allocated:\n",
           parent ? "non-" : "", getClassName(), this);
   printf ("   old = (%d, %d, %d + (%d + %d))\n",
           this->allocation.x, this->allocation.y, this->allocation.width,
           this->allocation.ascent, this->allocation.descent);
   printf ("   new = (%d, %d, %d + (%d + %d))\n",
           allocation->x, allocation->y, allocation->width, allocation->ascent,
           allocation->descent);
   printf ("   NEEDS_ALLOCATE = %s\n", needsAllocate () ? "true" : "false");*/

   if (needsAllocate () ||
       allocation->x != this->allocation.x ||
       allocation->y != this->allocation.y ||
       allocation->width != this->allocation.width ||
       allocation->ascent != this->allocation.ascent ||
       allocation->descent != this->allocation.descent) {

      if (wasAllocated ()) {
         layout->queueDrawExcept (
            this->allocation.x,
            this->allocation.y,
            this->allocation.width,
            this->allocation.ascent + this->allocation.descent,
            allocation->x,
            allocation->y,
            allocation->width,
            allocation->ascent + allocation->descent);
      }

      sizeAllocateImpl (allocation);

      //DEBUG_MSG (DEBUG_ALLOC, "... to %d, %d, %d x %d x %d\n",
      //           widget->allocation.x, widget->allocation.y,
      //           widget->allocation.width, widget->allocation.ascent,
      //           widget->allocation.descent);

      this->allocation = *allocation;
      unsetFlags (NEEDS_ALLOCATE);
      setFlags (WAS_ALLOCATED);

      resizeDrawImpl ();

      DBG_OBJ_SET_NUM ("allocation.x", this->allocation.x);
      DBG_OBJ_SET_NUM ("allocation.y", this->allocation.y);
      DBG_OBJ_SET_NUM ("allocation.width", this->allocation.width);
      DBG_OBJ_SET_NUM ("allocation.ascent", this->allocation.ascent);
      DBG_OBJ_SET_NUM ("allocation.descent", this->allocation.descent);
   }

   /*unsetFlags (NEEDS_RESIZE);*/

   leaveSizeAllocate ();

   DBG_OBJ_LEAVE ();
}

bool Widget::buttonPress (EventButton *event)
{
   return buttonPressImpl (event);
}

bool Widget::buttonRelease (EventButton *event)
{
   return buttonReleaseImpl (event);
}

bool Widget::motionNotify (EventMotion *event)
{
   return motionNotifyImpl (event);
}

void Widget::enterNotify (EventCrossing *event)
{
   enterNotifyImpl (event);
}

void Widget::leaveNotify (EventCrossing *event)
{
   leaveNotifyImpl (event);
}

/**
 *  \brief Change the style of a widget.
 *
 * The old style is automatically unreferred, the new is referred. If this
 * call causes the widget to change its size, dw::core::Widget::queueResize
 * is called.
 */
void Widget::setStyle (style::Style *style)
{
   bool sizeChanged;

   if (widgetImgRenderer && this->style && this->style->backgroundImage)
      this->style->backgroundImage->removeExternalImgRenderer
         (widgetImgRenderer);

   style->ref ();

   if (this->style) {
      sizeChanged = this->style->sizeDiffs (style);
      this->style->unref ();
   } else
      sizeChanged = true;

   this->style = style;

   DBG_OBJ_ASSOC_CHILD (style);

   if (style && style->backgroundImage) {
      // Create instance of WidgetImgRenderer when needed. Until this
      // widget is deleted, "widgetImgRenderer" will be kept, since it
      // is not specific to the style, but only to this widget.
      if (widgetImgRenderer == NULL)
         widgetImgRenderer = new WidgetImgRenderer (this);
      style->backgroundImage->putExternalImgRenderer (widgetImgRenderer);
   }

   if (layout != NULL) {
      layout->updateCursor ();
   }

   if (sizeChanged)
      queueResize (0, true);
   else
      queueDraw ();

   // These should better be attributed to the style itself, and a
   // script processing RTFL messages could transfer it to something
   // equivalent:

   DBG_OBJ_SET_NUM ("style.margin.top", style->margin.top);
   DBG_OBJ_SET_NUM ("style.margin.bottom", style->margin.bottom);
   DBG_OBJ_SET_NUM ("style.margin.left", style->margin.left);
   DBG_OBJ_SET_NUM ("style.margin.right", style->margin.right);

   DBG_OBJ_SET_NUM ("style.border-width.top", style->borderWidth.top);
   DBG_OBJ_SET_NUM ("style.border-width.bottom", style->borderWidth.bottom);
   DBG_OBJ_SET_NUM ("style.border-width.left", style->borderWidth.left);
   DBG_OBJ_SET_NUM ("style.border-width.right", style->borderWidth.right);

   DBG_OBJ_SET_NUM ("style.padding.top", style->padding.top);
   DBG_OBJ_SET_NUM ("style.padding.bottom", style->padding.bottom);
   DBG_OBJ_SET_NUM ("style.padding.left", style->padding.left);
   DBG_OBJ_SET_NUM ("style.padding.right", style->padding.right);

   DBG_OBJ_SET_NUM ("style.border-spacing (h)", style->hBorderSpacing);
   DBG_OBJ_SET_NUM ("style.border-spacing (v)", style->vBorderSpacing);
}

/**
 * \brief Set the background "behind" the widget, if it is not the
 *    background of the parent widget, e.g. the background of a table
 *    row.
 */
void Widget::setBgColor (style::Color *bgColor)
{
   this->bgColor = bgColor;
}

/**
 * \brief Get the actual background of a widget.
 */
style::Color *Widget::getBgColor ()
{
   Widget *widget = this;

   while (widget != NULL) {
      if (widget->style->backgroundColor)
         return widget->style->backgroundColor;
      if (widget->bgColor)
         return widget->bgColor;

      widget = widget->parent;
   }

   return layout->getBgColor ();
}


/**
 * \brief Draw borders and background of a widget part, which allocation is
 *    given by (x, y, width, height) (widget coordinates).
 *
 * area is given in widget coordinates.
 */
void Widget::drawBox (View *view, style::Style *style, Rectangle *area,
                      int x, int y, int width, int height, bool inverse)
{
   Rectangle canvasArea;
   canvasArea.x = area->x + allocation.x;
   canvasArea.y = area->y + allocation.y;
   canvasArea.width = area->width;
   canvasArea.height = area->height;

   style::drawBorder (view, layout, &canvasArea,
                      allocation.x + x, allocation.y + y,
                      width, height, style, inverse);

   // This method is used for inline elements, where the CSS 2 specification
   // does not define what here is called "reference area". To make it look
   // smoothly, the widget padding box is used.

   int xPad, yPad, widthPad, heightPad;
   getPaddingArea (&xPad, &yPad, &widthPad, &heightPad);
   style::drawBackground
      (view, layout, &canvasArea,
       allocation.x + x + style->margin.left + style->borderWidth.left,
       allocation.y + y + style->margin.top + style->borderWidth.top,
       width - style->margin.left - style->borderWidth.left
       - style->margin.right - style->borderWidth.right,
       height - style->margin.top - style->borderWidth.top
       - style->margin.bottom - style->borderWidth.bottom,
       xPad, yPad, widthPad, heightPad, style, inverse, false);
}

/**
 * \brief Draw borders and background of a widget.
 *
 * area is given in widget coordinates.
 *
 */
void Widget::drawWidgetBox (View *view, Rectangle *area, bool inverse)
{
   Rectangle canvasArea;
   canvasArea.x = area->x + allocation.x;
   canvasArea.y = area->y + allocation.y;
   canvasArea.width = area->width;
   canvasArea.height = area->height;

   style::drawBorder (view, layout, &canvasArea, allocation.x, allocation.y,
                      allocation.width, getHeight (), style, inverse);

   int xPad, yPad, widthPad, heightPad;
   getPaddingArea (&xPad, &yPad, &widthPad, &heightPad);
   style::drawBackground (view, layout, &canvasArea,
                          xPad, yPad, widthPad, heightPad,
                          xPad, yPad, widthPad, heightPad,
                          style, inverse, parent == NULL);
}

/*
 * This function is used by some widgets, when they are selected (as a whole).
 *
 * \todo This could be accelerated by using clipping bitmaps. Two important
 * issues:
 *
 *     (i) There should always been a pixel in the upper-left corner of the
 *         *widget*, so probably two different clipping bitmaps have to be
 *         used (10/01 and 01/10).
 *
 *    (ii) Should a new GC always be created?
 *
 * \bug Not implemented.
 */
void Widget::drawSelected (View *view, Rectangle *area)
{
}


void Widget::setButtonSensitive (bool buttonSensitive)
{
   this->buttonSensitive = buttonSensitive;
   buttonSensitiveSet = true;
}


/**
 * \brief Get the widget at the root of the tree, this widget is part from.
 */
Widget *Widget::getTopLevel ()
{
   Widget *widget = this;

   while (widget->parent)
      widget = widget->parent;

   return widget;
}

/**
 * \brief Get the level of the widget within the tree.
 *
 * The root widget has the level 0.
 */
int Widget::getLevel ()
{
   Widget *widget = this;
   int level = 0;

   while (widget->parent) {
      level++;
      widget = widget->parent;
   }

   return level;
}

/**
 * \brief Get the level of the widget within the tree, regarting the
 * generators, not the parents.
 *
 * The root widget has the level 0.
 */
int Widget::getGeneratorLevel ()
{
   Widget *widget = this;
   int level = 0;

   while (widget->getGenerator ()) {
      level++;
      widget = widget->getGenerator ();
   }

   return level;
}

/**
 * \brief Get the widget with the highest level, which is a direct ancestor of
 *    widget1 and widget2.
 */
Widget *Widget::getNearestCommonAncestor (Widget *otherWidget)
{
   Widget *widget1 = this, *widget2 = otherWidget;
   int level1 = widget1->getLevel (), level2 = widget2->getLevel();

   /* Get both widgets onto the same level.*/
   while (level1 > level2) {
      widget1 = widget1->parent;
      level1--;
   }

   while (level2 > level1) {
      widget2 = widget2->parent;
      level2--;
   }

   /* Search upwards. */
   while (widget1 != widget2) {
      if (widget1->parent == NULL) {
         MSG_WARN("widgets in different trees\n");
         return NULL;
      }

      widget1 = widget1->parent;
      widget2 = widget2->parent;
   }

   return widget1;
}


/**
 * \brief Search recursively through widget.
 *
 * Used by dw::core::Layout:getWidgetAtPoint.
 */
Widget *Widget::getWidgetAtPoint (int x, int y, int level)
{
   Iterator *it;
   Widget *childAtPoint;

   //printf ("%*s-> examining the %s %p (%d, %d, %d x (%d + %d))\n",
   //        3 * level, "", getClassName (), this, allocation.x, allocation.y,
   //        allocation.width, allocation.ascent, allocation.descent);

   if (x >= allocation.x &&
       y >= allocation.y &&
       x <= allocation.x + allocation.width &&
       y <= allocation.y + getHeight ()) {
      //_MSG ("%*s   -> inside\n", 3 * level, "");
      /*
       * Iterate over the children of this widget. Test recursively, whether
       * the point is within the child (or one of its children...). If there
       * is such a child, it is returned. Otherwise, this widget is returned.
       */
      childAtPoint = NULL;
      it = iterator ((Content::Type)
                     (Content::WIDGET_IN_FLOW | Content::WIDGET_OOF_CONT),
                     false);

      while (childAtPoint == NULL && it->next ()) {
         Widget *child = it->getContent()->widget;
         if (child->wasAllocated ())
            childAtPoint = child->getWidgetAtPoint (x, y, level + 1);
      }

      it->unref ();

      if (childAtPoint)
         return childAtPoint;
      else
         return this;
   } else
      return NULL;
}


void Widget::scrollTo (HPosition hpos, VPosition vpos,
               int x, int y, int width, int height)
{
   layout->scrollTo (hpos, vpos,
                     x + allocation.x, y + allocation.y, width, height);
}

/**
 * \brief Return the padding area (content plus padding).
 *
 * Used as "reference area" (ee comment of "style::drawBackground")
 * for backgrounds.
 */
void Widget::getPaddingArea (int *xPad, int *yPad, int *widthPad,
                             int *heightPad)
{
   *xPad = allocation.x + style->margin.left + style->borderWidth.left;
   *yPad = allocation.y + style->margin.top + style->borderWidth.top;
   *widthPad = allocation.width - style->margin.left - style->borderWidth.left
      - style->margin.right - style->borderWidth.right;
   *heightPad = getHeight () -  style->margin.top - style->borderWidth.top
      - style->margin.bottom - style->borderWidth.bottom;
}

void Widget::sizeAllocateImpl (Allocation *allocation)
{
}

void Widget::markSizeChange (int ref)
{
}

void Widget::markExtremesChange (int ref)
{
}

int Widget::applyPerWidth (int containerWidth, style::Length perWidth)
{
   return style::multiplyWithPerLength (containerWidth, perWidth)
      + boxDiffWidth ();
}

int Widget::applyPerHeight (int containerHeight, style::Length perHeight)
{
   return style::multiplyWithPerLength (containerHeight, perHeight)
      + boxDiffHeight ();
}

int Widget::getAvailWidthOfChild (Widget *child, bool forceValue)
{
   // This is a halfway suitable implementation for all
   // containers. For simplification, this will be used during the
   // development; then, a differentiation could be possible.

   // TODO Correct by extremes?

   DBG_OBJ_ENTER ("resize", 0, "getAvailWidthOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int width;

   if (child->getStyle()->width == style::LENGTH_AUTO) {
      DBG_OBJ_MSG ("resize", 1, "no specification");
      int availWidth = getAvailWidth (forceValue);
      if (availWidth == -1)
         width = -1;
      else
         width = misc::max (availWidth - boxDiffWidth (), 0);
   } else {
      // In most cases, the toplevel widget should be a container, so
      // the container is non-NULL when the parent is non-NULL. Just
      // in case ...:
      Widget *effContainer =
         child->container ? child->container : child->parent;
      
      if (effContainer == this) {
         if (style::isAbsLength (child->getStyle()->width)) {
            DBG_OBJ_MSGF ("resize", 1, "absolute width: %dpx",
                          style::absLengthVal (child->getStyle()->width));
            width = misc::max (style::absLengthVal (child->getStyle()->width)
                               + child->boxDiffWidth (), 0);
         } else {
            assert (style::isPerLength (child->getStyle()->width));
            DBG_OBJ_MSGF ("resize", 1, "percentage width: %g%%",
                          100 * style::perLengthVal_useThisOnlyForDebugging
                          (child->getStyle()->width));
                    
            int availWidth = getAvailWidth (forceValue);
            if (availWidth == -1)
               width = -1;
            else
               width =
                  misc::max (child->applyPerWidth (availWidth - boxDiffWidth (),
                                                   child->getStyle()->width),
                             0);
         }
      } else {
         DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
         DBG_OBJ_MSG_START ();
         width = effContainer->getAvailWidthOfChild (child, forceValue);
         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", width);
   DBG_OBJ_LEAVE ();

   return width;
}

int Widget::getAvailHeightOfChild (Widget *child, bool forceValue)
{
   // Again, a suitable implementation for all widgets (perhaps).

   // TODO Correct by extremes? (Height extemes?)

   DBG_OBJ_ENTER ("resize", 0, "getAvailHeightOfChild", "%p, %s",
                  child, forceValue ? "true" : "false");

   int height;

   if (child->getStyle()->height == style::LENGTH_AUTO) {
      DBG_OBJ_MSG ("resize", 1, "no specification");
      int availHeight = getAvailHeight (forceValue);
      if (availHeight == -1)
         height = -1;
      else
         height = misc::max (availHeight - boxDiffHeight (), 0);
   } else {
      // In most cases, the toplevel widget should be a container, so
      // the container is non-NULL when the parent is non-NULL. Just
      // in case ...:
      Widget *effContainer =
         child->container ? child->container : child->parent;
      
      if (effContainer == this) {
         if (style::isAbsLength (child->getStyle()->height)) {
            DBG_OBJ_MSGF ("resize", 1, "absolute height: %dpx",
                          style::absLengthVal (child->getStyle()->height));
            height = misc::max (style::absLengthVal (child->getStyle()->height)
                               + child->boxDiffHeight (), 0);
         } else {
            assert (style::isPerLength (child->getStyle()->height));
            DBG_OBJ_MSGF ("resize", 1, "percentage height: %g%%",
                          100 * style::perLengthVal_useThisOnlyForDebugging
                          (child->getStyle()->height));
                    
            int availHeight = getAvailHeight (forceValue);
            if (availHeight == -1)
               height = -1;
            else
               height =
                  misc::max (child->applyPerHeight (availHeight -
                                                    boxDiffHeight (),
                                                    child->getStyle()->height),
                             0);
         }
      } else {
         DBG_OBJ_MSG ("resize", 1, "delegated to (effective) container");
         DBG_OBJ_MSG_START ();
         height = effContainer->getAvailHeightOfChild (child, forceValue);
         DBG_OBJ_MSG_END ();
      }
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d", height);
   DBG_OBJ_LEAVE ();

   return height;
}

void Widget::correctRequisitionOfChild (Widget *child, Requisition *requisition,
                                        void (*splitHeightFun) (int, int*,
                                                                int*))
{
   // Again, a suitable implementation for all widgets (perhaps).

   // TODO Correct by extremes?

   DBG_OBJ_ENTER ("resize", 0, "correctRequisitionOfChild",
                  "%p, %d * (%d + %d), ...", child, requisition->width,
                  requisition->ascent, requisition->descent);

   correctReqWidthOfChild (child, requisition);
   correctReqHeightOfChild (child, requisition, splitHeightFun);

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent,
                 requisition->descent);
   DBG_OBJ_LEAVE ();
}

void Widget::correctReqWidthOfChild (Widget *child, Requisition *requisition)
{
   DBG_OBJ_ENTER ("resize", 0, "correctReqWidthOfChild", "%p, %d * (%d + %d)",
                  child, requisition->width, requisition->ascent,
                  requisition->descent);

   if (style::isAbsLength (child->getStyle()->width))
      requisition->width = style::absLengthVal (child->getStyle()->width)
         + child->boxDiffWidth ();
   else if (style::isPerLength (child->getStyle()->width)) {
      int availWidth = getAvailWidth (false);
      if (availWidth != -1) {
         int containerWidth = availWidth - boxDiffWidth ();
         requisition->width = child->applyPerWidth (containerWidth,
                                                    child->getStyle()->width);
      }
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent,
                 requisition->descent);
   DBG_OBJ_LEAVE ();
}

void Widget::correctReqHeightOfChild (Widget *child, Requisition *requisition,
                                      void (*splitHeightFun) (int, int*, int*))
{
   DBG_OBJ_ENTER ("resize", 0, "correctReqHeightOfChild",
                  "%p, %d * (%d + %d), ...", child, requisition->width,
                  requisition->ascent, requisition->descent);

   // TODO Perhaps split first, then add box ascent and descent.
   if (style::isAbsLength (child->getStyle()->height))
      splitHeightFun (style::absLengthVal (child->getStyle()->height)
                      + child->boxDiffHeight (),
                      &requisition->ascent, &requisition->descent);
   else if (style::isPerLength (child->getStyle()->height)) {
      int availHeight = getAvailHeight (false);
      if (availHeight != -1) {
         int containerHeight = availHeight - boxDiffHeight ();
         splitHeightFun (child->applyPerHeight (containerHeight,
                                                child->getStyle()->height),
                         &requisition->ascent, &requisition->descent);
      }
   }

   DBG_OBJ_MSGF ("resize", 1, "=> %d * (%d + %d)",
                 requisition->width, requisition->ascent,
                 requisition->descent);
   DBG_OBJ_LEAVE ();
}

void Widget::correctExtremesOfChild (Widget *child, Extremes *extremes)
{
   // See comment in correctRequisitionOfChild.

   // TODO Extremes only corrected?

   DBG_OBJ_ENTER ("resize", 0, "correctExtremesOfChild", "%p, %d / %d",
                  child, extremes->minWidth, extremes->maxWidth);

   if (style::isAbsLength (child->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize", 1, "absolute width: %dpx",
                    style::absLengthVal (child->getStyle()->width));
      extremes->minWidth = extremes->maxWidth =
         style::absLengthVal (child->getStyle()->width)
         + child->boxDiffWidth ();
   } else if (style::isPerLength (child->getStyle()->width)) {
      DBG_OBJ_MSGF ("resize", 1, "percentage width: %g%%",
                    100 * style::perLengthVal_useThisOnlyForDebugging
                    (child->getStyle()->width));
      int availWidth = getAvailWidth (false);
      if (availWidth != -1) {
         int containerWidth = availWidth - boxDiffWidth ();
         DBG_OBJ_MSGF ("resize", 1, "containerWidth = %d - %d = %d", 
                       availWidth, boxDiffWidth (), containerWidth);
         extremes->minWidth = extremes->maxWidth =
            child->applyPerWidth (containerWidth, child->getStyle()->width);
      }
   } else
      DBG_OBJ_MSG ("resize", 1, "no specification");

   DBG_OBJ_MSGF ("resize", 1, "=> %d / %d",
                 extremes->minWidth, extremes->maxWidth);
   DBG_OBJ_LEAVE ();
}

/**
 * \brief This method is called after a widget has been set as the top of a
 *    widget tree.
 * 
 * A widget may override this method when it is necessary to be notified.
 */
void Widget::notifySetAsTopLevel()
{
}

/**
 * \brief This method is called after a widget has been added to a parent. 
 * 
 * A widget may override this method when it is necessary to be notified.
 */
void Widget::notifySetParent()
{
}

bool Widget::isBlockLevel ()
{
   // Most widgets are not block-level.
   return false;
}

bool Widget::isPossibleContainer ()
{
   // In most (all?) cases identical to:
   return isBlockLevel ();
}

bool Widget::buttonPressImpl (EventButton *event)
{
   return false;
}

bool Widget::buttonReleaseImpl (EventButton *event)
{
   return false;
}

bool Widget::motionNotifyImpl (EventMotion *event)
{
   return false;
}

void Widget::enterNotifyImpl (EventCrossing *)
{
   style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onEnter();
}

void Widget::leaveNotifyImpl (EventCrossing *)
{
   style::Tooltip *tooltip = getStyle()->x_tooltip;

   if (tooltip)
      tooltip->onLeave();
}

void Widget::removeChild (Widget *child)
{
   // Should be implemented.
   misc::assertNotReached ();
}

// ----------------------------------------------------------------------

void splitHeightPreserveAscent (int height, int *ascent, int *descent)
{
   *descent = height - *ascent;
   if (*descent < 0) {
      *descent = 0;
      *ascent = height;
   }      
}

void splitHeightPreserveDescent (int height, int *ascent, int *descent)
{
   *ascent = height - *descent;
   if (*ascent < 0) {
      *ascent = 0;
      *descent = height;
   }      
}

} // namespace core
} // namespace dw
