#include "outofflowmgr.hh"
#include "textblock.hh"

using namespace lout::container::typed;
using namespace lout::misc;
using namespace dw::core;
using namespace dw::core::style;

namespace dw {

int OutOfFlowMgr::Float::yForContainer (OutOfFlowMgr *oofm, int y)
{
   return y - generatingBlock->getAllocation()->y +
      oofm->containingBlock->getAllocation()->y;
}

OutOfFlowMgr::OutOfFlowMgr (Textblock *containingBlock)
{
   //printf ("OutOfFlowMgr::OutOfFlowMgr\n");

   this->containingBlock = containingBlock;
   availWidth = availAscent = availDescent = -1;   
   leftFloats = new Vector<Float> (1, true);
   rightFloats = new Vector<Float> (1, true);
}

OutOfFlowMgr::~OutOfFlowMgr ()
{
   //printf ("OutOfFlowMgr::~OutOfFlowMgr\n");

   delete leftFloats;
   delete rightFloats;
}

void OutOfFlowMgr::sizeAllocate (Allocation *containingBlockAllocation)
{
   sizeAllocate (leftFloats, false, containingBlockAllocation);
   sizeAllocate (rightFloats, true, containingBlockAllocation);
}

void OutOfFlowMgr::sizeAllocate(Vector<Float> *list, bool right,
                                Allocation *containingBlockAllocation)
{
   int width =
      availWidth != -1 ? availWidth : containingBlockAllocation->width;
   
   for (int i = 0; i < list->size(); i++) {
      // TODO Missing: check newly calculated positions, collisions,
      // and queue resize, when neccessary.

      Float *vloat = list->get(i);
      assert (vloat->y != -1);
      ensureFloatSize (vloat);

      Allocation childAllocation;
      if (right)
         childAllocation.x =
            vloat->generatingBlock->getAllocation()->x + width
            - (vloat->size.width + vloat->borderWidth);
      else
         childAllocation.x =
            vloat->generatingBlock->getAllocation()->x + vloat->borderWidth;

      childAllocation.y = vloat->generatingBlock->getAllocation()->y + vloat->y;
      childAllocation.width = vloat->size.width;
      childAllocation.ascent = vloat->size.ascent;
      childAllocation.descent = vloat->size.descent;
      
      vloat->widget->sizeAllocate (&childAllocation);

      //printf ("allocate %s #%d -> (%d, %d), %d x (%d + %d)\n",
      //        right ? "right" : "left", i, childAllocation.x,
      //        childAllocation.y, childAllocation.width,
      //        childAllocation.ascent, childAllocation.descent);
   }
}


void OutOfFlowMgr::draw (View *view, Rectangle *area)
{
   draw (leftFloats, view, area);
   draw (rightFloats, view, area);
}

void OutOfFlowMgr::draw (Vector<Float> *list, View *view, Rectangle *area)
{
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      assert (vloat->y != -1);

      core::Rectangle childArea;
      if (vloat->widget->intersects (area, &childArea))
         vloat->widget->draw (view, &childArea);
   }
}

void OutOfFlowMgr::queueResize(int ref)
{
   // TODO Is there something to do?
}

bool OutOfFlowMgr::isWidgetOutOfFlow (core::Widget *widget)
{
   // Will be extended for absolute positions.
   return widget->getStyle()->vloat != FLOAT_NONE;
}

void OutOfFlowMgr::addWidget (Widget *widget, Widget *generatingBlock)
{
   if (widget->getStyle()->vloat != FLOAT_NONE) {
      Float *vloat = new Float ();
      vloat->widget = widget;
      vloat->generatingBlock = generatingBlock;
      vloat->dirty = true;
      vloat->y = -1;

      switch (widget->getStyle()->vloat) {
      case FLOAT_LEFT:
         leftFloats->put (vloat);
         widget->parentRef = createRefLeftFloat (leftFloats->size() - 1);
         break;

      case FLOAT_RIGHT:
         rightFloats->put (vloat);
         widget->parentRef = createRefRightFloat (rightFloats->size() - 1);
         break;

      default:
         assertNotReached();
      }
   } else
      // Will continue here for absolute positions.
      assertNotReached();
}

OutOfFlowMgr::Float *OutOfFlowMgr::findFloatByWidget (Widget *widget)
{
   Vector<Float> *list = getFloatList (widget);

   for(int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      if(vloat->widget == widget)
         return vloat;
   }

   assertNotReached();
   return NULL;
}

Vector<OutOfFlowMgr::Float> *OutOfFlowMgr::getFloatList (Widget *widget)
{
   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return leftFloats;

   case FLOAT_RIGHT:
      return rightFloats;

   default:
      assertNotReached();
      return NULL;
   }
}

Vector<OutOfFlowMgr::Float> *OutOfFlowMgr::getOppositeFloatList (Widget *widget)
{
   switch (widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return rightFloats;

   case FLOAT_RIGHT:
      return leftFloats;

   default:
      assertNotReached();
      return NULL;
   }
}

void OutOfFlowMgr::markSizeChange (int ref)
{
   //printf ("[%p] MARK_SIZE_CHANGE (%d)\n", containingBlock, ref);

   if (isRefFloat (ref)) {
      // Nothing can be done when the container has not been allocated.
      if (containingBlock->asWidget()->wasAllocated()) {
         Float *vloat;
         
         if (isRefLeftFloat (ref))
            vloat = leftFloats->get (getFloatIndexFromRef (ref));
         else if (isRefRightFloat (ref))
            vloat = rightFloats->get (getFloatIndexFromRef (ref));
         else {
            assertNotReached();
            vloat = NULL; // compiler happiness
         }
         
         vloat->dirty = true;
         // In some cases, the vertical position is not yet defined. Nothing
         // necessary then.
         if (vloat->y != -1 && vloat->generatingBlock->wasAllocated())
            // ContainingBlock::borderChanged expects coordinates
            // relative to the container.
            containingBlock->borderChanged (vloat->yForContainer (this));
      }
      // TODO When ContainingBlock::borderChanged has not been called
      // (because at least one of the widgets has not been allocated),
      // this has to be done later, in sizeAllocate. How is this
      // remembered? This is valid for all calls of
      // ContainingBlock::borderChanged; search this file for other
      // occurences.
   } else
      // later: absolute positions
      assertNotReached();
}


void OutOfFlowMgr::markExtremesChange (int ref)
{
   // TODO Something to do here?
}

Widget *OutOfFlowMgr::getWidgetAtPoint (int x, int y, int level)
{
   Widget *childAtPoint = getWidgetAtPoint (leftFloats, x, y, level);
   if (childAtPoint == NULL)
      childAtPoint = getWidgetAtPoint (rightFloats, x, y, level);
   return childAtPoint;
}

Widget *OutOfFlowMgr::getWidgetAtPoint (Vector<Float> *list,
                                        int x, int y, int level)
{
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      Widget *childAtPoint = vloat->widget->getWidgetAtPoint (x, y, level + 1);
      if (childAtPoint)
         return childAtPoint;
   }

   return NULL;
}


void OutOfFlowMgr::tellNoPosition (Widget *widget)
{
   Float *vloat = findFloatByWidget(widget);
   int oldY = vloat->y;
   vloat->y = -1;

   if (oldY != -1 && containingBlock->asWidget()->wasAllocated() &&
       vloat->generatingBlock->wasAllocated())
      // ContainingBlock::borderChanged expects coordinates relative
      // to the container.
      containingBlock->borderChanged (vloat->yForContainer (this, oldY));
}


void OutOfFlowMgr::getSize (int cbWidth, int cbHeight,
                            int *oofWidth, int *oofHeight)
{
   // CbWidth and cbHeight *do* contain padding, border, and
   // margin. See call in dw::Textblock::sizeRequest. (Notice that
   // this has changed from an earlier version.)

   // Also notice that Float::y includes margins etc.

   // TODO Is it correct to add padding, border, and margin to the
   // containing block? Check CSS spec.

   *oofWidth = cbWidth; /* This (or "<=" instead of "=") should be
                           the case for floats. */

#if 0
   // TODO Latest change: Check and re-activate.

   int oofHeightLeft = containingBlock->asWidget()->getStyle()->boxDiffWidth();
   int oofHeightRight = containingBlock->asWidget()->getStyle()->boxDiffWidth();

   for (int i = 0; i < leftFloats->size(); i++) {
      Float *vloat = leftFloats->get(i);
      if (vloat->y != -1) {
         ensureFloatSize (vloat);
         oofHeightLeft =
            max (oofHeightLeft,
                 vloat->y + vloat->size.ascent + vloat->size.descent
                 + containingBlock->asWidget()->getStyle()->boxRestHeight());
      }
   }

   for (int i = 0; i < rightFloats->size(); i++) {
      Float *vloat = rightFloats->get(i);
      if (vloat->y != -1) {
         ensureFloatSize (vloat);
         oofHeightRight =
            max (oofHeightRight,
                 vloat->y + vloat->size.ascent + vloat->size.descent
                 + containingBlock->asWidget()->getStyle()->boxRestHeight());
      }
   }

   *oofHeight = max (oofHeightLeft, oofHeightRight);
#endif
}

void OutOfFlowMgr::getExtremes (int cbMinWidth, int cbMaxWidth,
                                int *oofMinWidth, int *oofMaxWidth)
{
   *oofMinWidth = *oofMaxWidth = 0;
   accumExtremes (leftFloats, oofMinWidth, oofMaxWidth);
   accumExtremes (rightFloats, oofMinWidth, oofMaxWidth);
}

void OutOfFlowMgr::accumExtremes (Vector<Float> *list, int *oofMinWidth,
                                  int *oofMaxWidth)
{
   // TODO Latest change: Check and re-activate.

#if 0
   for (int i = 0; i < list->size(); i++) {
      Float *v = list->get(i);
      Extremes extr;
      v->widget->getExtremes (&extr);
      // TODO Calculation of borders is repeated quite much.
      int borderDiff = calcLeftBorderDiff (v) + calcRightBorderDiff (v);
      *oofMinWidth = max (*oofMinWidth, extr.minWidth + borderDiff);
      *oofMaxWidth = max (*oofMaxWidth, extr.maxWidth + borderDiff);
   }
#endif
}


void OutOfFlowMgr::tellPosition (Widget *widget, int y)
{
   // TODO Latest change: Check and reactivate. First, rather simple.
   assert (y >= 0);

   Float *vloat = findFloatByWidget(widget);
   ensureFloatSize (vloat);

   int oldY = vloat->y;
   vloat->y = y;

   if (oldY == -1)
      containingBlock->borderChanged (vloat->yForContainer (this));
   else if (vloat->y != oldY)
      containingBlock->borderChanged (vloat->yForContainer
                                      (this, min (oldY, vloat->y)));

#if 0
   assert (y >= 0);

   Float *vloat = findFloatByWidget(widget);
   ensureFloatSize (vloat);

   //printf ("[%p] tellPosition (%p, %d): %d ...\n",
   //        containingBlock, widget, y, vloat->y);
   
   int oldY = vloat->y;
   
   int realY = y;
   Vector<Float> *listSame = getFloatList (widget);   
   Vector<Float> *listOpp = getOppositeFloatList (widget);   
   bool collides;
   
   do {
      // Test collisions on the same side.
      collides = false;
      
      for (int i = 0; i < listSame->size(); i++) {
         Float *v = listSame->get(i);
         if (v != vloat) {
            ensureFloatSize (v);
            if (v->y != -1 && realY >= v->y && 
                realY < v->y + v->size.ascent + v->size.descent) {
               collides = true;
               realY = v->y + v->size.ascent + v->size.descent;
               break;
            }
         }
      }    
      
      // Test collisions on the other side.
      for (int i = 0; i < listOpp->size(); i++) {
         Float *v = listOpp->get(i);
         // Note: Since v is on the opposite side, the condition 
         // v != vloat (used above) is always true.
         ensureFloatSize (v);
         if (v->y != -1 && realY >= v->y && 
             realY < v->y + v->size.ascent + v->size.descent &&
             // For the other size, horizontal dimensions have to be
             // considered, too.
             v->size.width + v->borderWidth +
             vloat->size.width + vloat->borderWidth > availWidth) {
            collides = true;
            realY = v->y + v->size.ascent + v->size.descent;
            break;
         }
      } 
   } while (collides);

   vloat->y = realY;

   //printf ("  => %d\n", vloat->y);
   
   if (oldY == -1)
      containingBlock->borderChanged (vloat->y);
   else if (vloat->y != oldY)
      containingBlock->borderChanged (min (oldY, vloat->y));
#endif
}
   
/**
 * Get the left border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * The border includes marging/border/padding of the containging
 * block, but is 0 if there is no float, so a caller should also
 * consider other borders.
 */
int OutOfFlowMgr::getLeftBorder (Widget *widget, int y, int h)
{
   return getBorder (widget, leftFloats, y, h);
}

/**
 * Get the right border for the vertical position of *y*, for a height
 * of *h", based on floats.
 *
 * See also getLeftBorder(int, int);
 */
int OutOfFlowMgr::getRightBorder (Widget *widget, int y, int h)
{
   return getBorder (widget, rightFloats, y, h);
}

int OutOfFlowMgr::getBorder (Widget *widget, Vector<Float> *list, int y, int h)
{
   int border = 0;

   // To be a bit more efficient, one could use linear search to find
   // the first affected float.
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);
      int yWidget;
      if (vloat->y == -1)
         yWidget = -1;
      else if (widget == vloat->generatingBlock)
         yWidget = vloat->y;
      else {
         if (widget->wasAllocated() && vloat->generatingBlock->wasAllocated())
            yWidget = vloat->y + vloat->generatingBlock->getAllocation()->y
               - widget->getAllocation()->y;
         else
            yWidget = -1;
      }

      if (yWidget != -1 && y + h >= yWidget &&
          y < yWidget + vloat->size.ascent + vloat->size.descent)
         // It is not sufficient to find the first float, since a line
         // (with height h) may cover the region of multiple float, of
         // which the widest has to be choosen.
         border = max (border, vloat->size.width + vloat->borderWidth);

      // To be a bit more efficient, the loop could be stopped when
      // (i) at least one float has been found, and (ii) the next float is
      // below y + h.
   }

   return border;
}

bool OutOfFlowMgr::hasFloatLeft (Widget *widget, int y, int h)
{
   return hasFloat (widget, leftFloats, y, h);
}

bool OutOfFlowMgr::hasFloatRight (Widget *widget, int y, int h)
{
   return hasFloat (widget, rightFloats, y, h);
}

bool OutOfFlowMgr::hasFloat (Widget *widget, Vector<Float> *list, int y, int h)
{
   // TODO Latest change: Many changes neccessary. Re-actiavate.

#if 0
   // Compare to getBorder().
   for (int i = 0; i < list->size(); i++) {
      Float *vloat = list->get(i);
      ensureFloatSize (vloat);
      
      if (vloat->y != -1 && y + h >= vloat->y &&
          y < vloat->y + vloat->size.ascent + vloat->size.descent)
         return true;
   }
#endif

   return false;
}

void OutOfFlowMgr::ensureFloatSize (Float *vloat)
{
   if (vloat->dirty) {
      // TODO Ugly. Soon to be replaced by cleaner code? See also
      // comment in Textblock::calcWidgetSize.
      if (vloat->widget->usesHints ()) {
         if (isAbsLength (vloat->widget->getStyle()->width))
            vloat->widget->setWidth
               (absLengthVal (vloat->widget->getStyle()->width));
         else if (isPerLength (vloat->widget->getStyle()->width))
            vloat->widget->setWidth
               (availWidth * perLengthVal (vloat->widget->getStyle()->width));
      }

      // This is a bit hackish: We first request the size, then set
      // the available width (also considering the one of the
      // containing block, and the extremes of the float), then
      // request the size again, which may of course have a different
      // result. This is a fix for the bug:
      //
      //    Text in floats, which are wider because of an image, are
      //    broken at a too narrow width. Reproduce:
      //    test/floats2.html. After the image has been loaded, the
      //    text "Some text in a float." should not be broken
      //    anymore.
      //
      // If the call of setWidth not is neccessary, the second call
      // will read the size from the cache, so no redundant
      // calculation is necessary.

      // Furthermore, extremes are considered; especially, floats are too
      // wide, sometimes.
      Extremes extremes;
      vloat->widget->getExtremes (&extremes);

      vloat->widget->sizeRequest (&vloat->size);

      // Set width  ...
      int width = vloat->size.width;
      // Consider the available width of the containing block (when set):
      if (availWidth != -1 && width > availWidth)
         width = availWidth;
      // Finally, consider extremes (as described above).
      if (width < extremes.minWidth)
          width = extremes.minWidth;
      if (width > extremes.maxWidth)
         width = extremes.maxWidth;
          
      vloat->widget->setWidth (width);
      vloat->widget->sizeRequest (&vloat->size);
      
      vloat->borderWidth = calcBorderDiff (vloat);
      
      //printf ("   Float at %d: %d x (%d + %d)\n",
      //        vloat->y, vloat->width, vloat->ascent, vloat->descent);
          
      vloat->dirty = false;
   }
}

/**
 * Calculate the border diff, i. e., for left floats, the difference
 * between the left side of the allocation of the containing block and
 * the left side of the allocation of the float, and likewise for
 * right floats. Both values are positive.
 *
 * From how I have understood the CSS specification, the generating
 * block should be included. (In the case I am wrong: a change of this
 * method should be sufficient.)
 */
int OutOfFlowMgr::calcBorderDiff (Float *vloat)
{
   // TODO Makes some assumtions about the allocation of the widgets,
   // which are under typical conditions fulfilled. Because of the
   // latter, this could be used as a first approximation, and later,
   // when necessary, corrected by allocation.

   switch (vloat->widget->getStyle()->vloat) {
   case FLOAT_LEFT:
      return calcLeftBorderDiff (vloat);
      
   case FLOAT_RIGHT:
      return calcRightBorderDiff (vloat);

   default:
      // Only used for floats.
      assertNotReached();
      return 0; // compiler happiness
   }
}

int OutOfFlowMgr::calcLeftBorderDiff (Float *vloat)
{
   int d = containingBlock->asWidget()->getStyle()->boxOffsetX();
   for (Widget *w = vloat->generatingBlock; w != containingBlock->asWidget();
        w = w->getParent())
      d += w->getStyle()->boxOffsetX();
   return d;
}

int OutOfFlowMgr::calcRightBorderDiff (Float *vloat)
{
   int d = containingBlock->asWidget()->getStyle()->boxRestWidth();
   for (Widget *w = vloat->generatingBlock; w != containingBlock->asWidget();
        w = w->getParent())
      d += w->getStyle()->boxRestWidth();
   return d;
}

// TODO Latest change: Check also Textblock::borderChanged: looks OK,
// but the comment ("... (i) with canvas coordinates ...") looks wrong
// (and looks as having always been wrong).

// Another issue: does it make sense to call Textblock::borderChanged
// for generators, when the respective widgets have not been called
// yet?

} // namespace dw
