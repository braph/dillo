#include "textblock.hh"
#include "hyphenator.hh"
#include "../lout/msg.h"
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
   } else { // if (word->totalWidth > availWidth)
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
   switch (badnessState) {
   case NOT_STRETCHABLE:
      printf ("not stretchable");
      break;

   case TOO_TIGHT:
      printf ("too tight");
      break;

   case QUITE_LOOSE:
      printf ("quite loose (ratio = %d)", ratio);
      break;

   case BADNESS_VALUE:
      printf ("%d", badness);
      break;
   }

#ifdef DEBUG
   printf (" [%d + %d - %d vs. %d]",
           totalWidth, totalStretchability, totalShrinkability, idealWidth);
#endif

   printf (" + (");
   for (int i = 0; i < 2; i++) {
      if (penalty[i] == INT_MIN)
         printf ("-inf");
      else if (penalty[i] == INT_MAX)
         printf ("inf");
      else
         printf ("%d", penalty[i]);

      if (i == 0)
         printf (", ");
   }
   printf (")");
}

void Textblock::printWordShort (Word *word)
{
   core::Content::print (&(word->content));
}

void Textblock::printWordFlags (short flags)
{
   printf ("%s:%s:%s:%s:%s",
           (flags & Word::CAN_BE_HYPHENATED) ? "h?" : "--",
           (flags & Word::DIV_CHAR_AT_EOL) ? "de" : "--",
           (flags & Word::PERM_DIV_CHAR) ? "dp" : "--",
           (flags & Word::DRAW_AS_ONE_TEXT) ? "t1" : "--",
           (flags & Word::UNBREAKABLE_FOR_MIN_WIDTH) ? "um" : "--");
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
           word->size.width, word->origSpace, getStretchability(word),
           getShrinkability(word), word->totalWidth, word->totalStretchability,
           word->totalShrinkability);
   word->badnessAndPenalty.print ();
}

/*
 * ...
 *
 * diff ...
 */
void Textblock::justifyLine (Line *line, int diff)
{
   /* To avoid rounding errors, the calculation is based on accumulated
    * values. */

   if (diff > 0) {
      int stretchabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         stretchabilitySum += getStretchability(words->getRef(i));

      if (stretchabilitySum > 0) {
         int stretchabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            stretchabilityCum += getStretchability(word);
            int spaceDiff =
               stretchabilityCum * diff / stretchabilitySum - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            PRINTF ("         %d (of %d): diff = %d\n", i, words->size (),
                    spaceDiff);

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   } else if (diff < 0) {
      int shrinkabilitySum = 0;
      for (int i = line->firstWord; i < line->lastWord; i++)
         shrinkabilitySum += getShrinkability(words->getRef(i));

      if (shrinkabilitySum > 0) {
         int shrinkabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            shrinkabilityCum += getShrinkability(word);
            int spaceDiff =
               shrinkabilityCum * diff / shrinkabilitySum - spaceDiffCum;
            spaceDiffCum += spaceDiff;

            word->effSpace = word->origSpace + spaceDiff;
         }
      }
   }
}


Textblock::Line *Textblock::addLine (int firstWord, int lastWord,
                                     bool temporary)
{
   PRINTF ("[%p] ADD_LINE (%d, %d) => %d\n",
           this, firstWord, lastWord, lines->size ());

   //for (int i = firstWord; i <= lastWord; i++) {
   //   printf ("      word %d: ", i);
   //   printWord (words->getRef (i));
   //   printf ("\n");
   //}

   Word *lastWordOfLine = words->getRef(lastWord);
   // Word::totalWidth includes the hyphen (which is what we want here).
   int lineWidth = lastWordOfLine->totalWidth;
   // "lineWidth" is relative to leftOffset, so we may have to add
   // "line1OffsetEff" (remember: this is, for list items, negative).
   if (lines->size () == 0)
      lineWidth += line1OffsetEff;

   int maxOfMinWidth, sumOfMaxWidth;
   accumulateWordExtremes (firstWord, lastWord, &maxOfMinWidth,
                           &sumOfMaxWidth);

   PRINTF ("   words[%d]->totalWidth = %d\n", lastWord,
           lastWordOfLine->totalWidth);

   PRINTF ("[%p] ##### LINE ADDED: %d, from %d to %d #####\n",
           this, lines->size (), firstWord, lastWord);

   lines->increase ();
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

   alignLine (lineIndex);
   for (int i = line->firstWord; i < line->lastWord; i++) {
      Word *word = words->getRef (i);
      lineWidth += (word->effSpace - word->origSpace);
   }
  
   if (lines->size () == 1) {
      // first line
      line->top = 0;
      line->maxLineWidth = lineWidth;
   } else {
      Line *prevLine = lines->getRef (lines->size () - 2);
      line->top = prevLine->top + prevLine->boxAscent +
         prevLine->boxDescent + prevLine->breakSpace;
      line->maxLineWidth = misc::max (lineWidth, prevLine->maxLineWidth);
   }
 
   for(int i = line->firstWord; i <= line->lastWord; i++)
      accumulateWordForLine (lineIndex, i);

   // Especially empty lines (possible when there are floats) have
   // zero height, which may cause endless loops. For this reasons,
   // the height should be positive.
   line->boxAscent = misc::max (line->boxAscent, 1);

   // Calculate offsetCompleteWidget, which includes also floats.
   int leftBorder;
   if (containingBlock->outOfFlowMgr && mustBorderBeRegarded (line)) {
      int y = line->top + getStyle()->boxOffsetY();
      int h = line->boxAscent + line->boxDescent;
      leftBorder = containingBlock->outOfFlowMgr->getLeftBorder (this, y, h);
   } else
      leftBorder = 0;

   line->offsetCompleteWidget =
      misc::max (leftBorder,
                 innerPadding + (lineIndex == 0 ? line1OffsetEff : 0))
      + line->leftOffset;
   
   PRINTF ("   line[%d].top = %d\n", lines->size () - 1, line->top);
   PRINTF ("   line[%d].boxAscent = %d\n", lines->size () - 1, line->boxAscent);
   PRINTF ("   line[%d].boxDescent = %d\n",
           lines->size () - 1, line->boxDescent);
   PRINTF ("   line[%d].contentAscent = %d\n", lines->size () - 1,
           line->contentAscent);
   PRINTF ("   line[%d].contentDescent = %d\n",
           lines->size () - 1, line->contentDescent);

   PRINTF ("   line[%d].maxLineWidth = %d\n",
           lines->size () - 1, line->maxLineWidth);

   mustQueueResize = true;

   //printWordShort (words->getRef (line->firstWord));
   //printf (" ... ");
   //printWordShort (words->getRef (line->lastWord));
   //printf (": ");
   //words->getRef(line->lastWord)->badnessAndPenalty.print ();
   //printf ("\n");
   
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
   wordWrap (wordIndex, false);
   handleWordExtremes (wordIndex);
}

/*
 * This method is called in two cases: (i) when a word is added
 * (ii) when a page has to be (partially) rewrapped. It does word wrap,
 * and adds new lines if necessary.
 */
void Textblock::wordWrap (int wordIndex, bool wrapAll)
{
   PRINTF ("[%p] WORD_WRAP (%d, %s)\n",
           this, wordIndex, wrapAll ? "true" : "false");

   if (!wrapAll)
      removeTemporaryLines ();

   initLine1Offset (wordIndex);

   Word *word = words->getRef (wordIndex);
   word->effSpace = word->origSpace;

   accumulateWordData (wordIndex);

   //printf ("   ");
   //printWord (word);
   //printf ("\n");

   if (word->content.type == core::Content::WIDGET_OOF_REF)
      wrapWidgetOofRef (wordIndex);

   int penaltyIndex = calcPenaltyIndexForNewLine ();

   bool newLine;
   do {
      // This variable is set to true, if, due to floats, this line is
      // smaller than following lines will be (and, at the end, there
      // will be surely lines without floats). If this is the case, lines
      // may, in an extreme case, be left empty.

      // (In other cases, lines are never left empty, even if this means
      // that the contents is wider than the available witdh. Leaving
      // lines empty does not make sense without floats, since there will
      // be no possibility with more space anymore.)

      bool thereWillBeMoreSpace;
#if 0
      if (containingBlock->outOfFlowMgr == NULL) {
         thereWillBeMoreSpace = false;
         PRINTF ("   thereWillBeMoreSpace = false (no OOFM)\n");
      } else {
         assert (diffXToContainingBlock != -1);
         assert (restWidthToContainingBlock != -1);
         assert (diffYToContainingBlock != -1);

         int y =
            topOfPossiblyMissingLine (lines->size ()) + diffYToContainingBlock;
         int h = heightOfPossiblyMissingLine (lines->size ());

         // A previous version checked only the borders, not directly,
         // whether there are floats. The distinction is rather
         // disputable. (More on this later.)

         thereWillBeMoreSpace =
            containingBlock->outOfFlowMgr->hasFloatLeft (y, h) ||
            containingBlock->outOfFlowMgr->hasFloatRight (y, h);

         PRINTF ("   thereWillBeMoreSpace = %s (y = %d, h = %d)\n",
                 thereWillBeMoreSpace ? "true" : "false", y, h);
      }
#else
      thereWillBeMoreSpace = false;
#endif

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
         PRINTF ("   NEW LINE: last word\n");
      } else if (wordIndex >= firstIndex &&
                 // TODO: lineMustBeBroken should be independent of
                 // the penalty index?
                 word->badnessAndPenalty.lineMustBeBroken (penaltyIndex)) {
         newLine = true;
         searchUntil = wordIndex;
         PRINTF ("   NEW LINE: forced break\n");
      } else {
         // Break the line when too tight, but only when there is a
         // possible break point so far. (TODO: I've forgotten the
         // original bug which is fixed by this.)
         
         // Exception of the latter rule: thereWillBeMoreSpace; see
         // above, where it is defined.

         bool possibleLineBreak = false;
         for (int i = firstIndex;
              !(thereWillBeMoreSpace || possibleLineBreak)
                 && i <= wordIndex - 1;
              i++)
            if (words->getRef(i)->badnessAndPenalty
                .lineCanBeBroken (penaltyIndex))
               possibleLineBreak = true;

         if ((thereWillBeMoreSpace || possibleLineBreak)
             && word->badnessAndPenalty.lineTooTight ()) {
            newLine = true;
            searchUntil = wordIndex - 1;
            PRINTF ("   NEW LINE: line too tight\n");
         } else
            newLine = false;
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
         int wordIndexEnd = wordIndex;

         bool lineAdded;
         do {
            if (firstIndex > searchUntil) {
               // empty line
               assert (searchUntil == firstIndex - 1);
               addLine (firstIndex, firstIndex - 1, tempNewLine);
               lineAdded = true;
               penaltyIndex = calcPenaltyIndexForNewLine ();
            } else {
               int breakPos =
                  searchMinBap (firstIndex, searchUntil, penaltyIndex, wrapAll);
               int hyphenatedWord = considerHyphenation (breakPos);
               
               PRINTF ("[%p] breakPos = %d, hyphenatedWord = %d\n",
                       this, breakPos, hyphenatedWord);
               
               if(hyphenatedWord == -1) {
                  addLine (firstIndex, breakPos, tempNewLine);
                  PRINTF ("[%p]    new line %d (%s), from %d to %d\n",
                          this, lines->size() - 1,
                          tempNewLine ? "temporally" : "permanently",
                          firstIndex, breakPos);
                  lineAdded = true;
                  penaltyIndex = calcPenaltyIndexForNewLine ();
               } else {
                  // TODO hyphenateWord() should return whether
                  // something has changed at all. So that a second
                  // run, with !word->canBeHyphenated, is unnecessary.
                  // TODO Update: for this, searchUntil == 0 should be
                  // checked.
                  PRINTF ("[%p] old searchUntil = %d ...\n", this, searchUntil);
                  int n = hyphenateWord (hyphenatedWord);
                  searchUntil += n;
                  if (hyphenatedWord <= wordIndex)
                     wordIndexEnd += n;
                  PRINTF ("[%p] -> new searchUntil = %d ...\n",
                          this, searchUntil);
                  lineAdded = false;
               
                  // update word pointer as hyphenateWord() can trigger a
                  // reorganization of the words structure
                  word = words->getRef (wordIndex);
               }
               
               PRINTF ("[%p]       accumulating again from %d to %d\n",
                       this, breakPos + 1, wordIndexEnd);
               for(int i = breakPos + 1; i <= wordIndexEnd; i++)
                  accumulateWordData (i);
            }
         } while(!lineAdded);
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
}

void Textblock::wrapWidgetOofRef (int wordIndex)
{
   int top;
   if (lines->size() == 0)
      top = 0;
   else {
      Line *prevLine = lines->getLastRef ();
      top = prevLine->top + prevLine->boxAscent +
         prevLine->boxDescent + prevLine->breakSpace;
   }

   containingBlock->outOfFlowMgr->tellPosition
      (words->getRef(wordIndex)->content.widget,
       top + getStyle()->boxOffsetY());
   
   // TODO: compare old/new values of calcAvailWidth(...) (?)
   int firstIndex =
      lines->size() == 0 ? 0 : lines->getLastRef()->lastWord + 1;
   assert (firstIndex <= wordIndex);
   for (int i = firstIndex; i <= wordIndex; i++)
      accumulateWordData (i);
}

int Textblock::searchMinBap (int firstWord, int lastWord, int penaltyIndex,
                             bool correctAtEnd)
{
   PRINTF ("   searching from %d to %d\n", firstWord, lastWord);

   int pos = -1;

   for (int i = firstWord; i <= lastWord; i++) {
      Word *w = words->getRef(i);
      
      //printf ("      %d (of %d): ", i, words->size ());
      //printWord (w);
      //printf ("\n");
               
      if (pos == -1 ||
          w->badnessAndPenalty.compareTo (penaltyIndex,
                                          &words->getRef(pos)
                                          ->badnessAndPenalty) <= 0)
         // "<=" instead of "<" in the next lines tends to result in
         // more words per line -- theoretically. Practically, the
         // case "==" will never occur.
         pos = i;
   }

   PRINTF ("      found at %d\n", pos);

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
      if (correctedBap.compareTo(penaltyIndex,
                                 &words->getRef(pos)->badnessAndPenalty) <= 0) {
         pos = lastWord;
         PRINTF ("      corrected => %d\n", pos);
      }
   }
           
   return pos;
}


/**
 * Suggest a word to hyphenate, when breaking at breakPos is
 * planned. Return a word index or -1, when hyphenation makes no
 * sense.
 */
int Textblock::considerHyphenation (int breakPos)
{
   int hyphenatedWord = -1;

   Word *word1 = words->getRef(breakPos);
   PRINTF ("[%p] line (broken at word %d): ", this, breakPos);
   //word1->badnessAndPenalty.print ();
   PRINTF ("\n");

   // A tight line: maybe, after hyphenation, some parts of the last
   // word of this line can be put into the next line.
   if (word1->badnessAndPenalty.lineTight () &&
       isHyphenationCandidate (word1))
      hyphenatedWord = breakPos;

   // A loose line: maybe, after hyphenation, some parts of the first
   // word of the next line can be put into this line.
   if (word1->badnessAndPenalty.lineLoose () &&
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
      word->content.type == core::Content::TEXT &&
      Hyphenator::isHyphenationCandidate (word->content.text);
}

/**
 * Counter part to wordWrap(), but for extremes, not size calculation.
 */
void Textblock::handleWordExtremes (int wordIndex)
{
   // TODO Overall, clarify penalty index.

   Word *word = words->getRef (wordIndex);
   core::Extremes wordExtremes;
   getWordExtremes (word, &wordExtremes);

   //printf ("[%p] HANDLE_WORD_EXTREMES (%d): ", this, wordIndex);
   //printWordWithFlags (word);
   //printf (" => %d / %d\n", wordExtremes.minWidth, wordExtremes.maxWidth);

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

      PRINTF ("      new par: %d\n", paragraphs->size() - 1);
   }

   PRINTF ("      last par: %d\n", paragraphs->size() - 1);
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

   PRINTF ("      (lastPar from %d to %d; corrDiffMin = %d, corDiffMax = %d)\n",
           lastPar->firstWord, lastPar->lastWord, corrDiffMin, corrDiffMax);

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

   PRINTF ("   => parMin = %d (max = %d), parMax = %d (max = %d)\n",
           lastPar->parMin, lastPar->maxParMin, lastPar->parMax,
           lastPar->maxParMax);

   lastPar->lastWord = wordIndex;
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
      PRINTF ("[%p]       ... => %d words\n", this, words->size ());

      // Adjust anchor indexes.
      for (int i = 0; i < anchors->size (); i++) {
         Anchor *anchor = anchors->getRef (i);
         if (anchor->wordIndex > wordIndex)
            anchor->wordIndex += numBreaks;
      }
      
      for (int i = 0; i < numBreaks + 1; i++) {
         Word *w = words->getRef (wordIndex + i);

         fillWord (w, wordSize[i].width, wordSize[i].ascent,
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

         // Note: there are numBreaks + 1 word parts.
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
               fillSpace (w, origWord.spaceStyle);
               PRINTF ("      [%d] + space\n", wordIndex + i);
            } else {
               PRINTF ("      [%d] + nothing\n", wordIndex + i);
            }
         }
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
   PRINTF ("[%p] ACCUMULATE_WORD_DATA (%d); lineIndex = %d: ...\n",
           this, wordIndex, lineIndex);

   int availWidth = calcAvailWidth (lineIndex);

   PRINTF ("      (%s existing line %d starts with word %d)\n",
           lineIndex < lines->size () ? "already" : "not yet",
           lineIndex, firstWordOfLine);

   if (wordIndex == firstWordOfLine) {
      // first word of the (not neccessarily yet existing) line
      word->totalWidth = word->size.width + word->hyphenWidth;
      word->totalStretchability = 0;
      word->totalShrinkability = 0;
   } else {
      Word *prevWord = words->getRef (wordIndex - 1);

      word->totalWidth = prevWord->totalWidth
         + prevWord->origSpace - prevWord->hyphenWidth
         + word->size.width + word->hyphenWidth;
      word->totalStretchability =
         prevWord->totalStretchability + getStretchability(prevWord);
      word->totalShrinkability =
         prevWord->totalShrinkability + getShrinkability(prevWord);
   }

   word->badnessAndPenalty.calcBadness (word->totalWidth, availWidth,
                                        word->totalStretchability,
                                        word->totalShrinkability);

   //printf ("      => ");
   //printWord (word);
   //printf ("\n");
}

int Textblock::calcAvailWidth (int lineIndex)
{
   int availWidth = this->availWidth - innerPadding;
   if (limitTextWidth &&
       layout->getUsesViewport () &&
       // margin/border/padding will be subtracted later,  via OOFM.
       availWidth - getStyle()->boxDiffWidth()
       > layout->getWidthViewport () - 10)
      availWidth = layout->getWidthViewport () - 10;
   if (lineIndex == 0)
      availWidth -= line1OffsetEff;

   // TODO if the result is too small, but only in some cases
   // (e. g. because of floats), the caller should skip a
   // line. General distinction?
   int leftBorder, rightBorder;
   if (containingBlock->outOfFlowMgr && mustBorderBeRegarded (lineIndex)) {
      int y = topOfPossiblyMissingLine (lineIndex);
      int h = heightOfPossiblyMissingLine (lineIndex);
      leftBorder = containingBlock->outOfFlowMgr->getLeftBorder (this, y, h);
      rightBorder = containingBlock->outOfFlowMgr->getRightBorder (this, y, h);
   } else
      leftBorder = rightBorder = 0;

   availWidth -= (leftBorder + rightBorder);

   PRINTF ("[%p] CALC_AVAIL_WIDTH (%d of %d) => %d - %d - (%d + %d)"
           " = %d\n", this, lineIndex, lines->size(), this->availWidth, 
           innerPadding, leftBorder, rightBorder,
           availWidth);

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
               indent = misc::roundInt(this->availWidth *
                        core::style::perLengthVal (getStyle()->textIndent));
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
   Line *line = lines->getRef (lineIndex);
   int availWidth = calcAvailWidth (lineIndex);
   Word *firstWord = words->getRef (line->firstWord);
   Word *lastWord = words->getRef (line->lastWord);

   for (int i = line->firstWord; i < line->lastWord; i++)
      words->getRef(i)->origSpace = words->getRef(i)->effSpace;

   if (firstWord->content.type != core::Content::BREAK) {
      switch (firstWord->style->textAlign) {
      case core::style::TEXT_ALIGN_LEFT:
      case core::style::TEXT_ALIGN_STRING:   /* handled elsewhere (in the
                                              * future)? */
         line->leftOffset = 0;
         break;
      case core::style::TEXT_ALIGN_JUSTIFY:  /* see some lines above */
         line->leftOffset = 0;
         if(lastWord->content.type != core::Content::BREAK &&
            line->lastWord != words->size () - 1) {
            PRINTF ("      justifyLine => %d vs. %d\n",
                    lastWord->totalWidth, availWidth);
            justifyLine (line, availWidth - lastWord->totalWidth);
         }
         break;
      case core::style::TEXT_ALIGN_RIGHT:
         line->leftOffset = availWidth - lastWord->totalWidth;
         break;
      case core::style::TEXT_ALIGN_CENTER:
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
}

/**
 * Rewrap the page from the line from which this is necessary.
 * There are basically two times we'll want to do this:
 * either when the viewport is resized, or when the size changes on one
 * of the child widgets.
 */
void Textblock::rewrap ()
{
   PRINTF ("[%p] REWRAP: wrapRef = %d\n", this, wrapRef);

   if (wrapRefLines == -1)
      /* page does not have to be rewrapped */
      return;

   /* All lines up from wrapRef will be rebuild from the word list,
    * the line list up from this position is rebuild. */
   lines->setSize (wrapRefLines);
   nonTemporaryLines = misc::min (nonTemporaryLines, wrapRefLines);

   int firstWord;
   if (lines->size () > 0)
      firstWord = lines->getLastRef()->lastWord + 1;
   else
      firstWord = 0;

   for (int i = firstWord; i < words->size (); i++) {
      Word *word = words->getRef (i);
      if (word->content.type == core::Content::WIDGET_OOF_REF)
         containingBlock->outOfFlowMgr->tellNoPosition (word->content.widget);
   }

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

   /* Next time, the page will not have to be rewrapped. */
   wrapRefLines = -1;
}

/**
 * Counter part to rewrap(), but for extremes, not size calculation.
 */
void Textblock::fillParagraphs ()
{
   if (wrapRefParagraphs == -1)
      return;

   // Notice that wrapRefParagraphs refers to the lines, not to the paragraphs.
   int firstWordOfLine;
   if (lines->size () > 0 && wrapRefParagraphs > 0)
      firstWordOfLine = lines->getRef(wrapRefParagraphs - 1)->lastWord + 1;
   else
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
}

void Textblock::showMissingLines ()
{
   int firstWordToWrap = lines->size () > 0 ?
      lines->getRef(lines->size () - 1)->lastWord + 1 : 0;
   PRINTF ("[%p] SHOW_MISSING_LINES: wrap from %d to %d\n",
           this, firstWordToWrap, words->size () - 1);
   for (int i = firstWordToWrap; i < words->size (); i++)
      wordWrap (i, true);
}


void Textblock::removeTemporaryLines ()
{
   lines->setSize (nonTemporaryLines);
}

int Textblock::getShrinkability(struct Word *word)
{
   if (word->spaceStyle->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
      return word->origSpace / 3;
   else
      return 0;
}

int Textblock::getStretchability(struct Word *word)
{
   return word->origSpace / 2;
}

} // namespace dw
