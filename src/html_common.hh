#ifndef __HTML_COMMON_HH__
#define __HTML_COMMON_HH__

#include "url.h"
#include "bw.h"

#include "lout/misc.hh"
#include "dw/core.hh"
#include "dw/image.hh"
#include "dw/style.hh"

#include "image.hh"

#include "form.hh"

#include "styleengine.hh"

/*
 * Macros 
 */

// Dw to Textblock
#define DW2TB(dw)  ((Textblock*)dw)
// "html struct" to "Layout"
#define HT2LT(html)  ((Layout*)html->bw->render_layout)
// "Image" to "Dw Widget"
#define IM2DW(Image)  ((Widget*)Image->dw)
// Top of the parsing stack
#define S_TOP(html)  (html->stack->getRef(html->stack->size()-1))

// Add a bug-meter message.
#define BUG_MSG(...)                               \
   D_STMT_START {                                  \
         html->bugMessage(__VA_ARGS__);            \
   } D_STMT_END

/*
 * Change one toplevel attribute. var should be an identifier. val is
 * only evaluated once, so you can safely use a function call for it.
 */
#if 0
#define HTML_SET_TOP_ATTR(html, var, val) \
   do { \
      StyleAttrs style_attrs; \
      Style *old_style; \
       \
      old_style = S_TOP(html)->style; \
      style_attrs = *old_style; \
      style_attrs.var = (val); \
      S_TOP(html)->style = \
         Style::create (HT2LT(html), &style_attrs); \
      old_style->unref (); \
   } while (FALSE)
#else
#define HTML_SET_TOP_ATTR(html, var, val) 
#endif
/*
 * Typedefs 
 */

typedef struct _DilloLinkImage   DilloLinkImage;
typedef struct _DilloHtmlState   DilloHtmlState;

typedef enum {
   DT_NONE,           
   DT_HTML,           
   DT_XHTML
} DilloHtmlDocumentType;

typedef enum {
   DILLO_HTML_PARSE_MODE_INIT = 0,
   DILLO_HTML_PARSE_MODE_STASH,
   DILLO_HTML_PARSE_MODE_STASH_AND_BODY,
   DILLO_HTML_PARSE_MODE_VERBATIM,
   DILLO_HTML_PARSE_MODE_BODY,
   DILLO_HTML_PARSE_MODE_PRE
} DilloHtmlParseMode;

typedef enum {
   DILLO_HTML_TABLE_MODE_NONE,  /* no table at all */
   DILLO_HTML_TABLE_MODE_TOP,   /* outside of <tr> */
   DILLO_HTML_TABLE_MODE_TR,    /* inside of <tr>, outside of <td> */
   DILLO_HTML_TABLE_MODE_TD     /* inside of <td> */
} DilloHtmlTableMode;

typedef enum {
   HTML_LIST_NONE,
   HTML_LIST_UNORDERED,
   HTML_LIST_ORDERED
} DilloHtmlListMode;

typedef enum {
   IN_NONE        = 0,
   IN_HTML        = 1 << 0,
   IN_HEAD        = 1 << 1,
   IN_BODY        = 1 << 2,
   IN_FORM        = 1 << 3,
   IN_SELECT      = 1 << 4,
   IN_OPTION      = 1 << 5,
   IN_TEXTAREA    = 1 << 6,
   IN_MAP         = 1 << 7,
   IN_PRE         = 1 << 8,
   IN_BUTTON      = 1 << 9,
   IN_LI          = 1 << 10,
} DilloHtmlProcessingState;

/*
 * Data Structures 
 */

struct _DilloLinkImage {
   DilloUrl *url;
   DilloImage *image;
};

struct _DilloHtmlState {
   CssPropertyList *table_cell_props;
   DilloHtmlParseMode parse_mode;
   DilloHtmlTableMode table_mode;
   bool cell_text_align_set;
   DilloHtmlListMode list_type;
   int list_number;

   /* TagInfo index for the tag that's being processed */
   int tag_idx;

   dw::core::Widget *textblock, *table;

   /* This is used to align list items (especially in enumerated lists) */
   dw::core::Widget *ref_list_item;

   /* This makes image processing faster than a function
      a_Dw_widget_get_background_color. */
   int32_t current_bg_color;

   /* This is used for list items etc; if it is set to TRUE, breaks
      have to be "handed over" (see Html_add_indented and
      Html_eventually_pop_dw). */
   bool hand_over_break;
};

/*
 * Classes 
 */

class DilloHtml {
private:
   class HtmlLinkReceiver: public dw::core::Widget::LinkReceiver {
   public:
      DilloHtml *html;

      bool enter (dw::core::Widget *widget, int link, int img, int x, int y);
      bool press (dw::core::Widget *widget, int link, int img, int x, int y,
                  dw::core::EventButton *event);
      bool click (dw::core::Widget *widget, int link, int img, int x, int y,
                  dw::core::EventButton *event);
   };
   HtmlLinkReceiver linkReceiver;

public:  //BUG: for now everything is public

   BrowserWindow *bw;
   DilloUrl *page_url, *base_url;
   dw::core::Widget *dw;    /* this is duplicated in the stack */

   /* -------------------------------------------------------------------*/
   /* Variables required at parsing time                                 */
   /* -------------------------------------------------------------------*/
   size_t Buf_Consumed; /* amount of source from cache consumed */
   char *Start_Buf;
   int Start_Ofs;
   char *content_type, *charset;
   bool stop_parser;

   bool repush_after_head;

   size_t CurrTagOfs;
   size_t OldTagOfs, OldTagLine;

   DilloHtmlDocumentType DocType; /* as given by DOCTYPE tag */
   float DocTypeVersion;          /* HTML or XHTML version number */

   lout::misc::SimpleVector<DilloHtmlState> *stack;
   StyleEngine *styleEngine;

   int InFlags; /* tracks which elements we are in */

   Dstr *Stash;
   bool StashSpace;

   int pre_column;        /* current column, used in PRE tags with tabs */
   bool PreFirstChar;     /* used to skip the first CR or CRLF in PRE tags */
   bool PrevWasCR;        /* Flag to help parsing of "\r\n" in PRE tags */
   bool PrevWasOpenTag;   /* Flag to help deferred parsing of white space */
   bool PrevWasSPC;       /* Flag to help handling collapsing white space */
   bool InVisitedLink;    /* used to 'contrast_visited_colors' */
   bool ReqTagClose;      /* Flag to help handling bad-formed HTML */
   bool WordAfterLI;      /* Flag to help ignoring the 1st <P> after <LI> */
   bool TagSoup;          /* Flag to enable the parser's cleanup functions */
   char *NameVal;         /* used for validation of "NAME" and "ID" in <A> */

   /* element counters: used for validation purposes */
   uchar_t Num_HTML, Num_HEAD, Num_BODY, Num_TITLE;

   Dstr *attr_data;       /* Buffer for attribute value */

   int32_t link_color;
   int32_t visited_color;

   /* -------------------------------------------------------------------*/
   /* Variables required after parsing (for page functionality)          */
   /* -------------------------------------------------------------------*/
   lout::misc::SimpleVector<DilloHtmlForm*> *forms;
   lout::misc::SimpleVector<DilloHtmlInput*> *inputs_outside_form;
   lout::misc::SimpleVector<DilloUrl*> *links;
   lout::misc::SimpleVector<DilloLinkImage*> *images;
   dw::ImageMapsList maps;

private:
   void freeParseData();
   void initDw();  /* Used by the constructor */

public:
   DilloHtml(BrowserWindow *bw, const DilloUrl *url, const char *content_type);
   ~DilloHtml();
   void bugMessage(const char *format, ... );
   void connectSignals(dw::core::Widget *dw);
   void write(char *Buf, int BufSize, int Eof);
   int getCurTagLineNumber();
   void finishParsing(int ClientKey);
   int formNew(DilloHtmlMethod method, const DilloUrl *action,
               DilloHtmlEnc enc, const char *charset);
   DilloHtmlForm *getCurrentForm ();
   bool_t unloadedImages();
   void loadImages (const DilloUrl *pattern);
};

/*
 * Parser functions 
 */

int a_Html_tag_index(const char *tag);

const char *a_Html_get_attr(DilloHtml *html,
                            const char *tag,
                            int tagsize,
                            const char *attrname);

char *a_Html_get_attr_wdef(DilloHtml *html,
                           const char *tag,
                           int tagsize,
                           const char *attrname,
                           const char *def);

DilloUrl *a_Html_url_new(DilloHtml *html,
                         const char *url_str, const char *base_url,
                         int use_base_url);

DilloImage *a_Html_add_new_image(DilloHtml *html, const char *tag,
                                 int tagsize, DilloUrl *url,
                                 bool add);

char *a_Html_parse_entities(DilloHtml *html, const char *token, int toksize);
void a_Html_pop_tag(DilloHtml *html, int TagIdx);
void a_Html_stash_init(DilloHtml *html);
int32_t a_Html_color_parse(DilloHtml *html,
                           const char *subtag, int32_t default_color);
dw::core::style::Length a_Html_parse_length (DilloHtml *html,
                                             const char *attr);
void a_Html_tag_set_align_attr(DilloHtml *html, CssPropertyList *props,
                               const char *tag, int tagsize);
bool a_Html_tag_set_valign_attr(DilloHtml *html,
                                const char *tag, int tagsize,
                                CssPropertyList *props);

#endif /* __HTML_COMMON_HH__ */
