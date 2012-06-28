#include "textblock.hh"
#include "hyphenator.hh"
#include "../lout/msg.h"
#include "../lout/misc.hh"

#include <stdio.h>
#include <math.h>

using namespace lout;

namespace dw {

int Textblock::BadnessAndPenalty::badnessInfinities ()
{
   switch (badnessState) {
   case TOO_LOOSE:
   case TOO_TIGHT:
      return 1;

   case BADNESS_VALUE:
      return 0;
   }

   // compiler happiness
   lout::misc::assertNotReached ();
   return 0;
}

int Textblock::BadnessAndPenalty::penaltyInfinities ()
{
   switch (penaltyState) {
   case FORCE_BREAK:
      return -1;

   case PROHIBIT_BREAK:
      return 1;

   case PENALTY_VALUE:
      return 0;
   }

   // compiler happiness
   lout::misc::assertNotReached ();
   return 0;
}

int Textblock::BadnessAndPenalty::badnessValue ()
{
   return badnessState == BADNESS_VALUE ? badness : 0;
}

int Textblock::BadnessAndPenalty::penaltyValue ()
{
   return penaltyState == PENALTY_VALUE ? penalty : 0;
}

void Textblock::BadnessAndPenalty::calcBadness (int totalWidth, int idealWidth,
                                                int totalStretchability,
                                                int totalShrinkability)
{
   this->totalWidth = totalWidth;
   this->idealWidth = idealWidth;
   this->totalStretchability = totalStretchability;
   this->totalShrinkability = totalShrinkability;

   if (totalWidth == idealWidth) {
      badnessState = BADNESS_VALUE;
      badness = 0;
   } else if (totalWidth < idealWidth) {
      if (totalStretchability == 0)
         badnessState = TOO_LOOSE;
      else {
         ratio = 100 * (idealWidth - totalWidth) / totalStretchability;
         if (ratio > 1024)
            badnessState = TOO_LOOSE;
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

void Textblock::BadnessAndPenalty::setPenalty (int penalty)
{
   this->penalty = penalty;
   penaltyState = PENALTY_VALUE;
}

void Textblock::BadnessAndPenalty::setPenaltyProhibitBreak ()
{
   penaltyState = PROHIBIT_BREAK;
}

void Textblock::BadnessAndPenalty::setPenaltyForceBreak ()
{
   penaltyState = FORCE_BREAK;
}

bool Textblock::BadnessAndPenalty::lineLoose ()
{
   return
      badnessState == TOO_LOOSE || (badnessState == BADNESS_VALUE && ratio > 0);
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


bool Textblock::BadnessAndPenalty::lineMustBeBroken ()
{
   return penaltyState == FORCE_BREAK;
}

bool Textblock::BadnessAndPenalty::lineCanBeBroken ()
{
   return penaltyState != PROHIBIT_BREAK;
}

int Textblock::BadnessAndPenalty::compareTo (BadnessAndPenalty *other)
{
   int thisNumInfinities = badnessInfinities () + penaltyInfinities ();
   int otherNumInfinities =
      other->badnessInfinities () + other->penaltyInfinities ();
   int thisValue = badnessValue () + penaltyValue ();
   int otherValue = other->badnessValue () + other->penaltyValue ();

   if (thisNumInfinities == otherNumInfinities)
      return thisValue - otherValue;
   else
      return thisNumInfinities - otherNumInfinities;
}

void Textblock::BadnessAndPenalty::print ()
{
   switch (badnessState) {
   case TOO_LOOSE:
      PRINTF ("loose");
      break;

   case TOO_TIGHT:
      PRINTF ("tight");
      break;

   case BADNESS_VALUE:
      PRINTF ("%d", badness);
      break;
   }

   PRINTF (" [%d + %d - %d vs. %d] + ", totalWidth, totalStretchability,
           totalShrinkability, idealWidth);

   switch (penaltyState) {
   case FORCE_BREAK:
      PRINTF ("-inf");
      break;

   case PROHIBIT_BREAK:
      PRINTF ("inf");
      break;

   case PENALTY_VALUE:
      PRINTF ("%d", penalty);
      break;
   }
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
         stretchabilitySum += words->getRef(i)->stretchability;

      if (stretchabilitySum > 0) {
         int stretchabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            stretchabilityCum += word->stretchability;
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
         shrinkabilitySum += words->getRef(i)->shrinkability;

      if (shrinkabilitySum > 0) {
         int shrinkabilityCum = 0;
         int spaceDiffCum = 0;
         for (int i = line->firstWord; i < line->lastWord; i++) {
            Word *word = words->getRef (i);
            shrinkabilityCum += word->shrinkability;
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
   PRINTF ("[%p] ADD_LINE (%d, %d)\n", this, firstWord, lastWord);

   Word *lastWordOfLine = words->getRef(lastWord);
   // Word::totalWidth includes the hyphen (which is what we want here).
   int lineWidth = lastWordOfLine->totalWidth;
   int maxOfMinWidth, sumOfMaxWidth;
   accumulateWordExtremees (firstWord, lastWord, &maxOfMinWidth,
                            &sumOfMaxWidth);

   PRINTF ("   words[%d]->totalWidth = %d\n", lastWord,
           lastWordOfLine->totalWidth);

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

   alignLine (line);
   for (int i = line->firstWord; i < line->lastWord; i++) {
      Word *word = words->getRef (i);
      lineWidth += (word->effSpace - word->origSpace);
   }
   

   if (lines->size () == 1) {
      line->top = 0;

      // TODO What to do with this one: lastLine->maxLineWidth = line1OffsetEff;
      line->maxLineWidth = lineWidth;
      line->maxParMin = maxOfMinWidth;
      line->parMax = line->maxParMax = sumOfMaxWidth;
   } else {
      Line *prevLine = lines->getRef (lines->size () - 2);

      line->top = prevLine->top + prevLine->boxAscent +
         prevLine->boxDescent + prevLine->breakSpace;

      line->maxLineWidth = misc::max (lineWidth, prevLine->maxLineWidth);
      line->maxParMin = misc::max (maxOfMinWidth, prevLine->maxParMin);

      Word *lastWordOfPrevLine = words->getRef (prevLine->lastWord);
      if (lastWordOfPrevLine->content.type == core::Content::BREAK)
         // This line starts a new paragraph.
         line->parMax = sumOfMaxWidth;
      else
         // This line continues the paragraph from prevLine.
         line->parMax = prevLine->parMax + sumOfMaxWidth;

      line->maxParMax = misc::max (line->parMax, prevLine->maxParMax);

   }
   
   for(int i = line->firstWord; i <= line->lastWord; i++)
      accumulateWordForLine (lineIndex, i);

   PRINTF ("   line[%d].top = %d\n", lines->size () - 1, line->top);
   PRINTF ("   line[%d].boxAscent = %d\n", lines->size () - 1, line->boxAscent);
   PRINTF ("   line[%d].boxDescent = %d\n",
           lines->size () - 1, line->boxDescent);
   PRINTF ("   line[%d].contentAscent = %d\n", lines->size () - 1,
           line->contentAscent);
   PRINTF ("   line[%d].contentDescent = %d\n",
           lines->size () - 1, line->contentDescent);

   mustQueueResize = true;

   return line;
}

void Textblock::accumulateWordExtremees (int firstWord, int lastWord,
                                         int *maxOfMinWidth, int *sumOfMaxWidth)
{
   *maxOfMinWidth = *sumOfMaxWidth = 0;

   for (int i = firstWord; i <= lastWord; i++) {
      Word *word = words->getRef (i);
      core::Extremes extremes;
      getWordExtremes (word, &extremes);

      *maxOfMinWidth = misc::min (*maxOfMinWidth, extremes.minWidth);
      *sumOfMaxWidth += (extremes.maxWidth + word->origSpace);
      // Regarding the sum: if this is the end of the paragraph, it
      // does not matter, since word->space is 0 in this case.
   }
}

/*
 * This method is called in two cases: (i) when a word is added
 * (ii) when a page has to be (partially) rewrapped. It does word wrap,
 * and adds new lines if necessary.
 */
void Textblock::wordWrap (int wordIndex, bool wrapAll)
{
   Word *word;
   //core::Extremes wordExtremes;

   if (!wrapAll)
      removeTemporaryLines ();

   initLine1Offset (wordIndex);

   word = words->getRef (wordIndex);
   word->effSpace = word->origSpace;

   accumulateWordData (wordIndex);

   bool newLine;
   do {
      bool tempNewLine = false;
      int firstIndex = lines->size() == 0 ?
         0 : lines->getRef(lines->size() - 1)->lastWord + 1;
      int searchUntil;

      if (wrapAll && wordIndex >= firstIndex && wordIndex == words->size() -1) {
         newLine = true;
         searchUntil = wordIndex;
         tempNewLine = true;
         PRINTF ("NEW LINE: last word\n");
      } else if (wordIndex >= firstIndex &&
          word->badnessAndPenalty.lineMustBeBroken ()) {
         newLine = true;
         searchUntil = wordIndex;
         PRINTF ("NEW LINE: forced break\n");
      } else if (wordIndex > firstIndex &&
                 word->badnessAndPenalty.lineTooTight () &&
                 words->getRef(wordIndex- 1)
                 ->badnessAndPenalty.lineCanBeBroken ()) {
         // TODO Comment the last condition (also below where the minimun is
         // searched for)
         newLine = true;
         searchUntil = wordIndex - 1;
         PRINTF ("NEW LINE: line too tight\n");
      } else
         newLine = false;

      if(newLine) {
         PRINTF ("   searching from %d to %d\n", firstIndex, searchUntil);
         
         accumulateWordData (wordIndex);

         bool lineAdded;
         do {
            int breakPos = -1;
            for (int i = firstIndex; i <= searchUntil; i++) {
               Word *w = words->getRef(i);
               
               if(word->content.type && core::Content::REAL_CONTENT) {
                  PRINTF ("      %d (of %d): ", i, words->size ());

                  switch(w->content.type) {
                  case core::Content::TEXT:
                     PRINTF ("\"%s\"", w->content.text);
                     break;
                  case core::Content::WIDGET:
                     PRINTF ("<widget: %p>\n", w->content.widget);
                     break;
                  case core::Content::BREAK:
                     PRINTF ("<break>\n");
                     break;
                  default:
                     PRINTF ("<?>\n");
                     break;                 
                  }
                  
                  PRINTF (" [%d / %d + %d - %d] => ",
                          w->size.width, w->origSpace, w->stretchability,
                          w->shrinkability);
                  w->badnessAndPenalty.print ();
                  PRINTF ("\n");
               }
               
               
               // TODO: is this condition needed:
               // if(w->badnessAndPenalty.lineCanBeBroken ()) ?
               
               if (breakPos == -1 ||
                   w->badnessAndPenalty.compareTo
                   (&words->getRef(breakPos)->badnessAndPenalty) <= 0)
                  // "<=" instead of "<" in the next lines tends to result in
                  // more words per line -- theoretically. Practically, the
                  // case "==" will never occur.
                  breakPos = i;
            }
            
            if (wrapAll && searchUntil == words->size () - 1) {
               // Since no break and no space is added, the last word
               // will have a penalty of inf. Actually, it should be -inf,
               // since it is the last word. However, since more words may
               // follow, the penalty is not changesd, but here, the search
               // is corrected (maybe only temporary).
               Word *lastWord = words->getRef (searchUntil);
               BadnessAndPenalty correctedBap = lastWord->badnessAndPenalty;
               correctedBap.setPenaltyForceBreak ();
               if (correctedBap.compareTo
                   (&words->getRef(breakPos)->badnessAndPenalty) <= 0)
                  breakPos = searchUntil;
            }

            PRINTF ("breakPos = %d\n", breakPos);

            int hyphenatedWord = -1;
            Word *word1 = words->getRef(breakPos);
            if (word1->badnessAndPenalty.lineTight () &&
                word1->canBeHyphenated &&
                word1->content.type == core::Content::TEXT &&
                Hyphenator::isHyphenationCandidate (word1->content.text))
               hyphenatedWord = breakPos;
            
            if (word1->badnessAndPenalty.lineLoose () &&
                breakPos + 1 < words->size ()) {
               Word *word2 = words->getRef(breakPos + 1);
               if (word2->canBeHyphenated &&
                   word2->content.type == core::Content::TEXT  &&
                   Hyphenator::isHyphenationCandidate (word2->content.text))
                  hyphenatedWord = breakPos + 1;
            }

            if(hyphenatedWord == -1) {
               PRINTF ("   new line from %d to %d\n", firstIndex, breakPos);
               addLine (firstIndex, breakPos, tempNewLine);
               lineAdded = true;
               PRINTF ("   accumulating again from %d to %d\n",
                       breakPos + 1, wordIndex);
            } else {
               hyphenateWord (hyphenatedWord);
               lineAdded = false;
            }
            
            for(int i = breakPos + 1; i <= wordIndex; i++)
               accumulateWordData (i);

         } while(!lineAdded);
      }
   } while (newLine);
}

void Textblock::hyphenateWord (int wordIndex)
{
   Word *word = words->getRef(wordIndex);
   printf ("   considering to hyphenate word %d: '%s'\n",
           wordIndex, word->content.text);

   Hyphenator *hyphenator =
      Hyphenator::getHyphenator (layout->getPlatform (), "de"); // TODO lang
   
   int numBreaks;
   int *breakPos = hyphenator->hyphenateWord (word->content.text, &numBreaks);
   if (numBreaks > 0) {

      // TODO unref also spaceStyle
      
      const char *origText = word->content.text;
      int lenOrigText = strlen (origText);
      core::style::Style *origStyle = word->style;
      core::Requisition wordSize[numBreaks + 1];
      
      calcTextSizes (origText, lenOrigText, origStyle, numBreaks, breakPos,
                     wordSize);
      
      printf ("      ... %d words ...\n", words->size ());
      words->insert (wordIndex, numBreaks);
      printf ("      ... -> %d words.\n", words->size ());

      for (int i = 0; i < numBreaks + 1; i++) {
         Word *w = words->getRef (wordIndex + i);

         fillWord (w, wordSize[i].width, wordSize[i].ascent,
                   wordSize[i].descent, false, origStyle);

         // TODO There should be a method fillText0.
         w->content.type = core::Content::TEXT;

         int start = (i == 0 ? 0 : breakPos[i - 1]);
         int end = (i == numBreaks ? lenOrigText : breakPos[i]);
         w->content.text =
            layout->textZone->strndup(origText + start, end - start);
         //printf ("      '%s' from %d to %d => '%s'\n",
         //        origText, start, end, w->content.text);

         printf ("      [%d] -> '%s'\n", wordIndex + i, w->content.text);

         if (i < numBreaks - 1) {
            // TODO There should be a method fillHyphen.
            w->badnessAndPenalty.setPenalty (HYPHEN_BREAK);
            w->hyphenWidth = layout->textWidth (origStyle->font, "\xc2\xad", 2);
         } else  {
            // TODO There should be a method fillSpace.
            // TODO Add original space.
#if 0
            w->badnessAndPenalty.setPenalty (0);
            w->content.space = true;
            w->effSpace = word->origSpace = origStyle->font->spaceWidth +
               origStyle->wordSpacing;
            w->stretchability = w->origSpace / 2;
            if(origStyle->textAlign == core::style::TEXT_ALIGN_JUSTIFY)
               w->shrinkability = w->origSpace / 3;
            else
               w->shrinkability = 0;
#endif
         }

         accumulateWordData (wordIndex + i);
      }

      printf ("   finished\n");
      
      //delete origText; TODO: Via textZone?
      origStyle->unref ();

      delete breakPos;
   } else
      word->canBeHyphenated = false;
}

void Textblock::accumulateWordForLine (int lineIndex, int wordIndex)
{
   Line *line = lines->getRef (lineIndex);
   Word *word = words->getRef (wordIndex);

   PRINTF ("      %d + %d / %d + %d\n", line->boxAscent, line->boxDescent,
           word->size.ascent, word->size.descent);

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

   if (word->content.type == core::Content::WIDGET) {
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

      word->content.widget->parentRef = lineIndex;
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
   PRINTF ("[%p] ACCUMULATE_WORD_DATA: %d\n", this, wordIndex);

   Word *word = words->getRef (wordIndex);
   int availWidth = calcAvailWidth (); // todo: variable? parameter?

   if (wordIndex == 0 ||
       (lines->size () > 0 &&
        wordIndex == lines->getRef(lines->size () - 1)->lastWord + 1)) {
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
         prevWord->totalStretchability + prevWord->stretchability;
      word->totalShrinkability =
         prevWord->totalShrinkability + prevWord->shrinkability;
   }

   PRINTF("      line width: %d of %d\n", word->totalWidth, availWidth);
   PRINTF("      spaces: + %d - %d\n",
          word->totalStretchability, word->totalShrinkability);

   word->badnessAndPenalty.calcBadness (word->totalWidth, availWidth,
                                        word->totalStretchability,
                                        word->totalShrinkability);
}

int Textblock::calcAvailWidth ()
{
   int availWidth =
      this->availWidth - getStyle()->boxDiffWidth() - innerPadding;
   if (limitTextWidth &&
       layout->getUsesViewport () &&
       availWidth > layout->getWidthViewport () - 10)
      availWidth = layout->getWidthViewport () - 10;

   //PRINTF("[%p] CALC_AVAIL_WIDTH => %d - %d - %d = %d\n",
   //       this, this->availWidth, getStyle()->boxDiffWidth(), innerPadding,
   //       availWidth);

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

         if (word->content.type == core::Content::WIDGET &&
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
void Textblock::alignLine (Line *line)
{
   int availWidth = calcAvailWidth ();
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

   if (wrapRef == -1)
      /* page does not have to be rewrapped */
      return;

   /* All lines up from wrapRef will be rebuild from the word list,
    * the line list up from this position is rebuild. */
   lines->setSize (wrapRef);
   nonTemporaryLines = misc::min (nonTemporaryLines, wrapRef);

   int firstWord;
   if (lines->size () > 0)
      firstWord = lines->getLastRef()->lastWord + 1;
   else
      firstWord = 0;

   for (int i = firstWord; i < words->size (); i++) {
      Word *word = words->getRef (i);
         
      if (word->content.type == core::Content::WIDGET)
         calcWidgetSize (word->content.widget, &word->size);
      
      wordWrap (i, false);
      
      if (word->content.type == core::Content::WIDGET) {
         word->content.widget->parentRef = lines->size () - 1;
      }
   }

   /* Next time, the page will not have to be rewrapped. */
   wrapRef = -1;
}

void Textblock::showMissingLines ()
{
   int firstWordToWrap = lines->size () > 0 ?
      lines->getRef(lines->size () - 1)->lastWord + 1 : 0;
   for (int i = firstWordToWrap; i < words->size (); i++)
      wordWrap (i, true);
}


void Textblock::removeTemporaryLines ()
{
   lines->setSize (nonTemporaryLines);
}

} // namespace dw
