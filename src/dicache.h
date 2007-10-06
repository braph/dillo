#ifndef __DICACHE_H__
#define __DICACHE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "bitvec.h"
#include "image.hh"
#include "cache.h"

/* These will reflect the entry's "state" */
typedef enum {
   DIC_Empty,      /* Just created the entry */
   DIC_SetParms,   /* Parameters set */
   DIC_SetCmap,    /* Color map set */
   DIC_Write,      /* Feeding the entry */
   DIC_Close,      /* Whole image got! */
   DIC_Abort       /* Image transfer aborted */
} DicEntryState;

typedef struct _DICacheEntry DICacheEntry;

struct _DICacheEntry {
   DilloUrl *url;          /* Image URL for this entry */
   uint_t width, height;   /* As taken from image data */
   DilloImgType type;      /* Image type */
   uchar_t *cmap;          /* Color map */
   uchar_t *linebuf;       /* Decompressed RGB buffer for one line */
   void *v_imgbuf;         /* Void pointer to an Imgbuf object */
   size_t TotalSize;       /* Amount of memory the image takes up */
   int Y;                  /* Current decoding row */
   bitvec_t *BitVec;       /* Bit vector for decoded rows */
   DicEntryState State;    /* Current status for this entry */
   int RefCount;           /* Reference Counter */
   int version;            /* Version number, used for different
                              versions of the same URL image */

   DICacheEntry *next;     /* Link to the next "newer" version */
};


void a_Dicache_init (void);

DICacheEntry *a_Dicache_get_entry(const DilloUrl *Url);
DICacheEntry *a_Dicache_add_entry(const DilloUrl *Url);

void a_Dicache_callback(int Op, CacheClient_t *Client);

void a_Dicache_set_parms(DilloUrl *url, int version, DilloImage *Image,
                         uint_t width, uint_t height, DilloImgType type);
void a_Dicache_set_cmap(DilloUrl *url, int version, DilloImage *Image,
                        const uchar_t *cmap, uint_t num_colors,
                        int num_colors_max, int bg_index);
void a_Dicache_write(DilloImage *Image, DilloUrl *url, int version,
                     const uchar_t *buf, int x, uint_t Y);
void a_Dicache_close(DilloUrl *url, int version, CacheClient_t *Client);

void a_Dicache_invalidate_entry(const DilloUrl *Url);
DICacheEntry* a_Dicache_ref(const DilloUrl *Url, int version);
void a_Dicache_unref(const DilloUrl *Url, int version);
void a_Dicache_cleanup(void);
void a_Dicache_freeall(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __DICACHE_H__ */
