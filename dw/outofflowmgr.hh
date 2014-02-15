#ifndef __DW_OUTOFFLOWMGR_HH__
#define __DW_OUTOFFLOWMGR_HH__

#include "core.hh"

namespace dw {

class Textblock;

/**
 * \brief Represents additional data for containing blocks.
 */
class OutOfFlowMgr
{
   friend class WidgetInfo;

private:
   enum Side { LEFT, RIGHT };

   Textblock *containingBlock;

   // These two values are set by sizeAllocateStart(), and they are
   // accessable also within sizeAllocateEnd(), and also for the
   // containing block, for which allocation and WAS_ALLOCATED is set
   // *after* sizeAllocateEnd(). See the two inline functions
   // wasAllocated(Widget*) and getAllocation(Widget*) (further down)
   // for usage.
   core::Allocation containingBlockAllocation;
   bool containingBlockWasAllocated;

   class WidgetInfo: public lout::object::Object
   {
   private:
      bool wasAllocated;
      int xCB, yCB; // relative to the containing block
      int width, height;

      OutOfFlowMgr *oofm;
      core::Widget *widget;

   protected:
      OutOfFlowMgr *getOutOfFlowMgr () { return oofm; }

   public:
      WidgetInfo (OutOfFlowMgr *oofm, core::Widget *widget);
     
      inline bool wasThenAllocated () { return wasAllocated; }
      inline int getOldXCB () { return xCB; }
      inline int getOldYCB () { return yCB; }
      inline int getOldWidth () { return width; }
      inline int getOldHeight () { return height; }

      inline bool isNowAllocated () { return widget->wasAllocated (); }
      inline int getNewXCB () { return widget->getAllocation()->x -
            oofm->containingBlockAllocation.x; }
      inline int getNewYCB () { return widget->getAllocation()->y -
            oofm->containingBlockAllocation.y; }
      inline int getNewWidth () { return widget->getAllocation()->width; }
      inline int getNewHeight () { return widget->getAllocation()->ascent +
            widget->getAllocation()->descent; }
      
      void update (bool wasAllocated, int xCB, int yCB, int width, int height);
      void updateAllocation ();

      inline core::Widget *getWidget () { return widget; }
   };

   class Float: public WidgetInfo
   {
   public:
      class ComparePosition: public lout::object::Comparator
      {
      private:
         OutOfFlowMgr *oofm;
         Textblock *refTB;

      public:
         ComparePosition (OutOfFlowMgr *oofm, Textblock *refTB)
         { this->oofm = oofm; this->refTB = refTB; }
         int compare(Object *o1, Object *o2);
      };

      class CompareSideSpanningIndex: public lout::object::Comparator
      {
      public:
         int compare(Object *o1, Object *o2);
      };

      class CompareGBAndExtIndex: public lout::object::Comparator
      {
      private:
         OutOfFlowMgr *oofm;

      public:
         CompareGBAndExtIndex (OutOfFlowMgr *oofm) { this->oofm = oofm; }
         int compare(Object *o1, Object *o2);
      };

      Textblock *generatingBlock;
      int externalIndex;
      int yReq, yReal; // relative to generator, not container
      int index;       /* When GB is not yet allocated: position
                          within TBInfo::leftFloatsGB or
                          TBInfo::rightFloatsGB, respectively. When GB
                          is allocated: position within leftFloatsCB
                          or rightFloatsCB, respectively, even when
                          the floats are still elements of
                          TBInfo::*FloatsGB. */
      int sideSpanningIndex, mark;
      core::Requisition size;
      int cbAvailWidth; /* On which the calculation of relative sizes
                           is based. Height not yet used, and probably
                           not added before size redesign. */
      bool dirty, sizeChangedSinceLastAllocation;
      bool inCBList; /* Neccessary to prevent floats from being moved
                        twice from GB to CB list.  */

      Float (OutOfFlowMgr *oofm, core::Widget *widget,
             Textblock *generatingBlock, int externalIndex);

      void intoStringBuffer(lout::misc::StringBuffer *sb);

      int yForTextblock (Textblock *textblock, int y);
      inline int yForTextblock (Textblock *textblock)
      { return yForTextblock (textblock, yReal); }
      int yForContainer (int y);
      inline int yForContainer () { return yForContainer (yReal); }
      bool covers (Textblock *textblock, int y, int h);
   };

   /**
    * This list is kept sorted.
    *
    * To prevent accessing methods of the base class in an
    * uncontrolled way, the inheritance is private, not public; this
    * means that all methods must be delegated (see iterator(), size()
    * etc. below.)
    *
    * TODO Update comment: still sorted, but ...
    *
    * More: add() and change() may check order again.
    */
   class SortedFloatsVector: private lout::container::typed::Vector<Float>
   {
   public:
      enum Type { GB, CB } type;

   private:
      OutOfFlowMgr *oofm;
      Side side;
          
   public:
      inline SortedFloatsVector (OutOfFlowMgr *oofm, Side side, Type type) :
         lout::container::typed::Vector<Float> (1, false)
      { this->oofm = oofm; this->side = side; this->type = type; }

      int findFloatIndex (Textblock *lastGB, int lastExtIndex);
      int find (Textblock *textblock, int y, int start, int end);
      int findFirst (Textblock *textblock, int y, int h, Textblock *lastGB,
                     int lastExtIndex);
      int findLastBeforeSideSpanningIndex (int sideSpanningIndex);
      void put (Float *vloat);

      inline lout::container::typed::Iterator<Float> iterator()
      { return lout::container::typed::Vector<Float>::iterator (); }
      inline int size ()
      { return lout::container::typed::Vector<Float>::size (); }
      inline Float *get (int pos)
      { return lout::container::typed::Vector<Float>::get (pos); }
      inline void clear ()
      { lout::container::typed::Vector<Float>::clear (); }
   };

   class TBInfo: public WidgetInfo
   {
   public:
      int availWidth;
      int index; // position within "tbInfos"

      TBInfo *parent;
      int parentExtIndex;

      // These two lists store all floats generated by this textblock,
      // as long as this textblock is not allocates.
      SortedFloatsVector *leftFloatsGB, *rightFloatsGB;

      TBInfo (OutOfFlowMgr *oofm, Textblock *textblock,
              TBInfo *parent, int parentExtIndex);
      ~TBInfo ();

      inline Textblock *getTextblock () { return (Textblock*)getWidget (); }
   };

   class AbsolutelyPositioned: public lout::object::Object
   {
   public:
      core::Widget *widget;
      int xCB, yCB; // relative to the containing block
      int width, height;
      bool dirty;

      AbsolutelyPositioned (OutOfFlowMgr *oofm, core::Widget *widget,
                            Textblock *generatingBlock, int externalIndex);
   };

   // These two lists store all floats, in the order in which they are
   // defined. Only used for iterators.
   lout::container::typed::Vector<Float> *leftFloatsAll, *rightFloatsAll;

   // These two lists store all floats whose generators are already
   // allocated.
   SortedFloatsVector *leftFloatsCB, *rightFloatsCB;

   lout::container::typed::HashTable<lout::object::TypedPointer
                                     <dw::core::Widget>, Float> *floatsByWidget;

   lout::container::typed::Vector<TBInfo> *tbInfos;
   lout::container::typed::HashTable<lout::object::TypedPointer <Textblock>,
                                     TBInfo> *tbInfosByTextblock;
   
   lout::container::typed::Vector<AbsolutelyPositioned> *absolutelyPositioned;

   int lastLeftTBIndex, lastRightTBIndex, leftFloatsMark, rightFloatsMark;

   /**
    * Variant of Widget::wasAllocated(), which can also be used within
    * OOFM::sizeAllocateEnd(), and also for the generating block.
    */
   inline bool wasAllocated (core::Widget *widget) {
      return widget->wasAllocated () ||
         (widget == (core::Widget*)containingBlock &&
          containingBlockWasAllocated); }

   /**
    * Variant of Widget::getAllocation(), which can also be used
    * within OOFM::sizeAllocateEnd(), and also for the generating
    * block.
    */
   inline core::Allocation *getAllocation (core::Widget *widget) {
      return widget == (core::Widget*)containingBlock ?
         &containingBlockAllocation : widget->getAllocation (); }
   
   void moveExternalIndices (SortedFloatsVector *list, int oldStartIndex,
                             int diff);
   Float *findFloatByWidget (core::Widget *widget);

   void moveFromGBToCB (Side side);
   void sizeAllocateFloats (Side side);
   int calcFloatX (Float *vloat, Side side, int gbX, int gbWidth,
                   int gbAvailWidth);

   bool hasRelationChanged (TBInfo *tbInfo,int *minFloatPos,
                            core::Widget **minFloat);
   bool hasRelationChanged (TBInfo *tbInfo, Side side, int *minFloatPos,
                            core::Widget **minFloat);
   bool hasRelationChanged (bool oldTBAlloc,
                            int oldTBx, int oldTBy, int oldTBw, int oldTBh,
                            int newTBx, int newTBy, int newTBw, int newTBh,
                            bool oldFlAlloc,
                            int oldFlx, int oldFly, int oldFlw, int oldFlh,
                            int newFlx, int newFly, int newFlw, int newFlh,
                            Side side, int *floatPos);

   bool isTextblockCoveredByFloat (Float *vloat, Textblock *tb,
                                   int tbx, int tby, int tbWidth, int tbHeight,
                                   int *floatPos);

   void checkChangedFloatSizes ();
   void checkChangedFloatSizes (SortedFloatsVector *list);
   
   void drawFloats (SortedFloatsVector *list, core::View *view,
                    core::Rectangle *area);
   void drawAbsolutelyPositioned (core::View *view, core::Rectangle *area);
   core::Widget *getFloatWidgetAtPoint (SortedFloatsVector *list, int x, int y,
                                        int level);
   core::Widget *getAbsolutelyPositionedWidgetAtPoint (int x, int y, int level);

   bool collides (Float *vloat, Float *other, int *yReal);

   void getFloatsListsAndSide (Float *vloat, SortedFloatsVector **listSame,
                               SortedFloatsVector **listOpp, Side *side);

   void getFloatsSize (SortedFloatsVector *list, Side side, int *width,
                       int *height);
   void accumExtremes (SortedFloatsVector *list, Side side, int *oofMinWidth,
                       int *oofMaxWidth);
   int getMinBorderDiff (Float *vloat, Side side);
   int getMaxBorderDiff (Float *vloat, Side side);

   TBInfo *getTextblock (Textblock *textblock);
   int getBorder (Textblock *textblock, Side side, int y, int h,
                  Textblock *lastGB, int lastExtIndex);
   SortedFloatsVector *getFloatsListForTextblock (Textblock *textblock,
                                                  Side side);
   bool hasFloat (Textblock *textblock, Side side, int y, int h,
                  Textblock *lastGB, int lastExtIndex);

   int getClearPosition (Textblock *tb, Side side);

   void ensureFloatSize (Float *vloat);
   int getBorderDiff (Textblock *textblock, Float *vloat, Side side);

   void tellFloatPosition (core::Widget *widget, int yReq);

   void getAbsolutelyPositionedSize (int *oofWidthAbsPos, int *oofHeightAbsPos);
   void ensureAbsolutelyPositionedSizeAndPosition (AbsolutelyPositioned
                                                   *abspos);
   int calcValueForAbsolutelyPositioned (AbsolutelyPositioned *abspos,
                                         core::style::Length styleLen,
                                         int refLen);
   void sizeAllocateAbsolutelyPositioned ();

   static inline bool isWidgetFloat (core::Widget *widget)
   { return widget->getStyle()->vloat != core::style::FLOAT_NONE; }
   static inline bool isWidgetAbsolutelyPositioned (core::Widget *widget)
   { return widget->getStyle()->position == core::style::POSITION_ABSOLUTE; }

   /*
    * Format for parent ref (see also below for isRefOutOfFlow,
    * createRefNormalFlow, and getLineNoFromRef.
    *
    * Widget in flow:
    *
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *    |                line number                | 0 |
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *
    * So, anything with the least signifant bit set to 1 is out of flow.
    *
    * Floats:
    *
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *    |          left float index         | 0 | 0 | 1 |
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *    |         right float index         | 1 | 0 | 1 |
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    * 
    * Absolutely positioned blocks:
    *
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    *    |                 index                 | 1 | 1 |
    *    +---+ - - - +---+---+- - - - - -+---+---+---+---+
    */
   
   inline static bool isRefFloat (int ref)
   { return ref != -1 && (ref & 3) == 1; }
   inline static bool isRefLeftFloat (int ref)
   { return ref != -1 && (ref & 7) == 1; }
   inline static bool isRefRightFloat (int ref)
   { return ref != -1 && (ref & 7) == 5; }
   inline static bool isRefAbsolutelyPositioned (int ref)
   { return ref != -1 && (ref & 3) == 3; }

   inline static int createRefLeftFloat (int index)
   { return (index << 3) | 1; }
   inline static int createRefRightFloat (int index)
   { return (index << 3) | 5; }
   inline static int createRefAbsolutelyPositioned (int index)
   { return (index << 2) | 3; }

   inline static int getFloatIndexFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 3); }
   inline static int getAbsolutelyPositionedIndexFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 2); }

public:
   OutOfFlowMgr (Textblock *containingBlock);
   ~OutOfFlowMgr ();

   void sizeAllocateStart (core::Allocation *containingBlockAllocation);
   void sizeAllocateEnd ();
   void draw (core::View *view, core::Rectangle *area);

   void markSizeChange (int ref);
   void markExtremesChange (int ref);
   core::Widget *getWidgetAtPoint (int x, int y, int level);

   static bool isWidgetOutOfFlow (core::Widget *widget);
   static bool isWidgetHandledByOOFM (core::Widget *widget);
   void addWidgetInFlow (Textblock *textblock, Textblock *parentBlock,
                         int externalIndex);
   void addWidgetOOF (core::Widget *widget, Textblock *generatingBlock,
                      int externalIndex);
   void moveExternalIndices (Textblock *generatingBlock, int oldStartIndex,
                             int diff);

   void tellPosition (core::Widget *widget, int yReq);

   void getSize (int cbWidth, int cbHeight, int *oofWidth, int *oofHeight);
   void getExtremes (int cbMinWidth, int cbMaxWidth, int *oofMinWidth,
                     int *oofMaxWidth);

   int getLeftBorder (Textblock *textblock, int y, int h, Textblock *lastGB,
                      int lastExtIndex);
   int getRightBorder (Textblock *textblock, int y, int h, Textblock *lastGB,
                       int lastExtIndex);

   bool hasFloatLeft (Textblock *textblock, int y, int h, Textblock *lastGB,
                      int lastExtIndex);
   bool hasFloatRight (Textblock *textblock, int y, int h, Textblock *lastGB,
                       int lastExtIndex);

   int getClearPosition (Textblock *tb);

   inline static bool isRefOutOfFlow (int ref)
   { return ref != -1 && (ref & 1) != 0; }
   inline static int createRefNormalFlow (int lineNo) { return lineNo << 1; }
   inline static int getLineNoFromRef (int ref)
   { return ref == -1 ? ref : (ref >> 1); }

   // for iterators
   inline int getNumWidgets () {
      return leftFloatsAll->size() + rightFloatsAll->size() +
         absolutelyPositioned->size(); }

   inline core::Widget *getWidget (int i) {
      if (i < leftFloatsAll->size())
         return leftFloatsAll->get(i)->getWidget ();
      else if (i < leftFloatsAll->size() + rightFloatsAll->size())
         return rightFloatsAll->get(i - leftFloatsAll->size())->getWidget ();
      else
         return absolutelyPositioned->get(i - (leftFloatsAll->size() +
                                               rightFloatsAll->size()))->widget;
   }

   inline bool affectsLeftBorder (core::Widget *widget) {
      return widget->getStyle()->vloat == core::style::FLOAT_LEFT; }
   inline bool affectsRightBorder (core::Widget *widget) {
      return widget->getStyle()->vloat == core::style::FLOAT_RIGHT; }
};

} // namespace dw

#endif // __DW_OUTOFFLOWMGR_HH__
