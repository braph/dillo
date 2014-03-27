/*
 * Dillo Widget
 *
 * Copyright 2005-2007, 2012-2013 Sebastian Geerken <sgeerken@dillo.org>
 *
 * (Parts of this file were originally part of textblock.cc.)
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


#include "textblock.hh"
#include "hyphenator.hh"
#include "../lout/msg.h"
#include "../lout/debug.hh"
#include "../lout/misc.hh"

#include <stdio.h>
#include <math.h>

using namespace lout;

namespace dw {

int Textblock::BadnessAndPenalty::badnessValue (int infLevel)
{
   switch (badnessState) {
   case NOT_STRETCHABLE:
      return infLevel == INF_NOT_STRETCHABLE ? 1 : 0;

   case QUITE_LOOSE:
      return infLevel == INF_LARGE ? 1 : 0;

   case BADNESS_VALUE:
      return infLevel == INF_VALUE ? badness : 0;

   case TOO_TIGHT:
      return infLevel == INF_TOO_TIGHT ? 1 : 0;
   }

   // compiler happiness
   lout::misc::assertNotReached ();
   return 0;
}

int Textblock::BadnessAndPenalty::penaltyValue (int index, int infLevel)
{
   if (penalty[index] == INT_MIN)
      return infLevel == INF_PENALTIES ? -1 : 0;
   else if (penalty[index] == INT_MAX)
      return infLevel == INF_PENALTIES ? 1 : 0;
   else
      return  infLevel == INF_VALUE ? penalty[index] : 0;
}

void Textblock::BadnessAndPenalty::calcBadness (int totalWidth, int idealWidth,
                                                int totalStretchability,
                                                int totalShrinkability)
{
#ifdef DEBUG
   this->totalWidth = totalWidth;
   this->idealWidth = idealWidth;
   this->totalStretchability = totalStretchability;
   this->totalShrinkability = totalShrinkability;
#endif

   ratio = 0; // because this is used in print()

   if (totalWidth == idealWidth) {
      badnessState = BADNESS_VALUE;
      badness = 0;
   } else if (totalWidth < idealWidth) {
      if (totalStretchability == 0)
         badnessState = NOT_STRETCHABLE;
      else {
         ratio = 100 * (idealWidth - totalWidth) / totalStretchability;
         if (ratio > 1024)
            badnessState = QUITE_LOOSE;
         else {
            badnessState = BADNESS_VALUE;
            badness = ratio * ratio * ratio;
         }
      }
   } else { // if (totalWidth > availWidth)
      if (totalShrinkability == 0)
         badnessState = TOO_TIGHT;
      else {
         // ratio is negative here
         ratio = 100 * (idealWidth - totalWidth) / totalShrinkability;
         if (ratio <= - 100)
            badnessState = TOO_TIGHT;
         else {
            badnessState = BADNESS_VALUE;
            badness = - ratio * ratio * ratio;
         }
      }
   }
}

/**
 * Sets the penalty, multiplied by 100. Multiplication is necessary
 * to deal with fractional numbers, without having to use floating
 * point numbers. So, to set a penalty to 0.5, pass 50.
 *
 * INT_MAX and INT_MIN (representing inf and -inf, respectively) are
 * also allowed.
 *
 * The definition of penalties depends on the definition of badness,
 * which adheres to the description in \ref dw-line-breaking, section
 * "Criteria for Line-Breaking". The exact calculation may vary, but
 * this definition of should be rather stable: (i)&nbsp;A perfectly
 * fitting line has a badness of 0. (ii)&nbsp;A line, where all spaces
 * are extended by exactly the stretchability, as well as a line, where
 * all spaces are reduced by the shrinkability, have a badness of 1.
 *
 * (TODO plural: penalties, not penalty. Correct above comment)
 */
void Textblock::BadnessAndPenalty::setPenalties (int penalty1, int penalty2)
{
   // TODO Check here some cases, e.g. both or no penalty INT_MIN.
   setSinglePenalty(0, penalty1);
   setSinglePenalty(1, penalty2);
}

void Textblock::BadnessAndPenalty::setSinglePenalty (int index, int penalty)
{
   if (penalty == INT_MAX || penalty == INT_MIN)
      this->penalty[index] = penalty;
   else
      // This factor consists of: (i) 100^3, since in calcBadness(), the
      // ratio is multiplied with 100 (again, to use integer numbers for
      // fractional numbers), and the badness (which has to be compared
      // to the penalty!) is the third power or it; (ii) the denominator
      // 100, of course, since 100 times the penalty is passed to this
      // method.
      this->penalty[index] = penalty * (100 * 100 * 100 / 100);
}

bool Textblock::BadnessAndPenalty::lineLoose ()
{
   return
      badnessState == NOT_STRETCHABLE || badnessState == QUITE_LOOSE ||
      (badnessState == BADNESS_VALUE && ratio > 0);
}

bool Textblock::BadnessAndPenalty::lineTight ()
{
   return
      badnessState == TOO_TIGHT || (badnessState == BADNESS_VALUE && ratio < 0);
}

bool Textblock::BadnessAndPenalty::lineTooTight ()
{
   return badnessState == TOO_TIGHT;
}


bool Textblock::BadnessAndPenalty::lineMustBeBroken (int penaltyIndex)
{
   return penalty[penaltyIndex] == PENALTY_FORCE_BREAK;
}

bool Textblock::BadnessAndPenalty::lineCanBeBroken (int penaltyIndex)
{
   return penalty[penaltyIndex] != PENALTY_PROHIBIT_BREAK;
}

int Textblock::BadnessAndPenalty::compareTo (int penaltyIndex,
                                             BadnessAndPenalty *other)
{
   for (int l = INF_MAX; l >= 0; l--) {
      int thisValue = badnessValue (l) + penaltyValue (penaltyIndex, l);
      int otherValue =
         other->badnessValue (l) + other->penaltyValue (penaltyIndex, l);

      if (thisValue != otherValue)
         return thisValue - otherValue;
   }
   
   return 0;
}

void Textblock::BadnessAndPenalty::print ()
{
   misc::StringBuffer sb;
   intoStringBuffer(&sb);
   printf ("%s", sb.getChars ());
}

void Textblock::BadnessAndPenalty::intoStringBuffer(misc::StringBuffer *sb)
{
   switch (badnessState) {
   case NOT_STRETCHABLE:
      sb->append ("not stretchable");
      break;

   case TOO_TIGHT:
      sb->append ("too tight");
      break;

   case QUITE_LOOSE:
      sb->append ("quite loose (ratio = ");
      sb->appendInt (ratio);
      sb->append (")");
      break;

   case BADNESS_VALUE:
      sb->appendInt (badness);
      break;
   }

#ifdef DEBUG
   sb->append (" [");
   sb->appendInt (totalWidth);
   sb->append (" + ");
   sb->appendInt (totalStretchability);
   sb->append (" - ");
   sb->appendInt (totalShrinkability);
   sb->append (" vs. ");
   sb->appendInt (idealWidth);
   sb->append ("]");
#endif

   sb->append (" + (");
   for (int i = 0; i < 2; i++) {
      if (penalty[i] == INT_MIN)
         sb->append ("-inf");
      else if (penalty[i] == INT_MAX)
         sb->append ("inf");
      else
         sb->appendInt (penalty[i]);

      if (i == 0)
         sb->append (", ");
   }
   sb->append (")");
}

void Textblock::printWordShort (Word *word)
{
   core::Content::print (&(word->content));
}

void Textblock::printWordFlags (short flags)
{
   printf ("%s:%s:%s:%s:%s:%s:%s",
           (flags & Word::CAN_BE_HYPHENATED) ? "h?" : "--",
           (flags & Word::DIV_CHAR_AT_EOL) ? "de" : "--",
           (flags & Word::PERM_DIV_CHAR) ? "dp" : "--",
           (flags & Word::DRAW_AS_ONE_TEXT) ? "t1" : "--",
           (flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) ? "um" : "--",
           (flags & Word::WORD_START) ? "st" : "--",
           (flags & Word::WORD_END) ? "en" : "--");
}

void Textblock::printWordWithFlags (Word *word)
{
   printWordShort (word);
   printf (" (flags = ");
   printWordFlags (word->flags);
   printf (")");
}

void Textblock::printWord (Word *word)
{
   printWordWithFlags (word);

   printf (" [%d / %d + %d - %d => %d + %d - %d] => ",
           word->size.width, word->origSpace, getSpaceStretchability(word),
           getSpaceShrinkability(word), word->totalWidth,
           word->totalSpaceStretchability, word->totalSpaceShrinkability);
   word->badnessAndPenalty.print ();
}

/*
 * ...
 *
 * diff ...
 */
void Textblock::justifyLine (Line *line, int diff)
{
   DBG_OBJ_MSGF ("construct.line", 0, "<b>justifyLine</b> (..., %d)", diff);
   DBG_OBJ_MSG_START ();

   // To avoid rounding errors, the calculation is based on accumulated
   // values. See doc/rounding-errors.doc.

   if (diff > 0) {
      int spaceStretchabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         spaceStretchabilitySum += getSpaceStretchability(words->getRef(i));

      if (spaceStretchabilitySum > 0) {
         int spaceStretchabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            spaceStretchabilityCum += getSpaceStretchability(word);
            int spaceDiff =
               spaceStretchabilityCum * diff / spaceStretchabilitySum
               - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            DBG_OBJ_MSGF ("construct.line", 1, "%d (of %d): diff = %d",
                          i, words->size (), spaceDiff);

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   } else if (diff < 0) {
      int spaceShrinkabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         spaceShrinkabilitySum += getSpaceShrinkability(words->getRef(i));

      if (spaceShrinkabilitySum > 0) {
         int spaceShrinkabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            spaceShrinkabilityCum += getSpaceShrinkability(word);
            int spaceDiff =
               spaceShrinkabilityCum * diff / spaceShrinkabilitySum
               - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            DBG_OBJ_MSGF ("construct.line", 1, "%d (of %d): diff = %d",
                          i, words->size (), spaceDiff);

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   }

   DBG_OBJ_MSG_END ();
}


Textblock::Line *Textblock::addLine (int firstWord, int lastWord,
                                     bool temporary)
{
   DBG_OBJ_MSGF ("construct.line", 0, "<b>addLine</b> (%d, %d) => %d",
                 firstWord, lastWord, lines->size ());
   DBG_OBJ_MSG_START ();
  
   int lineWidth;
   if (lastWord >= firstWord) {
      DBG_MSG_WORD ("construct.line", 1, "<i>first word:</i> ", firstWord, "");
      DBG_MSG_WORD ("construct.line", 1, "<i>last word:</i> ", lastWord, "");

      Word *lastWordOfLine = words->getRef(lastWord);
      // Word::totalWidth includes the hyphen (which is what we want here).
      lineWidth = lastWordOfLine->totalWidth;
      DBG_OBJ_MSGF ("construct.line", 1, "lineWidth (from last word): %d",
                    lineWidth);
   } else {
      // empty line
      lineWidth = 0;
      DBG_OBJ_MSGF ("construct.line", 1, "lineWidth (empty line): %d",
                    lineWidth);
   }

   // "lineWidth" is relative to leftOffset, so we may have to add
   // "line1OffsetEff" (remember: this is, for list items, negative).
   if (lines->size () == 0) {
      lineWidth += line1OffsetEff;
      DBG_OBJ_MSGF ("construct.line", 1, "lineWidth (line1OffsetEff): %d",
                    lineWidth);
   }

   int maxOfMinWidth, sumOfMaxWidth;
   accumulateWordExtremes (firstWord, lastWord, &maxOfMinWidth,
                           &sumOfMaxWidth);

   lines->increase ();
   DBG_OBJ_SET_NUM ("lines.size", lines->size ());

   if(!temporary) {
      // If the last line was temporary, this will be temporary, too, even
      // if not requested.
      if (lines->size () == 1 || nonTemporaryLines == lines->size () -1)
         nonTemporaryLines = lines->size ();
   }

   PRINTF ("nonTemporaryLines = %d\n", nonTemporaryLines);

   int lineIndex = lines->size () - 1;
   Line *line = lines->getRef (lineIndex);

   line->firstWord = firstWord;
   line->lastWord = lastWord;
   line->boxAscent = line->contentAscent = 0;
   line->boxDescent = line->contentDescent = 0;
   line->marginDescent = 0;
   line->breakSpace = 0;
   line->leftOffset = 0;
   line->finished = false;

   alignLine (lineIndex);

   for (int i = line->firstWord; i < line->lastWord; i++) {
      Word *word = words->getRef (i);
      lineWidth += (word->effSpace - word->origSpace);
      DBG_OBJ_MSGF ("construct.line", 1,
                    "lineWidth [corrected space (%d - %d) after word %d]: %d",
                    word->effSpace, word->origSpace, i, lineWidth);
   }
  
   if (lines->size () == 1) {
      // first line
      line->top = 0;
      line->maxLineWidth = lineWidth;
      line->lastOofRefPositionedBeforeThisLine = -1;
   } else {
      Line *prevLine = lines->getRef (lines->size () - 2);
      line->top = prevLine->top + prevLine->boxAscent +
         prevLine->boxDescent + prevLine->breakSpace;
      line->maxLineWidth = misc::max (lineWidth, prevLine->maxLineWidth);
      line->lastOofRefPositionedBeforeThisLine =
         prevLine->lastOofRefPositionedBeforeThisLine;
   }
 
   for(int i = line->firstWord; i <= line->lastWord; i++)
      accumulateWordForLine (lineIndex, i);

   // Especially empty lines (possible when there are floats) have
   // zero height, which may cause endless loops. For this reasons,
   // the height should be positive.
   line->boxAscent = misc::max (line->boxAscent, 1);

   // Calculate offsetCompleteWidget, which includes also floats.
   int leftBorder = mustBorderBeRegarded (line) ? newLineLeftBorder : 0;
   line->offsetCompleteWidget =
      misc::max (leftBorder,
                 getStyle()->boxOffsetX() + innerPadding
                 + (lineIndex == 0 ? line1OffsetEff : 0))
      + line->leftOffset;

   DBG_OBJ_MSGF ("construct.line", 1, "top = %d\n", line->top);
   DBG_OBJ_MSGF ("construct.line", 1, "boxAscent = %d\n", line->boxAscent);
   DBG_OBJ_MSGF ("construct.line", 1, "boxDescent = %d\n", line->boxDescent);
   DBG_OBJ_MSGF ("construct.line", 1, "contentAscent = %d\n",
                 line->contentAscent);
   DBG_OBJ_MSGF ("construct.line", 1, "contentDescent = %d\n",
                 line->contentDescent);
   DBG_OBJ_MSGF ("construct.line", 1, "maxLineWidth = %d (lineWidth = %d)\n",
                 line->maxLineWidth, lineWidth);
   DBG_OBJ_MSGF ("construct.line", 1, "offsetCompleteWidget = %d\n",
                 line->offsetCompleteWidget);

   mustQueueResize = true;

   //printWordShort (words->getRef (line->firstWord));
   //printf (" ... ");
   //printWordShort (words->getRef (line->lastWord));
   //printf (": ");
   //words->getRef(line->lastWord)->badnessAndPenalty.print ();
   //printf ("\n");

   int xWidget = line->offsetCompleteWidget;
   for (int i = firstWord; i <= lastWord; i++) {
      Word *word = words->getRef (i);
      if (word->wordImgRenderer)
         word->wordImgRenderer->setData (xWidget, lines->size () - 1);
      if (word->spaceImgRenderer)
         word->spaceImgRenderer->setData (xWidget, lines->size () - 1);
      xWidget += word->size.width + word->effSpace;
   }
   
   line->finished = true;

   initNewLine ();

   DBG_OBJ_MSG_END ();
   return line;
}

void Textblock::accumulateWordExtremes (int firstWord, int lastWord,
                                        int *maxOfMinWidth, int *sumOfMaxWidth)
{
   int parMin = 0;
   *maxOfMinWidth = *sumOfMaxWidth = 0;

   for (int i = firstWord; i <= lastWord; i++) {
      Word *word = words->getRef (i);
      bool atLastWord = i == lastWord;

      core::Extremes extremes;
      getWordExtremes (word, &extremes);

      // Minimum: between two *possible* breaks (or at the end).
      // TODO This is redundant to getExtremesImpl().
      // TODO: Again, index 1 is used for lineCanBeBroken(). See getExtremes().
      if (word->badnessAndPenalty.lineCanBeBroken (1) || atLastWord) {
         parMin += extremes.minWidth + word->hyphenWidth;
         *maxOfMinWidth = misc::max (*maxOfMinWidth, parMin);
         parMin = 0;
      } else
         // Shrinkability could be considered, but really does not play a
         // role.
         parMin += extremes.minWidth + word->origSpace;

      //printf ("[%p] after word: ", this);
      //printWord (word);
      //printf ("\n");

      //printf ("[%p] (%d / %d) => parMin = %d, maxOfMinWidth = %d\n",
      //        this, extremes.minWidth, extremes.maxWidth, parMin,
      //        *maxOfMinWidth);

      *sumOfMaxWidth += (extremes.maxWidth + word->origSpace);
      // Notice that the last space is added. See also: Line::parMax.
   }
}

void Textblock::processWord (int wordIndex)
{
   bool wordListChanged = wordWrap (wordIndex, false);

   if (wordListChanged) {
      // If wordWrap has called hyphenateWord here, this has an effect
      // on the call of handleWordExtremes. To avoid adding values
      // more than one time (original un-hyphenated word, plus all
      // parts of the hyphenated word, except the first one), the
      // whole paragraph is recalculated again.

      int firstWord;
      if (paragraphs->size() > 0) {
         firstWord = paragraphs->getLastRef()->firstWord;
         paragraphs->setSize (paragraphs->size() - 1);
      } else
         firstWord = 0;

      for (int i = firstWord; i <= wordIndex - 1; i++)
         handleWordExtremes (i);
   }

   handleWordExtremes (wordIndex);
}

/*
 * This method is called in two cases: (i) when a word is added
 * (ii) when a page has to be (partially) rewrapped. It does word wrap,
 * and adds new lines if necessary.
 *
 * Returns whether the words list has changed at, or before, the word
 * index.
 */
bool Textblock::wordWrap (int wordIndex, bool wrapAll)
{
   DBG_OBJ_MSGF ("construct.word", 0, "<b>wordWrap</b> (%d, %s)",
                 wordIndex, wrapAll ? "true" : "false");
   DBG_OBJ_MSG_START ();

   DBG_MSG_WORD ("construct.word", 1, "<i>wrapped word:</i> ", wordIndex, "");

   if (!wrapAll)
      removeTemporaryLines ();

   initLine1Offset (wordIndex);

   Word *word = words->getRef (wordIndex);
   word->effSpace = word->origSpace;

   accumulateWordData (wordIndex);

   //printf ("   ");
   //printWord (word);
   //printf ("\n");

   bool b;
   if (word->content.type == core::Content::WIDGET_OOF_REF)
      b = false;
   else
      b = wrapWordInFlow (wordIndex, wrapAll);

   DBG_OBJ_MSG_END ();

   return b;
}

bool Textblock::wrapWordInFlow (int wordIndex, bool wrapAll)
{
   DBG_OBJ_MSGF ("construct.word", 0, "<b>wrapWordInFlow</b> (%d, %s)",
                 wordIndex, wrapAll ? "true" : "false");
   DBG_OBJ_MSG_START ();

   Word *word = words->getRef (wordIndex);
   bool wordListChanged = false;

   int penaltyIndex = calcPenaltyIndexForNewLine ();

   bool newLine;
   do {
      // This variable, thereWillBeMoreSpace, is set to true, if, due
      // to floats, this line is smaller than following lines will be
      // (and, at the end, there will be surely lines without
      // floats). If this is the case, lines may, in an extreme case,
      // be left empty.

      // (In other cases, lines are never left empty, even if this means
      // that the contents is wider than the available witdh. Leaving
      // lines empty does not make sense without floats, since there will
      // be no possibility with more space anymore.)

      bool regardBorder =  mustBorderBeRegarded (lines->size ());
      bool thereWillBeMoreSpace = regardBorder ?
         newLineHasFloatLeft || newLineHasFloatRight :
         false;

      DBG_OBJ_MSGF ("construct.word", 1,
                    "thereWillBeMoreSpace = %s ? %s || %s : false = %s",
                    regardBorder ? "true" : "false",
                    newLineHasFloatLeft ? "true" : "false",
                    newLineHasFloatRight ? "true" : "false",
                    thereWillBeMoreSpace ? "true" : "false");
      
                    
      bool tempNewLine = false;
      int firstIndex =
         lines->size() == 0 ? 0 : lines->getLastRef()->lastWord + 1;
      int searchUntil;

      if (wordIndex < firstIndex)
          // Current word is already part of a line (ending with
          // firstIndex - 1), so no new line has to be added.
          newLine = false;
      else if (wrapAll && wordIndex >= firstIndex &&
               wordIndex == words->size() -1) {
         newLine = true;
         searchUntil = wordIndex;
         tempNewLine = true;
         DBG_OBJ_MSG ("construct.word", 1, "<b>new line:</b> last word");
      } else if (wordIndex >= firstIndex &&
                 // TODO: lineMustBeBroken should be independent of
                 // the penalty index?
                 word->badnessAndPenalty.lineMustBeBroken (penaltyIndex)) {
         newLine = true;
         searchUntil = wordIndex;
         DBG_OBJ_MSG ("construct.word", 1, "<b>new line:</b> forced break");
      } else {
         // Break the line when too tight, but only when there is a
         // possible break point so far. (TODO: I've forgotten the
         // original bug which is fixed by this.)
         
         // Exception of the latter rule: thereWillBeMoreSpace; see
         // above, where it is defined.
         
         DBG_OBJ_MSGF ("construct.word", 1,
                       "possible line break between %d and %d?",
                       firstIndex, wordIndex - 1);
         DBG_OBJ_MSG_START ();

         bool possibleLineBreak = false;
         for (int i = firstIndex;
              !(thereWillBeMoreSpace || possibleLineBreak)
                 && i <= wordIndex - 1;
              i++) {
            DBG_OBJ_MSGF ("construct.word", 2, "examining word %d", i);
            if (words->getRef(i)->badnessAndPenalty
                .lineCanBeBroken (penaltyIndex)) {
               DBG_MSG_WORD ("construct.word", 2, "break possible for word:",
                             i, "");
               possibleLineBreak = true;
            }
         }

         DBG_OBJ_MSG_END ();
         DBG_OBJ_MSGF ("construct.word", 1, "=> %s",
                       possibleLineBreak ? "true" : "false");

         DBG_OBJ_MSGF ("construct.word", 1, "word->... too tight: %s",
                       word->badnessAndPenalty.lineTooTight () ?
                       "true" : "false");

         if ((thereWillBeMoreSpace || possibleLineBreak)
             && word->badnessAndPenalty.lineTooTight ()) {
            newLine = true;
            searchUntil = wordIndex - 1;
            DBG_OBJ_MSG ("construct.word", 1,
                         "<b>new line:</b> line too tight");
         } else {
            DBG_OBJ_MSG ("construct.word", 1, "no <b>new line</b>");
            newLine = false;
         }
      }

      if(!newLine && !wrapAll)
         // No new line is added. "mustQueueResize" must,
         // nevertheless, be set, so that flush() will call
         // queueResize(), and later sizeRequestImpl() is called,
         // which then calls showMissingLines(), which eventually
         // calls this method again, with wrapAll == true, so that
         // newLine is calculated as "true".
         mustQueueResize = true;

      PRINTF ("[%p] special case? newLine = %s, wrapAll = %s => "
              "mustQueueResize = %s\n", this, newLine ? "true" : "false",
              wrapAll ? "true" : "false", mustQueueResize ? "true" : "false");

      if(newLine) {
         accumulateWordData (wordIndex);

         int wordIndexEnd, breakPos =
            searchBreakPos (wordIndex, firstIndex, &searchUntil, tempNewLine,
                            penaltyIndex, wrapAll,&wordListChanged,
                            &wordIndexEnd);
         int height = calcLinePartHeight (firstIndex, breakPos);
         int lastFloatPos = lines->size() > 0 ?
            lines->getLastRef()->lastOofRefPositionedBeforeThisLine : -1;

         bool floatHandled;

         int yNewLine = yOffsetOfPossiblyMissingLine (lines->size ());

         do {
            DBG_OBJ_MSG ("construct.word", 1, "<i>floatHandled loop cycle</i>");
            DBG_OBJ_MSG_START ();

            DBG_OBJ_MSGF ("construct.word", 2,
                          "breakPos = %d, height = %d, lastFloatPos = %d",
                          breakPos, height, lastFloatPos);
            
            int startSearch = misc::max (firstIndex, lastFloatPos);
            int newFloatPos = -1;

            DBG_OBJ_MSGF ("construct.word", 2, "searching from %d to %d",
                          startSearch, breakPos);
            for (int i = startSearch; newFloatPos == -1 && i <= breakPos; i++) {
               core::Content *content = &(words->getRef(i)->content);
               if (content->type == core::Content::WIDGET_OOF_REF &&
                   // Later, absolutepositioned elements (which do not affect
                   // borders) can be ignored at this point.
                   (containingBlock->outOfFlowMgr->affectsLeftBorder
                    (content->widget) ||
                    containingBlock->outOfFlowMgr->affectsRightBorder
                    (content->widget)))
                  newFloatPos = i;
            }

            DBG_OBJ_MSGF ("construct.word", 2, "newFloatPos = %d", newFloatPos);

            if (newFloatPos == -1)
               floatHandled = false;
            else {
               lastFloatPos = newFloatPos;

               containingBlock->outOfFlowMgr->tellPosition
                  (words->getRef(lastFloatPos)->content.widget, yNewLine);

               calcBorders (lastFloatPos, height);

               for(int i = firstIndex; i <= wordIndexEnd; i++)
                  accumulateWordData (i);
                              
               breakPos =
                  searchBreakPos (wordIndex, firstIndex, &searchUntil,
                                  tempNewLine, penaltyIndex, wrapAll,
                                  &wordListChanged, &wordIndexEnd);
               height = calcLinePartHeight (firstIndex, breakPos);

               // TODO: Iterate until the height converges.
            }

            DBG_OBJ_MSG_END ();
         } while (floatHandled);

         addLine (firstIndex, breakPos, tempNewLine);
         lines->getLastRef()->lastOofRefPositionedBeforeThisLine = lastFloatPos;

         DBG_OBJ_MSGF ("construct.word", 1,
                       "accumulating again from %d to %d\n",
                       breakPos + 1, wordIndexEnd);
         for(int i = breakPos + 1; i <= wordIndexEnd; i++)
            accumulateWordData (i);
         
         // update word pointer as hyphenateWord() can trigger a
         // reorganization of the words structure
         word = words->getRef (wordIndex);

         penaltyIndex = calcPenaltyIndexForNewLine ();
      }
   } while (newLine);

   if(word->content.type == core::Content::WIDGET_IN_FLOW) {
      // Set parentRef for the child, when necessary.
      //
      // parentRef is set for the child already, when a line is
      // added. There are a couple of different situations to
      // consider, e.g. when called from showMissingLines(), this word
      // may already have been added in a previous call. To make
      // things simple, we just check whether this word is contained
      // within any line, or still "missing".

      int firstWordWithoutLine;
      if (lines->size() == 0)
         firstWordWithoutLine = 0;
      else
         firstWordWithoutLine = lines->getLastRef()->lastWord + 1;
   
      if (wordIndex >= firstWordWithoutLine) {
         word->content.widget->parentRef =
            OutOfFlowMgr::createRefNormalFlow (lines->size ());
         PRINTF ("The %s %p is assigned parentRef = %d.\n",
                 word->content.widget->getClassName(), word->content.widget,
                 word->content.widget->parentRef);
      }
   }

   DBG_OBJ_MSG_END ();

   return wordListChanged;
}

int Textblock::searchBreakPos (int wordIndex, int firstIndex, int *searchUntil,
                               bool tempNewLine, int penaltyIndex, bool wrapAll,
                               bool *wordListChanged, int *wordIndexEnd)
{
   DBG_OBJ_MSGF ("construct.word", 0,
                 "<b>searchBreakPos</b> (%d, %d. %d, %d, %s, ...)",
                 wordIndex, firstIndex, *searchUntil, penaltyIndex,
                 wrapAll ? "true" : "false");
   DBG_OBJ_MSG_START ();

   int result;
   bool lineAdded;

   *wordIndexEnd = wordIndex;

   do {
      DBG_OBJ_MSG ("construct.word", 1, "<i>searchBreakPos loop cycle</i>");
      DBG_OBJ_MSG_START ();
      
      if (firstIndex > *searchUntil) {
         // empty line
         DBG_OBJ_MSG ("construct.word", 1, "empty line");
         assert (*searchUntil == firstIndex - 1);
         result = firstIndex - 1;
         lineAdded = true;
      } else {
         DBG_OBJ_MSG ("construct.word", 1, "non-empty line");
         
         int breakPos =
            searchMinBap (firstIndex, *searchUntil, penaltyIndex, wrapAll);
         int hyphenatedWord = considerHyphenation (firstIndex, breakPos);
      
         DBG_OBJ_MSGF ("construct.word", 1, "breakPos = %d", breakPos);
         DBG_MSG_WORD ("construct.word", 1, "<i>break at word:</i> ",
                       breakPos, "");
         DBG_OBJ_MSGF ("construct.word", 1, "hyphenatedWord = %d",
                       hyphenatedWord);
         if (hyphenatedWord != -1)
            DBG_MSG_WORD ("construct.word", 1,
                          "<i>hyphenate at word:</i> ",
                          hyphenatedWord, "");
         
         if(hyphenatedWord == -1) {
            result = breakPos;
            lineAdded = true;
         } else {
            // TODO hyphenateWord() should return whether something
            // has changed at all. So that a second run, with
            // !word->canBeHyphenated, is unnecessary.
            // TODO Update: The return value of hyphenateWord() should
            // be checked.
            DBG_OBJ_MSGF ("construct.word", 1, "old searchUntil = %d",
                          *searchUntil);
            int n = hyphenateWord (hyphenatedWord);
            *searchUntil += n;
            if (hyphenatedWord <= wordIndex)
               *wordIndexEnd += n;
            DBG_OBJ_MSGF ("construct.word", 1, "new searchUntil = %d",
                          *searchUntil);
            lineAdded = false;
            result = 1000000000; // compiler happiness
                       
            if (n > 0 && hyphenatedWord <= wordIndex)
               *wordListChanged = true;

            DBG_OBJ_MSGF ("construct.word", 1,
                          "accumulating again from %d to %d\n",
                          breakPos + 1, *wordIndexEnd);
            for(int i = breakPos + 1; i <= *wordIndexEnd; i++)
               accumulateWordData (i);
         }       
      }
      
      DBG_OBJ_MSG_END ();
   } while(!lineAdded);

   DBG_OBJ_MSGF ("construct.word", 1, "=> %d", result);
   DBG_OBJ_MSG_END ();
   
   return result;
}

int Textblock::searchMinBap (int firstWord, int lastWord, int penaltyIndex,
                             bool correctAtEnd)
{
   DBG_OBJ_MSGF ("construct.word", 0, "<b>searchMinBap</b> (%d, %d, %d, %s)",
                 firstWord, lastWord, penaltyIndex,
                 correctAtEnd ? "true" : "false");
   DBG_OBJ_MSG_START ();

   int pos = -1;

   DBG_OBJ_MSG_START ();
   for (int i = firstWord; i <= lastWord; i++) {
      Word *w = words->getRef(i);
      
      DBG_IF_RTFL {
         misc::StringBuffer sb;
         w->badnessAndPenalty.intoStringBuffer (&sb);
         DBG_OBJ_MSGF ("construct.word", 2, "%d (of %d): b+p: %s",
                       i, words->size (), sb.getChars ());
         DBG_MSG_WORD ("construct.word", 2, "(<i>i. e.:</i> ", i, ")");
      }

      if (pos == -1 ||
          w->badnessAndPenalty.compareTo (penaltyIndex,
                                          &words->getRef(pos)
                                          ->badnessAndPenalty) <= 0)
         // "<=" instead of "<" in the next lines tends to result in
         // more words per line -- theoretically. Practically, the
         // case "==" will never occur.
         pos = i;
   }
   DBG_OBJ_MSG_END ();

   DBG_OBJ_MSGF ("construct.word", 1, "found at %d\n", pos);

   if (correctAtEnd && lastWord == words->size () - 1) {
      // Since no break and no space is added, the last word will have
      // a penalty of inf. Actually, it should be less, since it is
      // the last word. However, since more words may follow, the
      // penalty is not changed, but here, the search is corrected
      // (maybe only temporary).
      
      // (Notice that it was once (temporally) set to -inf, not 0, but
      // this will make e.g. test/table-1.html not work.)
      Word *w = words->getRef (lastWord);
      BadnessAndPenalty correctedBap = w->badnessAndPenalty;
      correctedBap.setPenalty (0);

      DBG_IF_RTFL {
         misc::StringBuffer sb;
         correctedBap.intoStringBuffer (&sb);
         DBG_OBJ_MSGF ("construct.word", 1, "corrected b+p: %s",
                       sb.getChars ());
      }
                    
      if (correctedBap.compareTo(penaltyIndex,
                                 &words->getRef(pos)->badnessAndPenalty) <= 0) {
         pos = lastWord;
         DBG_OBJ_MSGF ("construct.word", 1, "corrected: %d\n", pos);
      }
   }
           
   DBG_OBJ_MSG_END ();
   return pos;
}

/**
 * Suggest a word to hyphenate, when breaking at breakPos is
 * planned. Return a word index or -1, when hyphenation makes no
 * sense.
 */
int Textblock::considerHyphenation (int firstIndex, int breakPos)
{
   int hyphenatedWord = -1;

   Word *wordBreak = words->getRef(breakPos);
   //printf ("[%p] line (broken at word %d): ", this, breakPos);
   //printWord (wordBreak);
   //printf ("\n");

   // A tight line: maybe, after hyphenation, some parts of the last
   // word of this line can be put into the next line.
   if (wordBreak->badnessAndPenalty.lineTight ()) {
      // Sometimes, it is not the last word, which must be hyphenated,
      // but some word before. Here, we search for the first word
      // which can be hyphenated, *and* makes the line too tight.     
      for (int i = breakPos; i >= firstIndex; i--) {
         Word *word1 = words->getRef (i);
         if (word1->badnessAndPenalty.lineTight () &&
             isHyphenationCandidate (word1))
            hyphenatedWord = i;
      }
   }

   // A loose line: maybe, after hyphenation, some parts of the first
   // word of the next line can be put into this line.
   if (wordBreak->badnessAndPenalty.lineLoose () &&
       breakPos + 1 < words->size ()) {
      Word *word2 = words->getRef(breakPos + 1);
      if (isHyphenationCandidate (word2))
         hyphenatedWord = breakPos + 1;
   }

   return hyphenatedWord;
}

bool Textblock::isHyphenationCandidate (Word *word)
{
   return (word->flags & Word::CAN_BE_HYPHENATED) &&
      word->style->x_lang[0] &&
      isBreakAllowed(word) &&
      word->content.type == core::Content::TEXT &&
      Hyphenator::isHyphenationCandidate (word->content.text);
}


int Textblock::calcLinePartHeight (int firstWord, int lastWord)
{
   int ascent = 0, descent = 0;

   for (int i = firstWord; i <= lastWord; i++) {
      Word *word = words->getRef (i);
      ascent = misc::max (ascent, word->size.ascent);
      descent = misc::max (descent, word->size.descent);
   }

   return ascent + descent;
}

/**
 * Counter part to wordWrap(), but for extremes, not size calculation.
 */
void Textblock::handleWordExtremes (int wordIndex)
{
   // TODO Overall, clarify penalty index.

   DBG_OBJ_MSGF ("construct.paragraph", 0,
                 "<b>handleWordExtremes</b> (%d)", wordIndex);
   DBG_OBJ_MSG_START ();

   Word *word = words->getRef (wordIndex);
   DBG_MSG_WORD ("construct.paragraph", 1,
                 "<i>handled word:</i> ", wordIndex, "");

   core::Extremes wordExtremes;
   getWordExtremes (word, &wordExtremes);
   DBG_OBJ_MSGF ("construct.paragraph", 1, "extremes: %d / %d",
                 wordExtremes.minWidth, wordExtremes.maxWidth);

   if (wordIndex == 0) {
      wordExtremes.minWidth += line1Offset;
      wordExtremes.maxWidth += line1Offset;
   }

   if (paragraphs->size() == 0 ||
       words->getRef(paragraphs->getLastRef()->lastWord)
       ->badnessAndPenalty.lineMustBeBroken (1)) {
      // Add a new paragraph.
      paragraphs->increase ();
      Paragraph *prevPar = paragraphs->size() == 1 ?
         NULL : paragraphs->getRef(paragraphs->size() - 2);
      Paragraph *par = paragraphs->getLastRef();

      par->firstWord = par->lastWord = wordIndex;
      par->parMin = par->parMax = 0;

      if (prevPar) {
         par->maxParMin = prevPar->maxParMin;
         par->maxParMax = prevPar->maxParMax;
      } else
         par->maxParMin = par->maxParMax = 0;

      DBG_OBJ_MSGF ("construct.paragraph", 1, "new par: %d",
                    paragraphs->size() - 1);
   }

   DBG_OBJ_MSGF ("construct.paragraph", 1, "last par: %d",
                 paragraphs->size() - 1);
   Paragraph *lastPar = paragraphs->getLastRef();

   int corrDiffMin, corrDiffMax;
   if (wordIndex - 1 >= lastPar->firstWord) {
      Word *lastWord = words->getRef (wordIndex - 1);
      if (lastWord->badnessAndPenalty.lineCanBeBroken (1) &&
          (lastWord->flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) == 0)
         corrDiffMin = 0;
      else
         corrDiffMin = lastWord->origSpace - lastWord->hyphenWidth;
         
      corrDiffMax = lastWord->origSpace - lastWord->hyphenWidth;
   } else
      corrDiffMin = corrDiffMax = 0;

   DBG_OBJ_MSGF ("construct.paragraph", 1,
                 "(lastPar from %d to %d; corrDiffMin = %d, corDiffMax = %d)",
                 lastPar->firstWord, lastPar->lastWord, corrDiffMin,
                 corrDiffMax);

   // Minimum: between two *possible* breaks.
   // Shrinkability could be considered, but really does not play a role.
   lastPar->parMin += wordExtremes.minWidth + word->hyphenWidth + corrDiffMin;
   lastPar->maxParMin = misc::max (lastPar->maxParMin, lastPar->parMin);
   if (word->badnessAndPenalty.lineCanBeBroken (1) &&
       (word->flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) == 0)
      lastPar->parMin = 0;

   // Maximum: between two *necessary* breaks.
   lastPar->parMax += wordExtremes.maxWidth + word->hyphenWidth + corrDiffMax;
   lastPar->maxParMax = misc::max (lastPar->maxParMax, lastPar->parMax);

   DBG_OBJ_MSGF ("construct.paragraph", 1,
                 "=> parMin = %d (max = %d), parMax = %d (max = %d)",
                 lastPar->parMin, lastPar->maxParMin, lastPar->parMax,
                 lastPar->maxParMax);

   lastPar->lastWord = wordIndex;
   DBG_OBJ_MSG_END ();
}

/**
 * Called when something changed for the last word (space, hyphens etc.).
 */
void Textblock::correctLastWordExtremes ()
{
   if (paragraphs->size() > 0) {
      Word *word = words->getLastRef ();
      if (word->badnessAndPenalty.lineCanBeBroken (1) &&
          (word->flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) == 0) {
         paragraphs->getLastRef()->parMin = 0;
         PRINTF ("   => corrected; parMin = %d\n",
                 paragraphs->getLastRef()->parMin);
      }
   }
}


int Textblock::hyphenateWord (int wordIndex)
{
   Word *hyphenatedWord = words->getRef(wordIndex);
   char lang[3] = { hyphenatedWord->style->x_lang[0], 
                    hyphenatedWord->style->x_lang[1], 0 };
   Hyphenator *hyphenator = Hyphenator::getHyphenator (lang);
   PRINTF ("[%p]    considering to hyphenate word %d, '%s', in language '%s'\n",
           this, wordIndex, words->getRef(wordIndex)->content.text, lang);
   int numBreaks;
   int *breakPos =
      hyphenator->hyphenateWord (layout->getPlatform (),
                                 hyphenatedWord->content.text, &numBreaks);

   if (numBreaks > 0) {
      Word origWord = *hyphenatedWord;

      core::Requisition wordSize[numBreaks + 1];
      calcTextSizes (origWord.content.text, strlen (origWord.content.text),
                     origWord.style, numBreaks, breakPos, wordSize);
      
      PRINTF ("[%p]       %d words ...\n", this, words->size ());
      words->insert (wordIndex, numBreaks);

      DBG_IF_RTFL {
         for (int i = wordIndex + numBreaks; i < words->size (); i++)
            DBG_SET_WORD (i);
      }

      for (int i = 0; i < numBreaks; i++)
         initWord (wordIndex + i);
      PRINTF ("[%p]       ... => %d words\n", this, words->size ());

      if (containingBlock->outOfFlowMgr)
         containingBlock->outOfFlowMgr->moveExternalIndices (this, wordIndex,
                                                             numBreaks);

      // Adjust anchor indexes.
      for (int i = 0; i < anchors->size (); i++) {
         Anchor *anchor = anchors->getRef (i);
         if (anchor->wordIndex > wordIndex)
            anchor->wordIndex += numBreaks;
      }
      
      for (int i = 0; i < numBreaks + 1; i++) {
         Word *w = words->getRef (wordIndex + i);
         fillWord (wordIndex + i, wordSize[i].width, wordSize[i].ascent,
                   wordSize[i].descent, false, origWord.style);

         // TODO There should be a method fillText0.
         w->content.type = core::Content::TEXT;

         int start = (i == 0 ? 0 : breakPos[i - 1]);
         int end = (i == numBreaks ?
                    strlen (origWord.content.text) : breakPos[i]);
         w->content.text =
            layout->textZone->strndup (origWord.content.text + start,
                                       end - start);
         PRINTF ("      [%d] -> '%s'\n", wordIndex + i, w->content.text);

         DBG_OBJ_ARRATTRSET_SYM ("words", wordIndex + i, "type", "TEXT");
         DBG_OBJ_ARRATTRSET_STR ("words", wordIndex + i,
                                 "text/widget/breakSpace", w->content.text);

         // Note: there are numBreaks + 1 word parts.
         if (i == 0)
            w->flags |= Word::WORD_START;
         else 
            w->flags &= ~Word::WORD_START;

         if (i == numBreaks)
            w->flags |= Word::WORD_END;
         else 
            w->flags &= ~Word::WORD_END;

         if (i < numBreaks) {
            // TODO There should be a method fillHyphen.
            w->badnessAndPenalty.setPenalties (penalties[PENALTY_HYPHEN][0],
                                               penalties[PENALTY_HYPHEN][1]);
            // "\xe2\x80\x90" is an unconditional hyphen.
            w->hyphenWidth =
               layout->textWidth (w->style->font, hyphenDrawChar,
                                  strlen (hyphenDrawChar));
            w->flags |= (Word::DRAW_AS_ONE_TEXT | Word::DIV_CHAR_AT_EOL |
                         Word::UNBREAKABLE_FOR_MIN_WIDTH);

            PRINTF ("      [%d] + hyphen\n", wordIndex + i);
         } else {
            if (origWord.content.space) {
               fillSpace (wordIndex + i, origWord.spaceStyle);
               PRINTF ("      [%d] + space\n", wordIndex + i);
            } else {
               PRINTF ("      [%d] + nothing\n", wordIndex + i);
            }
         }

         //printf ("[%p] %d: hyphenated word part: ", this, wordIndex + i);
         //printWordWithFlags (w);
         //printf ("\n");
      }

      // AccumulateWordData() will calculate the width, which depends
      // on the borders (possibly limited by floats), which depends on
      // the widgeds so far. For this reason, it is important to first
      // make all words consistent before calling
      // accumulateWordData(); therefore the second loop.

      for (int i = 0; i < numBreaks + 1; i++)
         accumulateWordData (wordIndex + i);

      PRINTF ("   finished\n");
      
      //delete origword->content.text; TODO: Via textZone?
      origWord.style->unref ();
      origWord.spaceStyle->unref ();

      free (breakPos);
   } else
      words->getRef(wordIndex)->flags &= ~Word::CAN_BE_HYPHENATED;

   return numBreaks;
}

void Textblock::accumulateWordForLine (int lineIndex, int wordIndex)
{
   Line *line = lines->getRef (lineIndex);
   Word *word = words->getRef (wordIndex);

   PRINTF ("[%p] ACCUMULATE_WORD_FOR_LINE (%d, %d): %d + %d / %d + %d\n",
           this, lineIndex, wordIndex, line->boxAscent, line->boxDescent,
           word->size.ascent, word->size.descent);
   //printf ("   ");
   //printWord (word);
   //printf ("\n");

   line->boxAscent = misc::max (line->boxAscent, word->size.ascent);
   line->boxDescent = misc::max (line->boxDescent, word->size.descent);

   int len = word->style->font->ascent;
   if (word->style->valign == core::style::VALIGN_SUPER)
      len += len / 2;
   line->contentAscent = misc::max (line->contentAscent, len);
         
   len = word->style->font->descent;
   if (word->style->valign == core::style::VALIGN_SUB)
      len += word->style->font->ascent / 3;
   line->contentDescent = misc::max (line->contentDescent, len);

   if (word->content.type == core::Content::WIDGET_IN_FLOW) {
      int collapseMarginTop = 0;

      line->marginDescent =
         misc::max (line->marginDescent,
                    word->size.descent +
                    word->content.widget->getStyle()->margin.bottom);

      if (lines->size () == 1 &&
          word->content.widget->blockLevel () &&
          getStyle ()->borderWidth.top == 0 &&
          getStyle ()->padding.top == 0) {
         // collapse top margins of parent element and its first child
         // see: http://www.w3.org/TR/CSS21/box.html#collapsing-margins
         collapseMarginTop = getStyle ()->margin.top;
      }

      line->boxAscent =
            misc::max (line->boxAscent,
                       word->size.ascent,
                       word->size.ascent
                       + word->content.widget->getStyle()->margin.top
                       - collapseMarginTop);

      word->content.widget->parentRef =
         OutOfFlowMgr::createRefNormalFlow (lineIndex);
   } else {
      line->marginDescent =
         misc::max (line->marginDescent, line->boxDescent);

      if (word->content.type == core::Content::BREAK)
         line->breakSpace =
            misc::max (word->content.breakSpace,
                       line->marginDescent - line->boxDescent,
                       line->breakSpace);
   }
}

void Textblock::accumulateWordData (int wordIndex)
{
   DBG_OBJ_MSGF ("construct.word", 1, "<b>accumulateWordData</b> (%d)",
                 wordIndex);
   DBG_OBJ_MSG_START ();

   // Typically, the word in question is in the last line; in any case
   // quite at the end of the text, so that linear search is actually
   // the fastest option.
   int lineIndex = lines->size ();
   while (lineIndex > 0 && wordIndex <= lines->getRef(lineIndex - 1)->lastWord)
      lineIndex--;

   int firstWordOfLine;
   if (lineIndex == 0)
      firstWordOfLine = 0;
   else
      firstWordOfLine = lines->getRef(lineIndex - 1)->lastWord + 1;

   Word *word = words->getRef (wordIndex);
   DBG_OBJ_MSGF ("construct.word", 2, "lineIndex = %d", lineIndex);

   int availWidth = calcAvailWidth (lineIndex);

   DBG_OBJ_MSGF ("construct.word", 2,
                 "(%s existing line %d starts with word %d; availWidth = %d)",
                 lineIndex < lines->size () ? "already" : "not yet",
                 lineIndex, firstWordOfLine, availWidth);

   if (wordIndex == firstWordOfLine) {
      // first word of the (not neccessarily yet existing) line
      word->totalWidth = word->size.width + word->hyphenWidth;
      word->maxAscent = word->size.ascent;
      word->maxDescent = word->size.descent;
      word->totalSpaceStretchability = 0;
      word->totalSpaceShrinkability = 0;

      DBG_OBJ_MSGF ("construct.word", 1,
                    "first word of line: words[%d].totalWidth = %d + %d = %d",
                    wordIndex, word->size.width, word->hyphenWidth,
                    word->totalWidth);
   } else {
      Word *prevWord = words->getRef (wordIndex - 1);

      word->totalWidth = prevWord->totalWidth
         + prevWord->origSpace - prevWord->hyphenWidth
         + word->size.width + word->hyphenWidth;
      word->maxAscent = misc::max (prevWord->size.ascent, word->size.ascent);
      word->maxDescent = misc::max (prevWord->size.descent, word->size.descent);
      word->totalSpaceStretchability =
         prevWord->totalSpaceStretchability + getSpaceStretchability(prevWord);
      word->totalSpaceShrinkability =
         prevWord->totalSpaceShrinkability + getSpaceShrinkability(prevWord);

      DBG_OBJ_MSGF ("construct.word", 1,
                    "not first word of line: words[%d].totalWidth = %d + %d - "
                    "%d + %d + %d = %d",
                    wordIndex, prevWord->totalWidth, prevWord->origSpace,
                    prevWord->hyphenWidth, word->size.width,
                    word->hyphenWidth, word->totalWidth);
   }

   int totalStretchability =
      word->totalSpaceStretchability + getLineStretchability (word);
   int totalShrinkability =
      word->totalSpaceShrinkability + getLineShrinkability (word);
   word->badnessAndPenalty.calcBadness (word->totalWidth, availWidth,
                                        totalStretchability,
                                        totalShrinkability);

   DBG_IF_RTFL {
      misc::StringBuffer sb;
      word->badnessAndPenalty.intoStringBuffer (&sb);
      DBG_OBJ_MSGF ("construct.word", 1, "b+p: %s", sb.getChars ());
   }

   DBG_OBJ_MSG_END ();
}

int Textblock::calcAvailWidth (int lineIndex)
{
   DBG_OBJ_MSGF ("construct.word", 1, "<b>calcAvailWidth</b> (%d <i>of %d</i>)",
                 lineIndex, lines->size());
   DBG_OBJ_MSG_START ();

   int availWidth = this->availWidth - innerPadding;
   if (limitTextWidth &&
       layout->getUsesViewport () &&
       // margin/border/padding will be subtracted later,  via OOFM.
       availWidth - getStyle()->boxDiffWidth()
       > layout->getWidthViewport () - 10)
      availWidth = layout->getWidthViewport () - 10;
   if (lineIndex == 0)
      availWidth -= line1OffsetEff;

   int leftBorder, rightBorder;
   if (mustBorderBeRegarded (lineIndex)) {
      leftBorder = newLineLeftBorder;
      rightBorder = newLineRightBorder;
   } else
      leftBorder = rightBorder = 0;

   leftBorder = misc::max (leftBorder, getStyle()->boxOffsetX());
   rightBorder = misc::max (rightBorder, getStyle()->boxRestWidth());

   availWidth -= (leftBorder + rightBorder);

   DBG_OBJ_MSGF ("construct.word", 2, "=> %d - %d - (%d + %d) = %d\n",
                 this->availWidth, innerPadding, leftBorder, rightBorder,
                 availWidth);

   DBG_OBJ_MSG_END ();
   return availWidth;
}

void Textblock::initLine1Offset (int wordIndex)
{
   Word *word = words->getRef (wordIndex);

   /* Test whether line1Offset can be used. */
   if (wordIndex == 0) {
      if (ignoreLine1OffsetSometimes &&
          line1Offset + word->size.width > availWidth) {
         line1OffsetEff = 0;
      } else {
         int indent = 0;

         if (word->content.type == core::Content::WIDGET_IN_FLOW &&
             word->content.widget->blockLevel() == true) {
            /* don't use text-indent when nesting blocks */
         } else {
            if (core::style::isPerLength(getStyle()->textIndent)) {
               indent = core::style::multiplyWithPerLengthRounded
                           (this->availWidth, getStyle()->textIndent);
            } else {
               indent = core::style::absLengthVal (getStyle()->textIndent);
            }
         }
         line1OffsetEff = line1Offset + indent;
      }
   }
}

/**
 * Align the line.
 *
 * \todo Use block's style instead once paragraphs become proper blocks.
 */
void Textblock::alignLine (int lineIndex)
{
   DBG_OBJ_MSGF ("construct.line", 0, "<b>alignLine</b> (%d)", lineIndex);
   DBG_OBJ_MSG_START ();

   Line *line = lines->getRef (lineIndex);
   int availWidth = calcAvailWidth (lineIndex);
   if (line->firstWord <= line->lastWord) {
      Word *firstWord = words->getRef (line->firstWord);
      Word *lastWord = words->getRef (line->lastWord);
      
      for (int i = line->firstWord; i < line->lastWord; i++)
         words->getRef(i)->origSpace = words->getRef(i)->effSpace;
      
      if (firstWord->content.type != core::Content::BREAK) {
         switch (firstWord->style->textAlign) {
         case core::style::TEXT_ALIGN_LEFT:
            DBG_OBJ_MSG ("construct.line", 1,
                         "first word has 'text-align: left'");
            line->leftOffset = 0;
            break;
         case core::style::TEXT_ALIGN_STRING:   /* handled elsewhere (in the
                                                 * future)? */
            DBG_OBJ_MSG ("construct.line", 1,
                         "first word has 'text-align: string'");
            line->leftOffset = 0;
            break;
         case core::style::TEXT_ALIGN_JUSTIFY:  /* see some lines above */
            line->leftOffset = 0;
            DBG_OBJ_MSG ("construct.line", 1,
                         "first word has 'text-align: justify'");
            // Do not justify the last line of a paragraph (which ends on a
            // BREAK or with the last word of the page).
            if(!(lastWord->content.type == core::Content::BREAK ||
                 line->lastWord == words->size () - 1) ||
               // In some cases, however, an unjustified line would be too wide:
               // when the line would be shrunken otherwise. (This solution is
               // far from perfect, but a better solution would make changes in
               // the line breaking algorithm necessary.)
               availWidth < lastWord->totalWidth)
               justifyLine (line, availWidth - lastWord->totalWidth);
            break;
         case core::style::TEXT_ALIGN_RIGHT:
            DBG_OBJ_MSG ("construct.line", 1,
                         "first word has 'text-align: right'");
            line->leftOffset = availWidth - lastWord->totalWidth;
            break;
         case core::style::TEXT_ALIGN_CENTER:
            DBG_OBJ_MSG ("construct.line", 1,
                         "first word has 'text-align: center'");
            line->leftOffset = (availWidth - lastWord->totalWidth) / 2;
            break;
         default:
            /* compiler happiness */
            line->leftOffset = 0;
         }

         /* For large lines (images etc), which do not fit into the viewport: */
         if (line->leftOffset < 0)
            line->leftOffset = 0;
      }
   } else
      // empty line
      line->leftOffset = 0;

   DBG_OBJ_MSG_END ();
}

/**
 * Rewrap the page from the line from which this is necessary.
 * There are basically two times we'll want to do this:
 * either when the viewport is resized, or when the size changes on one
 * of the child widgets.
 */
void Textblock::rewrap ()
{
   DBG_OBJ_MSG ("construct.line", 0, "<b>rewrap</b> ()");
   DBG_OBJ_MSG_START ();

   if (wrapRefLines == -1)
      DBG_OBJ_MSG ("construct.line", 0, "does not have to be rewrapped");
   else {
      // All lines up from wrapRef will be rebuild from the word list,
      // the line list up from this position is rebuild.
      lines->setSize (wrapRefLines);
      DBG_OBJ_SET_NUM ("lines.size", lines->size ());
      nonTemporaryLines = misc::min (nonTemporaryLines, wrapRefLines);
      
      initNewLine ();
      
      int firstWord;
      if (lines->size () > 0) {
         Line *lastLine = lines->getLastRef();
         firstWord = lastLine->lastWord + 1;
      } else
         firstWord = 0;
      
      DBG_OBJ_MSGF ("construct.line", 0, "starting with word %d", firstWord);

      for (int i = firstWord; i < words->size (); i++) {
         Word *word = words->getRef (i);
         
         if (word->content.type == core::Content::WIDGET_IN_FLOW)
            calcWidgetSize (word->content.widget, &word->size);
         
         wordWrap (i, false);
         
         // Somewhat historical, but still important, note:
         //
         // For the case that something else is done with this word, it
         // is important that wordWrap() may insert some new words; since
         // NotSoSimpleVector is used for the words list, the internal
         // structure may have changed, so getRef() must be called again.
         //
         // So this is necessary: word = words->getRef (i);
      }

      // Next time, the page will not have to be rewrapped.
      wrapRefLines = -1;
      DBG_OBJ_SET_NUM ("wrapRefLines", wrapRefLines);
   }
      
   DBG_OBJ_MSG_END ();
}

/**
 * Counter part to rewrap(), but for extremes, not size calculation.
 */
void Textblock::fillParagraphs ()
{
   DBG_OBJ_MSG ("resize", 0, "<b>fillParagraphs</b>");
   DBG_OBJ_MSG_START ();

   if (wrapRefParagraphs == -1)
      return;

   // Notice that wrapRefParagraphs refers to the lines, not to the paragraphs.
   int firstWordOfLine;
   if (lines->size () > 0 && wrapRefParagraphs > 0) {
      // Sometimes, wrapRefParagraphs is larger than lines->size(), due to
      // floats? (Has to be clarified.)
      int lineNo = misc::min (wrapRefParagraphs, lines->size ()) - 1;
      firstWordOfLine = lines->getRef(lineNo)->lastWord + 1;
   } else
      firstWordOfLine = 0;

   int parNo;
   if (paragraphs->size() > 0 &&
       firstWordOfLine > paragraphs->getLastRef()->firstWord)
      // A special case: the paragraphs list has been partly built, but
      // not yet the paragraph containing the word in question. In
      // this case, only the rest of the paragraphs list must be
      // constructed. (Without this check, findParagraphOfWord would
      // return -1 in this case, so that all paragraphs would be
      // rebuilt.)
      parNo = paragraphs->size ();
   else
      // If there are no paragraphs yet, findParagraphOfWord will return
      // -1: use 0 then instead.
      parNo = misc::max (0, findParagraphOfWord (firstWordOfLine));

   paragraphs->setSize (parNo);

   int firstWord;
   if (paragraphs->size () > 0)
      firstWord = paragraphs->getLastRef()->lastWord + 1;
   else
      firstWord = 0;

   PRINTF ("[%p] FILL_PARAGRAPHS: now %d paragraphs; starting from word %d\n",
           this, parNo, firstWord);

   for (int i = firstWord; i < words->size (); i++)
      handleWordExtremes (i);

   wrapRefParagraphs = -1;
   DBG_OBJ_SET_NUM ("wrapRefParagraphs", wrapRefParagraphs);     

   DBG_OBJ_MSG_END ();
}

void Textblock::initNewLine ()
{
   DBG_OBJ_MSG ("construct.line", 0, "<b>initNewLine</b> ()");
   DBG_OBJ_MSG_START ();

   // At the very beginning, in Textblock::Textblock, where this
   // method is called, containingBlock is not yet defined.

   if (containingBlock && containingBlock->outOfFlowMgr) {
      if (lines->size () == 0) {
         int clearPosition =
            containingBlock->outOfFlowMgr->getClearPosition (this);
         setVerticalOffset (misc::max (clearPosition, 0));
      }
   }

   calcBorders (lines->size() > 0 ?
                lines->getLastRef()->lastOofRefPositionedBeforeThisLine : -1,
                1);

   newLineAscent = newLineDescent = 0;

   DBG_OBJ_SET_NUM ("newLineAscent", newLineAscent);
   DBG_OBJ_SET_NUM ("newLineDescent", newLineDescent);

   DBG_OBJ_MSG_END ();
}

void Textblock::calcBorders (int lastOofRef, int height)
{
   DBG_OBJ_MSGF ("construct.line", 0, "<b>calcBorders</b> (%d, %d)",
                 lastOofRef, height);
   DBG_OBJ_MSG_START ();

   if (containingBlock && containingBlock->outOfFlowMgr) {
      int y = yOffsetOfPossiblyMissingLine (lines->size ());

      newLineHasFloatLeft =
         containingBlock->outOfFlowMgr->hasFloatLeft (this, y, height, this,
                                                      lastOofRef);
      newLineHasFloatRight =
         containingBlock->outOfFlowMgr->hasFloatRight (this, y, height, this,
                                                       lastOofRef);
      newLineLeftBorder =
         containingBlock->outOfFlowMgr->getLeftBorder (this, y, height, this,
                                                       lastOofRef);
      newLineRightBorder =
         containingBlock->outOfFlowMgr->getRightBorder (this, y, height, this,
                                                        lastOofRef);
      
      DBG_OBJ_MSGF ("construct.line", 0,
                    "%d (%s) / %d (%s), at %d (%d), until %d\n",
                    newLineLeftBorder, newLineHasFloatLeft ? "true" : "false",
                    newLineRightBorder, newLineHasFloatRight ? "true" : "false",
                    y, height, lastOofRef);
   } else {
      newLineHasFloatLeft = newLineHasFloatRight = false;
      newLineLeftBorder = newLineRightBorder = 0;
      
      DBG_OBJ_MSG ("construct.line", 0, "<i>no CB of OOFM</i>");
   }

   DBG_OBJ_MSG_END ();
}

void Textblock::showMissingLines ()
{
   DBG_OBJ_MSG ("construct.line", 0, "<b>showMissingLines</b> ()");
   DBG_OBJ_MSG_START ();
   int firstWordToWrap =
      lines->size () > 0 ? lines->getLastRef()->lastWord + 1 : 0;
   DBG_OBJ_MSGF ("construct.line", 1, "firstWordToWrap = %d (of %d)",
                 firstWordToWrap, words->size ());

   for (int i = firstWordToWrap; i < words->size (); i++)
      wordWrap (i, true);

   // In some cases, there are some words of type WIDGET_OOF_REF left, which
   // are not added to line, since addLine() is only called within
   // wrapWordInFlow(), but not within wrapWordOofRef(). The missing line
   // is created here.

   int firstWordNotInLine =
      lines->size () > 0 ? lines->getLastRef()->lastWord + 1: 0;
   DBG_OBJ_MSGF ("construct.line", 1, "firstWordNotInLine = %d (of %d)",
                 firstWordNotInLine, words->size ());
   if (firstWordNotInLine < words->size ())
      addLine (firstWordNotInLine, words->size () -1, true);

   DBG_OBJ_MSG_END ();
}


void Textblock::removeTemporaryLines ()
{
   lines->setSize (nonTemporaryLines);
   DBG_OBJ_SET_NUM ("lines.size", lines->size ());
}

int Textblock::getSpaceShrinkability(struct Word *word)
{
   if (word->spaceStyle->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
      return word->origSpace / 3;
   else
      return 0;
}

int Textblock::getSpaceStretchability(struct Word *word)
{
   if (word->spaceStyle->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
      return word->origSpace / 2;
   else
      return 0;

   // Alternative: return word->origSpace / 2;
}

int Textblock::getLineShrinkability(Word *lastWord)
{
   return 0;
}

int Textblock::getLineStretchability(Word *lastWord)
{
   if (lastWord->spaceStyle->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
      return 0;
   else
      return stretchabilityFactor * (lastWord->maxAscent
                                     + lastWord->maxDescent) / 100;

   // Alternative: return 0;
}

} // namespace dw
