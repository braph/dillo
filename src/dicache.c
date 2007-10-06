/*
 * File: dicache.c
 *
 * Copyright 2000-2005 Jorge Arellano Cid <jcid@dillo.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include <sys/time.h>       /* for libc5 compatibility */
#include <string.h>         /* for memset */
#include <stdio.h>
#include <stdlib.h>

#include "image.hh"
#include "web.hh"
#include "dicache.h"
#include "cache.h"
#include "prefs.h"

typedef struct _DICacheNode DICacheNode;

struct _DICacheNode {
   int valid;            /* flag */
   DilloUrl *url;        /* primary "Key" for this dicache entry */
   DICacheEntry *first;  /* pointer to the first dicache entry in this list */
};

/*
 * List of DICacheNode. One node per URL. Each node may have several
 * versions of the same image in a linked list.
 */
static Dlist *CachedIMGs = NULL;

static int dicache_size_total; /* invariant: dicache_size_total is
                                * the sum of the image sizes (3*w*h)
                                * of all the images in the dicache. */

/*
 * Compare two dicache nodes
 */
static int Dicache_node_cmp(const void *v1, const void *v2)
{
   const DICacheNode *n1 = v1, *n2 = v2;

   return a_Url_cmp(n1->url, n2->url);
}

/*
 * Compare function for searching a node by Url
 */
static int Dicache_node_by_url_cmp(const void *v1, const void *v2)
{
   const DICacheNode *node = v1;
   const DilloUrl *url = v2;

   return a_Url_cmp(node->url, url);
}

/*
 * Initialize dicache data
 */
void a_Dicache_init(void)
{
   CachedIMGs = dList_new(256);
   dicache_size_total = 0;
}

/*
 * Create, and initialize a new, empty, dicache entry
 */
static DICacheEntry *Dicache_entry_new(void)
{
   DICacheEntry *entry = dNew(DICacheEntry, 1);

   entry->width = 0;
   entry->height = 0;
   entry->type = DILLO_IMG_TYPE_NOTSET;
   entry->cmap = NULL;
   entry->linebuf = NULL;
   entry->v_imgbuf = NULL;
   entry->RefCount = 1;
   entry->TotalSize = 0;
   entry->Y = 0;
   entry->BitVec = NULL;
   entry->State = DIC_Empty;
   entry->version = 0;
   entry->next = NULL;

   return entry;
}

/*
 * Add a new entry in the dicache
 * (a single node (URL) may have several entries)
 */
DICacheEntry *a_Dicache_add_entry(const DilloUrl *Url)
{
   DICacheEntry *entry;
   DICacheNode *node;

   entry = Dicache_entry_new();

   if ((node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp))) {
      /* this URL is already in CachedIMGs, add entry at the END of the list */
      DICacheEntry *ptr = node->first;

      node->valid = 1;
      for (  ; ptr->next; ptr = ptr->next);
      ptr->next = entry;
      entry->version = ptr->version+1;
      entry->url = node->url;

   } else { /* no node yet, so create one */
      DICacheNode *node = dNew(DICacheNode, 1);

      node->url = a_Url_dup(Url);
      entry->url = node->url;
      node->first = entry;
      node->valid = 1;
      dList_insert_sorted(CachedIMGs, node, Dicache_node_cmp);
   }

   return entry;
}

/*
 * Search an entry in the dicache (given the Url).
 * Return value: a pointer to the entry of the _newest_ (i.e. highest)
 *               version if found; NULL otherwise.
 */
DICacheEntry *a_Dicache_get_entry(const DilloUrl *Url)
{
   DICacheNode *node;
   DICacheEntry *entry;

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);

   if (!node || !node->valid)
      return NULL;

   for (entry = node->first; (entry && entry->next); entry = entry->next);

   return entry;
}

/*
 * Search a particular version of a URL in the Dicache.
 * Return value: a pointer to the entry if found; NULL otherwise.
 */
static DICacheEntry *Dicache_get_entry_version(const DilloUrl *Url,
                                               int version)
{
   DICacheNode *node;
   DICacheEntry *entry;

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   entry = (node) ? node->first : NULL;

   while (entry && entry->version != version)
      entry = entry->next;

   return entry;
}

/*
 * Actually free a dicache entry, given the URL and the version number.
 */
static void Dicache_remove(const DilloUrl *Url, int version)
{
   DICacheNode *node;
   DICacheEntry *entry, *prev;

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   prev = entry = (node) ? node->first : NULL;

   while (entry && (entry->version != version) ) {
      prev = entry;
      entry = entry->next;
   }

   if (entry) {
      /* Eliminate this dicache entry */
      dFree(entry->cmap);
      dFree(entry->linebuf);
      a_Bitvec_free(entry->BitVec);
      a_Image_imgbuf_unref(entry->v_imgbuf);
      dicache_size_total -= entry->TotalSize;

      if (node->first == entry) {
         if (!entry->next) {
            /* last entry with this URL. Remove the node as well */
            dList_remove(CachedIMGs, node);
            a_Url_free(node->url);
            dFree(node);
         } else
            node->first = entry->next;
      } else {
         prev->next = entry->next;
      }
      dFree(entry);
   }
}

/*
 * Unrefs the counter of a dicache entry, and _if_ no DwImage is acessing
 * this buffer, then we call Dicache_free to do the dirty job.
 */
void a_Dicache_unref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   if ((entry = Dicache_get_entry_version(Url, version))) {
      /*if (--entry->RefCount == 0 && (entry->next || !prefs.use_dicache)) {*/
      if (--entry->RefCount == 0) {
         Dicache_remove(Url, version);
      }
   }
}

/*
 * Refs the counter of a dicache entry.
 */

DICacheEntry* a_Dicache_ref(const DilloUrl *Url, int version)
{
   DICacheEntry *entry;

   if ((entry = Dicache_get_entry_version(Url, version))) {
      ++entry->RefCount;
   }
   return entry;
}

/*
 * Invalidate this entry. This is used for the reloading mechanism.
 * Can't erase current versions, but a_Dicache_get_entry must return NULL.
 */
void a_Dicache_invalidate_entry(const DilloUrl *Url)
{
   DICacheNode *node;

   node = dList_find_sorted(CachedIMGs, Url, Dicache_node_by_url_cmp);
   if (node)
      node->valid = 0;
}


/* ------------------------------------------------------------------------- */

/*
 * This function is a cache client; (but feeds its clients from dicache)
 */
void a_Dicache_callback(int Op, CacheClient_t *Client)
{
   /* todo: Handle Op = CA_Abort (to show what was got)  --Jcid */
   uint_t i;
   DilloWeb *Web = Client->Web;
   DilloImage *Image = Web->Image;
   DICacheEntry *DicEntry = a_Dicache_get_entry(Web->url);

   dReturn_if_fail ( DicEntry != NULL );

   /* when the data stream is not an image 'v_imgbuf' keeps NULL */
   if (Op == CA_Send && DicEntry->v_imgbuf) {
      if (Image->height == 0 && DicEntry->State >= DIC_SetParms) {
         /* Set parms */
         a_Image_set_parms(
            Image, DicEntry->v_imgbuf, DicEntry->url,
            DicEntry->version, DicEntry->width, DicEntry->height,
            DicEntry->type);
      }
      if (DicEntry->State == DIC_Write) {
         for (i = 0; i < DicEntry->height; ++i)
            if (a_Bitvec_get_bit(DicEntry->BitVec, (int)i) &&
                !a_Bitvec_get_bit(Image->BitVec, (int)i) )
               a_Image_write(Image, DicEntry->v_imgbuf,
                             DicEntry->linebuf, i, FALSE);
      }
   } else if (Op == CA_Close || Op == CA_Abort) {
      a_Image_close(Web->Image);
      a_Bw_close_client(Web->bw, Client->Key);
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Set image's width, height & type
 * (By now, we'll use the image information despite the html tags --Jcid)
 */
void a_Dicache_set_parms(DilloUrl *url, int version, DilloImage *Image,
                         uint_t width, uint_t height, DilloImgType type)
{
   DICacheEntry *DicEntry;
   size_t Size = width * height * 3;

   dReturn_if_fail ( Image != NULL && width && height );
   /* Find the DicEntry for this Image */
   DicEntry = Dicache_get_entry_version(url, version);
   dReturn_if_fail ( DicEntry != NULL );

   /* Initialize the DicEntry */
   DicEntry->linebuf = dNew(uchar_t, width * 3);
   dReturn_if_fail ( DicEntry->linebuf != NULL );

   /* BUG: there's just one image-type now */
   #define I_RGB 0
   DicEntry->v_imgbuf = a_Image_imgbuf_new(Image->dw, I_RGB, width, height);

   /* This extra reference activates the dicache ALWAYS.
    * Extra code is necessary in Imgbuf to be able to free it */
   //a_Image_imgbuf_ref(DicEntry->v_imgbuf);

   DicEntry->TotalSize = Size;
   DicEntry->width = width;
   DicEntry->height = height;
   DicEntry->type = type;
   DicEntry->BitVec = a_Bitvec_new((int)height);
   DicEntry->State = DIC_SetParms;

   dicache_size_total += Size;

   /* Allocate and initialize this image */
   a_Image_set_parms(Image, DicEntry->v_imgbuf, url, version,
                     width, height, type);
}

/*
 * Implement the set_cmap method for the Image
 */
void a_Dicache_set_cmap(DilloUrl *url, int version, DilloImage *Image,
                        const uchar_t *cmap, uint_t num_colors,
                        int num_colors_max, int bg_index)
{
   DICacheEntry *DicEntry = Dicache_get_entry_version(url, version);

   dReturn_if_fail ( DicEntry != NULL );

   dFree(DicEntry->cmap);
   DicEntry->cmap = dNew0(uchar_t, 3 * num_colors_max);
   memcpy(DicEntry->cmap, cmap, 3 * num_colors);
   if (bg_index >= 0 && (uint_t)bg_index < num_colors) {
      DicEntry->cmap[bg_index * 3]     = (Image->bg_color >> 16) & 0xff;
      DicEntry->cmap[bg_index * 3 + 1] = (Image->bg_color >> 8) & 0xff;
      DicEntry->cmap[bg_index * 3 + 2] = (Image->bg_color) & 0xff;
   }

   a_Image_set_cmap(Image, DicEntry->cmap);
   DicEntry->State = DIC_SetCmap;
}

/*
 * Implement the write method
 * (Write a scan line into the Dicache entry)
 * buf: row buffer
 * Y  : row number
 * x  : horizontal offset? (always zero)
 */
void a_Dicache_write(DilloImage *Image, DilloUrl *url, int version,
                     const uchar_t *buf, int x, uint_t Y)
{
   DICacheEntry *DicEntry;

   dReturn_if_fail ( Image != NULL );
   DicEntry = Dicache_get_entry_version(url, version);
   dReturn_if_fail ( DicEntry != NULL );
   dReturn_if_fail ( DicEntry->width > 0 && DicEntry->height > 0 );

   a_Image_write(Image, DicEntry->v_imgbuf, buf, Y, TRUE);
   DicEntry->Y = Y;
   a_Bitvec_set_bit(DicEntry->BitVec, (int)Y);
   DicEntry->State = DIC_Write;
}

/*
 * Implement the close method of the decoding process
 */
void a_Dicache_close(DilloUrl *url, int version, CacheClient_t *Client)
{
   DilloWeb *Web = Client->Web;
   DICacheEntry *DicEntry = Dicache_get_entry_version(url, version);

   dReturn_if_fail ( DicEntry != NULL );

   DicEntry->State = DIC_Close;
   dFree(DicEntry->cmap);
   DicEntry->cmap = NULL;
   dFree(DicEntry->linebuf);
   DicEntry->linebuf = NULL;
   a_Image_close(Web->Image);
   a_Bw_close_client(Web->bw, Client->Key);
}

/*
 * Free the imgbuf (RGB data) of unused entries.
 */
void a_Dicache_cleanup(void)
{
   int i;
   DICacheNode *node;
   DICacheEntry *entry;

   for (i = 0; i < dList_length(CachedIMGs); ++i) {
      node = dList_nth_data(CachedIMGs, i);
      /* iterate each entry of this node */
      for (entry = node->first; entry; entry = entry->next) {
         if (entry->v_imgbuf &&
             a_Image_imgbuf_last_reference(entry->v_imgbuf)) {
            /* free this unused entry */
            if (entry->next) {
               Dicache_remove(node->url, entry->version);
            } else {
               Dicache_remove(node->url, entry->version);
               --i;
               break;
            }
         }
      }
   }
}

/* ------------------------------------------------------------------------- */

/*
 * Deallocate memory used by dicache module
 * (Call this one at exit time)
 */
void a_Dicache_freeall(void)
{
   DICacheNode *node;
   DICacheEntry *entry;

   /* Remove every dicache node and its entries */
   while ((node = dList_nth_data(CachedIMGs, 0))) {
      while ((entry = node->first)) {
         node->first = entry->next;
         dFree(entry->cmap);
         dFree(entry->linebuf);
         a_Bitvec_free(entry->BitVec);
         a_Image_imgbuf_unref(entry->v_imgbuf);
         dicache_size_total -= entry->TotalSize;
      }
      dList_remove(CachedIMGs, node);
      a_Url_free(node->url);
      dFree(node);
   }
   dList_free(CachedIMGs);
}
