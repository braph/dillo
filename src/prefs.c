/*
 * Preferences for dillo
 *
 * Copyright (C) 2006-2007 Jorge Arellano Cid <jcid@dillo.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>       /* for strchr */
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>       /* for setlocale */
#include <ctype.h>        /* for isspace */
#include "prefs.h"
#include "colors.h"
#include "misc.h"
#include "msg.h"

#define RCNAME "dillorc"

#define DILLO_START_PAGE "about:splash"
#define DILLO_HOME "http://www.dillo.org/"

#define D_VW_FONTNAME "DejaVu Sans"
#define D_FW_FONTNAME "DejaVu Sans Mono"
#define D_SEARCH_URL "http://www.google.com/search?ie=UTF-8&oe=UTF-8&q=%s"
#define D_SAVE_DIR "/tmp/"

#define DW_COLOR_DEFAULT_BGND   0xdcd1ba
#define DW_COLOR_DEFAULT_TEXT   0x000000
#define DW_COLOR_DEFAULT_LINK   0x0000ff
#define DW_COLOR_DEFAULT_VLINK  0x800080

/*-----------------------------------------------------------------------------
 * Global Data
 *---------------------------------------------------------------------------*/
DilloPrefs prefs;

/*-----------------------------------------------------------------------------
 * Local types
 *---------------------------------------------------------------------------*/

/* define enumeration values to be returned for specific symbols */
typedef enum {
   DRC_TOKEN_MIDDLE_CLICK_DRAGS_PAGE,
   DRC_TOKEN_ALLOW_WHITE_BG,
   DRC_TOKEN_BG_COLOR,
   DRC_TOKEN_CONTRAST_VISITED_COLOR,
   DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT,
   DRC_TOKEN_FOCUS_NEW_TAB,
   DRC_TOKEN_FONT_FACTOR,
   DRC_TOKEN_FORCE_MY_COLORS,
   DRC_TOKEN_FULLWINDOW_START,
   DRC_TOKEN_FW_FONT,
   DRC_TOKEN_GENERATE_SUBMIT,
   DRC_TOKEN_GEOMETRY,
   DRC_TOKEN_HOME,
   DRC_TOKEN_LIMIT_TEXT_WIDTH,
   DRC_TOKEN_LINK_COLOR,
   DRC_TOKEN_LOAD_IMAGES,
   DRC_TOKEN_BUFFERED_DRAWING,
   DRC_TOKEN_MIDDLE_CLICK_OPENS_NEW_TAB,
   DRC_TOKEN_NOPROXY,
   DRC_TOKEN_PANEL_SIZE,
   DRC_TOKEN_PROXY,
   DRC_TOKEN_PROXYUSER,
   DRC_TOKEN_REFERER,
   DRC_TOKEN_SAVE_DIR,
   DRC_TOKEN_SEARCH_URL,
   DRC_TOKEN_SHOW_BACK,
   DRC_TOKEN_SHOW_BOOKMARKS,
   DRC_TOKEN_SHOW_CLEAR_URL,
   DRC_TOKEN_SHOW_EXTRA_WARNINGS,
   DRC_TOKEN_SHOW_FORW,
   DRC_TOKEN_SHOW_HOME,
   DRC_TOKEN_SHOW_FILEMENU,
   DRC_TOKEN_SHOW_MSG,
   DRC_TOKEN_SHOW_PROGRESS_BOX,
   DRC_TOKEN_SHOW_RELOAD,
   DRC_TOKEN_SHOW_SAVE,
   DRC_TOKEN_SHOW_SEARCH,
   DRC_TOKEN_SHOW_STOP,
   DRC_TOKEN_SHOW_TOOLTIP,
   DRC_TOKEN_SHOW_URL,
   DRC_TOKEN_SMALL_ICONS,
   DRC_TOKEN_STANDARD_WIDGET_COLORS,
   DRC_TOKEN_START_PAGE,
   DRC_TOKEN_TEXT_COLOR,
   DRC_TOKEN_VISITED_COLOR,
   DRC_TOKEN_VW_FONT,
   DRC_TOKEN_W3C_PLUS_HEURISTICS
} RcToken_t;

typedef struct SymNode_ SymNode_t;

struct SymNode_ {
   char *name;
   RcToken_t token;
};

/*-----------------------------------------------------------------------------
 * Local data
 *---------------------------------------------------------------------------*/

/* Symbol array, sorted alphabetically */
static const SymNode_t symbols[] = {
   { "allow_white_bg", DRC_TOKEN_ALLOW_WHITE_BG },
   { "bg_color", DRC_TOKEN_BG_COLOR },
   { "buffered_drawing", DRC_TOKEN_BUFFERED_DRAWING },
   { "contrast_visited_color", DRC_TOKEN_CONTRAST_VISITED_COLOR },
   { "enterpress_forces_submit", DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT },
   { "focus_new_tab", DRC_TOKEN_FOCUS_NEW_TAB },
   { "font_factor", DRC_TOKEN_FONT_FACTOR },
   { "force_my_colors", DRC_TOKEN_FORCE_MY_COLORS },
   { "fullwindow_start", DRC_TOKEN_FULLWINDOW_START },
   { "fw_fontname", DRC_TOKEN_FW_FONT },
   { "generate_submit", DRC_TOKEN_GENERATE_SUBMIT },
   { "geometry", DRC_TOKEN_GEOMETRY },
   { "home", DRC_TOKEN_HOME },
   { "http_proxy", DRC_TOKEN_PROXY },
   { "http_proxyuser", DRC_TOKEN_PROXYUSER },
   { "http_referer", DRC_TOKEN_REFERER },
   { "limit_text_width", DRC_TOKEN_LIMIT_TEXT_WIDTH },
   { "link_color", DRC_TOKEN_LINK_COLOR },
   { "load_images", DRC_TOKEN_LOAD_IMAGES },
   { "middle_click_drags_page", DRC_TOKEN_MIDDLE_CLICK_DRAGS_PAGE },
   { "middle_click_opens_new_tab", DRC_TOKEN_MIDDLE_CLICK_OPENS_NEW_TAB },
   { "no_proxy", DRC_TOKEN_NOPROXY },
   { "panel_size", DRC_TOKEN_PANEL_SIZE },
   { "save_dir", DRC_TOKEN_SAVE_DIR },
   { "search_url", DRC_TOKEN_SEARCH_URL },
   { "show_back", DRC_TOKEN_SHOW_BACK },
   { "show_bookmarks", DRC_TOKEN_SHOW_BOOKMARKS },
   { "show_clear_url", DRC_TOKEN_SHOW_CLEAR_URL },
   { "show_extra_warnings", DRC_TOKEN_SHOW_EXTRA_WARNINGS },
   { "show_filemenu", DRC_TOKEN_SHOW_FILEMENU },
   { "show_forw", DRC_TOKEN_SHOW_FORW },
   { "show_home", DRC_TOKEN_SHOW_HOME },
   { "show_msg", DRC_TOKEN_SHOW_MSG },
   { "show_progress_box", DRC_TOKEN_SHOW_PROGRESS_BOX },
   { "show_reload", DRC_TOKEN_SHOW_RELOAD },
   { "show_save", DRC_TOKEN_SHOW_SAVE },
   { "show_search", DRC_TOKEN_SHOW_SEARCH },
   { "show_stop", DRC_TOKEN_SHOW_STOP },
   { "show_tooltip", DRC_TOKEN_SHOW_TOOLTIP },
   { "show_url", DRC_TOKEN_SHOW_URL },
   { "small_icons", DRC_TOKEN_SMALL_ICONS },
   { "standard_widget_colors", DRC_TOKEN_STANDARD_WIDGET_COLORS },
   { "start_page", DRC_TOKEN_START_PAGE },
   { "text_color", DRC_TOKEN_TEXT_COLOR },
   { "visited_color", DRC_TOKEN_VISITED_COLOR, },
   { "vw_fontname", DRC_TOKEN_VW_FONT },
   { "w3c_plus_heuristics", DRC_TOKEN_W3C_PLUS_HEURISTICS }
};

static const uint_t n_symbols = sizeof (symbols) / sizeof (symbols[0]);

/*
 *- Mini parser -------------------------------------------------------------
 */

/*
 * Comparison function for binary search
 */
static int Prefs_symbol_cmp(const void *a, const void *b)
{
   return strcmp(((SymNode_t*)a)->name, ((SymNode_t*)b)->name);
}

/*
 * Parse a name/value pair and set preferences accordingly.
 */
static int Prefs_parse_pair(char *name, char *value)
{
   int st;
   SymNode_t key, *node;

   key.name = name;
   node = bsearch(&key, symbols, n_symbols,
                  sizeof(SymNode_t), Prefs_symbol_cmp);
   if (!node) {
      MSG("prefs: {%s} is not a recognized token.\n", name);
      return -1;
   }

   switch (node->token) {
   case DRC_TOKEN_GEOMETRY:
      a_Misc_parse_geometry(value, &prefs.xpos, &prefs.ypos,
                            &prefs.width, &prefs.height);
      break;
   case DRC_TOKEN_PROXY:
      a_Url_free(prefs.http_proxy);
      prefs.http_proxy = a_Url_new(value, NULL);
      break;
   case DRC_TOKEN_PROXYUSER:
      dFree(prefs.http_proxyuser);
      prefs.http_proxyuser = dStrdup(value);
      break;
   case DRC_TOKEN_REFERER:
      dFree(prefs.http_referer);
      prefs.http_referer = dStrdup(value);
      break;
   case DRC_TOKEN_NOPROXY:
      dFree(prefs.no_proxy);
      prefs.no_proxy = dStrdup(value);
      break;
   case DRC_TOKEN_LINK_COLOR:
      prefs.link_color = a_Color_parse(value, prefs.link_color, &st);
      break;
   case DRC_TOKEN_VISITED_COLOR:
      prefs.visited_color = a_Color_parse(value, prefs.visited_color, &st);
      break;
   case DRC_TOKEN_TEXT_COLOR:
      prefs.text_color = a_Color_parse(value, prefs.text_color, &st);
      break;
   case DRC_TOKEN_BG_COLOR:
      prefs.bg_color = a_Color_parse(value, prefs.bg_color, &st);
      break;
   case DRC_TOKEN_ALLOW_WHITE_BG:
      prefs.allow_white_bg = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_MIDDLE_CLICK_DRAGS_PAGE:
      prefs.middle_click_drags_page = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FORCE_MY_COLORS:
      prefs.force_my_colors = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_CONTRAST_VISITED_COLOR:
      prefs.contrast_visited_color = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_STANDARD_WIDGET_COLORS:
      prefs.standard_widget_colors = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_PANEL_SIZE:
      if (!dStrcasecmp(value, "tiny"))
         prefs.panel_size = P_tiny;
      else if (!dStrcasecmp(value, "small"))
         prefs.panel_size = P_small;
      else if (!dStrcasecmp(value, "medium"))
         prefs.panel_size = P_medium;
      else /* default to "medium" */
         prefs.panel_size = P_medium;
      break;
   case DRC_TOKEN_SMALL_ICONS:
      prefs.small_icons = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_START_PAGE:
      a_Url_free(prefs.start_page);
      prefs.start_page = a_Url_new(value, NULL);
      break;
   case DRC_TOKEN_HOME:
      a_Url_free(prefs.home);
      prefs.home = a_Url_new(value, NULL);
      break;
   case DRC_TOKEN_SHOW_TOOLTIP:
      prefs.show_tooltip = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FOCUS_NEW_TAB:
      prefs.focus_new_tab = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FONT_FACTOR:
      prefs.font_factor = strtod(value, NULL);
      break;
   case DRC_TOKEN_LIMIT_TEXT_WIDTH:
      prefs.limit_text_width = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_W3C_PLUS_HEURISTICS:
      prefs.w3c_plus_heuristics = (strcmp(value,"YES") == 0);
      break;
   case DRC_TOKEN_SHOW_BACK:
      prefs.show_back = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_FORW:
      prefs.show_forw = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_HOME:
      prefs.show_home = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_RELOAD:
      prefs.show_reload = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_SAVE:
      prefs.show_save = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_STOP:
      prefs.show_stop = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_BOOKMARKS:
      prefs.show_bookmarks = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_FILEMENU:
      prefs.show_filemenu = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_CLEAR_URL:
      prefs.show_clear_url = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_URL:
      prefs.show_url = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_SEARCH:
      prefs.show_search = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_PROGRESS_BOX:
      prefs.show_progress_box = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_FULLWINDOW_START:
      prefs.fullwindow_start = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_LOAD_IMAGES:
      prefs.load_images = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_BUFFERED_DRAWING:
      prefs.buffered_drawing = atoi(value);
      break;
   case DRC_TOKEN_FW_FONT:
      dFree(prefs.fw_fontname);
      prefs.fw_fontname = dStrdup(value);
      break;
   case DRC_TOKEN_VW_FONT:
      dFree(prefs.vw_fontname);
      prefs.vw_fontname = dStrdup(value);
      break;
   case DRC_TOKEN_GENERATE_SUBMIT:
      prefs.generate_submit = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_ENTERPRESS_FORCES_SUBMIT:
      prefs.enterpress_forces_submit = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_MIDDLE_CLICK_OPENS_NEW_TAB:
      prefs.middle_click_opens_new_tab = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SEARCH_URL:
      dFree(prefs.search_url);
      prefs.search_url = dStrdup(value);
      break;
   case DRC_TOKEN_SAVE_DIR:
      dFree(prefs.save_dir);
      prefs.save_dir = dStrdup(value);
   case DRC_TOKEN_SHOW_MSG:
      prefs.show_msg = (strcmp(value, "YES") == 0);
      break;
   case DRC_TOKEN_SHOW_EXTRA_WARNINGS:
      prefs.show_extra_warnings = (strcmp(value, "YES") == 0);
      break;
   default:
      MSG_WARN("prefs: {%s} IS recognized but not handled!\n", name);
      break;   /* Not reached */
   }

   return 0;
}

/*
 * Parse dillorc and set the values in the prefs structure.
 */
static int Prefs_parse_dillorc(void)
{
   FILE *F_in;
   char *filename, *line, *name, *value;
   int ret = -1;

   filename = dStrconcat(dGethomedir(), "/.dillo/", RCNAME, NULL);
   if (!(F_in = fopen(filename, "r"))) {
      MSG("prefs: Can't open %s file: %s\n", RCNAME, filename);
      if (!(F_in = fopen(DILLORC_SYS, "r"))) {
         MSG("prefs: Can't open %s file: %s\n", RCNAME, DILLORC_SYS);
         MSG("prefs: Using internal defaults.\n");
      } else {
         MSG("prefs: Using %s\n", DILLORC_SYS);
      }
   }

   if (F_in) {
      /* scan dillorc line by line */
      while ((line = dGetline(F_in)) != NULL) {
         if (dParser_get_rc_pair(&line, &name, &value) == 0) {
            _MSG("{%s}, {%s}\n", name, value);
            Prefs_parse_pair(name, value);
         } else if (line[0] && line[0] != '#' && (!name || !value)) {
            MSG("prefs: Syntax error in %s: name=\"%s\" value=\"%s\"\n",
                RCNAME, name, value);
         }
         dFree(line);
      }
      fclose(F_in);
      ret = 0;
   }
   dFree(filename);

   return ret;
}

/*---------------------------------------------------------------------------*/

void a_Prefs_init(void)
{
   char *old_locale;

   prefs.width = D_GEOMETRY_DEFAULT_WIDTH;
   prefs.height = D_GEOMETRY_DEFAULT_HEIGHT;
   prefs.xpos = D_GEOMETRY_DEFAULT_XPOS;
   prefs.ypos = D_GEOMETRY_DEFAULT_YPOS;
   prefs.http_proxy = NULL;
   prefs.http_proxyuser = NULL;
   prefs.http_referer = dStrdup("host");
   prefs.no_proxy = NULL;
   prefs.link_color = DW_COLOR_DEFAULT_LINK;
   prefs.visited_color = DW_COLOR_DEFAULT_VLINK;
   prefs.bg_color = DW_COLOR_DEFAULT_BGND;
   prefs.text_color = DW_COLOR_DEFAULT_TEXT;
   prefs.start_page = a_Url_new(DILLO_START_PAGE, NULL);
   prefs.home = a_Url_new(DILLO_HOME, NULL);
   prefs.allow_white_bg = TRUE;
   prefs.force_my_colors = FALSE;
   prefs.contrast_visited_color = TRUE;
   prefs.standard_widget_colors = FALSE;
   prefs.show_tooltip = TRUE;
   prefs.panel_size = P_medium;
   prefs.small_icons = FALSE;
   prefs.limit_text_width = FALSE;
   prefs.w3c_plus_heuristics = TRUE;
   prefs.focus_new_tab = TRUE;
   prefs.font_factor = 1.0;
   prefs.show_back=TRUE;
   prefs.show_forw=TRUE;
   prefs.show_home=TRUE;
   prefs.show_reload=TRUE;
   prefs.show_save=TRUE;
   prefs.show_stop=TRUE;
   prefs.show_bookmarks=TRUE;
   prefs.show_filemenu=TRUE;
   prefs.show_clear_url=TRUE;
   prefs.show_url=TRUE;
   prefs.show_search=TRUE;
   prefs.show_progress_box=TRUE;
   prefs.fullwindow_start=FALSE;
   prefs.load_images=TRUE;
   prefs.buffered_drawing=1;
   prefs.vw_fontname = dStrdup(D_VW_FONTNAME);
   prefs.fw_fontname = dStrdup(D_FW_FONTNAME);
   prefs.generate_submit = FALSE;
   prefs.enterpress_forces_submit = FALSE;
   prefs.middle_click_opens_new_tab = TRUE;
   prefs.search_url = dStrdup(D_SEARCH_URL);
   prefs.save_dir = dStrdup(D_SAVE_DIR);
   prefs.show_msg = TRUE;
   prefs.show_extra_warnings = FALSE;
   prefs.middle_click_drags_page = TRUE;

   /* this locale stuff is to avoid parsing problems with float numbers */
   old_locale = dStrdup (setlocale (LC_NUMERIC, NULL));
   setlocale (LC_NUMERIC, "C");

   Prefs_parse_dillorc();

   setlocale (LC_NUMERIC, old_locale);
   dFree (old_locale);

}

/*
 *  Preferences memory-deallocation
 *  (Call this one at exit time)
 */
void a_Prefs_freeall(void)
{
   dFree(prefs.http_proxyuser);
   dFree(prefs.http_referer);
   dFree(prefs.no_proxy);
   a_Url_free(prefs.http_proxy);
   dFree(prefs.fw_fontname);
   dFree(prefs.vw_fontname);
   a_Url_free(prefs.start_page);
   a_Url_free(prefs.home);
   dFree(prefs.search_url);
   dFree(prefs.save_dir);
}
