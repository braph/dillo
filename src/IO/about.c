/*
 * File: about.c
 *
 * Copyright (C) 1999-2007 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <config.h>

/*
 * HTML text for startup screen
 */
const char *const AboutSplash=
"<!DOCTYPE HTML PUBLIC '-//W3C//DTD HTML 4.01 Transitional//EN'>\n"
"<html>\n"
"<head>\n"
"<title>Splash screen for dillo-" VERSION "</title>\n"
"</head>\n"
"<body bgcolor='#778899' text='#000000' link='#000000' vlink='#000000'>\n"
"\n"
"\n"
"<!--   the head of the page   -->\n"
"\n"
"<table width='100%' border='0' cellspacing='1' cellpadding='3'>\n"
" <tr><td>\n"
"  <table border='1' cellspacing='1' cellpadding='0'>\n"
"   <tr>\n"
"   <td bgcolor='#000000'>\n"
"    <table width='100%' border='0' bgcolor='#ffffff'>\n"
"    <tr>\n"
"     <td valign='top' align='left'>\n"
"      <h1>&nbsp;Welcome to Dillo " VERSION "&nbsp;</h1>\n"
"    </table>\n"
"  </table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"\n"
"<!-- the main layout table, definition -->\n"
"\n"
"<table width='100%' border='0' cellspacing='0' cellpadding='0'>\n"
"<tr><td valign='top' width='150' align='center'>\n"
"\n"
"\n"
"<!--   The navigation bar   -->\n"
"\n"
"<table border='0' cellspacing='0' cellpadding='0' width='140' bgcolor='#000000'>\n"
"<tr>\n"
" <td>\n"
"  <table width='100%' border='0' cellspacing='1' cellpadding='3'>\n"
"  <tr>\n"
"   <td colspan='1' bgcolor='#CCCCCC'>Dillo\n"
"  <tr>\n"
"   <td bgcolor='#FFFFFF'>\n"
"    <table border='0' cellspacing='0' cellpadding='5'><tr><td>\n"
"    <table border='0' cellspacing='0' cellpadding='2'><tr>\n"
"    <td>\n"
"    <td>\n"
"     <a href='http://www.dillo.org/dillo2-help.html'>\n"
"     Help</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.dillo.org/'>Home</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.dillo.org/funding/objectives.html'>\n"
"     Objectives</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://hg.dillo.org/dillo/file/tip/ChangeLog'>\n"
"     ChangeLog</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.dillo.org/interview.html'>\n"
"       Interview</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.dillo.org/D_authors.html'>\n"
"     Authors</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.dillo.org/donations.html'>\n"
"     Donate</a>\n"
"    </table>\n"
"    </table>\n"
"  </table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellspacing='0' cellpadding='0' width='140' bgcolor='#000000'>\n"
"<tr>\n"
" <td>\n"
"  <table width='100%' border='0' cellspacing='1' cellpadding='3'>\n"
"  <tr>\n"
"    <td colspan='1' bgcolor='#CCCCCC'>News\n"
"\n"
"  <tr>\n"
"   <td bgcolor='#FFFFFF'>\n"
"    <table border='0' cellspacing='0' cellpadding='5'><tr><td>\n"
"    <table border='0' cellpadding='2'>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://lwn.net/'>LWN</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://slashdot.org/'>Slashdot</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.linux.org.uk/Portaloo.cs'>Linux.org.uk</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.commondreams.org/'>C.&nbsp;Dreams</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.voltairenet.org/'>VoltaireNet</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td>\n"
"     <a href='http://www.nexusmagazine.com/'>Nexus&nbsp;M.</a>\n"
"    </table>\n"
"    </table>\n"
"  </table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellspacing='0' cellpadding='0' width='140' bgcolor='#000000'>\n"
"<tr>\n"
" <td>\n"
"  <table width='100%' border='0' cellspacing='1' cellpadding='3'>\n"
"  <tr>\n"
"   <td colspan='1' bgcolor='#CCCCCC'>Additional Stuff\n"
"\n"
"  <tr>\n"
"   <td bgcolor='#FFFFFF'>\n"
"    <table border='0' cellspacing='0' cellpadding='5'><tr><td>\n"
"    <table border='0' cellpadding='2'><tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://www.google.com/'>Google</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://www.wikipedia.org/'>Wikipedia</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://www.gutenberg.org/'>P.&nbsp;Gutenberg</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://freshmeat.net/'>FreshMeat</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://www.gnu.org/gnu/thegnuproject.html'>GNU\n"
"     project</a>\n"
"    <tr>\n"
"    <td>&nbsp;&nbsp;\n"
"    <td><a href='http://www.linuxfund.org/'>LinuxFund</a>\n"
"    </table>\n"
"    </table>\n"
"  </table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellspacing='0' cellpadding='0' width='140' bgcolor='#000000'>\n"
"<tr>\n"
" <td>\n"
"   <table width='100%' border='0' cellspacing='1' cellpadding='3'>\n"
"   <tr>\n"
"    <td colspan='1' bgcolor='#CCCCCC'>Essential Readings\n"
"\n"
"   <tr>\n"
"    <td bgcolor='#FFFFFF'>\n"
"     <table border='0' cellspacing='0' cellpadding='5'><tr><td>\n"
"     <table border='0' cellpadding='2'>\n"
"     <tr><td>&nbsp;&nbsp;\n"
"     <td><a href='http://www.violence.de'>Peace&amp;Violence</a>\n"
"     <tr><td>&nbsp;&nbsp;\n"
"     <td><a href='http://www.gnu.org/philosophy/right-to-read.html'>\n"
"      Right to Read</a>\n"
"     </table>\n"
"     </table>\n"
"   </table>\n"
"</table>\n"
"\n"
"<table border='0' width='100%' cellpadding='0' cellspacing='0'><tr><td height='10'></table>\n"
"\n"
"\n"
"<!-- the main layout table, a small vertical spacer -->\n"
"\n"
"<td width='20'><td valign='top'>\n"
"\n"
"\n"
"<!--   Main Part of the page   -->\n"
"\n"
"<table border='0' cellpadding='0' cellspacing='0' align='center' bgcolor='#000000' width='100%'><tr><td>\n"
"<table border='0' cellpadding='5' cellspacing='1' width='100%'>\n"
"<tr>\n"
" <td bgcolor='#CCCCCC'>\n"
"  <h4>Free Software</h4>\n"
"<tr>\n"
" <td bgcolor='#FFFFFF'>\n"
"  <table border='0' cellspacing='0' cellpadding='5'><tr><td>\n"
"  <p>\n"
"  Dillo is Free Software under the terms of version 3 of the\n"
"  <a href='http://www.gnu.org/licenses/gpl.html'>GPL</a>.\n"
"  This means you have four basic freedoms:\n"
"  <ul>\n"
"   <li>Freedom to use the program any way you see fit.\n"
"   <li>Freedom to study and modify the source code.\n"
"   <li>Freedom to make backup copies.\n"
"   <li>Freedom to redistribute it.\n"
"  </ul>\n"
"  The GPL is the legal mechanism that gives you these freedoms.\n"
"  It also protects you from having them taken away: any derivative work\n"
"  based on the program must be under GPLv3 as well.<br>\n"
"  </table>\n"
"</table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellpadding='0' cellspacing='0' align='center' bgcolor='#000000' width='100%'><tr><td>\n"
"<table border='0' cellpadding='5' cellspacing='1' width='100%'>\n"
"<tr>\n"
" <td bgcolor='#CCCCCC'>\n"
"  <h4>Release overview</h4>\n"
"  July 3, 2009\n"
"<tr>\n"
" <td bgcolor='#FFFFFF'>\n"
"  <table border='0' cellspacing='0' cellpadding='5'>\n"
"  <tr>\n"
"   <td>\n"
"<p>\n"
"This release of Dillo improves image size checking. It builds upon version\n"
"2.1, which brought significant improvements such as basic CSS support and\n"
"configurable keybindings.\n"
"<p>\n"
"Remember that the dillo project uses a release model where every new\n"
"version shall be better than the last.\n"
"<EM>Keep up with the latest one!</EM>\n"
"  </table>\n"
"</table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellpadding='0' cellspacing='0' align='center' bgcolor='#000000' width='100%'><tr><td>\n"
"<table border='0' cellpadding='5' cellspacing='1' width='100%'>\n"
"<tr>\n"
" <td bgcolor='#CCCCCC'>\n"
"  <h4>ChangeLog highlights</h4>\n"
"  (Extracted from the\n"
"  <a href='http://hg.dillo.org/dillo/file/tip/ChangeLog'>full\n"
"  ChangeLog</a>)\n"
"<tr>\n"
" <td bgcolor='#FFFFFF'>\n"
"  <table border='0' cellspacing='0' cellpadding='5'>\n"
"  <tr>\n"
"   <td>\n"
"2.1.1:<br>\n"
"<ul>\n"
"<li>Add additional size checks for images.\n"
"<li>Added support for css colors of the form rgb(255, 255, 255).\n"
"<li>Added the 'nop' keybinding (nop = NO_OPERATION; cancels a default hook).\n"
"<li>Added 'stop' key action (not bound by default).\n"
"<li>Reduced 'warning: ignoring return value of ...'\n"
"</ul>\n"
"2.1:<br>\n"
"<ul>\n"
"<li>Implemented basic CSS infrastructure!\n"
"<li>Read user style from ~/.dillo/style.css.\n"
"<li>Added configurable keybindings! (in ~/.dillo/keysrc)\n"
"<li>Implemented \"search previous\" in string searches.\n"
"<li>Ported the command line interface from dillo1\n"
"<li>Set middle click to submit in a new TAB. (Helps to keep form data!)\n"
"<li>Implemented Basic authentication!\n"
"<li>Implemented a close-tab button for the GUI.\n"
"<li>Implemented a tools menu.\n"
"<li>Added dillo(1) man page.\n"
"<li>Added \"font_max_size\", \"font_min_size\" dillorc options.\n"
"<li>Added instant client-side redirects (aka. zero-delay META refresh).\n"
"<li>Proxy support for HTTPS.\n"
"<li>Updated the URL resolver to comply with RFC-3986.\n"
"<li>Fixed Bookmarks modify's HTML so it wraps nicely on handhelds.\n"
"<li>Made cookierc parsing more robust.\n"
"<li>Fix: recover page focus when clicking outside of a widget.\n"
"<li>Added support for the Q element. BUG#343\n"
"<li>Added a right-click menu to form controls (show hiddens, submit, reset)\n"
"<li>Added the \"http_language\" dillorc option for setting HTTP's Accept-Language.\n"
"<li>Replace image loading button and page menu option with a tools menu option.\n"
"<li>Implemented the \"overline\" text-decoration.\n"
"<li>Enhanced and cleaned up text decorations for SUB and SUP.\n"
"<li>Added \"View Stylesheets\" to the page menu.\n"
"<li>System config files have moved to sysconfdir/dillo/\n"
"<li>Allowed compilation with older machines by removing a few C99isms.\n"
"<li>Switched SSL-enabled to configure.in (./configure --enable-ssl).\n"
"<li>Removed redundant caller NULL checks already in the API.\n"
"<li>Added use of inttypes.h when stdint.h isn't found.\n"
"<li>Made the parser recognize \"[^ ]/>\"-terminated XML elements.\n"
"<li>Brought in Sebastian's CSS parser from dillo-0.8.0-css-3.\n"
"<li>Support CSS @import directive.\n"
"<li>Improved CSS selector matching performance using hash tables.\n"
"<li>Added support for descendant and child selectors.\n"
"<li>Support selector specificity.\n"
"<li>Replace bg_color dillorc option.\n"
"<li>Remove text_color, link_color, and force_my_colors dillorc options.\n"
"<li>Replace visited_color dillorc option.\n"
"<li>Allow negative values for specific CSS properties only.\n"
"<li>Disable negative margins for now as dw/* does not support them yet.\n"
"<li>Disable form widgets while stylesheets are loading.\n"
"<li>Implement --xid command line option (used by claws mail client).\n"
"<li>Added the \"middle_click_drags_page\" dillorc option.\n"
"<li>Set the File menu label to hide when the File menu-button is shown.\n"
"<li>Made a big cleanup of cache.c WRT charset decoding (fixes bugs).\n"
"<li>Made an extensive cleanup/fixup of the whole image handling process.\n"
"<li>Fixed handling of META's content-type with no MIME type (e.g. only charset).\n"
"<li>Added support for a quoted URL in META refresh.\n"
"<li>Updated the GPL copyright note in the source files.\n"
"</ul>\n"
"  </table>\n"
"</table>\n"
"</table>\n"
"\n"
"<br>\n"
"\n"
"<table border='0' cellpadding='0' cellspacing='0' align='center' bgcolor='#000000' width='100%'><tr><td>\n"
"<table border='0' cellpadding='5' cellspacing='1' width='100%'>\n"
"<tr>\n"
" <td bgcolor='#CCCCCC'>\n"
"  <h4>Notes</h4>\n"
"<tr>\n"
" <td bgcolor='#FFFFFF'>\n"
"  <table border='0' cellspacing='0' cellpadding='5'>\n"
"  <tr>\n"
"   <td>\n"
"<ul>\n"
" <li> There's a\n"
"   <a href='http://www.dillo.org/dillorc'>dillorc</a>\n"
"   (readable  config)  file within the tarball; It is well-commented\n"
"   and  has  plenty  of  options to customize dillo, so <STRONG>copy\n"
"   it</STRONG>  to  your  <STRONG>~/.dillo/</STRONG>  directory, and\n"
"   modify it to your taste.\n"
" <li> Documentation for developers is in the <CODE>/doc</CODE>\n"
"   dir  within  the  tarball;  you can find directions on everything\n"
"   else at the home page.\n"
" <li> The right mouse button brings up a context-sensitive menu\n"
"   (available on pages, links, images, forms, the Back and Forward buttons,\n"
"    and the bug meter).\n"
" <li> Dillo behaves very nicely when browsing local files, images, and HTML.\n"
"   It's also very good for Internet searching.\n"
" <li> This release is mainly intended <strong>for developers</strong>\n"
"   and <em>advanced users</em>.\n"
" <li> Frames, Java and Javascript are not supported.\n"
"</ul>\n"
"<br>\n"
"  </table>\n"
"</table>\n"
"</table>\n"
"\n"
"<table border='0' width='100%' cellpadding='0' cellspacing='0'><tr><td height='10'></table>\n"
"\n"
"\n"
"<!-- the main layout table, a small vertical spacer -->\n"
"\n"
"<td width='20'>\n"
"\n"
"\n"
"\n"
"<!--   The right column (info)   -->\n"
"<td valign='top' align='center'>\n"
"\n"
"\n"
"\n"
"<!-- end of the main layout table -->\n"
"\n"
"\n"
"</table>\n"
"\n"
"<!--   footnotes   -->\n"
"\n"
"<br><br><center>\n"
"<hr size='2'>\n"
"<hr size='2'>\n"
"</center>\n"
"</body>\n"
"</html>\n";

