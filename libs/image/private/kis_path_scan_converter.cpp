/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

/***************************************************************************/
/*                                                                         */
/*  qgrayraster.c, derived from ftgrays.c                                  */
/*                                                                         */
/*    A new `perfect' anti-aliasing renderer (body).                       */
/*                                                                         */
/*  Copyright 2000-2016 by                                                 */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, ../../3rdparty/freetype/docs/FTL.TXT.  By continuing to use,  */
/*  modify, or distribute this file you indicate that you have read        */
/*  the license and understand and accept it fully.                        */
/*                                                                         */
/***************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* This file can be compiled without the rest of the FreeType engine, by */
  /* defining the _STANDALONE_ macro when compiling it.  You also need to  */
  /* put the files `ftgrays.h' and `ftimage.h' into the current            */
  /* compilation directory.  Typically, you could do something like        */
  /*                                                                       */
  /* - copy `src/smooth/ftgrays.c' (this file) to your current directory   */
  /*                                                                       */
  /* - copy `include/freetype/ftimage.h' and `src/smooth/ftgrays.h' to the */
  /*   same directory                                                      */
  /*                                                                       */
  /* - compile `ftgrays' with the _STANDALONE_ macro defined, as in        */
  /*                                                                       */
  /*     cc -c -D_STANDALONE_ ftgrays.c                                    */
  /*                                                                       */
  /* The renderer can be initialized with a call to                        */
  /* `qt_ft_gray_raster.raster_new'; an anti-aliased bitmap can be generated  */
  /* with a call to `qt_ft_gray_raster.raster_render'.                        */
  /*                                                                       */
  /* See the comments and documentation in the file `ftimage.h' for more   */
  /* details on how the raster works.                                      */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* This is a new anti-aliasing scan-converter for FreeType 2.  The       */
  /* algorithm used here is _very_ different from the one in the standard  */
  /* `ftraster' module.  Actually, `ftgrays' computes the _exact_          */
  /* coverage of the outline on each pixel cell.                           */
  /*                                                                       */
  /* It is based on ideas that I initially found in Raph Levien's          */
  /* excellent LibArt graphics library (see http://www.levien.com/libart   */
  /* for more information, though the web pages do not tell anything       */
  /* about the renderer; you'll have to dive into the source code to       */
  /* understand how it works).                                             */
  /*                                                                       */
  /* Note, however, that this is a _very_ different implementation         */
  /* compared to Raph's.  Coverage information is stored in a very         */
  /* different way, and I don't use sorted vector paths.  Also, it doesn't */
  /* use floating point values.                                            */
  /*                                                                       */
  /* This renderer has the following advantages:                           */
  /*                                                                       */
  /* - It doesn't need an intermediate bitmap.  Instead, one can supply a  */
  /*   callback function that will be called by the renderer to draw gray  */
  /*   spans on any target surface.  You can thus do direct composition on */
  /*   any kind of bitmap, provided that you give the renderer the right   */
  /*   callback.                                                           */
  /*                                                                       */
  /* - A perfect anti-aliaser, i.e., it computes the _exact_ coverage on   */
  /*   each pixel cell.                                                    */
  /*                                                                       */
  /* - It performs a single pass on the outline (the `standard' FT2        */
  /*   renderer makes two passes).                                         */
  /*                                                                       */
  /* - It can easily be modified to render to _any_ number of gray levels  */
  /*   cheaply.                                                            */
  /*                                                                       */
  /* - For small (< 20) pixel sizes, it is faster than the standard        */
  /*   renderer.                                                           */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* The macro QT_FT_COMPONENT is used in trace mode.  It is an implicit      */
  /* parameter of the QT_FT_TRACE() and QT_FT_ERROR() macros, used to print/log  */
  /* messages during execution.                                            */
  /*                                                                       */
#undef  QT_FT_COMPONENT
#define QT_FT_COMPONENT  trace_smooth


/* Auxiliary macros for token concatenation. */
#define QT_FT_ERR_XCAT( x, y )  x ## y
#define QT_FT_ERR_CAT( x, y )   QT_FT_ERR_XCAT( x, y )

#define QT_FT_BEGIN_STMNT  do {
#define QT_FT_END_STMNT    } while ( 0 )

#define QT_FT_MAX( a, b )  ( (a) > (b) ? (a) : (b) )
#define QT_FT_ABS( a )     ( (a) < 0 ? -(a) : (a) )


/*
 *  Approximate sqrt(x*x+y*y) using the `alpha max plus beta min'
 *  algorithm.  We use alpha = 1, beta = 3/8, giving us results with a
 *  largest error less than 7% compared to the exact value.
 */
#define QT_FT_HYPOT( x, y )                 \
        ( x = QT_FT_ABS( x ),             \
          y = QT_FT_ABS( y ),             \
          x > y ? x + ( 3 * y >> 3 )   \
                : y + ( 3 * x >> 3 ) )

#define ErrRaster_MemoryOverflow   -4

#if defined(VXWORKS)
#  include <vxWorksCommon.h>    /* needed for setjmp.h */
#endif
#include <string.h>             /* for qt_ft_memcpy() */
#include <setjmp.h>
#include <limits.h>

#define QT_FT_UINT_MAX  UINT_MAX

#define qt_ft_memset   memset

#define qt_ft_setjmp   setjmp
#define qt_ft_longjmp  longjmp
#define qt_ft_jmp_buf  jmp_buf

#include <stddef.h>
typedef ptrdiff_t  QT_FT_PtrDist;

#define ErrRaster_Invalid_Mode      -2
#define ErrRaster_Invalid_Outline   -1
#define ErrRaster_Invalid_Argument  -3
#define ErrRaster_Memory_Overflow   -4
#define ErrRaster_OutOfMemory       -6

#define QT_FT_BEGIN_HEADER
#define QT_FT_END_HEADER

#include "kis_path_scan_converter_p.h"

#include <PkPainterPath.h>
#include <PkRect.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace KisPathRasterizer::Private {

using QT_FT_Pos = int;
using QT_FT_Int = int;

struct QT_FT_Vector { QT_FT_Pos x; QT_FT_Pos y; };
struct QT_FT_BBox { QT_FT_Pos xMin, yMin, xMax, yMax; };
struct QT_FT_Bitmap {
    int rows;
    int width;
    int pitch;
    unsigned char *buffer;
    short num_grays;
    char pixel_mode;
    char palette_mode;
    void *palette;
};
struct QT_FT_Outline {
    int n_contours;
    int n_points;
    QT_FT_Vector *points;
    char *tags;
    int *contours;
    int flags;
};

#define QT_FT_OUTLINE_NONE 0x0
#define QT_FT_OUTLINE_EVEN_ODD_FILL 0x2
#define QT_FT_CURVE_TAG(flag) ((flag) & 3)
#define QT_FT_CURVE_TAG_ON 1
#define QT_FT_CURVE_TAG_CONIC 0
#define QT_FT_CURVE_TAG_CUBIC 2

struct QT_FT_Span {
    short x;
    unsigned short len;
    short y;
    unsigned char coverage;
};
using QT_FT_SpanFunc = void (*)(int, const QT_FT_Span *, void *);
#define QT_FT_Raster_Span_Func QT_FT_SpanFunc

struct TRaster_;
using QT_FT_Raster = TRaster_ *;
using QT_FT_Raster_BitTest_Func = int (*)(int, int, void *);
using QT_FT_Raster_BitSet_Func = void (*)(int, int, void *);

#define QT_FT_RASTER_FLAG_AA 0x1
#define QT_FT_RASTER_FLAG_DIRECT 0x2
#define QT_FT_RASTER_FLAG_CLIP 0x4

struct QT_FT_Raster_Params {
    QT_FT_Bitmap *target;
    void *source;
    int flags;
    QT_FT_SpanFunc gray_spans;
    QT_FT_SpanFunc black_spans;
    QT_FT_Raster_BitTest_Func bit_test;
    QT_FT_Raster_BitSet_Func bit_set;
    void *user;
    QT_FT_BBox clip_box;
    int skip_spans;
};

enum QT_FT_Glyph_Format { QT_FT_GLYPH_FORMAT_OUTLINE = 0x6f75746c };
using QT_FT_Raster_NewFunc = int (*)(QT_FT_Raster *);
using QT_FT_Raster_ResetFunc = void (*)(QT_FT_Raster, unsigned char *, unsigned long);
using QT_FT_Raster_SetModeFunc = int (*)(QT_FT_Raster, unsigned long, void *);
using QT_FT_Raster_RenderFunc = int (*)(QT_FT_Raster, QT_FT_Raster_Params *);
using QT_FT_Raster_DoneFunc = void (*)(QT_FT_Raster);
#define QT_FT_Raster_New_Func QT_FT_Raster_NewFunc
#define QT_FT_Raster_Reset_Func QT_FT_Raster_ResetFunc
#define QT_FT_Raster_Set_Mode_Func QT_FT_Raster_SetModeFunc
#define QT_FT_Raster_Render_Func QT_FT_Raster_RenderFunc
#define QT_FT_Raster_Done_Func QT_FT_Raster_DoneFunc
#define MINIMUM_POOL_SIZE 8192
#define Q_FALLTHROUGH() [[fallthrough]]
struct QT_FT_Raster_Funcs {
    QT_FT_Glyph_Format glyph_format;
    QT_FT_Raster_NewFunc raster_new;
    QT_FT_Raster_ResetFunc raster_reset;
    QT_FT_Raster_SetModeFunc raster_set_mode;
    QT_FT_Raster_RenderFunc raster_render;
    QT_FT_Raster_DoneFunc raster_done;
};

#define QT_FT_UNUSED( x )  (void) x

#define QT_FT_TRACE5( x )  do { } while ( 0 )     /* nothing */
#define QT_FT_TRACE7( x )  do { } while ( 0 )     /* nothing */
#define QT_FT_ERROR( x )   do { } while ( 0 )     /* nothing */
#define QT_FT_THROW( e )   QT_FT_ERR_CAT( ErrRaster_, e )

#ifndef QT_FT_MEM_SET
#define QT_FT_MEM_SET( d, s, c )  qt_ft_memset( d, s, c )
#endif

#ifndef QT_FT_MEM_ZERO
#define QT_FT_MEM_ZERO( dest, count )  QT_FT_MEM_SET( dest, 0, count )
#endif


#define RAS_ARG   PWorker  worker
#define RAS_ARG_  PWorker  worker,

#define RAS_VAR   worker
#define RAS_VAR_  worker,

#define ras       (*worker)

  /* must be at least 6 bits! */
#define PIXEL_BITS  8

#define ONE_PIXEL       ( 1L << PIXEL_BITS )
#define TRUNC( x )      ( (TCoord)( (x) >> PIXEL_BITS ) )
#define SUBPIXELS( x )  ( (TPos)(x) * ONE_PIXEL )
#define FLOOR( x )      ( (x) & -ONE_PIXEL )
#define CEILING( x )    ( ( (x) + ONE_PIXEL - 1 ) & -ONE_PIXEL )
#define ROUND( x )      ( ( (x) + ONE_PIXEL / 2 ) & -ONE_PIXEL )

#if PIXEL_BITS >= 6
#define UPSCALE( x )    ( (x) * ( ONE_PIXEL >> 6 ) )
#define DOWNSCALE( x )  ( (x) >> ( PIXEL_BITS - 6 ) )
#else
#define UPSCALE( x )    ( (x) >> ( 6 - PIXEL_BITS ) )
#define DOWNSCALE( x )  ( (x) * ( 64 >> PIXEL_BITS ) )
#endif

/* Compute `dividend / divisor' and return both its quotient and     */
/* remainder, cast to a specific type.  This macro also ensures that */
/* the remainder is always positive.                                 */
#define QT_FT_DIV_MOD( type, dividend, divisor, quotient, remainder ) \
QT_FT_BEGIN_STMNT                                                   \
  (quotient)  = (type)( (dividend) / (divisor) );                \
  (remainder) = (type)( (dividend) % (divisor) );                \
  if ( (remainder) < 0 )                                         \
  {                                                              \
    (quotient)--;                                                \
    (remainder) += (type)(divisor);                              \
  }                                                              \
QT_FT_END_STMNT

  /* These macros speed up repetitive divisions by replacing them */
  /* with multiplications and right shifts.                       */
#define QT_FT_UDIVPREP( b )                                       \
  long  b ## _r = (long)( ULONG_MAX >> PIXEL_BITS ) / ( b )
#define QT_FT_UDIV( a, b )                                        \
  ( ( (unsigned long)( a ) * (unsigned long)( b ## _r ) ) >>   \
    ( sizeof( long ) * CHAR_BIT - PIXEL_BITS ) )


  /*************************************************************************/
  /*                                                                       */
  /*   TYPE DEFINITIONS                                                    */
  /*                                                                       */

  /* don't change the following types to QT_FT_Int or QT_FT_Pos, since we might */
  /* need to define them to "float" or "double" when experimenting with   */
  /* new algorithms                                                       */

  typedef long   TCoord;   /* integer scanline/pixel coordinate */
  typedef long   TPos;     /* sub-pixel coordinate              */
  typedef long   TArea ;   /* cell areas, coordinate products   */

  /* maximal number of gray spans in a call to the span callback */
#define QT_FT_MAX_GRAY_SPANS  256


  typedef struct TCell_*  PCell;

  typedef struct  TCell_
  {
    int    x;
    int    cover;
    TArea  area;
    PCell  next;

  } TCell;


  typedef struct  TWorker_
  {
    TCoord  ex, ey;
    TPos    min_ex, max_ex;
    TPos    min_ey, max_ey;
    TPos    count_ex, count_ey;

    TArea   area;
    int     cover;
    int     invalid;

    PCell   cells;
    QT_FT_PtrDist     max_cells;
    QT_FT_PtrDist     num_cells;

    TPos    x,  y;

    QT_FT_Outline  outline;
    QT_FT_Bitmap   target;
    QT_FT_BBox     clip_box;

    QT_FT_Span     gray_spans[QT_FT_MAX_GRAY_SPANS];
    int         num_gray_spans;

    QT_FT_Raster_Span_Func  render_span;
    void*                render_span_data;

    int  band_size;
    int  band_shoot;

    qt_ft_jmp_buf  jump_buffer;

    void*       buffer;
    long        buffer_size;

    PCell*     ycells;
    TPos       ycount;

    int        skip_spans;
  } TWorker, *PWorker;


  typedef struct TRaster_
  {
    void*    buffer;
    long     buffer_size;
    long     buffer_allocated_size;
    int      band_size;
    void*    memory;
    PWorker  worker;

  } TRaster, *PRaster;

  int q_gray_rendered_spans(TRaster *raster)
  {
    if ( raster && raster->worker )
      return raster->worker->skip_spans > 0 ? 0 : -raster->worker->skip_spans;
    return 0;
  }

  /*************************************************************************/
  /*                                                                       */
  /* Initialize the cells table.                                           */
  /*                                                                       */
  static void
  gray_init_cells( RAS_ARG_ void*  buffer,
                            long   byte_size )
  {
    ras.buffer      = buffer;
    ras.buffer_size = byte_size;

    ras.ycells      = (PCell*) buffer;
    ras.cells       = NULL;
    ras.max_cells   = 0;
    ras.num_cells   = 0;
    ras.area        = 0;
    ras.cover       = 0;
    ras.invalid     = 1;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Compute the outline bounding box.                                     */
  /*                                                                       */
  static void
  gray_compute_cbox( RAS_ARG )
  {
    QT_FT_Outline*  outline = &ras.outline;
    QT_FT_Vector*   vec     = outline->points;
    QT_FT_Vector*   limit   = vec + outline->n_points;


    if ( outline->n_points <= 0 )
    {
      ras.min_ex = ras.max_ex = 0;
      ras.min_ey = ras.max_ey = 0;
      return;
    }

    ras.min_ex = ras.max_ex = vec->x;
    ras.min_ey = ras.max_ey = vec->y;

    vec++;

    for ( ; vec < limit; vec++ )
    {
      TPos  x = vec->x;
      TPos  y = vec->y;


      if ( x < ras.min_ex ) ras.min_ex = x;
      if ( x > ras.max_ex ) ras.max_ex = x;
      if ( y < ras.min_ey ) ras.min_ey = y;
      if ( y > ras.max_ey ) ras.max_ey = y;
    }

    /* truncate the bounding box to integer pixels */
    ras.min_ex = ras.min_ex >> 6;
    ras.min_ey = ras.min_ey >> 6;
    ras.max_ex = ( ras.max_ex + 63 ) >> 6;
    ras.max_ey = ( ras.max_ey + 63 ) >> 6;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Record the current cell in the table.                                 */
  /*                                                                       */
  static PCell
  gray_find_cell( RAS_ARG )
  {
    PCell  *pcell, cell;
    TPos    x = ras.ex;


    if ( x > ras.count_ex )
      x = ras.count_ex;

    pcell = &ras.ycells[ras.ey];
    for (;;)
    {
      cell = *pcell;
      if ( cell == NULL || cell->x > x )
        break;

      if ( cell->x == x )
        goto Exit;

      pcell = &cell->next;
    }

    if ( ras.num_cells >= ras.max_cells )
      qt_ft_longjmp( ras.jump_buffer, 1 );

    cell        = ras.cells + ras.num_cells++;
    cell->x     = x;
    cell->area  = 0;
    cell->cover = 0;

    cell->next  = *pcell;
    *pcell      = cell;

  Exit:
    return cell;
  }


  static void
  gray_record_cell( RAS_ARG )
  {
    if ( ras.area | ras.cover )
    {
      PCell  cell = gray_find_cell( RAS_VAR );


      cell->area  += ras.area;
      cell->cover += ras.cover;
    }
  }


  /*************************************************************************/
  /*                                                                       */
  /* Set the current cell to a new position.                               */
  /*                                                                       */
  static void
  gray_set_cell( RAS_ARG_ TCoord  ex,
                          TCoord  ey )
  {
    /* Move the cell pointer to a new position.  We set the `invalid'      */
    /* flag to indicate that the cell isn't part of those we're interested */
    /* in during the render phase.  This means that:                       */
    /*                                                                     */
    /* . the new vertical position must be within min_ey..max_ey-1.        */
    /* . the new horizontal position must be strictly less than max_ex     */
    /*                                                                     */
    /* Note that if a cell is to the left of the clipping region, it is    */
    /* actually set to the (min_ex-1) horizontal position.                 */

    /* All cells that are on the left of the clipping region go to the */
    /* min_ex - 1 horizontal position.                                 */
    ey -= ras.min_ey;

    if ( ex > ras.max_ex )
      ex = ras.max_ex;

    ex -= ras.min_ex;
    if ( ex < 0 )
      ex = -1;

    /* are we moving to a different cell ? */
    if ( ex != ras.ex || ey != ras.ey )
    {
      /* record the current one if it is valid */
      if ( !ras.invalid )
        gray_record_cell( RAS_VAR );

      ras.area  = 0;
      ras.cover = 0;
      ras.ex    = ex;
      ras.ey    = ey;
    }

    ras.invalid = ( (unsigned int)ey >= (unsigned int)ras.count_ey ||
                                  ex >= ras.count_ex           );
  }


  /*************************************************************************/
  /*                                                                       */
  /* Start a new contour at a given cell.                                  */
  /*                                                                       */
  static void
  gray_start_cell( RAS_ARG_ TCoord  ex,
                            TCoord  ey )
  {
    if ( ex > ras.max_ex )
      ex = (TCoord)( ras.max_ex );

    if ( ex < ras.min_ex )
      ex = (TCoord)( ras.min_ex - 1 );

    ras.area    = 0;
    ras.cover   = 0;
    ras.ex      = ex - ras.min_ex;
    ras.ey      = ey - ras.min_ey;
    ras.invalid = 0;

    gray_set_cell( RAS_VAR_ ex, ey );
  }

// The new render-line implementation is not yet used
#if 1

  /*************************************************************************/
  /*                                                                       */
  /* Render a scanline as one or more cells.                               */
  /*                                                                       */
  static void
  gray_render_scanline( RAS_ARG_ TCoord  ey,
                                 TPos    x1,
                                 TCoord  y1,
                                 TPos    x2,
                                 TCoord  y2 )
  {
    TCoord  ex1, ex2, fx1, fx2, delta, mod;
    int     p, first, dx;
    int     incr;


    dx = x2 - x1;

    ex1 = TRUNC( x1 );
    ex2 = TRUNC( x2 );
    fx1 = (TCoord)( x1 - SUBPIXELS( ex1 ) );
    fx2 = (TCoord)( x2 - SUBPIXELS( ex2 ) );

    /* trivial case.  Happens often */
    if ( y1 == y2 )
    {
      gray_set_cell( RAS_VAR_ ex2, ey );
      return;
    }

    /* everything is located in a single cell.  That is easy! */
    /*                                                        */
    if ( ex1 == ex2 )
    {
      delta      = y2 - y1;
      ras.area  += (TArea)( fx1 + fx2 ) * delta;
      ras.cover += delta;
      return;
    }

    /* ok, we'll have to render a run of adjacent cells on the same */
    /* scanline...                                                  */
    /*                                                              */
    p     = ( ONE_PIXEL - fx1 ) * ( y2 - y1 );
    first = ONE_PIXEL;
    incr  = 1;

    if ( dx < 0 )
    {
      p     = fx1 * ( y2 - y1 );
      first = 0;
      incr  = -1;
      dx    = -dx;
    }

    QT_FT_DIV_MOD( TCoord, p, dx, delta, mod );

    ras.area  += (TArea)( fx1 + first ) * delta;
    ras.cover += delta;

    ex1 += incr;
    gray_set_cell( RAS_VAR_ ex1, ey );
    y1  += delta;

    if ( ex1 != ex2 )
    {
      TCoord  lift, rem;


      p = ONE_PIXEL * ( y2 - y1 + delta );
      QT_FT_DIV_MOD( TCoord, p, dx, lift, rem );

      mod -= (int)dx;

      while ( ex1 != ex2 )
      {
        delta = lift;
        mod  += rem;
        if ( mod >= 0 )
        {
          mod -= (TCoord)dx;
          delta++;
        }

        ras.area  += (TArea)ONE_PIXEL * delta;
        ras.cover += delta;
        y1        += delta;
        ex1       += incr;
        gray_set_cell( RAS_VAR_ ex1, ey );
      }
    }

    delta      = y2 - y1;
    ras.area  += (TArea)( fx2 + ONE_PIXEL - first ) * delta;
    ras.cover += delta;
  }


  /*************************************************************************/
  /*                                                                       */
  /* Render a given line as a series of scanlines.                         */
  /*                                                                       */
  static void
  gray_render_line( RAS_ARG_ TPos  to_x,
                             TPos  to_y )
  {
    TCoord  ey1, ey2, fy1, fy2, mod;
    TPos    dx, dy, x, x2;
    int     p, first;
    int     delta, rem, lift, incr;


    ey1 = TRUNC( ras.y );
    ey2 = TRUNC( to_y );     /* if (ey2 >= ras.max_ey) ey2 = ras.max_ey-1; */
    fy1 = (TCoord)( ras.y - SUBPIXELS( ey1 ) );
    fy2 = (TCoord)( to_y - SUBPIXELS( ey2 ) );

    dx = to_x - ras.x;
    dy = to_y - ras.y;

    /* perform vertical clipping */
    if ( ( ey1 >= ras.max_ey && ey2 >= ras.max_ey ) ||
         ( ey1 <  ras.min_ey && ey2 <  ras.min_ey ) )
      goto End;

    /* everything is on a single scanline */
    if ( ey1 == ey2 )
    {
      gray_render_scanline( RAS_VAR_ ey1, ras.x, fy1, to_x, fy2 );
      goto End;
    }

    /* vertical line - avoid calling gray_render_scanline */
    if ( dx == 0 )
    {
      TCoord  ex     = TRUNC( ras.x );
      TCoord  two_fx = (TCoord)( ( ras.x - SUBPIXELS( ex ) ) << 1 );
      TPos    area, max_ey1;


      first = ONE_PIXEL;
      if ( dy < 0 )
        first = 0;

      delta      = (int)( first - fy1 );
      ras.area  += (TArea)two_fx * delta;
      ras.cover += delta;

      delta = (int)( first + first - ONE_PIXEL );
      area  = (TArea)two_fx * delta;
      max_ey1 = ras.count_ey + ras.min_ey;
      if (dy < 0) {
        if (ey1 > max_ey1) {
          ey1 = (max_ey1 > ey2) ? max_ey1 : ey2;
          gray_set_cell( &ras, ex, ey1 );
        } else {
          ey1--;
          gray_set_cell( &ras, ex, ey1 );
        }
        while ( ey1 > ey2 && ey1 >= ras.min_ey)
        {
          ras.area  += area;
          ras.cover += delta;
          ey1--;

          gray_set_cell( &ras, ex, ey1 );
        }
        if (ey1 != ey2) {
          ey1 = ey2;
          gray_set_cell( &ras, ex, ey1 );
        }
      } else {
        if (ey1 < ras.min_ey) {
          ey1 = (ras.min_ey < ey2) ? ras.min_ey : ey2;
          gray_set_cell( &ras, ex, ey1 );
        } else {
          ey1++;
          gray_set_cell( &ras, ex, ey1 );
        }
        while ( ey1 < ey2 && ey1 < max_ey1)
        {
          ras.area  += area;
          ras.cover += delta;
          ey1++;

          gray_set_cell( &ras, ex, ey1 );
        }
        if (ey1 != ey2) {
          ey1 = ey2;
          gray_set_cell( &ras, ex, ey1 );
        }
      }

      delta      = (int)( fy2 - ONE_PIXEL + first );
      ras.area  += (TArea)two_fx * delta;
      ras.cover += delta;

      goto End;
    }

    /* ok, we have to render several scanlines */
    p     = ( ONE_PIXEL - fy1 ) * dx;
    first = ONE_PIXEL;
    incr  = 1;

    if ( dy < 0 )
    {
      p     = fy1 * dx;
      first = 0;
      incr  = -1;
      dy    = -dy;
    }

    delta = (int)( p / dy );
    mod   = (int)( p % dy );
    if ( mod < 0 )
    {
      delta--;
      mod += (TCoord)dy;
    }

    x = ras.x + delta;
    gray_render_scanline( RAS_VAR_ ey1, ras.x, fy1, x, (TCoord)first );

    ey1 += incr;
    gray_set_cell( RAS_VAR_ TRUNC( x ), ey1 );

    if ( ey1 != ey2 )
    {
      p     = ONE_PIXEL * dx;
      QT_FT_DIV_MOD( int, p, dy, lift, rem );
      mod -= (int)dy;

      while ( ey1 != ey2 )
      {
        delta = lift;
        mod  += rem;
        if ( mod >= 0 )
        {
          mod -= (int)dy;
          delta++;
        }

        x2 = x + delta;
        gray_render_scanline( RAS_VAR_ ey1, x,
                                       (TCoord)( ONE_PIXEL - first ), x2,
                                       (TCoord)first );
        x = x2;

        ey1 += incr;
        gray_set_cell( RAS_VAR_ TRUNC( x ), ey1 );
      }
    }

    gray_render_scanline( RAS_VAR_ ey1, x,
                                   (TCoord)( ONE_PIXEL - first ), to_x,
                                   fy2 );

  End:
    ras.x       = to_x;
    ras.y       = to_y;
  }


#else

  /*************************************************************************/
  /*                                                                       */
  /* Render a straight line across multiple cells in any direction.        */
  /*                                                                       */
  static void
  gray_render_line( RAS_ARG_ TPos  to_x,
                             TPos  to_y )
  {
    TPos    dx, dy, fx1, fy1, fx2, fy2;
    TCoord  ex1, ex2, ey1, ey2;


    ex1 = TRUNC( ras.x );
    ex2 = TRUNC( to_x );
    ey1 = TRUNC( ras.y );
    ey2 = TRUNC( to_y );

    /* perform vertical clipping */
    if ( ( ey1 >= ras.max_ey && ey2 >= ras.max_ey ) ||
         ( ey1 <  ras.min_ey && ey2 <  ras.min_ey ) )
      goto End;

    dx = to_x - ras.x;
    dy = to_y - ras.y;

    fx1 = ras.x - SUBPIXELS( ex1 );
    fy1 = ras.y - SUBPIXELS( ey1 );

    if ( ex1 == ex2 && ey1 == ey2 )       /* inside one cell */
      ;
    else if ( dy == 0 ) /* ex1 != ex2 */  /* any horizontal line */
    {
      ex1 = ex2;
      gray_set_cell( RAS_VAR_ ex1, ey1 );
    }
    else if ( dx == 0 )
    {
      if ( dy > 0 )                       /* vertical line up */
        do
        {
          fy2 = ONE_PIXEL;
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * fx1 * 2;
          fy1 = 0;
          ey1++;
          gray_set_cell( RAS_VAR_ ex1, ey1 );
        } while ( ey1 != ey2 );
      else                                /* vertical line down */
        do
        {
          fy2 = 0;
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * fx1 * 2;
          fy1 = ONE_PIXEL;
          ey1--;
          gray_set_cell( RAS_VAR_ ex1, ey1 );
        } while ( ey1 != ey2 );
    }
    else                                  /* any other line */
    {
      TArea  prod = dx * fy1 - dy * fx1;
      QT_FT_UDIVPREP( dx );
      QT_FT_UDIVPREP( dy );


      /* The fundamental value `prod' determines which side and the  */
      /* exact coordinate where the line exits current cell.  It is  */
      /* also easily updated when moving from one cell to the next.  */
      do
      {
        if      ( prod                                   <= 0 &&
                  prod - dx * ONE_PIXEL                  >  0 ) /* left */
        {
          fx2 = 0;
          fy2 = (TPos)QT_FT_UDIV( -prod, -dx );
          prod -= dy * ONE_PIXEL;
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * ( fx1 + fx2 );
          fx1 = ONE_PIXEL;
          fy1 = fy2;
          ex1--;
        }
        else if ( prod - dx * ONE_PIXEL                  <= 0 &&
                  prod - dx * ONE_PIXEL + dy * ONE_PIXEL >  0 ) /* up */
        {
          prod -= dx * ONE_PIXEL;
          fx2 = (TPos)QT_FT_UDIV( -prod, dy );
          fy2 = ONE_PIXEL;
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * ( fx1 + fx2 );
          fx1 = fx2;
          fy1 = 0;
          ey1++;
        }
        else if ( prod - dx * ONE_PIXEL + dy * ONE_PIXEL <= 0 &&
                  prod                  + dy * ONE_PIXEL >= 0 ) /* right */
        {
          prod += dy * ONE_PIXEL;
          fx2 = ONE_PIXEL;
          fy2 = (TPos)QT_FT_UDIV( prod, dx );
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * ( fx1 + fx2 );
          fx1 = 0;
          fy1 = fy2;
          ex1++;
        }
        else /* ( prod                  + dy * ONE_PIXEL <  0 &&
                  prod                                   >  0 )    down */
        {
          fx2 = (TPos)QT_FT_UDIV( prod, -dy );
          fy2 = 0;
          prod += dx * ONE_PIXEL;
          ras.cover += ( fy2 - fy1 );
          ras.area  += ( fy2 - fy1 ) * ( fx1 + fx2 );
          fx1 = fx2;
          fy1 = ONE_PIXEL;
          ey1--;
        }

        gray_set_cell( RAS_VAR_ ex1, ey1 );
      } while ( ex1 != ex2 || ey1 != ey2 );
    }

    fx2 = to_x - SUBPIXELS( ex2 );
    fy2 = to_y - SUBPIXELS( ey2 );

    ras.cover += ( fy2 - fy1 );
    ras.area  += ( fy2 - fy1 ) * ( fx1 + fx2 );

  End:
    ras.x       = to_x;
    ras.y       = to_y;
  }

#endif

  static void
  gray_split_conic( QT_FT_Vector*  base )
  {
    TPos  a, b;


    base[4].x = base[2].x;
    b = base[1].x;
    a = base[3].x = ( base[2].x + b ) / 2;
    b = base[1].x = ( base[0].x + b ) / 2;
    base[2].x = ( a + b ) / 2;

    base[4].y = base[2].y;
    b = base[1].y;
    a = base[3].y = ( base[2].y + b ) / 2;
    b = base[1].y = ( base[0].y + b ) / 2;
    base[2].y = ( a + b ) / 2;
  }


  static void
  gray_render_conic( RAS_ARG_ const QT_FT_Vector*  control,
                              const QT_FT_Vector*  to )
  {
    QT_FT_Vector   bez_stack[16 * 2 + 1];  /* enough to accommodate bisections */
    QT_FT_Vector*  arc = bez_stack;
    TPos        dx, dy;
    int         draw, split;


    arc[0].x = UPSCALE( to->x );
    arc[0].y = UPSCALE( to->y );
    arc[1].x = UPSCALE( control->x );
    arc[1].y = UPSCALE( control->y );
    arc[2].x = ras.x;
    arc[2].y = ras.y;

    /* short-cut the arc that crosses the current band */
    if ( ( TRUNC( arc[0].y ) >= ras.max_ey &&
           TRUNC( arc[1].y ) >= ras.max_ey &&
           TRUNC( arc[2].y ) >= ras.max_ey ) ||
         ( TRUNC( arc[0].y ) <  ras.min_ey &&
           TRUNC( arc[1].y ) <  ras.min_ey &&
           TRUNC( arc[2].y ) <  ras.min_ey ) )
    {
      ras.x = arc[0].x;
      ras.y = arc[0].y;
      return;
    }

    dx = QT_FT_ABS( arc[2].x + arc[0].x - 2 * arc[1].x );
    dy = QT_FT_ABS( arc[2].y + arc[0].y - 2 * arc[1].y );
    if ( dx < dy )
      dx = dy;

    /* We can calculate the number of necessary bisections because  */
    /* each bisection predictably reduces deviation exactly 4-fold. */
    /* Even 32-bit deviation would vanish after 16 bisections.      */
    draw = 1;
    while ( dx > ONE_PIXEL / 4 )
    {
      dx >>= 2;
      draw <<= 1;
    }

    /* We use decrement counter to count the total number of segments */
    /* to draw starting from 2^level. Before each draw we split as    */
    /* many times as there are trailing zeros in the counter.         */
    do
    {
      split = 1;
      while ( ( draw & split ) == 0 )
      {
        gray_split_conic( arc );
        arc += 2;
        split <<= 1;
      }

      gray_render_line( RAS_VAR_ arc[0].x, arc[0].y );
      arc -= 2;

    } while ( --draw );
  }


  static void
  gray_split_cubic( QT_FT_Vector*  base )
  {
    TPos  a, b, c, d;


    base[6].x = base[3].x;
    c = base[1].x;
    d = base[2].x;
    base[1].x = a = ( base[0].x + c ) / 2;
    base[5].x = b = ( base[3].x + d ) / 2;
    c = ( c + d ) / 2;
    base[2].x = a = ( a + c ) / 2;
    base[4].x = b = ( b + c ) / 2;
    base[3].x = ( a + b ) / 2;

    base[6].y = base[3].y;
    c = base[1].y;
    d = base[2].y;
    base[1].y = a = ( base[0].y + c ) / 2;
    base[5].y = b = ( base[3].y + d ) / 2;
    c = ( c + d ) / 2;
    base[2].y = a = ( a + c ) / 2;
    base[4].y = b = ( b + c ) / 2;
    base[3].y = ( a + b ) / 2;
  }


  static void
  gray_render_cubic( RAS_ARG_ const QT_FT_Vector*  control1,
                              const QT_FT_Vector*  control2,
                              const QT_FT_Vector*  to )
  {
    QT_FT_Vector   bez_stack[16 * 3 + 1];  /* enough to accommodate bisections */
    QT_FT_Vector*  arc = bez_stack;
    TPos        dx, dy, dx_, dy_;
    TPos        dx1, dy1, dx2, dy2;
    TPos        L, s, s_limit;


    arc[0].x = UPSCALE( to->x );
    arc[0].y = UPSCALE( to->y );
    arc[1].x = UPSCALE( control2->x );
    arc[1].y = UPSCALE( control2->y );
    arc[2].x = UPSCALE( control1->x );
    arc[2].y = UPSCALE( control1->y );
    arc[3].x = ras.x;
    arc[3].y = ras.y;

    /* short-cut the arc that crosses the current band */
    if ( ( TRUNC( arc[0].y ) >= ras.max_ey &&
           TRUNC( arc[1].y ) >= ras.max_ey &&
           TRUNC( arc[2].y ) >= ras.max_ey &&
           TRUNC( arc[3].y ) >= ras.max_ey ) ||
         ( TRUNC( arc[0].y ) <  ras.min_ey &&
           TRUNC( arc[1].y ) <  ras.min_ey &&
           TRUNC( arc[2].y ) <  ras.min_ey &&
           TRUNC( arc[3].y ) <  ras.min_ey ) )
    {
      ras.x = arc[0].x;
      ras.y = arc[0].y;
      return;
    }

    for (;;)
    {
      /* Decide whether to split or draw. See `Rapid Termination          */
      /* Evaluation for Recursive Subdivision of Bezier Curves' by Thomas */
      /* F. Hain, at                                                      */
      /* http://www.cis.southalabama.edu/~hain/general/Publications/Bezier/Camera-ready%20CISST02%202.pdf */


      /* dx and dy are x and y components of the P0-P3 chord vector. */
      dx = dx_ = arc[3].x - arc[0].x;
      dy = dy_ = arc[3].y - arc[0].y;

      L = QT_FT_HYPOT( dx_, dy_ );

      /* Avoid possible arithmetic overflow below by splitting. */
      if ( L > 32767 )
        goto Split;

      /* Max deviation may be as much as (s/L) * 3/4 (if Hain's v = 1). */
      s_limit = L * (TPos)( ONE_PIXEL / 6 );

      /* s is L * the perpendicular distance from P1 to the line P0-P3. */
      dx1 = arc[1].x - arc[0].x;
      dy1 = arc[1].y - arc[0].y;
      s = QT_FT_ABS( dy * dx1 - dx * dy1 );

      if ( s > s_limit )
        goto Split;

      /* s is L * the perpendicular distance from P2 to the line P0-P3. */
      dx2 = arc[2].x - arc[0].x;
      dy2 = arc[2].y - arc[0].y;
      s = QT_FT_ABS( dy * dx2 - dx * dy2 );

      if ( s > s_limit )
        goto Split;

      /* Split super curvy segments where the off points are so far
         from the chord that the angles P0-P1-P3 or P0-P2-P3 become
         acute as detected by appropriate dot products. */
      if ( dx1 * ( dx1 - dx ) + dy1 * ( dy1 - dy ) > 0 ||
           dx2 * ( dx2 - dx ) + dy2 * ( dy2 - dy ) > 0 )
        goto Split;

      gray_render_line( RAS_VAR_ arc[0].x, arc[0].y );

      if ( arc == bez_stack )
        return;

      arc -= 3;
      continue;

    Split:
      gray_split_cubic( arc );
      arc += 3;
    }
  }



  static int
  gray_move_to( const QT_FT_Vector*  to,
                PWorker           worker )
  {
    TPos  x, y;


    /* record current cell, if any */
    if ( !ras.invalid )
      gray_record_cell( worker );

    /* start to a new position */
    x = UPSCALE( to->x );
    y = UPSCALE( to->y );

    gray_start_cell( worker, TRUNC( x ), TRUNC( y ) );

    ras.x = x;
    ras.y = y;
    return 0;
  }

  static void
  gray_render_span( int                count,
                    const QT_FT_Span*  spans,
                    PWorker            worker )
  {
    unsigned char*  p;
    QT_FT_Bitmap*      map = &worker->target;

    for ( ; count > 0; count--, spans++ )
    {
      unsigned char  coverage = spans->coverage;

      /* first of all, compute the scanline offset */
      p = (unsigned char*)map->buffer - spans->y * map->pitch;
      if ( map->pitch >= 0 )
        p += ( map->rows - 1 ) * (unsigned int)map->pitch;


      if ( coverage )
      {
        unsigned char*  q = p + spans->x;


        /* For small-spans it is faster to do it by ourselves than
         * calling `memset'.  This is mainly due to the cost of the
         * function call.
         */
        switch ( spans->len )
        {
        case 7: *q++ = coverage; Q_FALLTHROUGH();
        case 6: *q++ = coverage; Q_FALLTHROUGH();
        case 5: *q++ = coverage; Q_FALLTHROUGH();
        case 4: *q++ = coverage; Q_FALLTHROUGH();
        case 3: *q++ = coverage; Q_FALLTHROUGH();
        case 2: *q++ = coverage; Q_FALLTHROUGH();
        case 1: *q   = coverage; Q_FALLTHROUGH();
        case 0: break;
        default:
          QT_FT_MEM_SET( q, coverage, spans->len );
        }
      }
    }
  }


  static void
  gray_hline( RAS_ARG_ TCoord  x,
                       TCoord  y,
                       TPos    area,
                       int     acount )
  {
    int coverage;


    /* compute the coverage line's coverage, depending on the    */
    /* outline fill rule                                         */
    /*                                                           */
    /* the coverage percentage is area/(PIXEL_BITS*PIXEL_BITS*2) */
    /*                                                           */
    coverage = (int)( area >> ( PIXEL_BITS * 2 + 1 - 8 ) );
                                                    /* use range 0..256 */
    if ( coverage < 0 )
      coverage = -coverage;

    if ( ras.outline.flags & QT_FT_OUTLINE_EVEN_ODD_FILL )
    {
      coverage &= 511;

      if ( coverage > 256 )
        coverage = 512 - coverage;
      else if ( coverage == 256 )
        coverage = 255;
    }
    else
    {
      /* normal non-zero winding rule */
      if ( coverage >= 256 )
        coverage = 255;
    }

    y += (TCoord)ras.min_ey;
    x += (TCoord)ras.min_ex;

    /* QT_FT_Span.x is a 16-bit short, so limit our coordinates appropriately */
    if ( x >= 32767 )
      x = 32767;

    /* QT_FT_Span.y is a 16-bit short, so limit our coordinates appropriately */
    if ( y >= 32767 )
      y = 32767;

    if ( coverage )
    {
      QT_FT_Span*  span;
      int       count;
      int       skip;


      /* see whether we can add this span to the current list */
      count = ras.num_gray_spans;
      span  = ras.gray_spans + count - 1;
      if ( count > 0                          &&
           span->y == y                       &&
           (int)span->x + span->len == (int)x &&
           span->coverage == coverage         )
      {
        span->len = (unsigned short)( span->len + acount );
        return;
      }

      if ( count >= QT_FT_MAX_GRAY_SPANS )
      {
        if ( ras.render_span && count > ras.skip_spans )
        {
          skip = ras.skip_spans > 0 ? ras.skip_spans : 0;
          ras.render_span( ras.num_gray_spans - skip,
                           ras.gray_spans + skip,
                           ras.render_span_data );
        }

        ras.skip_spans -= ras.num_gray_spans;

        /* ras.render_span( span->y, ras.gray_spans, count ); */

#ifdef DEBUG_GRAYS

        if ( 1 )
        {
          int  n;


          fprintf( stderr, "y=%3d ", y );
          span = ras.gray_spans;
          for ( n = 0; n < count; n++, span++ )
            fprintf( stderr, "[%d..%d]:%02x ",
                     span->x, span->x + span->len - 1, span->coverage );
          fprintf( stderr, "\n" );
        }

#endif /* DEBUG_GRAYS */

        ras.num_gray_spans = 0;

        span  = ras.gray_spans;
      }
      else
        span++;

      /* add a gray span to the current list */
      span->x        = (short)x;
      span->len      = (unsigned short)acount;
      span->y        = (short)y;
      span->coverage = (unsigned char)coverage;

      ras.num_gray_spans++;
    }
  }


#ifdef DEBUG_GRAYS

  /* to be called while in the debugger */
  gray_dump_cells( RAS_ARG )
  {
    int  yindex;


    for ( yindex = 0; yindex < ras.ycount; yindex++ )
    {
      PCell  cell;


      printf( "%3d:", yindex );

      for ( cell = ras.ycells[yindex]; cell != NULL; cell = cell->next )
        printf( " (%3d, c:%4d, a:%6d)", cell->x, cell->cover, cell->area );
      printf( "\n" );
    }
  }

#endif /* DEBUG_GRAYS */


  static void
  gray_sweep( RAS_ARG_ const QT_FT_Bitmap*  target )
  {
    int  yindex;

    QT_FT_UNUSED( target );


    if ( ras.num_cells == 0 )
      return;

    QT_FT_TRACE7(( "gray_sweep: start\n" ));

    for ( yindex = 0; yindex < ras.ycount; yindex++ )
    {
      PCell   cell  = ras.ycells[yindex];
      TCoord  cover = 0;
      TCoord  x     = 0;


      for ( ; cell != NULL; cell = cell->next )
      {
        TArea  area;


        if ( cell->x > x && cover != 0 )
          gray_hline( RAS_VAR_ x, yindex, cover * ( ONE_PIXEL * 2 ),
                      cell->x - x );

        cover += cell->cover;
        area   = cover * ( ONE_PIXEL * 2 ) - cell->area;

        if ( area != 0 && cell->x >= 0 )
          gray_hline( RAS_VAR_ cell->x, yindex, area, 1 );

        x = cell->x + 1;
      }

      if ( ras.count_ex > x && cover != 0 )
        gray_hline( RAS_VAR_ x, yindex, cover * ( ONE_PIXEL * 2 ),
                    ras.count_ex - x );
    }

    QT_FT_TRACE7(( "gray_sweep: end\n" ));
  }

  /*************************************************************************/
  /*                                                                       */
  /*  The following function should only compile in stand_alone mode,      */
  /*  i.e., when building this component without the rest of FreeType.     */
  /*                                                                       */
  /*************************************************************************/

  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    QT_FT_Outline_Decompose                                               */
  /*                                                                       */
  /* <Description>                                                         */
  /*    Walks over an outline's structure to decompose it into individual  */
  /*    segments and Bezier arcs.  This function is also able to emit      */
  /*    `move to' and `close to' operations to indicate the start and end  */
  /*    of new contours in the outline.                                    */
  /*                                                                       */
  /* <Input>                                                               */
  /*    outline        :: A pointer to the source target.                  */
  /*                                                                       */
  /*    user           :: A typeless pointer which is passed to each       */
  /*                      emitter during the decomposition.  It can be     */
  /*                      used to store the state during the               */
  /*                      decomposition.                                   */
  /*                                                                       */
  /* <Return>                                                              */
  /*    Error code.  0 means success.                                      */
  /*                                                                       */
  static
  int  QT_FT_Outline_Decompose( const QT_FT_Outline*        outline,
                                void*                       user )
  {
#undef SCALED
#define SCALED( x )  (x)

    QT_FT_Vector   v_last;
    QT_FT_Vector   v_control;
    QT_FT_Vector   v_start;

    QT_FT_Vector*  point;
    QT_FT_Vector*  limit;
    char*       tags;

    int   n;         /* index of contour in outline     */
    int   first;     /* index of first point in contour */
    int   error;
    char  tag;       /* current point's state           */

    if ( !outline )
      return ErrRaster_Invalid_Outline;

    first = 0;

    for ( n = 0; n < outline->n_contours; n++ )
    {
      int  last;  /* index of last point in contour */


      last  = outline->contours[n];
      if ( last < 0 )
        goto Invalid_Outline;
      limit = outline->points + last;

      v_start   = outline->points[first];
      v_start.x = SCALED( v_start.x );
      v_start.y = SCALED( v_start.y );

      v_last   = outline->points[last];
      v_last.x = SCALED( v_last.x );
      v_last.y = SCALED( v_last.y );

      v_control = v_start;

      point = outline->points + first;
      tags  = outline->tags  + first;
      tag   = QT_FT_CURVE_TAG( tags[0] );

      /* A contour cannot start with a cubic control point! */
      if ( tag == QT_FT_CURVE_TAG_CUBIC )
        goto Invalid_Outline;

      /* check first point to determine origin */
      if ( tag == QT_FT_CURVE_TAG_CONIC )
      {
        /* first point is conic control.  Yes, this happens. */
        if ( QT_FT_CURVE_TAG( outline->tags[last] ) == QT_FT_CURVE_TAG_ON )
        {
          /* start at last point if it is on the curve */
          v_start = v_last;
          limit--;
        }
        else
        {
          /* if both first and last points are conic,         */
          /* start at their middle and record its position    */
          /* for closure                                      */
          v_start.x = ( v_start.x + v_last.x ) / 2;
          v_start.y = ( v_start.y + v_last.y ) / 2;

          v_last = v_start;
        }
        point--;
        tags--;
      }

      QT_FT_TRACE5(( "  move to (%.2f, %.2f)\n",
                     v_start.x / 64.0, v_start.y / 64.0 ));
      error = gray_move_to( &v_start, static_cast<PWorker>( user ) );
      if ( error )
        goto Exit;

      while ( point < limit )
      {
        point++;
        tags++;

        tag = QT_FT_CURVE_TAG( tags[0] );
        switch ( tag )
        {
        case QT_FT_CURVE_TAG_ON:  /* emit a single line_to */
          {
            QT_FT_Vector  vec;


            vec.x = SCALED( point->x );
            vec.y = SCALED( point->y );

            QT_FT_TRACE5(( "  line to (%.2f, %.2f)\n",
                           vec.x / 64.0, vec.y / 64.0 ));
            gray_render_line(static_cast<PWorker>( user ), UPSCALE(vec.x), UPSCALE(vec.y));
            continue;
          }

        case QT_FT_CURVE_TAG_CONIC:  /* consume conic arcs */
          {
            v_control.x = SCALED( point->x );
            v_control.y = SCALED( point->y );

          Do_Conic:
            if ( point < limit )
            {
              QT_FT_Vector  vec;
              QT_FT_Vector  v_middle;


              point++;
              tags++;
              tag = QT_FT_CURVE_TAG( tags[0] );

              vec.x = SCALED( point->x );
              vec.y = SCALED( point->y );

              if ( tag == QT_FT_CURVE_TAG_ON )
              {
                QT_FT_TRACE5(( "  conic to (%.2f, %.2f)"
                               " with control (%.2f, %.2f)\n",
                               vec.x / 64.0, vec.y / 64.0,
                               v_control.x / 64.0, v_control.y / 64.0 ));
                gray_render_conic(static_cast<PWorker>( user ), &v_control, &vec);
                continue;
              }

              if ( tag != QT_FT_CURVE_TAG_CONIC )
                goto Invalid_Outline;

              v_middle.x = ( v_control.x + vec.x ) / 2;
              v_middle.y = ( v_control.y + vec.y ) / 2;

              QT_FT_TRACE5(( "  conic to (%.2f, %.2f)"
                             " with control (%.2f, %.2f)\n",
                             v_middle.x / 64.0, v_middle.y / 64.0,
                             v_control.x / 64.0, v_control.y / 64.0 ));
              gray_render_conic(static_cast<PWorker>( user ), &v_control, &v_middle);

              v_control = vec;
              goto Do_Conic;
            }

            QT_FT_TRACE5(( "  conic to (%.2f, %.2f)"
                           " with control (%.2f, %.2f)\n",
                           v_start.x / 64.0, v_start.y / 64.0,
                           v_control.x / 64.0, v_control.y / 64.0 ));
            gray_render_conic(static_cast<PWorker>( user ), &v_control, &v_start);
            goto Close;
          }

        default:  /* QT_FT_CURVE_TAG_CUBIC */
          {
            QT_FT_Vector  vec1, vec2;


            if ( point + 1 > limit                             ||
                 QT_FT_CURVE_TAG( tags[1] ) != QT_FT_CURVE_TAG_CUBIC )
              goto Invalid_Outline;

            point += 2;
            tags  += 2;

            vec1.x = SCALED( point[-2].x );
            vec1.y = SCALED( point[-2].y );

            vec2.x = SCALED( point[-1].x );
            vec2.y = SCALED( point[-1].y );

            if ( point <= limit )
            {
              QT_FT_Vector  vec;


              vec.x = SCALED( point->x );
              vec.y = SCALED( point->y );

              QT_FT_TRACE5(( "  cubic to (%.2f, %.2f)"
                             " with controls (%.2f, %.2f) and (%.2f, %.2f)\n",
                             vec.x / 64.0, vec.y / 64.0,
                             vec1.x / 64.0, vec1.y / 64.0,
                             vec2.x / 64.0, vec2.y / 64.0 ));
              gray_render_cubic(static_cast<PWorker>( user ), &vec1, &vec2, &vec);
              continue;
            }

            QT_FT_TRACE5(( "  cubic to (%.2f, %.2f)"
                           " with controls (%.2f, %.2f) and (%.2f, %.2f)\n",
                           v_start.x / 64.0, v_start.y / 64.0,
                           vec1.x / 64.0, vec1.y / 64.0,
                           vec2.x / 64.0, vec2.y / 64.0 ));
            gray_render_cubic(static_cast<PWorker>( user ), &vec1, &vec2, &v_start);
            goto Close;
          }
        }
      }

      /* close the contour with a line segment */
      QT_FT_TRACE5(( "  line to (%.2f, %.2f)\n",
                     v_start.x / 64.0, v_start.y / 64.0 ));
      gray_render_line(static_cast<PWorker>( user ), UPSCALE(v_start.x), UPSCALE(v_start.y));

   Close:
      first = last + 1;
    }

    QT_FT_TRACE5(( "FT_Outline_Decompose: Done\n", n ));
    return 0;

  Exit:
    QT_FT_TRACE5(( "FT_Outline_Decompose: Error %d\n", error ));
    return error;

  Invalid_Outline:
    return ErrRaster_Invalid_Outline;
  }

  typedef struct  TBand_
  {
    TPos  min, max;

  } TBand;

  static int
  gray_convert_glyph_inner( RAS_ARG )
  {
    volatile int  error = 0;

    if ( qt_ft_setjmp( ras.jump_buffer ) == 0 )
    {
      error = QT_FT_Outline_Decompose( &ras.outline, &ras );
      if ( !ras.invalid )
        gray_record_cell( RAS_VAR );
    }
    else
    {
      error = ErrRaster_Memory_Overflow;
    }

    return error;
  }


  static int
  gray_convert_glyph( RAS_ARG )
  {
    TBand            bands[40];
    TBand* volatile  band;
    int volatile     n, num_bands;
    TPos volatile    min, max, max_y;
    QT_FT_BBox*      clip;
    int              skip;

    ras.num_gray_spans = 0;

    /* Set up state in the raster object */
    gray_compute_cbox( RAS_VAR );

    /* clip to target bitmap, exit if nothing to do */
    clip = &ras.clip_box;

    if ( ras.max_ex <= clip->xMin || ras.min_ex >= clip->xMax ||
         ras.max_ey <= clip->yMin || ras.min_ey >= clip->yMax )
      return 0;

    if ( ras.min_ex < clip->xMin ) ras.min_ex = clip->xMin;
    if ( ras.min_ey < clip->yMin ) ras.min_ey = clip->yMin;

    if ( ras.max_ex > clip->xMax ) ras.max_ex = clip->xMax;
    if ( ras.max_ey > clip->yMax ) ras.max_ey = clip->yMax;

    ras.count_ex = ras.max_ex - ras.min_ex;
    ras.count_ey = ras.max_ey - ras.min_ey;

    /* set up vertical bands */
    num_bands = (int)( ( ras.max_ey - ras.min_ey ) / ras.band_size );
    if ( num_bands == 0 )
      num_bands = 1;
    if ( num_bands >= 39 )
      num_bands = 39;

    ras.band_shoot = 0;

    min   = ras.min_ey;
    max_y = ras.max_ey;

    for ( n = 0; n < num_bands; n++, min = max )
    {
      max = min + ras.band_size;
      if ( n == num_bands - 1 || max > max_y )
        max = max_y;

      bands[0].min = min;
      bands[0].max = max;
      band         = bands;

      while ( band >= bands )
      {
        TPos  bottom, top, middle;
        int   error;

        {
          PCell  cells_max;
          int    yindex;
          int    cell_start, cell_end, cell_mod;


          ras.ycells = (PCell*)ras.buffer;
          ras.ycount = band->max - band->min;

          cell_start = sizeof ( PCell ) * ras.ycount;
          cell_mod   = cell_start % sizeof ( TCell );
          if ( cell_mod > 0 )
            cell_start += sizeof ( TCell ) - cell_mod;

          cell_end  = ras.buffer_size;
          cell_end -= cell_end % sizeof( TCell );

          cells_max = (PCell)( (char*)ras.buffer + cell_end );
          ras.cells = (PCell)( (char*)ras.buffer + cell_start );
          if ( ras.cells >= cells_max )
            goto ReduceBands;

          ras.max_cells = (int)(cells_max - ras.cells);
          if ( ras.max_cells < 2 )
            goto ReduceBands;

          for ( yindex = 0; yindex < ras.ycount; yindex++ )
            ras.ycells[yindex] = NULL;
        }

        ras.num_cells = 0;
        ras.invalid   = 1;
        ras.min_ey    = band->min;
        ras.max_ey    = band->max;
        ras.count_ey  = band->max - band->min;

        error = gray_convert_glyph_inner( RAS_VAR );

        if ( !error )
        {
          gray_sweep( RAS_VAR_ &ras.target );
          band--;
          continue;
        }
        else if ( error != ErrRaster_Memory_Overflow )
          return 1;

      ReduceBands:
        /* render pool overflow; we will reduce the render band by half */
        bottom = band->min;
        top    = band->max;
        middle = bottom + ( ( top - bottom ) >> 1 );

        /* This is too complex for a single scanline; there must */
        /* be some problems.                                     */
        if ( middle == bottom )
        {
#ifdef DEBUG_GRAYS
          fprintf( stderr, "Rotten glyph!\n" );
#endif
          return ErrRaster_OutOfMemory;
        }

        if ( bottom-top >= ras.band_size )
          ras.band_shoot++;

        band[1].min = bottom;
        band[1].max = middle;
        band[0].min = middle;
        band[0].max = top;
        band++;
      }
    }

    if ( ras.render_span && ras.num_gray_spans > ras.skip_spans )
    {
        skip = ras.skip_spans > 0 ? ras.skip_spans : 0;
        ras.render_span( ras.num_gray_spans - skip,
                         ras.gray_spans + skip,
                         ras.render_span_data );
    }

    ras.skip_spans -= ras.num_gray_spans;

    if ( ras.band_shoot > 8 && ras.band_size > 16 )
      ras.band_size = ras.band_size / 2;

    return 0;
  }


  static int
  gray_raster_render( QT_FT_Raster                  raster,
                      const QT_FT_Raster_Params*  params )
  {
    const QT_FT_Outline*  outline    = (const QT_FT_Outline*)params->source;
    const QT_FT_Bitmap*   target_map = params->target;
    PWorker            worker;


    if ( !raster || !raster->buffer || !raster->buffer_size )
      return ErrRaster_Invalid_Argument;

    if ( raster->worker )
      raster->worker->skip_spans = params->skip_spans;

    /* If raster object and raster buffer are allocated, but  */
    /* raster size isn't of the minimum size, indicate out of */
    /* memory.                                                */
    if (raster->buffer_allocated_size < MINIMUM_POOL_SIZE )
      return ErrRaster_OutOfMemory;

    if ( !outline )
      return ErrRaster_Invalid_Outline;

    /* return immediately if the outline is empty */
    if ( outline->n_points == 0 || outline->n_contours <= 0 )
      return 0;

    if ( !outline->contours || !outline->points )
      return ErrRaster_Invalid_Outline;

    if ( outline->n_points !=
           outline->contours[outline->n_contours - 1] + 1 )
      return ErrRaster_Invalid_Outline;

    worker = raster->worker;

    /* if direct mode is not set, we must have a target bitmap */
    if ( ( params->flags & QT_FT_RASTER_FLAG_DIRECT ) == 0 )
    {
      if ( !target_map )
        return ErrRaster_Invalid_Argument;

      /* nothing to do */
      if ( !target_map->width || !target_map->rows )
        return 0;

      if ( !target_map->buffer )
        return ErrRaster_Invalid_Argument;
    }

    /* this version does not support monochrome rendering */
    if ( !( params->flags & QT_FT_RASTER_FLAG_AA ) )
      return ErrRaster_Invalid_Mode;

    /* compute clipping box */
    if ( ( params->flags & QT_FT_RASTER_FLAG_DIRECT ) == 0 )
    {
      /* compute clip box from target pixmap */
      ras.clip_box.xMin = 0;
      ras.clip_box.yMin = 0;
      ras.clip_box.xMax = target_map->width;
      ras.clip_box.yMax = target_map->rows;
    }
    else if ( params->flags & QT_FT_RASTER_FLAG_CLIP )
    {
      ras.clip_box = params->clip_box;
    }
    else
    {
      ras.clip_box.xMin = -32768L;
      ras.clip_box.yMin = -32768L;
      ras.clip_box.xMax =  32767L;
      ras.clip_box.yMax =  32767L;
    }

    gray_init_cells( worker, raster->buffer, raster->buffer_size );

    ras.outline   = *outline;
    ras.num_cells = 0;
    ras.invalid   = 1;
    ras.band_size = raster->band_size;

    if ( target_map )
      ras.target = *target_map;

    ras.render_span      = (QT_FT_Raster_Span_Func)gray_render_span;
    ras.render_span_data = &ras;

    if ( params->flags & QT_FT_RASTER_FLAG_DIRECT )
    {
      ras.render_span      = (QT_FT_Raster_Span_Func)params->gray_spans;
      ras.render_span_data = params->user;
    }

    return gray_convert_glyph( worker );
  }


  /**** RASTER OBJECT CREATION: In standalone mode, we simply use *****/
  /****                         a static object.                  *****/

  static int
  gray_raster_new( QT_FT_Raster*  araster )
  {
    *araster = static_cast<QT_FT_Raster>(malloc(sizeof(TRaster)));
    if (!*araster) {
        *araster = 0;
        return ErrRaster_Memory_Overflow;
    }
    QT_FT_MEM_ZERO(*araster, sizeof(TRaster));

    return 0;
  }


  static void
  gray_raster_done( QT_FT_Raster  raster )
  {
    free(raster);
  }


  static void
  gray_raster_reset( QT_FT_Raster  raster,
                     char*      pool_base,
                     long       pool_size )
  {
    PRaster  rast = (PRaster)raster;

    if ( raster )
    {
      if ( pool_base && ( pool_size >= MINIMUM_POOL_SIZE ) )
      {
        PWorker  worker = (PWorker)pool_base;


        rast->worker      = worker;
        rast->buffer      = pool_base +
                              ( ( sizeof ( TWorker ) + sizeof ( TCell ) - 1 ) &
                                ~( sizeof ( TCell ) - 1 ) );
        rast->buffer_size = (long)( ( pool_base + pool_size ) -
                                    (char*)rast->buffer ) &
                                      ~( sizeof ( TCell ) - 1 );
        rast->band_size   = (int)( rast->buffer_size /
                                     ( sizeof ( TCell ) * 8 ) );
      }
      else if ( pool_base)
      { /* Case when there is a raster pool allocated, but it                */
        /* doesn't have the minimum size (and so memory will be reallocated) */
          rast->buffer = pool_base;
          rast->worker = NULL;
          rast->buffer_size = pool_size;
      }
      else
      {
        rast->buffer      = NULL;
        rast->buffer_size = 0;
        rast->worker      = NULL;
      }
      rast->buffer_allocated_size = pool_size;
    }
  }

  const QT_FT_Raster_Funcs  qt_ft_grays_raster =
  {
    QT_FT_GLYPH_FORMAT_OUTLINE,

    (QT_FT_Raster_New_Func)     gray_raster_new,
    (QT_FT_Raster_Reset_Func)   gray_raster_reset,
    (QT_FT_Raster_Set_Mode_Func)0,
    (QT_FT_Raster_Render_Func)  gray_raster_render,
    (QT_FT_Raster_Done_Func)    gray_raster_done
  };

/* END */

namespace {

struct Point {
    double x;
    double y;
};

struct Cubic {
    Point p1;
    Point p2;
    Point p3;
    Point p4;
};

std::pair<Cubic, Cubic> splitCubic(const Cubic &b)
{
    const auto mid = [](const Point &a, const Point &c) {
        return Point{(a.x + c.x) * 0.5, (a.y + c.y) * 0.5};
    };
    const Point p12 = mid(b.p1, b.p2);
    const Point p23 = mid(b.p2, b.p3);
    const Point p34 = mid(b.p3, b.p4);
    const Point p1223 = mid(p12, p23);
    const Point p2334 = mid(p23, p34);
    const Point center = mid(p1223, p2334);
    return {{b.p1, p12, p1223, center}, {center, p2334, p34, b.p4}};
}

void addCubicToPolygon(const Cubic &curve, std::vector<Point> &polygon)
{
    // qbezier.cpp QBezier::addToPolygon(QDataBuffer<...>), threshold 0.25.
    Cubic beziers[10];
    int levels[10];
    beziers[0] = curve;
    levels[0] = 9;
    int top = 0;
    while (top >= 0) {
        Cubic *b = &beziers[top];
        const double y4y1 = b->p4.y - b->p1.y;
        const double x4x1 = b->p4.x - b->p1.x;
        double length = std::abs(x4x1) + std::abs(y4y1);
        double distance;
        if (length > 1.0) {
            distance = std::abs(x4x1 * (b->p1.y - b->p2.y)
                                - y4y1 * (b->p1.x - b->p2.x))
                + std::abs(x4x1 * (b->p1.y - b->p3.y)
                           - y4y1 * (b->p1.x - b->p3.x));
        } else {
            distance = std::abs(b->p1.x - b->p2.x) + std::abs(b->p1.y - b->p2.y)
                + std::abs(b->p1.x - b->p3.x) + std::abs(b->p1.y - b->p3.y);
            length = 1.0;
        }
        if (distance < 0.25 * length || levels[top] == 0) {
            polygon.push_back(b->p4);
            --top;
        } else {
            const auto halves = splitCubic(*b);
            b[1] = halves.first;
            b[0] = halves.second;
            levels[top + 1] = --levels[top];
            ++top;
        }
    }
}

int qtRoundToInt(double value)
{
    // Qt 5 qRound: positive halves away from zero; preserve the Qt negative
    // half behavior instead of delegating to the active C locale/runtime.
    return value >= 0.0
        ? int(value + 0.5)
        : int(value - int(value - 1.0) + 0.5) + int(value - 1.0);
}

QT_FT_Pos to26Dot6(double value)
{
    constexpr int limit = 32767 * 64;
    const double scaled = value * 64.0;
    if (scaled >= limit) {
        return limit;
    }
    if (scaled <= -limit) {
        return -limit;
    }
    return qtRoundToInt(scaled);
}

struct OutlineStorage {
    std::vector<QT_FT_Vector> points;
    std::vector<char> tags;
    std::vector<int> contours;
    QT_FT_Outline outline{};
};

bool containsPoint(const PkRect &clip, const Point &point)
{
    const double right = double(clip.x()) + double(clip.width());
    const double bottom = double(clip.y()) + double(clip.height());
    return point.x >= double(clip.x()) && point.x <= right
        && point.y >= double(clip.y()) && point.y <= bottom;
}

bool controlBoundsIntersectClip(const Cubic &curve, const PkRect &clip)
{
    const double minimumX = std::min({curve.p1.x, curve.p2.x, curve.p3.x, curve.p4.x});
    const double maximumX = std::max({curve.p1.x, curve.p2.x, curve.p3.x, curve.p4.x});
    const double minimumY = std::min({curve.p1.y, curve.p2.y, curve.p3.y, curve.p4.y});
    const double maximumY = std::max({curve.p1.y, curve.p2.y, curve.p3.y, curve.p4.y});
    const double right = double(clip.x()) + double(clip.width());
    const double bottom = double(clip.y()) + double(clip.height());
    return maximumX >= double(clip.x()) && minimumX <= right
        && maximumY >= double(clip.y()) && minimumY <= bottom;
}

OutlineStorage makeOutline(const PkPainterPath &path,
                           const PkRect &clip,
                           double originX,
                           double originY)
{
    OutlineStorage result;
    std::vector<Point> flattened;
    std::vector<int> contourStarts;
    int currentContour = -1;

    for (int index = 0; index < path.elementCount(); ++index) {
        const auto element = path.elementAt(index);
        switch (element.type) {
        case PkPainterPath::MoveToElement:
            if (index == path.elementCount() - 1) {
                continue;
            }
            if (currentContour >= 0
                && (flattened.back().x != flattened[currentContour].x
                    || flattened.back().y != flattened[currentContour].y)) {
                flattened.push_back(flattened[currentContour]);
            }
            currentContour = int(flattened.size());
            contourStarts.push_back(currentContour);
            flattened.push_back({element.x - originX, element.y - originY});
            break;
        case PkPainterPath::LineToElement:
            if (currentContour >= 0) {
                flattened.push_back({element.x - originX, element.y - originY});
            }
            break;
        case PkPainterPath::CurveToElement:
            if (currentContour >= 0 && index + 2 < path.elementCount()) {
                const auto control2 = path.elementAt(index + 1);
                const auto end = path.elementAt(index + 2);
                const Point start = flattened.back();
                const Cubic curve{start,
                                  {element.x - originX, element.y - originY},
                                  {control2.x - originX, control2.y - originY},
                                  {end.x - originX, end.y - originY}};
                if (!containsPoint(clip, curve.p4) && !controlBoundsIntersectClip(curve, clip)) {
                    flattened.push_back(curve.p4);
                } else {
                    addCubicToPolygon(curve, flattened);
                }
                index += 2;
            }
            break;
        case PkPainterPath::CurveToDataElement:
            break;
        }
    }
    if (currentContour >= 0
        && (flattened.back().x != flattened[currentContour].x
            || flattened.back().y != flattened[currentContour].y)) {
        flattened.push_back(flattened[currentContour]);
    }

    result.points.reserve(flattened.size());
    result.tags.assign(flattened.size(), QT_FT_CURVE_TAG_ON);
    for (const Point &point : flattened) {
        result.points.push_back({to26Dot6(point.x), to26Dot6(point.y)});
    }
    for (std::size_t i = 0; i < contourStarts.size(); ++i) {
        const int end = i + 1 < contourStarts.size()
            ? contourStarts[i + 1] - 1
            : int(flattened.size()) - 1;
        if (end >= contourStarts[i]) {
            result.contours.push_back(end);
        }
    }
    result.outline.n_contours = int(result.contours.size());
    result.outline.n_points = int(result.points.size());
    result.outline.points = result.points.data();
    result.outline.tags = result.tags.data();
    result.outline.contours = result.contours.data();
    result.outline.flags = path.fillRule() == Qt::WindingFill
        ? QT_FT_OUTLINE_NONE : QT_FT_OUTLINE_EVEN_ODD_FILL;
    return result;
}

struct GrayCallback {
    ProcessSpans process;
    void *userData;
    std::array<Span, QT_FT_MAX_GRAY_SPANS> converted;
};

void processGraySpans(int count, const QT_FT_Span *spans, void *userData)
{
    auto *callback = static_cast<GrayCallback *>(userData);
    assert(count >= 0 && count <= int(callback->converted.size()));
    for (int i = 0; i < count; ++i) {
        callback->converted[std::size_t(i)] = {
            spans[i].x, spans[i].y, spans[i].len, spans[i].coverage
        };
    }
    callback->process(callback->converted.data(), count, callback->userData);
}

struct GrayRasterDeleter {
    void operator()(TRaster_ *raster) const
    {
        if (raster) {
            gray_raster_done(raster);
        }
    }
};

void rasterizeAntialiased(QT_FT_Outline &outline,
                          const PkRect &clip,
                          ProcessSpans process,
                          void *userData)
{
    constexpr std::size_t poolSize = 1024 * 1024;
    std::unique_ptr<unsigned char[]> pool(new (std::nothrow) unsigned char[poolSize + 15]);
    if (!pool) {
        return;
    }

    QT_FT_Raster raster = nullptr;
    if (gray_raster_new(&raster) != 0) {
        return;
    }
    const std::unique_ptr<TRaster_, GrayRasterDeleter> rasterOwner(raster);
    const uintptr_t alignedAddress = (reinterpret_cast<uintptr_t>(pool.get()) + 15u) & ~uintptr_t(15u);
    auto *aligned = reinterpret_cast<unsigned char *>(alignedAddress);
    gray_raster_reset(raster, reinterpret_cast<char *>(aligned), poolSize);

    GrayCallback callback{process, userData, {}};
    QT_FT_Raster_Params params{};
    params.source = &outline;
    params.flags = QT_FT_RASTER_FLAG_AA | QT_FT_RASTER_FLAG_DIRECT | QT_FT_RASTER_FLAG_CLIP;
    params.gray_spans = processGraySpans;
    params.user = &callback;
    params.clip_box = {clip.x(), clip.y(),
                       int(int64_t(clip.x()) + clip.width()),
                       int(int64_t(clip.y()) + clip.height())};
    gray_raster_render(raster, &params);
}

using Q16Dot16 = int;
constexpr Q16Dot16 fixedOne = 65536;
constexpr int spanBufferSize = 256;
constexpr int scanChunkSize = 64;

Q16Dot16 fixedMultiply(Q16Dot16 x, Q16Dot16 y)
{
    return Q16Dot16((int64_t(x) * int64_t(y)) >> 16);
}

Q16Dot16 intToFixed(int value)
{
    const int64_t fixed = int64_t(value) * fixedOne;
    assert(fixed >= std::numeric_limits<Q16Dot16>::min()
           && fixed <= std::numeric_limits<Q16Dot16>::max());
    return Q16Dot16(fixed);
}

int fixedToInt(Q16Dot16 value)
{
    if (value >= 0) {
        return value / fixedOne;
    }
    return -int((-int64_t(value) + fixedOne - 1) / fixedOne);
}

Q16Dot16 floatToFixed(double value)
{
    const double scaled = value * fixedOne;
    if (scaled >= std::numeric_limits<Q16Dot16>::max()) {
        return std::numeric_limits<Q16Dot16>::max();
    }
    if (scaled <= std::numeric_limits<Q16Dot16>::min()) {
        return std::numeric_limits<Q16Dot16>::min();
    }
    return Q16Dot16(scaled);
}

class SpanBuffer
{
public:
    SpanBuffer(ProcessSpans process, void *userData, const PkRect &clip)
        : m_process(process)
        , m_userData(userData)
        , m_clip(clip)
    {
    }

    ~SpanBuffer()
    {
        flush();
    }

    void addSpan(int x, unsigned int length, int y, uint8_t coverage)
    {
        if (!coverage || !length) {
            return;
        }
        const int64_t right = int64_t(m_clip.x()) + m_clip.width();
        const int64_t bottom = int64_t(m_clip.y()) + m_clip.height();
        if (y < m_clip.y() || int64_t(y) >= bottom
            || x < m_clip.x() || int64_t(x) + length > right) {
            return;
        }
        m_spans[std::size_t(m_count++)] = {x, y, int(length), coverage};
        if (m_count == spanBufferSize) {
            flush();
        }
    }

private:
    void flush()
    {
        if (m_count) {
            m_process(m_spans.data(), m_count, m_userData);
            m_count = 0;
        }
    }

    std::array<Span, spanBufferSize> m_spans{};
    int m_count = 0;
    ProcessSpans m_process;
    void *m_userData;
    PkRect m_clip;
};

class ScanConverter
{
public:
    struct Line {
        Q16Dot16 x;
        Q16Dot16 delta;
        int top;
        int bottom;
        int winding;
    };

    void begin(int top, int bottom, int left, int right,
               bool windingFill, SpanBuffer *spanBuffer)
    {
        m_top = top;
        m_bottom = bottom;
        m_leftFixed = intToFixed(left);
        m_rightFixed = intToFixed(right + 1);
        m_lines.clear();
        m_fillRuleMask = windingFill ? ~0 : 1;
        m_spanBuffer = spanBuffer;
    }

    void mergeCurve(const QT_FT_Vector &pa, const QT_FT_Vector &pb,
                    const QT_FT_Vector &pc, const QT_FT_Vector &pd);
    void mergeLine(QT_FT_Vector a, QT_FT_Vector b);
    void end();

private:
    struct Intersection {
        int x;
        int winding;
        int left;
        int right;
    };

    bool clipLine(Q16Dot16 &xFixed, int &top, int &bottom,
                  Q16Dot16 slopeFixed, Q16Dot16 edgeFixed, int winding);
    void prepareChunk()
    {
        m_size = scanChunkSize;
        m_intersections.assign(scanChunkSize, Intersection{});
    }
    void allocate(int size)
    {
        if (int(m_intersections.size()) < size) {
            m_intersections.resize(std::size_t(size));
        }
    }
    void mergeIntersection(Intersection *root, const Intersection &intersection);
    void emitNode(const Intersection *node);
    void emitSpans(int chunk);

    std::vector<Line> m_lines;
    std::vector<Line *> m_active;
    std::vector<Intersection> m_intersections;
    int m_size = 0;
    int m_top = 0;
    int m_bottom = -1;
    Q16Dot16 m_leftFixed = 0;
    Q16Dot16 m_rightFixed = 0;
    int m_fillRuleMask = 1;
    int m_x = 0;
    int m_y = 0;
    int m_winding = 0;
    SpanBuffer *m_spanBuffer = nullptr;
};

void ScanConverter::emitNode(const Intersection *node)
{
tail_call:
    if (node->left) {
        emitNode(node + node->left);
    }
    if (m_winding & m_fillRuleMask) {
        m_spanBuffer->addSpan(m_x, unsigned(node->x - m_x), m_y, 0xff);
    }
    m_x = node->x;
    m_winding += node->winding;
    if (node->right) {
        node += node->right;
        goto tail_call;
    }
}

void ScanConverter::emitSpans(int chunk)
{
    for (int dy = 0; dy < scanChunkSize; ++dy) {
        m_x = 0;
        m_y = chunk + dy;
        m_winding = 0;
        emitNode(&m_intersections[std::size_t(dy)]);
    }
}

void ScanConverter::mergeIntersection(Intersection *root,
                                      const Intersection &intersection)
{
    Intersection *current = root;
    while (intersection.x != current->x) {
        int &next = intersection.x < current->x ? current->left : current->right;
        if (next) {
            current += next;
        } else {
            Intersection *last = m_intersections.data() + m_size;
            next = int(last - current);
            *last = intersection;
            ++m_size;
            return;
        }
    }
    current->winding += intersection.winding;
}

void ScanConverter::mergeCurve(const QT_FT_Vector &pa, const QT_FT_Vector &pb,
                               const QT_FT_Vector &pc, const QT_FT_Vector &pd)
{
    QT_FT_Vector beziers[4 + 3 * 32];
    QT_FT_Vector *b = beziers;
    b[0] = pa;
    b[1] = pb;
    b[2] = pc;
    b[3] = pd;
    constexpr QT_FT_Pos flatness = 16;

    while (b >= beziers) {
        const QT_FT_Vector delta = {b[3].x - b[0].x, b[3].y - b[0].y};
        const QT_FT_Pos length = std::abs(delta.x) + std::abs(delta.y);
        bool belowThreshold;
        if (length > 64) {
            const int64_t d2 = std::abs(int64_t(b[1].x - b[0].x) * delta.y
                                        - int64_t(b[1].y - b[0].y) * delta.x);
            const int64_t d3 = std::abs(int64_t(b[2].x - b[0].x) * delta.y
                                        - int64_t(b[2].y - b[0].y) * delta.x);
            belowThreshold = d2 + d3 <= int64_t(flatness) * length;
        } else {
            const QT_FT_Pos distance = std::abs(b[0].x - b[1].x)
                + std::abs(b[0].y - b[1].y)
                + std::abs(b[0].x - b[2].x)
                + std::abs(b[0].y - b[2].y);
            belowThreshold = distance <= flatness;
        }
        if (belowThreshold || b == beziers + 3 * 32) {
            mergeLine(b[0], b[3]);
            b -= 3;
            continue;
        }

        b[6] = b[3];
        const QT_FT_Pos xMiddle = (b[1].x + b[2].x) / 2;
        b[1].x = (b[0].x + b[1].x) / 2;
        b[5].x = (b[2].x + b[3].x) / 2;
        b[2].x = (b[1].x + xMiddle) / 2;
        b[4].x = (b[5].x + xMiddle) / 2;
        b[3].x = (b[2].x + b[4].x) / 2;
        const QT_FT_Pos yMiddle = (b[1].y + b[2].y) / 2;
        b[1].y = (b[0].y + b[1].y) / 2;
        b[5].y = (b[2].y + b[3].y) / 2;
        b[2].y = (b[1].y + yMiddle) / 2;
        b[4].y = (b[5].y + yMiddle) / 2;
        b[3].y = (b[2].y + b[4].y) / 2;
        b += 3;
    }
}

bool ScanConverter::clipLine(Q16Dot16 &xFixed, int &top, int &bottom,
                             Q16Dot16 slopeFixed, Q16Dot16 edgeFixed,
                             int winding)
{
    const bool right = edgeFixed == m_rightFixed;
    if (xFixed == edgeFixed) {
        if ((slopeFixed > 0) ^ right) {
            return false;
        }
        m_lines.push_back({edgeFixed, 0, top, bottom, winding});
        return true;
    }

    const Q16Dot16 lastFixed = Q16Dot16(int64_t(xFixed)
        + int64_t(slopeFixed) * (bottom - top));
    if (lastFixed == edgeFixed) {
        if ((slopeFixed < 0) ^ right) {
            return false;
        }
        m_lines.push_back({edgeFixed, 0, top, bottom, winding});
        return true;
    }

    if ((lastFixed < edgeFixed) ^ (xFixed < edgeFixed)) {
        const Q16Dot16 deltaY = Q16Dot16((edgeFixed - xFixed)
                                         / (double(slopeFixed) / fixedOne));
        if ((xFixed < edgeFixed) ^ right) {
            const int height = fixedToInt(deltaY + 1);
            const int middle = top + height;
            m_lines.push_back({edgeFixed, 0, top, middle, winding});
            if (middle != bottom) {
                xFixed += slopeFixed * (height + 1);
                top = middle + 1;
            } else {
                return true;
            }
        } else {
            const int height = fixedToInt(deltaY);
            const int middle = top + height;
            if (middle != bottom) {
                m_lines.push_back({edgeFixed, 0, middle + 1, bottom, winding});
                bottom = middle;
            }
        }
        return false;
    }
    if ((xFixed < edgeFixed) ^ right) {
        m_lines.push_back({edgeFixed, 0, top, bottom, winding});
        return true;
    }
    return false;
}

void ScanConverter::mergeLine(QT_FT_Vector a, QT_FT_Vector b)
{
    int winding = 1;
    if (a.y > b.y) {
        std::swap(a, b);
        winding = -1;
    }

    int top = std::max(m_top, int((a.y + 32) >> 6));
    int bottom = std::min(m_bottom, int((b.y - 32) >> 6));
    if (top > bottom) {
        return;
    }

    const Q16Dot16 aFixed = Q16Dot16(fixedOne / 2 + int64_t(a.x) * 1024);
    if (b.x == a.x) {
        m_lines.push_back({std::clamp(aFixed, m_leftFixed, m_rightFixed),
                           0, top, bottom, winding});
        return;
    }

    const double slope = double(b.x - a.x) / double(b.y - a.y);
    const Q16Dot16 slopeFixed = floatToFixed(slope);
    const Q16Dot16 yOffset = Q16Dot16(int64_t(intToFixed(top))
                                      + fixedOne / 2
                                      - int64_t(a.y) * 1024);
    Q16Dot16 xFixed = aFixed + fixedMultiply(slopeFixed, yOffset);
    if (clipLine(xFixed, top, bottom, slopeFixed, m_leftFixed, winding)) {
        return;
    }
    if (clipLine(xFixed, top, bottom, slopeFixed, m_rightFixed, winding)) {
        return;
    }
    assert(xFixed >= m_leftFixed);
    m_lines.push_back({xFixed, slopeFixed, top, bottom, winding});
}

void ScanConverter::end()
{
    if (m_lines.empty()) {
        m_active.clear();
        return;
    }
    std::sort(m_lines.begin(), m_lines.end(),
              [](const Line &a, const Line &b) { return a.top < b.top; });

    if (m_lines.size() <= 32) {
        const bool allVertical = std::all_of(m_lines.begin(), m_lines.end(),
                                             [](const Line &line) { return line.delta == 0; });
        std::size_t lineIndex = 0;
        for (int y = m_lines.front().top; y <= m_bottom; ++y) {
            while (lineIndex < m_lines.size() && m_lines[lineIndex].top == y) {
                Line *line = &m_lines[lineIndex++];
                if (allVertical) {
                    const auto position = std::upper_bound(
                        m_active.begin(), m_active.end(), line,
                        [](const Line *a, const Line *b) { return a->x < b->x; });
                    m_active.insert(position, line);
                } else {
                    m_active.push_back(line);
                }
            }
            if (!allVertical) {
                std::stable_sort(m_active.begin(), m_active.end(),
                                 [](const Line *a, const Line *b) { return a->x < b->x; });
            }
            int x = 0;
            int winding = 0;
            for (std::size_t i = 0; i < m_active.size();) {
                Line *line = m_active[i];
                const int current = fixedToInt(line->x);
                if (winding & m_fillRuleMask) {
                    m_spanBuffer->addSpan(x, unsigned(current - x), y, 0xff);
                }
                x = current;
                winding += line->winding;
                if (line->bottom == y) {
                    m_active.erase(m_active.begin() + std::ptrdiff_t(i));
                } else {
                    if (!allVertical) {
                        line->x += line->delta;
                    }
                    ++i;
                }
            }
        }
        m_active.clear();
    } else {
        for (int chunkTop = m_top; chunkTop <= m_bottom; chunkTop += scanChunkSize) {
            prepareChunk();
            Intersection intersection{0, 0, 0, 0};
            const int chunkBottom = chunkTop + scanChunkSize;
            for (Line &line : m_lines) {
                if (line.bottom < chunkTop || line.top > chunkBottom) {
                    continue;
                }
                const int top = std::max(0, line.top - chunkTop);
                const int bottom = std::min(scanChunkSize, line.bottom + 1 - chunkTop);
                allocate(m_size + bottom - top);
                intersection.winding = line.winding;
                Intersection *it = m_intersections.data() + top;
                Intersection *end = m_intersections.data() + bottom;
                if (line.delta) {
                    for (; it != end; ++it) {
                        intersection.x = fixedToInt(line.x);
                        line.x += line.delta;
                        mergeIntersection(it, intersection);
                    }
                } else {
                    intersection.x = fixedToInt(line.x);
                    for (; it != end; ++it) {
                        mergeIntersection(it, intersection);
                    }
                }
            }
            emitSpans(chunkTop);
        }
    }
}

void rasterizeBinary(const QT_FT_Outline &outline,
                     const PkRect &clip,
                     ProcessSpans process,
                     void *userData)
{
    if (outline.n_points < 3 || outline.n_contours == 0) {
        return;
    }
    QT_FT_Pos minimumY = outline.points[0].y;
    QT_FT_Pos maximumY = outline.points[0].y;
    for (int i = 1; i < outline.n_points; ++i) {
        minimumY = std::min(minimumY, outline.points[i].y);
        maximumY = std::max(maximumY, outline.points[i].y);
    }
    const int64_t clipBottom = int64_t(clip.y()) + clip.height() - 1;
    const int top = std::max(clip.y(), int((minimumY + 32) >> 6));
    const int bottom = std::min(int(clipBottom), int((maximumY - 32) >> 6));
    if (top > bottom) {
        return;
    }

    SpanBuffer spanBuffer(process, userData, clip);
    ScanConverter converter;
    const int right = int(int64_t(clip.x()) + clip.width() - 1);
    converter.begin(top, bottom, clip.x(), right,
                    outline.flags == QT_FT_OUTLINE_NONE, &spanBuffer);
    int first = 0;
    for (int contour = 0; contour < outline.n_contours; ++contour) {
        const int last = outline.contours[contour];
        for (int point = first; point < last; ++point) {
            if (outline.tags[point + 1] == QT_FT_CURVE_TAG_CUBIC) {
                assert(outline.tags[point + 2] == QT_FT_CURVE_TAG_CUBIC);
                converter.mergeCurve(outline.points[point], outline.points[point + 1],
                                     outline.points[point + 2], outline.points[point + 3]);
                point += 2;
            } else {
                converter.mergeLine(outline.points[point], outline.points[point + 1]);
            }
        }
        first = last + 1;
    }
    converter.end();
}

struct OffsetCallback {
    ProcessSpans process;
    void *userData;
    int originX;
    int originY;
    std::array<Span, spanBufferSize> spans;
};

void processOffsetSpans(const Span *spans, int count, void *userData)
{
    auto *callback = static_cast<OffsetCallback *>(userData);
    assert(count >= 0 && count <= int(callback->spans.size()));
    int outputCount = 0;
    for (int i = 0; i < count; ++i) {
        const int64_t x = int64_t(callback->originX) + spans[i].x;
        const int64_t y = int64_t(callback->originY) + spans[i].y;
        if (x < std::numeric_limits<int>::min() || x > std::numeric_limits<int>::max()
            || y < std::numeric_limits<int>::min() || y > std::numeric_limits<int>::max()) {
            continue;
        }
        callback->spans[std::size_t(outputCount++)] = {
            int(x), int(y), spans[i].length, spans[i].coverage
        };
    }
    if (outputCount) {
        callback->process(callback->spans.data(), outputCount, callback->userData);
    }
}

} // namespace

void rasterizePath(const PkPainterPath &path,
                   const PkRect &clip,
                   bool antialiased,
                   ProcessSpans process,
                   void *userData)
{
    if (!process || clip.isEmpty() || path.isEmpty()) {
        return;
    }
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        if (!std::isfinite(element.x) || !std::isfinite(element.y)) {
            return;
        }
    }

    const PkRect localClip(0, 0, clip.width(), clip.height());
    OutlineStorage storage = makeOutline(path, localClip,
                                         double(clip.x()), double(clip.y()));
    if (storage.outline.n_points < 3 || storage.outline.n_contours == 0) {
        return;
    }
    OffsetCallback callback{process, userData, clip.x(), clip.y(), {}};
    if (antialiased) {
        rasterizeAntialiased(storage.outline, localClip,
                             processOffsetSpans, &callback);
    } else {
        rasterizeBinary(storage.outline, localClip,
                        processOffsetSpans, &callback);
    }
}

} // namespace KisPathRasterizer::Private
