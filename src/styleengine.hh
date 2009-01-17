#ifndef __STYLEENGINE_HH__
#define __STYLEENGINE_HH__

#include "dw/core.hh"
#include "doctree.hh"
#include "css.hh"
#include "cssparser.hh"

class StyleEngine : public Doctree {
   private:
      class Node : public DoctreeNode {
         public:
            dw::core::style::Style *style;
            dw::core::style::Style *wordStyle;
            const char *styleAttribute;
            bool inheritBackgroundColor;
      };

      dw::core::Layout *layout;
      lout::misc::SimpleVector <Node> *stack;
      CssContext *cssContext;
      int num;

      dw::core::style::Style *style0 (CssPropertyList *nonCssHints = NULL);
      dw::core::style::Style *wordStyle0 (CssPropertyList *nonCssHints = NULL);
      void apply (dw::core::style::StyleAttrs *attrs, CssPropertyList *props);
      bool computeValue (int *dest, CssLength value, dw::core::style::Font *font);
      bool computeValue (int *dest, CssLength value, dw::core::style::Font *font,
         int percentageBase);
      bool computeLength (dw::core::style::Length *dest, CssLength value, dw::core::style::Font *font);

   public:
      StyleEngine (dw::core::Layout *layout);
      ~StyleEngine ();
   
      /* Doctree interface */
      inline const DoctreeNode *top () {
         return stack->getRef (stack->size () - 1);
      };

      inline const DoctreeNode *parent (const DoctreeNode *n) {
         if (n->depth > 1)
            return stack->getRef (n->depth - 1);
         else
            return NULL;
      };

      void parse (const char *buf, int buflen, int order_count, CssOrigin origin);
      void startElement (int tag);
      void setId (const char *id);
      void setClass (const char *klass);
      void setStyle (const char *style);
      void endElement (int tag);
      void setPseudoLink ();
      void setPseudoVisited ();
      void setNonCssHints (CssPropertyList *nonCssHints);
      void inheritBackgroundColor (); /* \todo get rid of this somehow */

      inline dw::core::style::Style *style () {
         dw::core::style::Style *s = stack->getRef (stack->size () - 1)->style;
         if (s)
            return s;
         else
            return style0 ();
      };
      inline dw::core::style::Style *wordStyle () {
         dw::core::style::Style *s = stack->getRef (stack->size () - 1)->wordStyle;
         if (s)
            return s;
         else
            return wordStyle0 ();
      };

};

#endif
