#ifndef __DW_OOFPOSFIXEDMGR_HH__
#define __DW_OOFPOSFIXEDMGR_HH__

#include "oofpositionedmgr.hh"

namespace dw {

class OOFPosFixedMgr: public OOFPositionedMgr
{
protected:
   int cbBoxOffsetX ();
   int cbBoxOffsetY ();
   int cbBoxRestWidth ();
   int cbBoxRestHeight ();

public:
   OOFPosFixedMgr (Textblock *containingBlock);
   ~OOFPosFixedMgr ();
};

} // namespace dw

#endif // __DW_OOFPOSFIXEDMGR_HH__
