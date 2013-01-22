#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

/**
 * \brief Represents additional data for containing blocks.
 */
class OutOfFlowMgr
{
public:
   class ContainingBlock
   {
   public:
      virtual void borderChanged (int y) = 0;
      virtual core::Widget *asWidget () = 0;
   };

private:
   ContainingBlock *containingBlock;
   int availWidth, availAscent, availDescent;

   class Float: public lout::object::Object
   {
   public:
      core::Widget *widget, *generatingBlock;
      // width includes border of the containing block
      int y;           // relative to generator, not container
      int borderWidth; // relative to container (difference to generator is 0)
      core::Requisition size;
      bool dirty;

      inline int yForContainer (OutOfFlowMgr *oofm, int y) {
         return y - generatingBlock->getAllocation()->y +
            oofm->containingBlock->asWidget()->getAllocation()->y;
      }

      inline int yForContainer (OutOfFlowMgr *oofm) {
         return yForContainer (oofm, y);
      }
   };

   //lout::container::typed::HashTable<lout::object::TypedPointer
   //                                <dw::core::Widget>, Float> *floatsByWidget;
   lout::container::typed::Vector<Float> *leftFloats, *rightFloats;

   Float *findFloatByWidget (core::Widget *widget);
   lout::container::typed::Vector<Float> *getFloatList (core::Widget *widget);
   lout::container::typed::Vector<Float> *getOppositeFloatList (core::Widget
                                                                *widget);
   void ensureFloatSize (Float *vloat);
   int calcBorderDiff (Float *vloat);
   int calcLeftBorderDiff (Float *vloat);
   int calcRightBorderDiff (Float *vloat);

   void sizeAllocate(lout::container::typed::Vector<Float> *list, bool right,
                     core::Allocation *containingBlockAllocation);
   void draw (lout::container::typed::Vector<Float> *list,
              core::View *view, core::Rectangle *area);
   core::Widget *getWidgetAtPoint (lout::container::typed::Vector<Float> *list,
                                   int x, int y, int level);
   void accumExtremes (lout::container::typed::Vector<Float> *list,
                       int *oofMinWidth, int *oofMaxWidth);
   int getBorder (core::Widget *widget,
                  lout::container::typed::Vector<Float> *list, int y, int h);
   bool hasFloat (core::Widget *widget,
                  lout::container::typed::Vector<Float> *list, int y, int h);

   inline static bool isRefFloat (int ref)
   { return ref != -1 && (ref & 1) == 1; }
   inline static bool isRefLeftFloat (int ref)
   { return ref != -1 && (ref & 3) == 1; }
   inline static bool isRefRightFloat (int ref)
   { return ref != -1 && (ref & 3) == 3; }

   inline static int createRefLeftFloat (int index)
   { return (index << 2) | 1; }
   inline static int createRefRightFloat (int index)
   { return (index << 2) | 3; }

   inline static int getFloatIndexFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 2); }

public:
   OutOfFlowMgr (ContainingBlock *containingBlock);
   ~OutOfFlowMgr ();

   void sizeAllocate(core::Allocation *containingBlockAllocation);
   void draw (core::View *view, core::Rectangle *area);
   void queueResize(int ref);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   core::Widget *getWidgetAtPoint (int x, int y, int level);

   static bool isWidgetOutOfFlow (core::Widget *widget);
   void addWidget (core::Widget *widget, core::Widget *generatingBlock);

   void tellNoPosition (core::Widget *widget);
   void tellPosition (core::Widget *widget, int y);

   void setWidth (int width) { availWidth = width; }
   void setAscent (int ascent) { availAscent = ascent; }
   void setDescent (int descent) { availDescent = descent; }

   void getSize (int cbWidth, int cbHeight, int *oofWidth, int *oofHeight);
   void getExtremes (int cbMinWidth, int cbMaxWidth, int *oofMinWidth,
                     int *oofMaxWidth);

   int getLeftBorder (core::Widget *widget, int y, int h);
   int getRightBorder (core::Widget *widget, int y, int h);

   bool hasFloatLeft (core::Widget *widget, int y, int h);
   bool hasFloatRight (core::Widget *widget, int y, int h);

   inline static bool isRefOutOfFlow (int ref)
   { return ref != -1 && (ref & 1) != 0; }
   inline static int createRefNormalFlow (int lineNo) { return lineNo << 1; }
   inline static int getLineNoFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 1); }

   //inline static bool isRefOutOfFlow (int ref) { return false; }
   //inline static int createRefNormalFlow (int lineNo) { return lineNo; }
   //inline static int getLineNoFromRef (int ref) { return ref; }

   // for iterators
   inline int getNumWidgets () {
      return leftFloats->size() + rightFloats->size(); }
   inline core::Widget *getWidget (int i) {
      return i < leftFloats->size() ? leftFloats->get(i)->widget :
         rightFloats->get(i - leftFloats->size())->widget; }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
