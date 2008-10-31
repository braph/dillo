/*
 * File: css.cc
 *
 * Copyright 2008 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include "prefs.h"
#include "html_common.hh"
#include "css.hh"

/** \todo dummy only */
CssPropertyList *CssPropertyList::parse (const char *buf) {
   return NULL;
}

void CssPropertyList::set (CssProperty::Name name, CssProperty::Value value) {
   for (int i = 0; i < size (); i++)
      if (getRef (i)->name == name) {
         getRef (i)->value = value;
         return;
      }

   increase ();
   getRef (size () - 1)->name = name;
   getRef (size () - 1)->value = value;
}

void CssPropertyList::apply (CssPropertyList *props) {
   for (int i = 0; i < size (); i++)
      props->set (getRef (i)->name, getRef (i)->value);
}

/** \todo dummy only */
CssSelector *CssSelector::parse (const char *buf) {
   return NULL;
}

/** \todo dummy only */
bool CssSelector::match (Doctree *docTree) {
   return tag < 0 || tag == docTree->top ()->tag;
}

void CssRule::apply (CssPropertyList *props, Doctree *docTree) {
   if (selector->match (docTree))
      this->props->apply (props);
}

void CssStyleSheet::addRule (CssSelector *selector, CssPropertyList *props) {
   CssRule *rule = new CssRule (selector, props);
   increase ();
   set (size () - 1, rule);
}

void CssStyleSheet::apply (CssPropertyList *props, Doctree *docTree) {
   for (int i = 0; i < size (); i++)
      get (i)->apply (props, docTree);
}

CssStyleSheet *CssContext::userAgentStyle;
CssStyleSheet *CssContext::userStyle;
CssStyleSheet *CssContext::userImportantStyle;

CssContext::CssContext () {
   for (int o = CSS_PRIMARY_USER_AGENT; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      sheet[o] = NULL;

   if (userAgentStyle == NULL) {
      userAgentStyle = buildUserAgentStyle ();
      userStyle = buildUserStyle (false);
      userImportantStyle = buildUserStyle (true);
   }

   sheet[CSS_PRIMARY_USER_AGENT] = userAgentStyle;
   sheet[CSS_PRIMARY_USER] = userStyle;
   sheet[CSS_PRIMARY_USER_IMPORTANT] = userImportantStyle;
}

void CssContext::apply (CssPropertyList *props, Doctree *docTree,
         CssPropertyList *tagStyle, CssPropertyList *nonCss) {

   for (int o = CSS_PRIMARY_USER_AGENT; o <= CSS_PRIMARY_USER; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (nonCss)
        nonCss->apply (props);

   for (int o = CSS_PRIMARY_AUTHOR; o <= CSS_PRIMARY_USER_IMPORTANT; o++)
      if (sheet[o])
         sheet[o]->apply (props, docTree);

   if (tagStyle)
        tagStyle->apply (props);
}

CssStyleSheet * CssContext::buildUserAgentStyle () {
   CssStyleSheet *s = new CssStyleSheet ();
   CssPropertyList *props;
   CssProperty::Value v;

   // <a>
   props = new CssPropertyList ();
   v.color = 0x0000ff;
   props->set (CssProperty::CSS_PROPERTY_COLOR, v);
   v.textDecoration = dw::core::style::TEXT_DECORATION_UNDERLINE;
   props->set (CssProperty::CSS_PROPERTY_TEXT_DECORATION, v);
   v.cursor = dw::core::style::CURSOR_POINTER;
   props->set (CssProperty::CSS_PROPERTY_CURSOR, v);
   s->addRule (new CssSelector(a_Html_tag_index("a"), NULL, NULL), props);

   // <b>
   props = new CssPropertyList ();
   v.weight = 700;
   props->set (CssProperty::CSS_PROPERTY_FONT_WEIGHT, v);
   s->addRule (new CssSelector(a_Html_tag_index("b"), NULL, NULL), props);

   // <i>
   props = new CssPropertyList ();
   v.fontStyle = dw::core::style::FONT_STYLE_ITALIC;
   props->set (CssProperty::CSS_PROPERTY_FONT_STYLE, v);
   s->addRule (new CssSelector(a_Html_tag_index("i"), NULL, NULL), props);

   // <h1>
   props = new CssPropertyList ();
   v.size = 40;
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, v);
   s->addRule (new CssSelector(a_Html_tag_index("h1"), NULL, NULL), props);

   // <h2>
   props = new CssPropertyList ();
   v.size = 30;
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, v);
   s->addRule (new CssSelector(a_Html_tag_index("h2"), NULL, NULL), props);

   // <h3>
   props = new CssPropertyList ();
   v.size = 20;
   props->set (CssProperty::CSS_PROPERTY_FONT_SIZE, v);
   s->addRule (new CssSelector(a_Html_tag_index("h3"), NULL, NULL), props);

   // <ol>
   props = new CssPropertyList ();
   v.listStyleType = dw::core::style::LIST_STYLE_TYPE_DECIMAL;
   props->set (CssProperty::CSS_PROPERTY_LIST_STYLE_TYPE, v);
   s->addRule (new CssSelector(a_Html_tag_index("ol"), NULL, NULL), props);

   // <pre>
   props = new CssPropertyList ();
   v.name = "DejaVu Sans Mono";
   props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, v);
   s->addRule (new CssSelector(a_Html_tag_index("pre"), NULL, NULL), props);
   
   return s;
}

CssStyleSheet * CssContext::buildUserStyle (bool important) {
   CssStyleSheet *s = new CssStyleSheet ();
   CssPropertyList *props;
   CssProperty::Value v;

   if (! important) {
      // <a>
      props = new CssPropertyList ();
      v.color = prefs.link_color;
      props->set (CssProperty::CSS_PROPERTY_COLOR, v);
      s->addRule (new CssSelector(a_Html_tag_index("a"), NULL, NULL), props);

      // <body>
      props = new CssPropertyList ();
      v.name = prefs.vw_fontname;
      props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, v);
      v.color = prefs.text_color;
      props->set (CssProperty::CSS_PROPERTY_COLOR, v);
      s->addRule (new CssSelector(a_Html_tag_index("body"), NULL, NULL), props);

      // <pre>
      props = new CssPropertyList ();
      v.name = prefs.fw_fontname;
      props->set (CssProperty::CSS_PROPERTY_FONT_FAMILY, v);
      s->addRule (new CssSelector(a_Html_tag_index("pre"), NULL, NULL), props);
   }

   return s;
}
