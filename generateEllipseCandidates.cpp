#include "mex.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <limits.h>
#include <float.h>
#include <iostream>
//#include "lapack.h"  //matlab 
//#include "include/lapacke_config.h"  //lapack�ֶ���δ�ɹ� (//lapack manual, unsuccessful)
//#include "include/lapacke.h"
#include "/usr/local/Cellar/opencv@2/2.4.13.7_12/include/opencv2/core/core.hpp"
#include "/usr/local/Cellar/opencv@2/2.4.13.7_12/include/opencv2/features2d/features2d.hpp"
#include "/usr/local/Cellar/opencv@2/2.4.13.7_12/include/opencv2/nonfree/features2d.hpp"
#include "/usr/local/Cellar/opencv@2/2.4.13.7_12/include/opencv2/highgui/highgui.hpp"
#include </usr/local/Cellar/opencv@2/2.4.13.7_12/include/opencv2/opencv.hpp>
#include "lapack.h"
///#include "/Applications/MATLAB_R2018a.app/extern/include/mex.hpp"
//#include "/Applications/MATLAB_R2018a.app/extern/include/mexAdapter.hpp"
using namespace cv;



#ifndef FALSE
#define FALSE 0
#endif /* !FALSE */

#ifndef TRUE
#define TRUE 1
#endif /* !TRUE */

/** Label for pixels with undefined gradient. */
#define NOTDEF -1024.0
/** PI */
#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif /* !M_PI */
#define M_1_2_PI 1.57079632679489661923
#define M_1_4_PI 0.785398163

#define M_3_4_PI 2.35619449

#define M_1_8_PI 0.392699081
#define M_3_8_PI 1.178097245
#define M_5_8_PI 1.963495408
#define M_7_8_PI 2.748893572
#define M_4_9_PI 1.396263401595464  //80��
#define M_1_9_PI  0.34906585  //20��
#define M_1_10_PI 0.314159265358979323846   //18��
#define M_1_12_PI 0.261799387   //15��
#define M_1_15_PI 0.20943951    //12��
#define M_1_18_PI 0.174532925   //10��
/** 3/2 pi */
#define M_3_2_PI 4.71238898038
/** 2 pi */
#define M_2__PI  6.28318530718
/** Doubles relative error factor
 */
#define RELATIVE_ERROR_FACTOR 100.0

struct point2i //(or pixel).
{
	int x,y;
};

struct point2d
{
	double x,y;
};

struct point1d1i
{
	double data;
	int cnt;
};

struct point3d
{
	double x,y;
	double r;
};

struct point3i
{
	int x,y;
	int z;
};

struct point2d1i
{
	double x,y;
	int z;
};

struct  point5d
{
	double x,y;
	double a,b;
	double phi;
};

/*----------------------------------------------------------------------------*/
/** Rectangle structure: line segment with width.
 */
struct rect
{
  double x1,y1,x2,y2;  /* first and second point of the line segment */
  double width;        /* rectangle width */
  double x,y;          /* center of the rectangle */
  double theta;        /* angle */
  double dx,dy;        /* (dx,dy) is vector oriented as the line segment,dx = cos(theta), dy = sin(theta) */
  int   polarity;     /* if the arc direction is the same as the edge direction, polarity = 1, else if opposite ,polarity = -1.*/
  double prec;         /* tolerance angle */
  double p;            /* probability of a point with angle within 'prec' */
};

typedef struct
{
  double vx[4];  /* rectangle's corner X coordinates in circular order */
  double vy[4];  /* rectangle's corner Y coordinates in circular order */
  double ys,ye;  /* start and end Y values of current 'column' */
  int x,y;       /* coordinates of currently explored pixel */
} rect_iter;

typedef struct image_double_s
{
  double * data;
  int xsize,ysize;
} * image_double;


//==================================================================================================
//=============================miscellaneous functions==============================================
inline double min(double v1,double v2)
{
	return (v1<v2?v1:v2);
}
inline double max(double v1,double v2)
{
	return (v1>v2?v1:v2);
}
/** Compare doubles by relative error.

    The resulting rounding error after floating point computations
    depend on the specific operations done. The same number computed by
    different algorithms could present different rounding errors. For a
    useful comparison, an estimation of the relative rounding error
    should be considered and compared to a factor times EPS. The factor
    should be related to the cumulated rounding error in the chain of
    computation. Here, as a simplification, a fixed factor is used.
 */
int double_equal(double a, double b)
{
  double abs_diff,aa,bb,abs_max;

  /* trivial case */
  if( a == b ) return TRUE;

  abs_diff = fabs(a-b);
  aa = fabs(a);
  bb = fabs(b);
  abs_max = aa > bb ? aa : bb;

  /* DBL_MIN is the smallest normalized number, thus, the smallest
     number whose relative error is bounded by DBL_EPSILON. For
     smaller numbers, the same quantization steps as for DBL_MIN
     are used. Then, for smaller numbers, a meaningful "relative"
     error should be computed by dividing the difference by DBL_MIN. */
  if( abs_max < DBL_MIN ) abs_max = DBL_MIN;

  /* equal if relative error <= factor x eps */
  return (abs_diff / abs_max) <= (RELATIVE_ERROR_FACTOR * DBL_EPSILON); //RELATIVE_ERROR_FACTOR=100.0,
}

/*----------------------------------------------------------------------------*/
/** Absolute value angle difference.
 */
//�õ�2�������ƽǶȵļнǵľ���ֵ
//Get the absolute value of the angle between 2 radians
double angle_diff(double a, double b)
{
  a -= b;
  while( a <= -M_PI ) a += M_2__PI;
  while( a >   M_PI ) a -= M_2__PI;
  if( a < 0.0 ) a = -a;
  return a;
}
/*----------------------------------------------------------------------------*/
/** Signed angle difference.
 */
double angle_diff_signed(double a, double b)
{
  a -= b;
  while( a <= -M_PI ) a += M_2__PI;
  while( a >   M_PI ) a -= M_2__PI;
  return a;
}

/*----------------------------------------------------------------------------*/
/** Fatal error, print a message to standard-error output and exit.
 */
void error(char * msg)
{
  fprintf(stderr,"circleDetection Error: %s\n",msg);
  exit(EXIT_FAILURE);
}

/*----------------------------------------------------------------------------*/
/** Computes Euclidean distance between point (x1,y1) and point (x2,y2).
 */
double dist(double x1, double y1, double x2, double y2)
{
  return sqrt( (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) );
}

//�����ڻ�
//vector inner product
double dotProduct(point2d vec1, point2d vec2)
{
	return (vec1.x*vec2.x+vec1.y*vec2.y);
}

/*----------------------------------------------------------------------------*/
/** Copy one rectangle structure to another.
 */
void rect_copy(struct rect * in, struct rect * out)//in is the src, out is the dst
{
  /* check parameters */
  if( in == NULL || out == NULL ) error("rect_copy: invalid 'in' or 'out'.");

  /* copy values */
  out->x1 = in->x1;
  out->y1 = in->y1;
  out->x2 = in->x2;
  out->y2 = in->y2;
  out->width = in->width;
  out->x = in->x;
  out->y = in->y;
  out->theta = in->theta;
  out->dx = in->dx;
  out->dy = in->dy;
  out->polarity = in->polarity;
  out->prec = in->prec;
  out->p = in->p;
}

/*----------------------------------------------------------------------------*/
/** Interpolate y value corresponding to 'x' value given, in
    the line 'x1,y1' to 'x2,y2'; if 'x1=x2' return the smaller
    of 'y1' and 'y2'.

    The following restrictions are required:
    - x1 <= x2
    - x1 <= x
    - x  <= x2
 */
double inter_low(double x, double x1, double y1, double x2, double y2)
{
  /* check parameters */
  if( x1 > x2 || x < x1 || x > x2 )
    error("inter_low: unsuitable input, 'x1>x2' or 'x<x1' or 'x>x2'.");

  /* interpolation */
  if( double_equal(x1,x2) && y1<y2 ) return y1;
  if( double_equal(x1,x2) && y1>y2 ) return y2;
  return y1 + (x-x1) * (y2-y1) / (x2-x1);
}

/*----------------------------------------------------------------------------*/
/** Interpolate y value corresponding to 'x' value given, in
    the line 'x1,y1' to 'x2,y2'; if 'x1=x2' return the larger
    of 'y1' and 'y2'.

    The following restrictions are required:
    - x1 <= x2
    - x1 <= x
    - x  <= x2
 */
double inter_hi(double x, double x1, double y1, double x2, double y2)
{
  /* check parameters */
  if( x1 > x2 || x < x1 || x > x2 )
    error("inter_hi: unsuitable input, 'x1>x2' or 'x<x1' or 'x>x2'.");

  /* interpolation */
  if( double_equal(x1,x2) && y1<y2 ) return y2;
  if( double_equal(x1,x2) && y1>y2 ) return y1;
  return y1 + (x-x1) * (y2-y1) / (x2-x1);
}

/*----------------------------------------------------------------------------*/
/** Free memory used by a rectangle iterator.
 */
void ri_del(rect_iter * iter)
{
  if( iter == NULL ) error("ri_del: NULL iterator.");
  free( (void *) iter );
}

/*----------------------------------------------------------------------------*/
/** Check if the iterator finished the full iteration.

    See details in \ref rect_iter
 */
int ri_end(rect_iter * i)
{
  /* check input */
  if( i == NULL ) error("ri_end: NULL iterator.");

  /* if the current x value is larger than the largest
     x value in the rectangle (vx[2]), we know the full
     exploration of the rectangle is finished. */
  return (double)(i->x) > i->vx[2];
}

/*----------------------------------------------------------------------------*/
/** Increment a rectangle iterator.

    See details in \ref rect_iter
 */
void ri_inc(rect_iter * i)
{
  /* check input */
  if( i == NULL ) error("ri_inc: NULL iterator.");

  /* if not at end of exploration,
     increase y value for next pixel in the 'column' */
  if( !ri_end(i) ) i->y++;

  /* if the end of the current 'column' is reached,
     and it is not the end of exploration,
     advance to the next 'column' */
  while( (double) (i->y) > i->ye && !ri_end(i) )
    {
      /* increase x, next 'column' */
      i->x++;

      /* if end of exploration, return */
      if( ri_end(i) ) return;

      /* update lower y limit (start) for the new 'column'.

         We need to interpolate the y value that corresponds to the
         lower side of the rectangle. The first thing is to decide if
         the corresponding side is

           vx[0],vy[0] to vx[3],vy[3] or
           vx[3],vy[3] to vx[2],vy[2]

         Then, the side is interpolated for the x value of the
         'column'. But, if the side is vertical (as it could happen if
         the rectangle is vertical and we are dealing with the first
         or last 'columns') then we pick the lower value of the side
         by using 'inter_low'.
       */
      if( (double) i->x < i->vx[3] )
        i->ys = inter_low((double)i->x,i->vx[0],i->vy[0],i->vx[3],i->vy[3]);
      else
        i->ys = inter_low((double)i->x,i->vx[3],i->vy[3],i->vx[2],i->vy[2]);

      /* update upper y limit (end) for the new 'column'.

         We need to interpolate the y value that corresponds to the
         upper side of the rectangle. The first thing is to decide if
         the corresponding side is

           vx[0],vy[0] to vx[1],vy[1] or
           vx[1],vy[1] to vx[2],vy[2]

         Then, the side is interpolated for the x value of the
         'column'. But, if the side is vertical (as it could happen if
         the rectangle is vertical and we are dealing with the first
         or last 'columns') then we pick the lower value of the side
         by using 'inter_low'.
       */
      if( (double)i->x < i->vx[1] )
        i->ye = inter_hi((double)i->x,i->vx[0],i->vy[0],i->vx[1],i->vy[1]);
      else
        i->ye = inter_hi((double)i->x,i->vx[1],i->vy[1],i->vx[2],i->vy[2]);

      /* new y */
      i->y = (int) ceil(i->ys);
    }
}

/*----------------------------------------------------------------------------*/
/** Create and initialize a rectangle iterator.

    See details in \ref rect_iter
 */
rect_iter * ri_ini(struct rect * r)
{
  double vx[4],vy[4];
  int n,offset;
  rect_iter * i;

  /* check parameters */
  if( r == NULL ) error("ri_ini: invalid rectangle.");

  /* get memory */
  i = (rect_iter *) malloc(sizeof(rect_iter));
  if( i == NULL ) error("ri_ini: Not enough memory.");

  /* build list of rectangle corners ordered
     in a circular way around the rectangle */
  //���߶ε����(x1,y1)����һ�˿�ʼ������ʱ���ع������ε��ĸ�����
  vx[0] = r->x1 - r->dy * r->width / 2.0;
  vy[0] = r->y1 + r->dx * r->width / 2.0;
  vx[1] = r->x2 - r->dy * r->width / 2.0;
  vy[1] = r->y2 + r->dx * r->width / 2.0;
  vx[2] = r->x2 + r->dy * r->width / 2.0;
  vy[2] = r->y2 - r->dx * r->width / 2.0;
  vx[3] = r->x1 + r->dy * r->width / 2.0;
  vy[3] = r->y1 - r->dx * r->width / 2.0;

  /* compute rotation of index of corners needed so that the first
     point has the smaller x.

     if one side is vertical, thus two corners have the same smaller x
     value, the one with the largest y value is selected as the first.
   */
  if( r->x1 < r->x2 && r->y1 <= r->y2 ) offset = 0;
  else if( r->x1 >= r->x2 && r->y1 < r->y2 ) offset = 1;
  else if( r->x1 > r->x2 && r->y1 >= r->y2 ) offset = 2;
  else offset = 3;

  /* apply rotation of index. */
  for(n=0; n<4; n++)
    {
      i->vx[n] = vx[(offset+n)%4];
      i->vy[n] = vy[(offset+n)%4];
    }

  /* Set an initial condition.

     The values are set to values that will cause 'ri_inc' (that will
     be called immediately) to initialize correctly the first 'column'
     and compute the limits 'ys' and 'ye'.

     'y' is set to the integer value of vy[0], the starting corner.

     'ys' and 'ye' are set to very small values, so 'ri_inc' will
     notice that it needs to start a new 'column'.

     The smallest integer coordinate inside of the rectangle is
     'ceil(vx[0])'. The current 'x' value is set to that value minus
     one, so 'ri_inc' (that will increase x by one) will advance to
     the first 'column'.
   */
  i->x = (int) ceil(i->vx[0]) - 1;
  i->y = (int) ceil(i->vy[0]);
  i->ys = i->ye = -DBL_MAX;

  /* advance to the first pixel */
  ri_inc(i);

  return i;
}


/*----------------------------------------------------------------------------*/
/** Free memory used in image_double 'i'.
 */
void free_image_double(image_double i)
{
  if( i == NULL || i->data == NULL )
    error("free_image_double: invalid input image.");
  free( (void *) i->data );
  free( (void *) i );
}

/*----------------------------------------------------------------------------*/
/** Create a new image_double of size 'xsize' times 'ysize'.
 */
image_double new_image_double(int xsize, int ysize)
{
  image_double image;

  /* check parameters */
  if( xsize == 0 || ysize == 0 ) error("new_image_double: invalid image size.");

  /* get memory */
  image = (image_double) malloc( sizeof(struct image_double_s) );
  if( image == NULL ) error("not enough memory.");
  image->data = (double *) calloc( (size_t) (xsize*ysize), sizeof(double) );
  if( image->data == NULL ) error("not enough memory.");

  /* set image size */
  image->xsize = xsize;
  image->ysize = ysize;

  return image;
}

/*----------------------------------------------------------------------------*/
/** Create a new image_double of size 'xsize' times 'ysize'
    with the data pointed by 'data'.
 */
image_double new_image_double_ptr( int xsize,
                                          int ysize, double * data )
{
  image_double image;

  /* check parameters */
  if( xsize == 0 || ysize == 0 )
    error("new_image_double_ptr: invalid image size.");
  if( data == NULL ) error("new_image_double_ptr: NULL data pointer.");

  /* get memory */
  image = (image_double) malloc( sizeof(struct image_double_s) );
  if( image == NULL ) error("not enough memory.");

  /* set image */
  image->xsize = xsize;
  image->ysize = ysize;
  image->data = data;

  return image;
}

//=================================================================================================================
//===========================================LSD functions=========================================================
/** ln(10) */
#ifndef M_LN10
#define M_LN10 2.30258509299404568402    //ln10
#endif /* !M_LN10 */

/** Label for pixels not used in yet. */
#define NOTUSED 0

/** Label for pixels already used in detection. */
#define USED    1

//���ڹ���Բ�������ر�Ǽ��ԣ�����ݶȵķ���ͻ��ķ���ָ��һ�£���ΪSAME_POLE,����ΪOPP_POLE,�ñ�ǳ�ʼ��Ϊ0
//For the polarity of the pixel marker that forms the arc, if the direction of the gradient is the same as the direction of the arc, it is SAME_POLE, otherwise it is OPP_POLE, and the marker is initially 0
#define NOTDEF_POL 0
#define SAME_POL 1
#define OPP_POL  -1
/*----------------------------------------------------------------------------*/
/** Chained list of coordinates.
 */
struct coorlist
{
  int x,y;
  struct coorlist * next;
};
typedef struct ntuple_list_s
{
  int size;
  int max_size;
  int dim;
  double * values;
} * ntuple_list;

/*----------------------------------------------------------------------------*/
/** Free memory used in n-tuple 'in'.
 */
static void free_ntuple_list(ntuple_list in)
{
  if( in == NULL || in->values == NULL )
    error("free_ntuple_list: invalid n-tuple input.");
  free( (void *) in->values );
  free( (void *) in );
}

/*----------------------------------------------------------------------------*/
/** Create an n-tuple list and allocate memory for one element.
    @param dim the dimension (n) of the n-tuple.
 */
static ntuple_list new_ntuple_list(int dim)
{
  ntuple_list n_tuple;

  /* check parameters */
  if( dim == 0 ) error("new_ntuple_list: 'dim' must be positive.");

  /* get memory for list structure */
  n_tuple = (ntuple_list) malloc( sizeof(struct ntuple_list_s) );
  if( n_tuple == NULL ) error("not enough memory.");

  /* initialize list */
  n_tuple->size = 0;
  n_tuple->max_size = 1;
  n_tuple->dim = dim;

  /* get memory for tuples */
  n_tuple->values = (double *) malloc( dim*n_tuple->max_size * sizeof(double) );
  if( n_tuple->values == NULL ) error("not enough memory.");

  return n_tuple;
}

/*----------------------------------------------------------------------------*/
/** Enlarge the allocated memory of an n-tuple list.
 */
static void enlarge_ntuple_list(ntuple_list n_tuple)
{
  /* check parameters */
  if( n_tuple == NULL || n_tuple->values == NULL || n_tuple->max_size == 0 )
    error("enlarge_ntuple_list: invalid n-tuple.");

  /* duplicate number of tuples */
  n_tuple->max_size *= 2;

  /* realloc memory */
  n_tuple->values = (double *) realloc( (void *) n_tuple->values,
                      n_tuple->dim * n_tuple->max_size * sizeof(double) );
  if( n_tuple->values == NULL ) error("not enough memory.");
}

/*----------------------------------------------------------------------------*/
/** Add a 7-tuple to an n-tuple list.
 */
static void add_7tuple( ntuple_list out, double v1, double v2, double v3,
                        double v4, double v5, double v6, double v7 )
{
  /* check parameters */
  if( out == NULL ) error("add_7tuple: invalid n-tuple input.");
  if( out->dim != 7 ) error("add_7tuple: the n-tuple must be a 7-tuple.");

  /* if needed, alloc more tuples to 'out' */
  if( out->size == out->max_size ) enlarge_ntuple_list(out);
  if( out->values == NULL ) error("add_7tuple: invalid n-tuple input.");

  /* add new 7-tuple */
  out->values[ out->size * out->dim + 0 ] = v1;
  out->values[ out->size * out->dim + 1 ] = v2;
  out->values[ out->size * out->dim + 2 ] = v3;
  out->values[ out->size * out->dim + 3 ] = v4;
  out->values[ out->size * out->dim + 4 ] = v5;
  out->values[ out->size * out->dim + 5 ] = v6;
  out->values[ out->size * out->dim + 6 ] = v7;

  /* update number of tuples counter */
  out->size++;
}
/*----------------------------------------------------------------------------*/
/** Add a 8-tuple to an n-tuple list.
 */
static void add_8tuple( ntuple_list out, double v1, double v2, double v3,
                        double v4, double v5, double v6, double v7, int v8)
{
  /* check parameters */
  if( out == NULL ) error("add_8tuple: invalid n-tuple input.");
  if( out->dim != 8 ) error("add_8tuple: the n-tuple must be a 8-tuple.");

  /* if needed, alloc more tuples to 'out' */
  if( out->size == out->max_size ) enlarge_ntuple_list(out);
  if( out->values == NULL ) error("add_8tuple: invalid n-tuple input.");

  /* add new 8-tuple */
  out->values[ out->size * out->dim + 0 ] = v1;
  out->values[ out->size * out->dim + 1 ] = v2;
  out->values[ out->size * out->dim + 2 ] = v3;
  out->values[ out->size * out->dim + 3 ] = v4;
  out->values[ out->size * out->dim + 4 ] = v5;
  out->values[ out->size * out->dim + 5 ] = v6;
  out->values[ out->size * out->dim + 6 ] = v7;
  out->values[ out->size * out->dim + 7 ] = v8;

  /* update number of tuples counter */
  out->size++;
}
/** char image data type

    The pixel value at (x,y) is accessed by:

      image->data[ x + y * image->xsize ]

    with x and y integer.
 */
typedef struct image_char_s
{
  unsigned char * data;
  unsigned int xsize,ysize;
} * image_char;

/*----------------------------------------------------------------------------*/
/** Free memory used in image_char 'i'.
 */
static void free_image_char(image_char i)
{
  if( i == NULL || i->data == NULL )
    error("free_image_char: invalid input image.");
  free( (void *) i->data );
  free( (void *) i );
}

/*----------------------------------------------------------------------------*/
/** Create a new image_char of size 'xsize' times 'ysize'.
 */
static image_char new_image_char(unsigned int xsize, unsigned int ysize)
{
  image_char image;

  /* check parameters */
  if( xsize == 0 || ysize == 0 ) error("new_image_char: invalid image size.");

  /* get memory */
  image = (image_char) malloc( sizeof(struct image_char_s) );
  if( image == NULL ) error("not enough memory.");
  image->data = (unsigned char *) calloc( (size_t) (xsize*ysize),
                                          sizeof(unsigned char) );
  if( image->data == NULL ) error("not enough memory.");

  /* set image size */
  image->xsize = xsize;
  image->ysize = ysize;

  return image;
}

/*----------------------------------------------------------------------------*/
/** Create a new image_char of size 'xsize' times 'ysize',
    initialized to the value 'fill_value'.
 */
static image_char new_image_char_ini( unsigned int xsize, unsigned int ysize,
                                      unsigned char fill_value )
{
  image_char image = new_image_char(xsize,ysize); /* create image */
  unsigned int N = xsize*ysize;
  unsigned int i;

  /* check parameters */
  if( image == NULL || image->data == NULL )
    error("new_image_char_ini: invalid image.");

  /* initialize */
  for(i=0; i<N; i++) image->data[i] = fill_value;

  return image;
}

/*----------------------------------------------------------------------------*/
/** int image data type

    The pixel value at (x,y) is accessed by:

      image->data[ x + y * image->xsize ]

    with x and y integer.
 */
typedef struct image_int_s
{
  int * data;
  unsigned int xsize,ysize;
} * image_int;

/*----------------------------------------------------------------------------*/
/** Create a new image_int of size 'xsize' times 'ysize'.
 */
static image_int new_image_int(unsigned int xsize, unsigned int ysize)
{
  image_int image;

  /* check parameters */
  if( xsize == 0 || ysize == 0 ) error("new_image_int: invalid image size.");

  /* get memory */
  image = (image_int) malloc( sizeof(struct image_int_s) );
  if( image == NULL ) error("not enough memory.");
  image->data = (int *) calloc( (size_t) (xsize*ysize), sizeof(int) );
  if( image->data == NULL ) error("not enough memory.");

  /* set image size */
  image->xsize = xsize;
  image->ysize = ysize;

  return image;
}

/*----------------------------------------------------------------------------*/
/** Create a new image_int of size 'xsize' times 'ysize',
    initialized to the value 'fill_value'.
 */
static image_int new_image_int_ini( unsigned int xsize, unsigned int ysize,
                                    int fill_value )
{
  image_int image = new_image_int(xsize,ysize); /* create image */
  unsigned int N = xsize*ysize;
  unsigned int i;

  /* initialize */
  for(i=0; i<N; i++) image->data[i] = fill_value;

  return image;
}
/** Compute a Gaussian kernel of length 'kernel->dim',
    standard deviation 'sigma', and centered at value 'mean'.

    For example, if mean=0.5, the Gaussian will be centered
    in the middle point2i between values 'kernel->values[0]'
    and 'kernel->values[1]'.
 */
static void gaussian_kernel(ntuple_list kernel, double sigma, double mean)
{
  double sum = 0.0;
  double val;
  int i;

  /* check parameters */
  if( kernel == NULL || kernel->values == NULL )
    error("gaussian_kernel: invalid n-tuple 'kernel'.");
  if( sigma <= 0.0 ) error("gaussian_kernel: 'sigma' must be positive.");

  /* compute Gaussian kernel */
  if( kernel->max_size < 1 ) enlarge_ntuple_list(kernel);
  kernel->size = 1;
  for(i=0;i<kernel->dim;i++)
    {
      val = ( (double) i - mean ) / sigma;
      kernel->values[i] = exp( -0.5 * val * val );
      sum += kernel->values[i];
    }

  /* normalization */
  if( sum >= 0.0 ) for(i=0;i<kernel->dim;i++) kernel->values[i] /= sum;
}

/*----------------------------------------------------------------------------*/
/** Scale the input image 'in' by a factor 'scale' by Gaussian sub-sampling.

    For example, scale=0.8 will give a result at 80% of the original size.

    The image is convolved with a Gaussian kernel
    @f[
        G(x,y) = \frac{1}{2\pi\sigma^2} e^{-\frac{x^2+y^2}{2\sigma^2}}
    @f]
    before the sub-sampling to prevent aliasing.

    The standard deviation sigma given by:
    -  sigma = sigma_scale / scale,   if scale <  1.0
    -  sigma = sigma_scale,           if scale >= 1.0

    To be able to sub-sample at non-integer steps, some interpolation
    is needed. In this implementation, the interpolation is done by
    the Gaussian kernel, so both operations (filtering and sampling)
    are done at the same time. The Gaussian kernel is computed
    centered on the coordinates of the required sample. In this way,
    when applied, it gives directly the result of convolving the image
    with the kernel and interpolated to that particular position.

    A fast algorithm is done using the separability of the Gaussian
    kernel. Applying the 2D Gaussian kernel is equivalent to applying
    first a horizontal 1D Gaussian kernel and then a vertical 1D
    Gaussian kernel (or the other way round). The reason is that
    @f[
        G(x,y) = G(x) * G(y)
    @f]
    where
    @f[
        G(x) = \frac{1}{\sqrt{2\pi}\sigma} e^{-\frac{x^2}{2\sigma^2}}.
    @f]
    The algorithm first applies a combined Gaussian kernel and sampling
    in the x axis, and then the combined Gaussian kernel and sampling
    in the y axis.
 */
static image_double gaussian_sampler( image_double in, double scale,
                                      double sigma_scale )
{
  image_double aux,out;
  ntuple_list kernel;
  int N,M,h,n,x,y,i;
  int xc,yc,j,double_x_size,double_y_size;
  double sigma,xx,yy,sum,prec;

  /* check parameters */
  if( in == NULL || in->data == NULL || in->xsize == 0 || in->ysize == 0 )
    error("gaussian_sampler: invalid image.");
  if( scale <= 0.0 ) error("gaussian_sampler: 'scale' must be positive.");
  if( sigma_scale <= 0.0 )
    error("gaussian_sampler: 'sigma_scale' must be positive.");

  /* compute new image size and get memory for images */
  if( in->xsize * scale > (double) UINT_MAX ||
      in->ysize * scale > (double) UINT_MAX )
    error("gaussian_sampler: the output image size exceeds the handled size.");
  N = (unsigned int) ceil( in->xsize * scale );//��ȡ�� (round up)
  M = (unsigned int) ceil( in->ysize * scale );
  aux = new_image_double(N,in->ysize);
  out = new_image_double(N,M);

  /* sigma, kernel size and memory for the kernel */
  sigma = scale < 1.0 ? sigma_scale / scale : sigma_scale;
  /*
     The size of the kernel is selected to guarantee that the
     the first discarded term is at least 10^prec times smaller
     than the central value. For that, h should be larger than x, with
       e^(-x^2/2sigma^2) = 1/10^prec.
     Then,
       x = sigma * sqrt( 2 * prec * ln(10) ).
   */
  prec = 3.0;//��˹�˵�����Χ����10^(-3) (The outermost periphery of the Gaussian kernel drops to 10^(-3))
  h = (unsigned int) ceil( sigma * sqrt( 2.0 * prec * log(10.0) ) );
  n = 1+2*h; /* kernel size */
  kernel = new_ntuple_list(n);

  /* auxiliary double image size variables */
  double_x_size = (int) (2 * in->xsize);
  double_y_size = (int) (2 * in->ysize);

  /* First subsampling: x axis */
  for(x=0;x<aux->xsize;x++)
    {
      /*
         x   is the coordinate in the new image.
         xx  is the corresponding x-value in the original size image.
         xc  is the integer value, the pixel coordinate of xx.
       */
      xx = (double) x / scale;
      /* coordinate (0.0,0.0) is in the center of pixel (0,0),
         so the pixel with xc=0 get the values of xx from -0.5 to 0.5 */
      xc = (int) floor( xx + 0.5 );
      gaussian_kernel( kernel, sigma, (double) h + xx - (double) xc );
      /* the kernel must be computed for each x because the fine
         offset xx-xc is different in each case */

      for(y=0;y<aux->ysize;y++)
        {
          sum = 0.0;
          for(i=0;i<kernel->dim;i++)
            {
              j = xc - h + i;

              /* symmetry boundary condition */
              while( j < 0 ) j += double_x_size;
              while( j >= double_x_size ) j -= double_x_size;
              if( j >= (int) in->xsize ) j = double_x_size-1-j;

              sum += in->data[ j + y * in->xsize ] * kernel->values[i];
            }
          aux->data[ x + y * aux->xsize ] = sum;
        }
    }

  /* Second subsampling: y axis */
  for(y=0;y<out->ysize;y++)
    {
      /*
         y   is the coordinate in the new image.
         yy  is the corresponding x-value in the original size image.
         yc  is the integer value, the pixel coordinate of xx.
       */
      yy = (double) y / scale;
      /* coordinate (0.0,0.0) is in the center of pixel (0,0),
         so the pixel with yc=0 get the values of yy from -0.5 to 0.5 */
      yc = (int) floor( yy + 0.5 );
      gaussian_kernel( kernel, sigma, (double) h + yy - (double) yc );
      /* the kernel must be computed for each y because the fine
         offset yy-yc is different in each case */

      for(x=0;x<out->xsize;x++)
        {
          sum = 0.0;
          for(i=0;i<kernel->dim;i++)
            {
              j = yc - h + i;

              /* symmetry boundary condition */
              while( j < 0 ) j += double_y_size;
              while( j >= double_y_size ) j -= double_y_size;
              if( j >= (int) in->ysize ) j = double_y_size-1-j;

              sum += aux->data[ x + j * aux->xsize ] * kernel->values[i];
            }
          out->data[ x + y * out->xsize ] = sum;
        }
    }

  /* free memory */
  free_ntuple_list(kernel);
  free_image_double(aux);

  return out;
}


/*----------------------------------------------------------------------------*/
/*--------------------------------- Gradient ---------------------------------*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/** Computes the direction of the level line of 'in' at each point2i.

    The result is:
    - an image_double with the angle at each pixel, or NOTDEF if not defined.
    - the image_double 'modgrad' (a point2ier is passed as argument)
      with the gradient magnitude at each point2i.
    - a list of pixels 'list_p' roughly ordered by decreasing
      gradient magnitude. (The order is made by classifying point2is
      into bins by gradient magnitude. The parameters 'n_bins' and
      'max_grad' specify the number of bins and the gradient modulus
      at the highest bin. The pixels in the list would be in
      decreasing gradient magnitude, up to a precision of the size of
      the bins.)
    - a point2ier 'mem_p' to the memory used by 'list_p' to be able to
      free the memory when it is not used anymore.
 */
//����һ���ݶȽǶ�˳ʱ����ת90����align�Ƕ�ͼangles������ݶȽǶ���(gx,gy)->(-gy,gx)��
//���ݶȵ�ģ��ͼmodgrad,Ȼ����n_bins����α���򷵻������ͷָ��list_p,������������
//Returns an align angle map angles after the gradient angle is rotated 90�� clockwise, 
//if the gradient angle is (gx,gy)->(-gy,gx), and the map modgrad of the gradient's modulus, 
//and then returns according to the pseudo-sort of n_bins The head pointer list_p of the linked list, 
//which stores the coordinates
static image_double ll_angle( image_double in, double threshold,
                              struct coorlist ** list_p,
                              image_double * modgrad, unsigned int n_bins )
{
  image_double g;
  unsigned int n,p,x,y,adr,i;
  double com1,com2,gx,gy,norm,norm2;
  /* the rest of the variables are used for pseudo-ordering
     the gradient magnitude values */
  int list_count = 0;
  //struct coorlist * list;
  struct coorlist *temp;
  struct coorlist ** range_l_s; /* array of point2iers to start of bin list,��ʾ1024��bin��ͷָ���ָ������ */
  struct coorlist ** range_l_e; /* array of point2iers to end of bin list����ʾ1024��bin��βָ���ָ������*/
  struct coorlist * start;
  struct coorlist * end;
  double max_grad = 0.0;

  /* check parameters */
  if( in == NULL || in->data == NULL || in->xsize == 0 || in->ysize == 0 )
    error("ll_angle: invalid image.");
  if( threshold < 0.0 ) error("ll_angle: 'threshold' must be positive.");
  if( list_p == NULL ) error("ll_angle: NULL point2ier 'list_p'.");
 // if( mem_p == NULL ) error("ll_angle: NULL point2ier 'mem_p'.");
  if( modgrad == NULL ) error("ll_angle: NULL point2ier 'modgrad'.");
  if( n_bins == 0 ) error("ll_angle: 'n_bins' must be positive.");

  /* image size shortcuts */
  n = in->ysize;
  p = in->xsize;

  /* allocate output image */
  g = new_image_double(in->xsize,in->ysize);

  /* get memory for the image of gradient modulus */
  *modgrad = new_image_double(in->xsize,in->ysize);

  /* get memory for "ordered" list of pixels */
  //list = (struct coorlist *) calloc( (size_t) (n*p), sizeof(struct coorlist) );
  //*mem_p = (void *) list;
  range_l_s = (struct coorlist **) calloc( (size_t) n_bins,
                                           sizeof(struct coorlist *) );
  range_l_e = (struct coorlist **) calloc( (size_t) n_bins,
                                           sizeof(struct coorlist *) );
 // if( list == NULL || range_l_s == NULL || range_l_e == NULL )
  if( range_l_s == NULL || range_l_e == NULL )
    error("not enough memory.");
  for(i=0;i<n_bins;i++) range_l_s[i] = range_l_e[i] = NULL;

  /* 'undefined' on the down and right boundaries */
  for(x=0;x<p;x++) g->data[(n-1)*p+x] = NOTDEF;// p = in->xsize
  for(y=0;y<n;y++) g->data[p*y+p-1]   = NOTDEF;// n = in->ysize;

  /* compute gradient on the remaining pixels */
  for(x=0;x<p-1;x++)
    for(y=0;y<n-1;y++)
      {
        adr = y*p+x;

        /*
           Norm 2 computation using 2x2 pixel window:
             A B
             C D
           and
             com1 = D-A,  com2 = B-C.
           Then
             gx = B+D - (A+C)   horizontal difference
             gy = C+D - (A+B)   vertical difference
           com1 and com2 are just to avoid 2 additions.
         */
        com1 = in->data[adr+p+1] - in->data[adr];
        com2 = in->data[adr+1]   - in->data[adr+p];

        gx = com1+com2; /* gradient x component */
        gy = com1-com2; /* gradient y component */
        norm2 = gx*gx+gy*gy;
        norm = sqrt( norm2 / 4.0 ); /* gradient norm */

        (*modgrad)->data[adr] = norm; /* store gradient norm */

        if( norm <= threshold ) /* norm too small, gradient no defined */
          g->data[adr] = NOTDEF; /* gradient angle not defined */
        else
          {
            /* gradient angle computation */
            g->data[adr] = atan2(gx,-gy);

            /* look for the maximum of the gradient */
            if( norm > max_grad ) max_grad = norm;
          }
      }

  /* compute histogram of gradient values */
  for(x=0;x<p-1;x++)
    for(y=0;y<n-1;y++)
      {
		temp = new coorlist();
		if(temp == NULL)
		{
			printf("not enough memory");
			system("pause");
		}
        norm = (*modgrad)->data[y*p+x];
        /* store the point2i in the right bin according to its norm */
        i = (unsigned int) (norm * (double) n_bins / max_grad);
        if( i >= n_bins ) i = n_bins-1;
        if( range_l_e[i] == NULL )
          range_l_s[i] = range_l_e[i] = temp;//��¼��i�������ͷָ�뵽range_l_s[i] (Record the head pointer of the i-th area to range_l_s[i])
        else
          {
            range_l_e[i]->next = temp;//��i��������βָ��range_l_e[i]��ɹ��� (The i-th area is linked by the tail pointer range_l_e[i])
            range_l_e[i] = temp;
          }
        range_l_e[i]->x = (int) x;//������(x,y)��¼����i������ (Record the coordinates (x,y) to the ith partition)
        range_l_e[i]->y = (int) y;
        range_l_e[i]->next = NULL;
      }

  /* Make the list of pixels (almost) ordered by norm value.
     It starts by the larger bin, so the list starts by the
     pixels with the highest gradient value. Pixels would be ordered
     by norm value, up to a precision given by max_grad/n_bins.
   */
  for(i=n_bins-1; i>0 && range_l_s[i]==NULL; i--);//�ҵ���һ����Ϊ�յķ���bin (Find the first partition bin that is not empty)
  start = range_l_s[i];
  end = range_l_e[i];
  if( start != NULL )
    while(i>0)
      {
        --i;
        if( range_l_s[i] != NULL )
          {
            end->next = range_l_s[i];
            end = range_l_e[i];
          }
      }
  *list_p = start;
 // *mem_p  = start;
  /* free memory */
  free( (void *) range_l_s );
  free( (void *) range_l_e );

  return g;
}
/*----------------------------------------------------------------------------*/
/** Is point2i (x,y) aligned to angle theta, up to precision 'prec'?
 */
static int isaligned( int x, int y, image_double angles, double theta,
                      double prec )
{
  double a;

  /* check parameters */
  if( angles == NULL || angles->data == NULL )
    error("isaligned: invalid image 'angles'.");
  if( x < 0 || y < 0 || x >= (int) angles->xsize || y >= (int) angles->ysize )
    error("isaligned: (x,y) out of the image.");
  if( prec < 0.0 ) error("isaligned: 'prec' must be positive.");

  /* angle at pixel (x,y) */
  a = angles->data[ x + y * angles->xsize ];

  /* pixels whose level-line angle is not defined
     are considered as NON-aligned */
  if( a == NOTDEF ) return FALSE;  /* there is no need to call the function
                                      'double_equal' here because there is
                                      no risk of problems related to the
                                      comparison doubles, we are only
                                      interested in the exact NOTDEF value */

  /* it is assumed that 'theta' and 'a' are in the range [-pi,pi] */
  theta -= a;
  if( theta < 0.0 ) theta = -theta;
  if( theta > M_3_2_PI )
    {
	//--------------------------------------
	//origin code
     /* theta -= M_2__PI;
      if( theta < 0.0 ) theta = -theta;*/
	//--------------------------------------
	  //-------------------------------------
	  //mycode
	  theta = M_2__PI-theta;
	  if(theta < 0.0) 
		 theta = -theta; 
	  //--------------------------------------
    }

  return theta <= prec;
}


/*----------------------------------------------------------------------------*/
/*----------------------------- NFA computation ------------------------------*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/** Computes the natural logarithm of the absolute value of
    the gamma function of x using the Lanczos approximation.
    See http://www.rskey.org/gamma.htm

    The formula used is
    @f[
      \Gamma(x) = \frac{ \sum_{n=0}^{N} q_n x^n }{ \Pi_{n=0}^{N} (x+n) }
                  (x+5.5)^{x+0.5} e^{-(x+5.5)}
    @f]
    so
    @f[
      \log\Gamma(x) = \log\left( \sum_{n=0}^{N} q_n x^n \right)
                      + (x+0.5) \log(x+5.5) - (x+5.5) - \sum_{n=0}^{N} \log(x+n)
    @f]
    and
      q0 = 75122.6331530,
      q1 = 80916.6278952,
      q2 = 36308.2951477,
      q3 = 8687.24529705,
      q4 = 1168.92649479,
      q5 = 83.8676043424,
      q6 = 2.50662827511.
 */
static double log_gamma_lanczos(double x)
{
  static double q[7] = { 75122.6331530, 80916.6278952, 36308.2951477,
                         8687.24529705, 1168.92649479, 83.8676043424,
                         2.50662827511 };
  double a = (x+0.5) * log(x+5.5) - (x+5.5);
  double b = 0.0;
  int n;

  for(n=0;n<7;n++)
    {
      a -= log( x + (double) n );
      b += q[n] * pow( x, (double) n );
    }
  return a + log(b);
}

/*----------------------------------------------------------------------------*/
/** Computes the natural logarithm of the absolute value of
    the gamma function of x using Windschitl method.
    See http://www.rskey.org/gamma.htm

    The formula used is
    @f[
        \Gamma(x) = \sqrt{\frac{2\pi}{x}} \left( \frac{x}{e}
                    \sqrt{ x\sinh(1/x) + \frac{1}{810x^6} } \right)^x
    @f]
    so
    @f[
        \log\Gamma(x) = 0.5\log(2\pi) + (x-0.5)\log(x) - x
                      + 0.5x\log\left( x\sinh(1/x) + \frac{1}{810x^6} \right).
    @f]
    This formula is a good approximation when x > 15.
 */
static double log_gamma_windschitl(double x)
{
  return 0.918938533204673 + (x-0.5)*log(x) - x
         + 0.5*x*log( x*sinh(1/x) + 1/(810.0*pow(x,6.0)) );
}

/*----------------------------------------------------------------------------*/
/** Computes the natural logarithm of the absolute value of
    the gamma function of x. When x>15 use log_gamma_windschitl(),
    otherwise use log_gamma_lanczos().
 */
#define log_gamma(x) ((x)>15.0?log_gamma_windschitl(x):log_gamma_lanczos(x))

/*----------------------------------------------------------------------------*/
/** Size of the table to store already computed inverse values.
 */
#define TABSIZE 100000

/*----------------------------------------------------------------------------*/
/** Computes -log10(NFA).

    NFA stands for Number of False Alarms:
    @f[
        \mathrm{NFA} = NT \cdot B(n,k,p)
    @f]

    - NT       - number of tests
    - B(n,k,p) - tail of binomial distribution with parameters n,k and p:
    @f[
        B(n,k,p) = \sum_{j=k}^n
                   \left(\begin{array}{c}n\\j\end{array}\right)
                   p^{j} (1-p)^{n-j}
    @f]

    The value -log10(NFA) is equivalent but more intuitive than NFA:
    - -1 corresponds to 10 mean false alarms
    -  0 corresponds to 1 mean false alarm
    -  1 corresponds to 0.1 mean false alarms
    -  2 corresponds to 0.01 mean false alarms
    -  ...

    Used this way, the bigger the value, better the detection,
    and a logarithmic scale is used.

    @param n,k,p binomial parameters.
    @param logNT logarithm of Number of Tests

    The computation is based in the gamma function by the following
    relation:
    @f[
        \left(\begin{array}{c}n\\k\end{array}\right)
        = \frac{ \Gamma(n+1) }{ \Gamma(k+1) \cdot \Gamma(n-k+1) }.
    @f]
    We use efficient algorithms to compute the logarithm of
    the gamma function.

    To make the computation faster, not all the sum is computed, part
    of the terms are neglected based on a bound to the error obtained
    (an error of 10% in the result is accepted).
 */
static double nfa(int n, int k, double p, double logNT)
{
  static double inv[TABSIZE];   /* table to keep computed inverse values */
  double tolerance = 0.1;       /* an error of 10% in the result is accepted */
  double log1term,term,bin_term,mult_term,bin_tail,err,p_term;
  int i;

  /* check parameters */
  if( n<0 || k<0 || k>n || p<=0.0 || p>=1.0 )
    error("nfa: wrong n, k or p values.");

  /* trivial cases */
  if( n==0 || k==0 ) return -logNT;
  if( n==k ) return -logNT - (double) n * log10(p);

  /* probability term */
  p_term = p / (1.0-p);

  /* compute the first term of the series */
  /*
     binomial_tail(n,k,p) = sum_{i=k}^n bincoef(n,i) * p^i * (1-p)^{n-i}
     where bincoef(n,i) are the binomial coefficients.
     But
       bincoef(n,k) = gamma(n+1) / ( gamma(k+1) * gamma(n-k+1) ).
     We use this to compute the first term. Actually the log of it.
   */
  log1term = log_gamma( (double) n + 1.0 ) - log_gamma( (double) k + 1.0 )
           - log_gamma( (double) (n-k) + 1.0 )
           + (double) k * log(p) + (double) (n-k) * log(1.0-p);
  term = exp(log1term);

  /* in some cases no more computations are needed */
  if( double_equal(term,0.0) )              /* the first term is almost zero */
    {
      if( (double) k > (double) n * p )     /* at begin or end of the tail?  */
        return -log1term / M_LN10 - logNT;  /* end: use just the first term  */
      else
        return -logNT;                      /* begin: the tail is roughly 1  */
    }

  /* compute more terms if needed */
  bin_tail = term;
  for(i=k+1;i<=n;i++)
    {
      /*
         As
           term_i = bincoef(n,i) * p^i * (1-p)^(n-i)
         and
           bincoef(n,i)/bincoef(n,i-1) = n-1+1 / i,
         then,
           term_i / term_i-1 = (n-i+1)/i * p/(1-p)
         and
           term_i = term_i-1 * (n-i+1)/i * p/(1-p).
         1/i is stored in a table as they are computed,
         because divisions are expensive.
         p/(1-p) is computed only once and stored in 'p_term'.
       */
      bin_term = (double) (n-i+1) * ( i<TABSIZE ?
                   ( inv[i]!=0.0 ? inv[i] : ( inv[i] = 1.0 / (double) i ) ) :
                   1.0 / (double) i );

      mult_term = bin_term * p_term;
      term *= mult_term;
      bin_tail += term;
      if(bin_term<1.0)
        {
          /* When bin_term<1 then mult_term_j<mult_term_i for j>i.
             Then, the error on the binomial tail when truncated at
             the i term can be bounded by a geometric series of form
             term_i * sum mult_term_i^j.                            */
          err = term * ( ( 1.0 - pow( mult_term, (double) (n-i+1) ) ) /
                         (1.0-mult_term) - 1.0 );

          /* One wants an error at most of tolerance*final_result, or:
             tolerance * abs(-log10(bin_tail)-logNT).
             Now, the error that can be accepted on bin_tail is
             given by tolerance*final_result divided by the derivative
             of -log10(x) when x=bin_tail. that is:
             tolerance * abs(-log10(bin_tail)-logNT) / (1/bin_tail)
             Finally, we truncate the tail if the error is less than:
             tolerance * abs(-log10(bin_tail)-logNT) * bin_tail        */
          if( err < tolerance * fabs(-log10(bin_tail)-logNT) * bin_tail ) break;
        }
    }
  double nfavalue = -log10(bin_tail) - logNT;
  return nfavalue;
}

/*----------------------------------------------------------------------------*/
/** Compute a rectangle's NFA value.
 */
static double rect_nfa(struct rect * rec, image_double angles, double logNT)
{
  rect_iter * i;
  int pts = 0;
  int alg = 0;

  /* check parameters */
  if( rec == NULL ) error("rect_nfa: invalid rectangle.");
  if( angles == NULL ) error("rect_nfa: invalid 'angles'.");

  /* compute the total number of pixels and of aligned point2is in 'rec' */
  for(i=ri_ini(rec); !ri_end(i); ri_inc(i)) /* rectangle iterator */
    if( i->x >= 0 && i->y >= 0 &&
        i->x < (int) angles->xsize && i->y < (int) angles->ysize )
      {
        ++pts; /* total number of pixels counter */
        if( isaligned(i->x, i->y, angles, rec->theta, rec->prec) )
          ++alg; /* aligned point2is counter */
      }
  ri_del(i); /* delete iterator */
  double NFAvalue = nfa(pts,alg,rec->p,logNT); /* compute NFA value */
  return NFAvalue;
}
/*---------------------------------- Regions ---------------------------------*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/** Compute region's angle as the principal inertia axis of the region.

    The following is the region inertia matrix A:
    @f[

        A = \left(\begin{array}{cc}
                                    Ixx & Ixy \\
                                    Ixy & Iyy \\
             \end{array}\right)

    @f]
    where

      Ixx =   sum_i G(i).(y_i - cx)^2

      Iyy =   sum_i G(i).(x_i - cy)^2

      Ixy = - sum_i G(i).(x_i - cx).(y_i - cy)

    and
    - G(i) is the gradient norm at pixel i, used as pixel's weight.
    - x_i and y_i are the coordinates of pixel i.
    - cx and cy are the coordinates of the center of th region.

    lambda1 and lambda2 are the eigenvalues of matrix A,
    with lambda1 >= lambda2. They are found by solving the
    characteristic polynomial:

      det( lambda I - A) = 0

    that gives:

      lambda1 = ( Ixx + Iyy + sqrt( (Ixx-Iyy)^2 + 4.0*Ixy*Ixy) ) / 2

      lambda2 = ( Ixx + Iyy - sqrt( (Ixx-Iyy)^2 + 4.0*Ixy*Ixy) ) / 2

    To get the line segment direction we want to get the angle the
    eigenvector associated to the smallest eigenvalue. We have
    to solve for a,b in:

      a.Ixx + b.Ixy = a.lambda2

      a.Ixy + b.Iyy = b.lambda2

    We want the angle theta = atan(b/a). It can be computed with
    any of the two equations:

      theta = atan( (lambda2-Ixx) / Ixy )

    or

      theta = atan( Ixy / (lambda2-Iyy) )

    When |Ixx| > |Iyy| we use the first, otherwise the second (just to
    get better numeric precision).
 */
static double get_theta( point2i * reg, int reg_size, double x, double y,
                         image_double modgrad, double reg_angle, double prec )
{
  double lambda,theta,weight;
  double Ixx = 0.0;
  double Iyy = 0.0;
  double Ixy = 0.0;
  double temp1,temp2;
  int i;

  /* check parameters */
  if( reg == NULL ) error("get_theta: invalid region.");
  if( reg_size <= 1 ) error("get_theta: region size <= 1.");
  if( modgrad == NULL || modgrad->data == NULL )
    error("get_theta: invalid 'modgrad'.");
  if( prec < 0.0 ) error("get_theta: 'prec' must be positive.");

  /* compute inertia matrix */
  for(i=0; i<reg_size; i++)
    {
      weight = modgrad->data[ reg[i].x + reg[i].y * modgrad->xsize ];
      Ixx += ( (double) reg[i].y - y ) * ( (double) reg[i].y - y ) * weight;
      Iyy += ( (double) reg[i].x - x ) * ( (double) reg[i].x - x ) * weight;
      Ixy -= ( (double) reg[i].x - x ) * ( (double) reg[i].y - y ) * weight;
    }
  if( double_equal(Ixx,0.0) && double_equal(Iyy,0.0) && double_equal(Ixy,0.0) )//�ж�Ixx��Iyy��Ixy��0�Ƿ�ǳ��ӽ�����������Ϊdouble���ͣ�����Ҫר�ŵĺ����ж� (To judge whether Ixx, Iyy, Ixy are very close to 0, because they are of double type, special function judgment is required)
    error("get_theta: null inertia matrix.");

  /* compute smallest eigenvalue */
  lambda = 0.5 * ( Ixx + Iyy - sqrt( (Ixx-Iyy)*(Ixx-Iyy) + 4.0*Ixy*Ixy ) );

  /* compute angle */
  theta = fabs(Ixx)>fabs(Iyy) ? atan2(lambda-Ixx,Ixy) : atan2(Ixy,lambda-Iyy);
  /* The previous procedure doesn't cares about orientation,
     so it could be wrong by 180 degrees. Here is corrected if necessary. */
  temp1 = angle_diff(theta,reg_angle);
  if( temp1 > prec )//���������ù��Ծ������������������Ľ�С����ֵ��Ӧ�ĽǶȺ͸�����ĽǶȿ������180�� (This is because the angle corresponding to the smaller eigenvalues ??of the two orthogonal axes calculated by the inertia matrix may differ by 180�� from the angle of the region)
  {
	  //------------------------------------------
	  //theta += M_PI;   //origin code
	  //------------------------------------------
	  //------------------------------------------
	  //my code,���Ӹöδ��룬����theta�� (-pi,pi)֮�� (my code, add this code, limit theta between (-pi,pi))
	  //int flag = 0;
	  temp2 = angle_diff(theta+M_PI,reg_angle);
	  if(temp2 < prec)
	  {
		  theta += M_PI;
		if(theta > M_PI)
		{
		   theta -= M_2__PI;
		   //flag = 1;
		   //if(angle_diff(theta,reg_angle) > prec)
		   //{
		   //	  //flag = 2;
		   //	  theta = reg_angle;
		   // }
		}
	  }
	  else
	  {
		  theta = (temp2 <= temp1) ? (theta+M_PI) : theta;
		  while( theta <= -M_PI ) theta += M_2__PI;
          while( theta >   M_PI ) theta -= M_2__PI;
	  }
	  
	  //--------------------------------------------
  }
  return theta;
}

/*----------------------------------------------------------------------------*/
/** Computes a rectangle that covers a region of point2is.
 */
static void region2rect( point2i * reg, int reg_size,
						image_double modgrad, double reg_angle,
                         double prec, double p, struct rect * rec )
{
  double x,y,dx,dy,l,w,theta,weight,sum,l_min,l_max,w_min,w_max;
  int i;

  /* check parameters */
  if( reg == NULL ) error("region2rect: invalid region.");
  if( reg_size <= 1 ) error("region2rect: region size <= 1.");
  if( modgrad == NULL || modgrad->data == NULL )
    error("region2rect: invalid image 'modgrad'.");
  if( rec == NULL ) error("region2rect: invalid 'rec'.");

  /* center of the region:

     It is computed as the weighted sum of the coordinates
     of all the pixels in the region. The norm of the gradient
     is used as the weight of a pixel. The sum is as follows:
       cx = \sum_i G(i).x_i
       cy = \sum_i G(i).y_i
     where G(i) is the norm of the gradient of pixel i
     and x_i,y_i are its coordinates.
   */
  //������� x,y (get centroid x,y)
  x = y = sum = 0.0;
  for(i=0; i<reg_size; i++)
    {
      weight = modgrad->data[ reg[i].x + reg[i].y * modgrad->xsize ];
      x += (double) reg[i].x * weight;
      y += (double) reg[i].y * weight;
      sum += weight;
    }
  if( sum <= 0.0 ) error("region2rect: weights sum equal to zero.");
  x /= sum;
  y /= sum;

  /* theta */
  //���ù��Ծ����ø�Ϊ��ȷ�ĽǶȹ��� (Use Inertia Matrix for More Accurate Angle Estimation)
  theta = get_theta(reg,reg_size,x,y,modgrad,reg_angle,prec);
  dx = cos(theta);
  dy = sin(theta);

  /* length and width:

     'l' and 'w' are computed as the distance from the center of the
     region to pixel i, projected along the rectangle axis (dx,dy) and
     to the orthogonal axis (-dy,dx), respectively.

     The length of the rectangle goes from l_min to l_max, where l_min
     and l_max are the minimum and maximum values of l in the region.
     Analogously, the width is selected from w_min to w_max, where
     w_min and w_max are the minimum and maximum of w for the pixels
     in the region.
   */
  //��Ϊ����ķ�������Ϊ (dx,dy) (Because the direction vector of the region is (dx,dy))
  /*
  ------------------->x
  |\
  | \  
  |  \(dx,dy)
  |   
 \|/
  y
  ���˳ʱ����ת90���� (-dy,dx)
  */
  l_min = l_max = w_min = w_max = 0.0;
  for(i=0; i<reg_size; i++)//�������ڻ������߶η�������߶η���ֱ�����ͶӰ��l,w (Use the inner product of vectors to find the projection of the line segment direction and the direction perpendicular to the line segment direction to find l, w)
    {
      l =  ( (double) reg[i].x - x) * dx + ( (double) reg[i].y - y) * dy;
      w = -( (double) reg[i].x - x) * dy + ( (double) reg[i].y - y) * dx;

      if( l > l_max ) l_max = l;
      if( l < l_min ) l_min = l;
      if( w > w_max ) w_max = w;
      if( w < w_min ) w_min = w;
    }

  /* store values */
  rec->x1 = x + l_min * dx;
  rec->y1 = y + l_min * dy;
  rec->x2 = x + l_max * dx;
  rec->y2 = y + l_max * dy;
  rec->width = w_max - w_min;
  rec->x = x;
  rec->y = y;
  rec->theta = theta;
  rec->dx = dx;
  rec->dy = dy;
  rec->prec = prec;
  rec->p = p;

  /* we impose a minimal width of one pixel

     A sharp horizontal or vertical step would produce a perfectly
     horizontal or vertical region. The width computed would be
     zero. But that corresponds to a one pixels width transition in
     the image.
   */
  if( rec->width < 1.0 ) 
	  rec->width = 1.0;
}

//�������ĺͽǶ��Ѿ�������ˣ����ֻ���о��ν��ơ���region2rect���⻹���������ĺͽǶȼ��㡣(Region centroids and angles are already calculated, so only rectangular approximations are made. And region2rect also performs centroid and angle calculations.)
static void region2rect2(point2i * reg, int reg_size,double reg_center_x,double reg_center_y,
					double reg_theta,double prec, double p, struct rect * rec )
{
  double dx,dy,l,w,l_min,l_max,w_min,w_max;
  int i;
  /* check parameters */
  if( reg == NULL ) error("region2rect: invalid region.");
  if( reg_size <= 1 ) error("region2rect: region size <= 1.");
  if( rec == NULL ) error("region2rect: invalid 'rec'.");

  //�������ķ�������(dx,dy) (Get the direction vector of the region (dx,dy))
  dx = cos(reg_theta);
  dy = sin(reg_theta);
  l_min = l_max = w_min = w_max = 0.0;
  for(i=0; i<reg_size; i++)//�������ڻ������߶η�������߶η���ֱ�����ͶӰ��l,w (Use the inner product of vectors to find the projection of the line segment direction and the direction perpendicular to the line segment direction to find l, w)
    {
      l =  ( (double) reg[i].x - reg_center_x) * dx + ( (double) reg[i].y - reg_center_y) * dy;
      w = -( (double) reg[i].x - reg_center_x) * dy + ( (double) reg[i].y - reg_center_y) * dx;

      if( l > l_max ) l_max = l;
      if( l < l_min ) l_min = l;
      if( w > w_max ) w_max = w;
      if( w < w_min ) w_min = w;
    }

  /* store values */
  rec->x1 = reg_center_x + l_min * dx;
  rec->y1 = reg_center_y + l_min * dy;
  rec->x2 = reg_center_x + l_max * dx;
  rec->y2 = reg_center_y + l_max * dy;
  rec->width = w_max - w_min;
  rec->x = reg_center_x;
  rec->y = reg_center_y;
  rec->theta = reg_theta;
  rec->dx = dx;
  rec->dy = dy;
  rec->prec = prec;
  rec->p = p;

  /* we impose a minimal width of one pixel

     A sharp horizontal or vertical step would produce a perfectly
     horizontal or vertical region. The width computed would be
     zero. But that corresponds to a one pixels width transition in
     the image.
   */
  if( rec->width < 1.0 ) 
	 rec->width = 1.0;
}
/*----------------------------------------------------------------------------*/
/** Build a region of pixels that share the same angle, up to a
    tolerance 'prec', starting at point2i (x,y).
 */
static void region_grow( int x, int y, image_double angles, struct point2i * reg,
                         int * reg_size, double * reg_angle, image_char used,
                         double prec )
{
  double sumdx,sumdy;
  int xx,yy,i; 

  /* check parameters */
  if( x < 0 || y < 0 || x >= (int) angles->xsize || y >= (int) angles->ysize )
    error("region_grow: (x,y) out of the image.");
  if( angles == NULL || angles->data == NULL )
    error("region_grow: invalid image 'angles'.");
  if( reg == NULL ) error("region_grow: invalid 'reg'.");
  if( reg_size == NULL ) error("region_grow: invalid point2ier 'reg_size'.");
  if( reg_angle == NULL ) error("region_grow: invalid point2ier 'reg_angle'.");
  if( used == NULL || used->data == NULL )
    error("region_grow: invalid image 'used'.");

  /* first point2i of the region */
  *reg_size = 1;
  reg[0].x = x;
  reg[0].y = y;
  *reg_angle = angles->data[x+y*angles->xsize];  /* region's angle */
  sumdx = cos(*reg_angle);
  sumdy = sin(*reg_angle);
  used->data[x+y*used->xsize] = USED;

  /* try neighbors as new region point2is */
  for(i=0; i<*reg_size; i++)
    for(xx=reg[i].x-1; xx<=reg[i].x+1; xx++)
      for(yy=reg[i].y-1; yy<=reg[i].y+1; yy++)
        if( xx>=0 && yy>=0 && xx<(int)used->xsize && yy<(int)used->ysize &&
            used->data[xx+yy*used->xsize] != USED &&
            isaligned(xx,yy,angles,*reg_angle,prec) )
          {
            /* add point2i */
            used->data[xx+yy*used->xsize] = USED;
            reg[*reg_size].x = xx;
            reg[*reg_size].y = yy;
            ++(*reg_size);

            /* update region's angle */
            sumdx += cos( angles->data[xx+yy*angles->xsize] );
            sumdy += sin( angles->data[xx+yy*angles->xsize] );
            *reg_angle = atan2(sumdy,sumdx);
          }
}

/*----------------------------------------------------------------------------*/
/** Try some rectangles variations to improve NFA value. Only if the
    rectangle is not meaningful (i.e., log_nfa <= log_eps).
 */
static double rect_improve( struct rect * rec, image_double angles,
                            double logNT, double log_eps )
{
  struct rect r;
  double log_nfa,log_nfa_new;
  double delta = 0.5;
  double delta_2 = delta / 2.0;
  int n;

  log_nfa = rect_nfa(rec,angles,logNT);

  if( log_nfa > log_eps ) return log_nfa;

  /* try finer precisions */
  rect_copy(rec,&r);
  for(n=0; n<5; n++)
    {
      r.p /= 2.0;
      r.prec = r.p * M_PI;
      log_nfa_new = rect_nfa(&r,angles,logNT);
      if( log_nfa_new > log_nfa )
        {
          log_nfa = log_nfa_new;
          rect_copy(&r,rec);
        }
    }

  if( log_nfa > log_eps ) return log_nfa;

  /* try to reduce width */
  rect_copy(rec,&r);
  for(n=0; n<5; n++)
    {
      if( (r.width - delta) >= 0.5 )
        {
          r.width -= delta;
          log_nfa_new = rect_nfa(&r,angles,logNT);
          if( log_nfa_new > log_nfa )
            {
              rect_copy(&r,rec);
              log_nfa = log_nfa_new;
            }
        }
    }

  if( log_nfa > log_eps ) return log_nfa;

  /* try to reduce one side of the rectangle */
  rect_copy(rec,&r);
  for(n=0; n<5; n++)
    {
      if( (r.width - delta) >= 0.5 )
        {
          r.x1 += -r.dy * delta_2;
          r.y1 +=  r.dx * delta_2;
          r.x2 += -r.dy * delta_2;
          r.y2 +=  r.dx * delta_2;
          r.width -= delta;
          log_nfa_new = rect_nfa(&r,angles,logNT);
          if( log_nfa_new > log_nfa )
            {
              rect_copy(&r,rec);
              log_nfa = log_nfa_new;
            }
        }
    }

  if( log_nfa > log_eps ) return log_nfa;

  /* try to reduce the other side of the rectangle */
  rect_copy(rec,&r);
  for(n=0; n<5; n++)
    {
      if( (r.width - delta) >= 0.5 )
        {
          r.x1 -= -r.dy * delta_2;
          r.y1 -=  r.dx * delta_2;
          r.x2 -= -r.dy * delta_2;
          r.y2 -=  r.dx * delta_2;
          r.width -= delta;
          log_nfa_new = rect_nfa(&r,angles,logNT);
          if( log_nfa_new > log_nfa )
            {
              rect_copy(&r,rec);
              log_nfa = log_nfa_new;
            }
        }
    }

  if( log_nfa > log_eps ) return log_nfa;

  /* try even finer precisions */
  rect_copy(rec,&r);
  for(n=0; n<5; n++)
    {
      r.p /= 2.0;
      r.prec = r.p * M_PI;
      log_nfa_new = rect_nfa(&r,angles,logNT);
      if( log_nfa_new > log_nfa )
        {
          log_nfa = log_nfa_new;
          rect_copy(&r,rec);
        }
    }

  return log_nfa;
}

/*----------------------------------------------------------------------------*/
/** Reduce the region size, by elimination the point2is far from the
    starting point2i, until that leads to rectangle with the right
    density of region point2is or to discard the region if too small.
 */
static int reduce_region_radius( struct point2i * reg, int * reg_size,
                                 image_double modgrad, double reg_angle,
                                 double prec, double p, struct rect * rec,
                                 image_char used, image_double angles,
                                 double density_th )
{
  double density,rad1,rad2,rad,xc,yc;
  int i;

  /* check parameters */
  if( reg == NULL ) error("reduce_region_radius: invalid point2ier 'reg'.");
  if( reg_size == NULL )
    error("reduce_region_radius: invalid point2ier 'reg_size'.");
  if( prec < 0.0 ) error("reduce_region_radius: 'prec' must be positive.");
  if( rec == NULL ) error("reduce_region_radius: invalid point2ier 'rec'.");
  if( used == NULL || used->data == NULL )
    error("reduce_region_radius: invalid image 'used'.");
  if( angles == NULL || angles->data == NULL )
    error("reduce_region_radius: invalid image 'angles'.");

  /* compute region point2is density */ //���ܶ��ж��Ѿ��ں������жϹ���Ӧ�ÿ��Բ������ж��˰� (The density judgment has already been judged outside the function, so it should not be necessary to judge it.)
  density = (double) *reg_size /
                         ( dist(rec->x1,rec->y1,rec->x2,rec->y2) * rec->width );

  // if the density criterion is satisfied there is nothing to do 
  if( density >= density_th ) return TRUE;
  

  /* compute region's radius */
  xc = (double) reg[0].x;
  yc = (double) reg[0].y;
  rad1 = dist( xc, yc, rec->x1, rec->y1 );
  rad2 = dist( xc, yc, rec->x2, rec->y2 );
  rad = rad1 > rad2 ? rad1 : rad2;

  /* while the density criterion is not satisfied, remove farther pixels */
  while( density < density_th )
    {
      rad *= 0.75; /* reduce region's radius to 75% of its value */

      /* remove point2is from the region and update 'used' map */
      for(i=0; i<*reg_size; i++)
        if( dist( xc, yc, (double) reg[i].x, (double) reg[i].y ) > rad )
          {
            /* point2i not kept, mark it as NOTUSED */
            used->data[ reg[i].x + reg[i].y * used->xsize ] = NOTUSED;
            /* remove point2i from the region */
            reg[i].x = reg[*reg_size-1].x; /* if i==*reg_size-1 copy itself */
            reg[i].y = reg[*reg_size-1].y;
            --(*reg_size);
            --i; /* to avoid skipping one point2i */
          }

      /* reject if the region is too small.
         2 is the minimal region size for 'region2rect' to work. */
      if( *reg_size < 2 ) return FALSE;

      /* re-compute rectangle */
      region2rect(reg,*reg_size,modgrad,reg_angle,prec,p,rec);

      /* re-compute region point2is density */
      density = (double) *reg_size /
                         ( dist(rec->x1,rec->y1,rec->x2,rec->y2) * rec->width );
    }

  /* if this point2i is reached, the density criterion is satisfied */
  return TRUE;
}

/*----------------------------------------------------------------------------*/
/** Refine a rectangle.

    For that, an estimation of the angle tolerance is performed by the
    standard deviation of the angle at point2is near the region's
    starting point2i. Then, a new region is grown starting from the same
    point2i, but using the estimated angle tolerance. If this fails to
    produce a rectangle with the right density of region point2is,
    'reduce_region_radius' is called to try to satisfy this condition.
 */
static int refine( struct point2i * reg, int * reg_size, image_double modgrad,
                   double reg_angle, double prec, double p, struct rect * rec,
                   image_char used, image_double angles, double density_th )
{
  double angle,ang_d,mean_angle,tau,density,xc,yc,ang_c,sum,s_sum;
  int i,n;

  /* check parameters */
  if( reg == NULL ) error("refine: invalid point2ier 'reg'.");
  if( reg_size == NULL ) error("refine: invalid point2ier 'reg_size'.");
  if( prec < 0.0 ) error("refine: 'prec' must be positive.");
  if( rec == NULL ) error("refine: invalid point2ier 'rec'.");
  if( used == NULL || used->data == NULL )
    error("refine: invalid image 'used'.");
  if( angles == NULL || angles->data == NULL )
    error("refine: invalid image 'angles'.");

  /* compute region point2is density */
  density = (double) *reg_size /
                         ( dist(rec->x1,rec->y1,rec->x2,rec->y2) * rec->width );

  /* if the density criterion is satisfied there is nothing to do */
  if( density >= density_th ) return TRUE;

  /*------ First try: reduce angle tolerance ------*/

  /* compute the new mean angle and tolerance */
  xc = (double) reg[0].x;
  yc = (double) reg[0].y;
  ang_c = angles->data[ reg[0].x + reg[0].y * angles->xsize ];
  sum = s_sum = 0.0;
  n = 0;
  for(i=0; i<*reg_size; i++)
    {
      used->data[ reg[i].x + reg[i].y * used->xsize ] = NOTUSED;
      if( dist( xc, yc, (double) reg[i].x, (double) reg[i].y ) < rec->width )
        {
          angle = angles->data[ reg[i].x + reg[i].y * angles->xsize ];
          ang_d = angle_diff_signed(angle,ang_c);
          sum += ang_d;//���ϽǶȲ� (plus angle difference)
          s_sum += ang_d * ang_d;//���ϽǶȲ��ƽ�� (plus the square of the angle difference)
          ++n;
        }
    }
  mean_angle = sum / (double) n;
  //��2����׼����Ϊ�µĽǶ����̶ȣ��ʼΪ22.5��*pi/180 (Take 2 standard deviations as the new angle tolerance, initially 22.5��*pi/180)
  tau = 2.0 * sqrt( (s_sum - 2.0 * mean_angle * sum) / (double) n  +  mean_angle*mean_angle ); /* 2 * standard deviation */
  //���µĽǶ����̶����½����������� (Re-do area growing with new angle tolerance)
  /* find a new region from the same starting point2i and new angle tolerance */
  region_grow(reg[0].x,reg[0].y,angles,reg,reg_size,&reg_angle,used,tau);

  /* if the region is too small, reject */
  if( *reg_size < 2 ) return FALSE;

  /* re-compute rectangle */
  region2rect(reg,*reg_size,modgrad,reg_angle,prec,p,rec);

  /* re-compute region point2is density */
  density = (double) *reg_size /
                      ( dist(rec->x1,rec->y1,rec->x2,rec->y2) * rec->width );

  /*------ Second try: reduce region radius ------*/
  if( density < density_th )
    return reduce_region_radius( reg, reg_size, modgrad, reg_angle, prec, p,
                                 rec, used, angles, density_th );

  /* if this point2i is reached, the density criterion is satisfied */
  return TRUE;
}
//--------------------------------------------------------
//my code
bool isArcSegment(point2i * reg, int reg_size, struct rect * main_rect, image_double ll_angles,image_char used,image_char pol,
                         double prec, double p, rect * rect_up, rect * rect_down)
{
	point2i * reg_up = (point2i*)malloc(reg_size*sizeof(point2i));
	point2i * reg_down = (point2i*)malloc(reg_size*sizeof(point2i));
	int   reg_up_size,reg_down_size;
	double reg_up_theta,reg_down_theta, main_theta;
	double reg_up_sin_s,reg_up_cos_s,reg_down_sin_s,reg_down_cos_s;
	double reg_up_x,reg_up_y,reg_down_x,reg_down_y;
	//double weight,sum;
	double temp1,temp2;
	int same_pol_cnt,opp_pol_cnt;
	int i;

	same_pol_cnt = opp_pol_cnt = 0;
	reg_up_size = reg_down_size = 0;

	for ( i = 0; i < reg_size; i++)
	{
		switch(pol->data[reg[i].y*pol->xsize+reg[i].x])
		{
			case SAME_POL: same_pol_cnt++;break;//ͳ��ͬ���Ե�pixel���� (Count the number of pixels with the same polarity)
			case OPP_POL : opp_pol_cnt++; break;//ͳ�Ʒ����Ե�pixel���� (Count the number of pixels with reversed polarity)
			default:break;
		}
	 //ѡ��theta�Ƕ�Ϊ���߷��򣬹����ĵ�ֱ�߷���Ϊ dx*(x-xi)+dy*(y-yi)=0,���뷽����ͬ�ĵ���뷽�̵õ�����d,d>=0����reg_up,d<0����reg_down
	 //(Select the angle with theta as the normal direction, and the equation of the line passing through the center of mass 
	 //is dx*(x-xi)+dy*(y-yi)=0, then the point with the same direction is substituted into the equation to get the distance d, 
	 //d>=0 normalization into reg_up, d<0 into reg_down)
	  if( main_rect->dx*( reg[i].x - main_rect->x ) + main_rect->dy*( reg[i].y - main_rect->y ) >= 0)
		  reg_up[reg_up_size++] = reg[i];
	  else
		  reg_down[reg_down_size++] = reg[i];
	}
	//�����Ѿ�����ǹ����Ե���������û��Ҫ�ٽ��м��Է��� (For regions that have been marked with polarity, we do not need to perform polarity analysis)
	if( (same_pol_cnt + opp_pol_cnt) > reg_size/2)
	{
		if(same_pol_cnt > opp_pol_cnt )
		{
			main_rect->polarity = 1;
		    rect_up->polarity = 1;
	        rect_down->polarity = 1;
		}
		else
		{
			main_rect->polarity = -1;
		    rect_up->polarity = -1;
	        rect_down->polarity = -1;
		}
		return TRUE;
	}
	//��������������ͬ���ϰ벿���������� (Calculate the centroid of the upper half area with the same main direction)
	reg_up_x = reg_up_y = 0;
	//sum = 0;
	reg_up_sin_s = reg_up_cos_s = 0;
	for ( i = 0; i< reg_up_size; i++)
	{
		//weight = modgrad->data[ reg_up[i].x + reg_up[i].y * modgrad->xsize ];
		//reg_up_x += (double)weight*reg_up[i].x;
		//reg_up_y += (double)weight*reg_up[i].y;
		//sum += weight;
		reg_up_sin_s += sin(ll_angles->data[ reg_up[i].x + reg_up[i].y * ll_angles->xsize ]);
		reg_up_cos_s += cos(ll_angles->data[ reg_up[i].x + reg_up[i].y * ll_angles->xsize ]);
	}
	//reg_up_x /= sum;
	//reg_up_y /= sum;
	reg_up_theta = atan2(reg_up_sin_s,reg_up_cos_s);
	//�����������ϵ��°벿���������� (Calculate the centroid of the lower half of the area in the main direction)
	reg_down_x = reg_down_y = 0;
	//sum = 0;
	reg_down_sin_s = reg_down_cos_s = 0;
	for ( i = 0; i< reg_down_size; i++)
	{
		//weight = modgrad->data[ reg_down[i].x + reg_down[i].y * modgrad->xsize ];
		//reg_down_x += (double)weight*reg_down[i].x;
		//reg_down_y += (double)weight*reg_down[i].y;
		//sum += weight;
		reg_down_sin_s += sin(ll_angles->data[ reg_down[i].x + reg_down[i].y * ll_angles->xsize ]);
		reg_down_cos_s += cos(ll_angles->data[ reg_down[i].x + reg_down[i].y * ll_angles->xsize ]);
	}
	//reg_down_x /= sum;
	//reg_down_y /= sum;
	reg_down_theta = atan2(reg_down_sin_s,reg_down_cos_s);
	main_theta  = atan2(reg_up_sin_s+reg_down_sin_s,reg_up_cos_s+reg_down_cos_s);
	//�������������� (Estimate two regional orientations)
	//reg_up_theta = get_theta(reg_up,reg_up_size,reg_up_x,reg_up_y,modgrad,main_rect->theta,prec);
	//reg_down_theta = get_theta(reg_down,reg_down_size,reg_down_x,reg_down_y,modgrad,main_rect->theta,prec);
	//��ת��0����бȽ�theta,reg_up_theta,reg_down_theta (Rotate to 0�� to compare theta,reg_up_theta,reg_down_theta)
	temp1 = angle_diff_signed(reg_up_theta,main_theta);
	temp2 = angle_diff_signed(reg_down_theta,main_theta);
	/*if(temp1>= M_PI/2 || temp1 <= -M_PI/2)
		temp1 += 0;
	if(temp2>= M_PI/2 || temp2 <= -M_PI/2)
		temp2 += 0;*/
	//if(temp1 >= prec/10 && temp2 <= -prec/10)//˳ʱ��,��Ե���ݶȷ����뻡��ָ��Բ�ķ����෴��polarity = -1 (Clockwise, the gradient direction of the edge is opposite to the direction of the arc pointing to the center of the circle, polarity = -1)
	if(temp1 >= M_1_8_PI/10 && temp2 <= -M_1_8_PI/10)//ʵ��֤��ȡ��ֵЧ������ (Experiments show that the fixed value is better)
	{
		main_rect->polarity = -1;
		rect_up->polarity = -1;
	    rect_down->polarity = -1;
		//��Ǽ��� (mark polarity)
	    for ( i = 0; i < reg_size; i++)
	    {
			pol->data[reg[i].y*pol->xsize+reg[i].x] = OPP_POL;//-1
	    }
	}
	//else if(temp1 <= -prec/10 && temp2 >= prec/10)//��ʱ�룬��Ե���ݶȷ����뻡��ָ��Բ�ķ�����ͬ��polarity = 1 (Counterclockwise, the gradient direction of the edge is the same as the direction of the arc pointing to the center of the circle, polarity = 1)
	else if(temp1 <= -M_1_8_PI/10 && temp2 >= M_1_8_PI/10)//ʵ��֤��ȡ��ֵЧ������ (Experiments show that the fixed value is better)
	{
		main_rect->polarity = 1;
		rect_up->polarity = 1;
	    rect_down->polarity = 1;
		//��Ǽ��� (mark polarity)
	    for ( i = 0; i < reg_size; i++)
	    {
			pol->data[reg[i].y*pol->xsize+reg[i].x] = SAME_POL;//1
	    }
	}
	else
	{
		//��region_grow���Ѿ���ΪUSED�� (It has been set to USED in region_grow)
		//for ( i = 0; i< reg_size; i++)
		//	used->data[reg[i].y*used->xsize+reg[i].x] = USED;
		return FALSE;
	}
	
	//region2rect2(reg_up,reg_up_size,reg_up_x,reg_up_y,reg_up_theta,prec,p,rect_up);
	//region2rect2(reg_down,reg_down_size,reg_down_x,reg_down_y,reg_down_theta,prec,p,rect_down);

	free(reg_up);
	free(reg_down);
	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*-------------------------- Line Segment Detector ---------------------------*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/** LSD full interface.
 */
double * LineSegmentDetection( int * n_out,
                               double * img, int X, int Y,
                               double scale, double sigma_scale, double quant,
                               double ang_th, double log_eps, double density_th,
                               int n_bins,
                               int ** reg_img, int * reg_x, int * reg_y )
{
  image_double image;
  ntuple_list out = new_ntuple_list(8);
  double * return_value;
  image_double scaled_image,angles,modgrad;
  image_char used;
  image_char pol;  //���ڹ���Բ�������ر�Ǽ��ԣ�����ݶȵķ���ͻ��ķ���ָ��һ�£���ΪSAME_POLE,����ΪOPP_POLE,�ñ�ǳ�ʼ��Ϊ0 (For the polarity of the pixel marker that forms the arc, if the direction of the gradient is the same as the direction of the arc, it is SAME_POLE, otherwise it is OPP_POLE, and the marker is initially 0)
  image_int region = NULL;
  struct coorlist * list_p;
  struct coorlist * list_p_temp;
//  struct coorlist * mem_p;
  struct rect main_rect;//main rect
  struct rect rect_up,rect_down;//divide the rect into 2 rects:rect_up and rect_down
  struct point2i * reg;
  int reg_size,min_reg_size,i;
  unsigned int xsize,ysize;
  double rho,reg_angle,prec,p;
  double log_nfa = -1,logNT;
//  double log_nfa1,log_nfa2;
  int ls_count = 0;                   /* line segments are numbered 1,2,3,... */
  int seed_cnt = 0;
  int refine_cnt = 0;
  int reg_size_toosmall_cnt=0;

  /* check parameters */
  if( img == NULL || X <= 0 || Y <= 0 ) error("invalid image input.");
  if( scale <= 0.0 ) error("'scale' value must be positive.");
  if( sigma_scale <= 0.0 ) error("'sigma_scale' value must be positive.");
  if( quant < 0.0 ) error("'quant' value must be positive.");
  if( ang_th <= 0.0 || ang_th >= 180.0 )
    error("'ang_th' value must be in the range (0,180).");
  if( density_th < 0.0 || density_th > 1.0 )
    error("'density_th' value must be in the range [0,1].");
  if( n_bins <= 0 ) error("'n_bins' value must be positive.");


  /* angle tolerance */
  prec = M_PI * ang_th / 180.0;
  p = ang_th / 180.0;

  rho = quant / sin(prec); /* gradient magnitude threshold */


  /* load and scale image (if necessary) and compute angle at each pixel */
  image = new_image_double_ptr( (unsigned int) X, (unsigned int) Y, img );
  if( scale != 1.0 )
    {
	  //����scale���и�˹��������ͼ��ע��������ȡ�����������߿�ΪimgN*imgM
	  //(The image for Gaussian downsampling according to the scale, note that the width and height are rounded up, 
	  //set the height and width after sampling to imgN*imgM)
      scaled_image = gaussian_sampler( image, scale, sigma_scale );
	  //����һ���ݶȽǶ�˳ʱ����ת90����align�Ƕ�ͼangles������ݶȽǶ���(gx,gy)->(-gy,gx)��
	  //���ݶȵ�ģ��ͼmodgrad,Ȼ����n_bins����α���򷵻������ͷָ��list_p,������������
	  //(Return a graph of align angles after the gradient angle is rotated 90�� clockwise. If the gradient angle 
	  //is (gx,gy)->(-gy,gx), and the graph modgrad of the gradient's modulus, then pseudo-sort according to 
	  //n_bins and return The head pointer list_p of the linked list, which stores the coordinates)
	  angles = ll_angle( scaled_image, rho, &list_p,&modgrad, (unsigned int) n_bins );
      free_image_double(scaled_image);
    }
  else
    angles = ll_angle( image, rho, &list_p,&modgrad,(unsigned int) n_bins );
  xsize = angles->xsize;//���������ͼ���x size�����imgM (The x size of the downsampled image, the width imgM)
  ysize = angles->ysize;//���������ͼ���y size���߶�imgN (The y size of the downsampled image, the height imgN)

  /* Number of Tests - NT

     The theoretical number of tests is Np.(XY)^(5/2)
     where X and Y are number of columns and rows of the image.
     Np corresponds to the number of angle precisions considered.
     As the procedure 'rect_improve' tests 5 times to halve the
     angle precision, and 5 more times after improving other factors,
     11 different precision values are potentially tested. Thus,
     the number of tests is
       11 * (X*Y)^(5/2)
     whose logarithm value is
       log10(11) + 5/2 * (log10(X) + log10(Y)).
  */
  logNT = 5.0 * ( log10( (double) xsize ) + log10( (double) ysize ) ) / 2.0
          + log10(11.0);
  min_reg_size = (int) (-logNT/log10(p)); /* minimal number of point2is in region that can give a meaningful event��ÿ������������align point2i��С����*/
  /* initialize some structures */
  if( reg_img != NULL && reg_x != NULL && reg_y != NULL ) /* save region data */
	//(Apply for an int type memory of the same size as the image after downsampling. 
	//The function of this memory is to mark the detected line segment serial number in the 
	//corresponding image grid. This part is optional)
    region = new_image_int_ini(angles->xsize,angles->ysize,0);//�����뽵������ͼ��һ����С��int���͵��ڴ棬���ڴ�������ǽ���⵽���߶���ű굽��Ӧ��ͼ�������ò��ֿ��п���
  used = new_image_char_ini(xsize,ysize,NOTUSED);//�����뽵������ͼ��һ����С��char���͵��ڴ� (Allocate a char type memory of the same size as the downsampled image)
  pol  = new_image_char_ini(xsize,ysize,NOTDEF_POL);//���ص㴦���ݶȺͻ�ָ��ķ���ļ��Ա�� (The gradient at the pixel point and the polarity marker of the direction the arc is pointing)
  reg = (struct point2i *) calloc( (size_t) (xsize*ysize), sizeof(struct point2i) );
  if( reg == NULL ) error("not enough memory!");

  //(Record the head pointer of the head linked list, which needs to be used for memory release later)
  list_p_temp = list_p;//��¼ͷ�����ͷָ�룬������Ҫ���ø�ͷָ������ڴ��ͷ�
  /* search for line segments */
  for(; list_p_temp != NULL; list_p_temp = list_p_temp->next )
    if( used->data[ list_p_temp->x + list_p_temp->y * used->xsize ] == NOTUSED &&
        angles->data[ list_p_temp->x + list_p_temp->y * angles->xsize ] != NOTDEF )
       /* there is no risk of double comparison problems here
          because we are only interested in the exact NOTDEF value */
      {
        /* find the region of connected point2i and ~equal angle */
		//reg�ǳ���ΪimgN*imgM��һάpoint2i�����飬���㹻��Ŀռ�洢����������reg_size������洢�����ݵ���������¼���������point2i
		//(reg is a one-dimensional point2i array of length imgN*imgM, which has enough space to store the growing area, 
		//reg_size is the amount of data stored in it, and records the point2i of the area)
		//reg_angle�Ǹ�������������double�ͱ�������ĽǶ��ǻ�����
		//(reg_angle is a double variable of the main direction of the area, and the stored angle is in radians)
		  seed_cnt ++;
        region_grow( list_p_temp->x, list_p_temp->y, angles, reg, &reg_size,&reg_angle, used, prec );

        /* reject small regions */
        if( reg_size < min_reg_size ) 
		{
			reg_size_toosmall_cnt++;
			continue;
		}

        /* construct rectangular approximation for the region */
		//��������������õ�������Ӿ���Ĳ��������β�������:��㣬�յ㣬����theta����ȵ�
		//(The parameters of the approximate circumscribed matrix are obtained according to the growing area. 
		//The parameters of the rectangle include: start point, end point, direction theta, width, etc.)
        region2rect(reg,reg_size,modgrad,reg_angle,prec,p,&main_rect);
		if( FALSE == isArcSegment(reg,reg_size,&main_rect,angles,used,pol,prec,p,&rect_up,&rect_down))
			continue;
        /* Check if the rectangle exceeds the minimal density of
           region point2is. If not, try to improve the region.
           The rectangle will be rejected if the final one does
           not fulfill the minimal density condition.
           This is an addition to the original LSD algorithm published in
           "LSD: A Fast Line Segment Detector with a False Detection Control"
           by R. Grompone von Gioi, J. Jakubowicz, J.M. Morel, and G. Randall.
           The original algorithm is obtained with density_th = 0.0.
         */

        //�ᴿ��ͨ�����������������ﵽ�������ܶ���ֵ 
		//(Purify, by regrowing the region to the desired density threshold)
        if( !refine( reg, &reg_size, modgrad, reg_angle,
                     prec, p, &main_rect, used, angles, density_th ) ) continue;

		refine_cnt++;
        // compute NFA value 
        log_nfa = rect_improve(&main_rect,angles,logNT,log_eps);//ͨ�����ƾ��������Գ��Եõ�������nfaֵ (Try to get the desired nfa value by refining the rectangular area)
        if( log_nfa <= log_eps ) //������� (error control)
			continue;
        // A New Line Segment was found! 
        ++ls_count;  // increase line segment counter 

        //
        //  The gradient was computed with a 2x2 mask, its value corresponds to
        //  point2is with an offset of (0.5,0.5), that should be added to output.
        //  The coordinates origin is at the center of pixel (0,0).
        //
        main_rect.x1 += 0.5; main_rect.y1 += 0.5;
        main_rect.x2 += 0.5; main_rect.y2 += 0.5;

        // scale the result values if a subsampling was performed */
        if( scale != 1.0 )
          {
            main_rect.x1 /= scale; main_rect.y1 /= scale;
            main_rect.x2 /= scale; main_rect.y2 /= scale;
          //  main_rect.width /= scale;
          }

        /* add line segment found to output */
		add_8tuple( out, main_rect.x1, main_rect.y1, main_rect.x2, main_rect.y2,main_rect.dx,main_rect.dy,
			dist(main_rect.x1, main_rect.y1, main_rect.x2, main_rect.y2), main_rect.polarity);

		//------------------------------------------------------------------------------------------------- 
		/*
		cout<<ls_count<<'\t'<<main_rect.theta<<'\t'<<main_rect.theta*180/M_PI<<"\t polarity:"<<main_rect.polarity<<endl;//��ӡtheta
		
			fstream file1,file2;
			if(ls_count == 1)//�������
			{
				file1.open("D:\\Graduate Design\\picture\\sp\\coor.txt",ios::out | ios::trunc);
				file1.close();
				file2.open("D:\\Graduate Design\\picture\\sp\\reg.txt",ios::out | ios::trunc);
				file2.close();
			}
			
			file1.open("D:\\Graduate Design\\picture\\sp\\coor.txt",ios::app);
			file1<<main_rect.x1<<'\t'<<main_rect.y1<<'\t'<<main_rect.x2<<'\t'<<main_rect.y2<<'\t'<<(main_rect.theta*180/M_PI)<<endl;
			file1.close();
			
			if(ls_count == 1)//���ֵ�1���߶ε����� (keep the area of ??the 1st line segment)
			{
				file2.open("D:\\Graduate Design\\picture\\sp\\reg.txt",ios::app);
				for(i=0; i<reg_size; i++)
					file2<<angles->data[ reg[i].x + reg[i].y * angles->xsize ]*180/M_PI<<endl;
				file2.close();
			}
			*/
		//-------------------------------------------------------------------------------------------------------
        /* add region number to 'region' image if needed */ //����⵽���߶���ű굽��Ӧ��ͼ�������ò��ֿ��п���
															//(Mark the detected line segment number in the corresponding image grid, this part is optional)
        if( region != NULL )
          for(i=0; i<reg_size; i++)
            region->data[ reg[i].x + reg[i].y * region->xsize ] = ls_count;
      }


  /* free memory */
  free( (void *) image );   /* only the double_image structure should be freed,
                               the data point2ier was provided to this functions
                               and should not be destroyed.                 */
  free_image_double(angles);
  free_image_double(modgrad);
  free_image_char(used);
  free_image_char(pol);
  free( (void *) reg );
//  free( (void *) mem_p );
  //�ͷŷֳ�1024���Ĵ洢�ݶȴӴ�С������,mycode
  //(Release the linked list of storage gradients divided into 1024 areas from large to small, mycode)
  //---------------------------------------
  list_p_temp = list_p->next;
  while(list_p_temp != NULL)
  {
	  free(list_p);
	  list_p = list_p_temp;
	  list_p_temp = list_p->next;
  }
  free(list_p);

  //cout<<"seed cnt:"<<seed_cnt<<endl;
  //cout<<"refine cnt:"<<refine_cnt<<endl;
  //cout<<"reg_size_toosmall cnt:"<<reg_size_toosmall_cnt<<endl;
  //----------------------------------------
  /* return the result */
  if( reg_img != NULL && reg_x != NULL && reg_y != NULL )
    {
      if( region == NULL ) error("'region' should be a valid image.");
      *reg_img = region->data;
      if( region->xsize > (unsigned int) INT_MAX ||
          region->xsize > (unsigned int) INT_MAX )
        error("region image to big to fit in INT sizes.");
      *reg_x = (int) (region->xsize);
      *reg_y = (int) (region->ysize);

      /* free the 'region' structure.
         we cannot use the function 'free_image_int' because we need to keep
         the memory with the image data to be returned by this function. */
      free( (void *) region );
    }
  if( out->size > (unsigned int) INT_MAX )
    error("too many detections to fit in an INT.");
  *n_out = (int) (out->size);

  return_value = out->values;
  free( (void *) out );  /* only the 'ntuple_list' structure must be freed,
                            but the 'values' point2ier must be keep to return
                            as a result. */
  return return_value;
}

/*------------------------------------------------------------------------------------------------*/
/**
my code,Alan Lu
���� (enter)
img  : ����ͼ���һάdouble������,��СΪY*X�����������ȴ洢������ǰ��Ҫӵ���ڴ�
X    : ����ͼ���columns
Y    ������ͼ���rows

img : The one-dimensional double array of the input image, the size is Y*X, it is stored in row priority, and memory is required before passing in
X : the columns of the input image
Y : the rows of the input image

��� (output)
n_out: lsd�㷨���õ����߶ε�����n��return�ķ���ֵ��n���߶Σ�Ϊһάdouble�����飬����Ϊ8*n��ÿ8��Ϊһ�飬����x1,y1,x2,y2,dx,dy,width,polarity
reg_img: ������������һά��int�����飬��Сreg_y*reg_x,����Ӧ������λ�ñ���������ڵ��߶�(1,2,3,...n),���ֵΪ0��ʾ�������κ��߶�.
         �����ⲿ��int * region_img,��ֻ��Ҫ &region_img,�Ϳ��Եõ��������ķ��أ�����Ҫʱֱ��NULL����
reg_x  : �����������columns,����Ҫʱֱ��NULL����
reg_y  : �����������rows,����Ҫʱֱ��NULL����

n_out: The number n of line segments detected by the lsd algorithm, the return value of return is n line segments, 
	   which is a one-dimensional double array with a length of 8*n, each 8 is a group, and stores x1, y1, x2, y2 ,dx,dy,width,polarity
reg_img: The output marking area is a one-dimensional int array, the size is reg_y*reg_x, and the line segment it belongs 
	     to (1, 2, 3, ... n) is marked at the corresponding pixel position. If the value is 0, it does not belong to any 
		 line segment. If the outside is int * region_img, you only need &region_img, you can get the return of the marked 
		 region, and directly pass in NULL when not needed
reg_x : output the columns of the marked area, directly pass in NULL when not needed
reg_y : Output the rows of the marked area, directly pass in NULL when not needed
*/
double * mylsd(int * n_out, double * img, int X, int Y, int ** reg_img, int * reg_x, int * reg_y)
{
	 /* LSD parameters */
  double scale = 0.8;       /* Scale the image by Gaussian filter to 'scale'. */
  double sigma_scale = 0.6; /* Sigma for Gaussian filter is computed as
                                sigma = sigma_scale/scale.                    */
  double quant = 2.0;       /* Bound to the quantization error on the
                                gradient norm.                                */
  double ang_th = 22.5;     /* Gradient angle tolerance in degrees.           */
  double log_eps = 0.0;     /* Detection threshold: -log10(NFA) > log_eps     */
  double density_th = 0.7;  /* Minimal density of region point2is in rectangle. */
  int n_bins = 1024;        /* Number of bins in pseudo-ordering of gradient
                               modulus.                                       */ 

  return LineSegmentDetection( n_out, img, X, Y, scale, sigma_scale, quant,
                               ang_th, log_eps, density_th, n_bins,
                               reg_img, reg_x, reg_y );
}
//lines: �����lines_num���߶Σ�ÿ���߶�8��ֵ������x1,y1,x2,y2,dx,dy,width,polarity
//(lines: input lines_num line segments, each line segment has 8 values, storing x1, y1, x2, y2, dx, dy, width, polarity)
//lines_num:
//new_lines_num: �ܾ����߶κ��new_lines_num���߶Σ�����lines��ǰ�棬���̵��߶λ�ŵ�β�ʹ�
//(new_lines_num: new_lines_num line segments after rejecting short line segments, exist in front of lines, and short line segments will be placed at the tail)
//�˴��������Ʋ�������Ҫ��Ŀǰȡ8^2, 14^2
//(The length limit parameter is very important here: currently take 8^2, 14^2)
void     rejectShortLines(double * lines, int lines_num, int * new_lines_num )
{
	int    new_num = 0;
	int    shor_lines_num = 0;
	double temp;
	new_num = lines_num - shor_lines_num;
	for ( int i = 0; i< new_num; i++)
	{
		if( lines[i*8+6] < 10)//reject short lines, the length threshold is important: 8,14 �����Ҫ���� (Finally need to adjust)
		{
			for ( int j = 0; j<8; j++)
			{
				temp = lines[i*8+j];
				lines[i*8+j] = lines[(new_num-1)*8+j];
				lines[(new_num-1)*8+j] = temp;
			}
			i--; //��������Ҫ�����������߶γ��ȣ���Ҫ���� (After the exchange, it is necessary to check the length of the exchanged line segment, and it needs to be rolled back.)
			shor_lines_num++;
			new_num = lines_num - shor_lines_num;
		}
	}
	*new_lines_num = new_num;
}

/*----------------------------------------------------------------------------*/
//���룺(enter)
//start_angle,end_angle, �Ƕȷ�λ��(-pi,pi). (The angular orientation is (-pi,pi).)
//  pi    ------->x  0
//        |
//        |
//       y\/ pi/2
//polarity: ��polarityΪ1ʱ����ʾ���Ǵ�start_angle������ʱ�뷽����ת��end_angle�ĽǶ�;��polarityΪ-1ʱ����ʾ���Ǵ�start_angle����˳ʱ�뷽����ת��end_angle�ĽǶ�;
//(polarity: When the polarity is 1, it means the angle from the start_angle to the end_angle in the counterclockwise direction; 
//			 when the polarity is -1, it means the angle from the start_angle to the end_angle in the clockwise direction;)
//����ֵ�� ��ת�Ƕ�coverage (Return value: rotation angle coverage)
inline double rotateAngle(double start_angle, double end_angle, int polarity)
{
	double coverage;
	//������Ҫ��angle1��angle2ת���� 0 ~ 2pi
	//(First need to convert angle1 and angle2 to 0 ~ 2pi)
	if(start_angle < 0) start_angle += M_2__PI;//���ƽǶ���0~2pi֮�� (The limit angle is between 0~2pi)
	if(end_angle < 0 ) end_angle += M_2__PI;
	if(polarity == 1)//����Ϊ1 (polarity is 1)
	{
		coverage = start_angle - end_angle;
	}
	else //����Ϊ-1 (Polarity is -1)
	{ 
		coverage = end_angle - start_angle;
	}
	if(coverage < 0) coverage += M_2__PI;
	return coverage;
}
//���߶ΰ���͹�Ժ;�����з��� (Group segments by convexity and distance)
//lines: �����lines_num���߶Σ�ÿ���߶�8��ֵ������x1,y1,x2,y2,dx,dy,length,polarity
//(lines: input lines_num line segments, each line segment has 8 values, storing x1, y1, x2, y2, dx, dy, length, polarity)
//lines_num:
//�������groups. ÿ������һ��vector<int> (Output grouped groups. Each group is a vector<int>)
//ע�⣺�м�����region,��Ҫ�ں��������ֶ��ͷ�region 
//(Note: Remember to use up the region, you need to manually release the region outside the function)
void groupLSs(double *lines, int line_num, int * region, int imgx, int imgy, vector<vector<int>> * groups)
{
	if(line_num == 0)
	{
		groups = NULL;
		return;
	}
	unsigned char isEnd = 0;//�Ƿ񻹿��Լ�����Ѱ (Is it possible to continue searching)
	int currentLine; //��ǰ�߶� (current line segment)
	char * label = (char*)calloc(line_num, sizeof(char));
	memset(label,0,sizeof(char)*line_num); //init the label all to be zero
	int * group_up = (int*)malloc(sizeof(int)*line_num);//�����㹻�ڴ棬�洢���߶η���õ��ķ�����߶� (Apply enough memory to store the grouped line segments obtained by extending the direction of the line segment)
	int * group_down = (int*)malloc(sizeof(int)*line_num);//�洢�߶η����������߶� (Stores line segments grouped in opposite directions)
	int group_up_cnt,group_down_cnt;
	//coorlist * head,*tail;
	vector<int> group_temp;
	point2d dir_vec1,dir_vec2;
	point2i *votebin = (point2i*)calloc(line_num,sizeof(point2i));//�����㹻�ڴ棬����ͶƱ. x��¼�߶�������y��¼Ʊ�� (Apply for enough memory to vote. x records the line segment index, y records the number of votes)
	int bincnt = 0;
	int xx,yy,temp;
	double start_angle,end_angle,angle_delta;
	for ( int i = 0; i<line_num; i++)
	{
		if( label[i] == 0)//δ������� (δ�������)
		{
			group_up_cnt = group_down_cnt = 0;//ÿ��ʼѰ��һ�飬��Ҫ���� (ÿ��ʼѰ��һ�飬��Ҫ����)
			//�ȴӵ�i���߶ε�ͷ����ʼ���������з���,�������group_up����
			//(First start the search from the head of the i-th line segment, group it, and store the result in group_up)
			group_up[group_up_cnt++] = i;//��¼�߶�i,ע���߶���0~line_num-1 (Record line segment i, note that the line segment is 0~line_num-1)
			isEnd = 0;//���㣬��ʾ�����Դӵ�ǰ�߶ο�ʼ��������δ���� (Set to zero, it means that the search can still start from the current line segment, but it is not over yet)
	     	currentLine = i;
			while(isEnd == 0)
			{
				label[currentLine] = 1; //��Ǹ��߶��Ѿ������� (mark that the line segment has been grouped)
				//head = tail = NULL;
		        bincnt = 0;
				dir_vec1.x = lines[currentLine*8+4];
				dir_vec1.y = lines[currentLine*8+5];
				if ( lines[currentLine*8+7] == 1)//����Ϊ�� (positive polarity)
				{
					//��dir_vec1��ʱ����ת45�� (Rotate dir_vec1 45�� counterclockwise)
					dir_vec2.x = (dir_vec1.x + dir_vec1.y)*0.707106781186548; // sqrt(2)/2 = 0.707106781186548
				    dir_vec2.y = (-dir_vec1.x + dir_vec1.y)*0.707106781186548;
				}
				else
				{
					//��dir_vec1˳ʱ����ת45�� (Rotate dir_vec1 45�� clockwise)
					dir_vec2.x = (dir_vec1.x - dir_vec1.y)*0.707106781186548; // sqrt(2)/2 = 0.707106781186548
				    dir_vec2.y = (dir_vec1.x + dir_vec1.y)*0.707106781186548;
				}
				for ( int j = 1; j<=4; j++)
					for ( int k = 1; k<=4; k++)//��4x4���������� (Search within a 4x4 neighborhood)
					{
						xx = (int)(lines[currentLine*8+2]*0.8+j*dir_vec1.x+k*dir_vec2.x);
						yy = (int)(lines[currentLine*8+3]*0.8+j*dir_vec1.y+k*dir_vec2.y);
						if(xx < 0 || xx >= imgx || yy < 0 || yy >= imgy)//Խ�� (out of bounds)
							continue;
						temp = region[yy*imgx+xx];
						if(temp>0)//��ʾ���߶ε�֧��������1~line_num (Indicates the support area of ??the line segment, in 1~line_num)
						{
							region[yy*imgx+xx] = -temp;//ȡ������� (take negative sign)
							for (xx = 0; xx<bincnt; xx++)
							{
								if(votebin[xx].x == temp - 1)//�����ǰͶƱ����ֱ������Ӧ��bin��Ʊ���ϼ�1 (If you have voted before, add 1 directly to the votes of the corresponding bin)
								{
									votebin[xx].y++;
									break;
								}
							}
							if(xx == bincnt)//�����ǰû��ͶƱ�������Ӹ��߶Σ�����¼Ʊ��Ϊ1 (If no votes have been made before, add the line segment and record the vote as 1)
							{
								if(bincnt == line_num)
									error("group ls error1");
								votebin[bincnt].x = temp - 1;
								votebin[bincnt].y = 1;
								bincnt++; //bin��������1 (Add 1 to the total number of bins)
							}
						}
					}
			    //Ѱ��ͶƱ�����߶Σ�������Ҫ������������һ��ֵ 
				//(Find the line segment with the most votes, and the number needs to be greater than a certain value)
			    temp = 0;
				for ( int j = 0; j<bincnt; j++)
				{
					if(votebin[j].y>temp)
					{
						temp = votebin[j].y;
						xx = votebin[j].x;//����xx���� (borrow xx variable)
					}
				}
				if ( temp >= 5 && label[xx] == 0 && lines[8*xx+7] == lines[8*i+7] )//��ʵ���������ֵ (Parameter values ??to be adjusted for experimentation)
				{
					if(group_up_cnt == line_num)
					   error("group ls error2");
					yy = group_up_cnt-1;//����yy����
					start_angle = atan2(lines[8*group_up[yy]+5],lines[8*group_up[yy]+4]);
					end_angle = atan2(lines[8*xx+5],lines[8*xx+4]);
					angle_delta = rotateAngle(start_angle,end_angle,(int)lines[8*i+7]);
					if(angle_delta <= M_3_8_PI)//�������߶ε���ת�н�Ҳ��Ҫ������pi/4�� (The rotation angle of two adjacent line segments also needs to be within pi/4)
					{
						group_up[group_up_cnt++] = xx;//ѹ���߶� (Push line segment)
						currentLine = xx; //���µ�ǰ�����߶� (Update the current search segment)
					}
					else
						isEnd = 1;
				}
				else
					isEnd = 1;//�������Ѿ��Ҳ������Է�����߶��� (It's over, I can't find any line segments that can be grouped)
			}
			//�ȴӵ�i���߶ε�β����ʼ���������з���,�������group_down���档��ס����i���߶���group_up��group_down�е�0��������������
			//(First start the search from the end of the i-th line segment, perform grouping, and store the results in group_down. 
			//Remember that the ith line segment is stored at index 0 in both group_up and group_down)
			group_down[group_down_cnt++] = i; 
			isEnd = 0;//���㣬��ʾ�����Դӵ�ǰ�߶ο�ʼ��������δ���� (Set to zero, it means that the search can still start from the current line segment, but it is not over yet)
	     	currentLine = i;
			while(isEnd == 0)
			{
				label[currentLine] = 1; //��Ǹ��߶��Ѿ������� (mark that the line segment has been grouped)
				//head = tail = NULL;
		        bincnt = 0;
				dir_vec1.x = -lines[currentLine*8+4];
				dir_vec1.y = -lines[currentLine*8+5];
				if ( lines[currentLine*8+7] == 1)//������ͬ (same polarity)
				{
					//��dir_vec1˳ʱ����ת45�� (Rotate dir_vec1 45�� clockwise)
					dir_vec2.x = (dir_vec1.x - dir_vec1.y)*0.707106781186548; // sqrt(2)/2 = 0.707106781186548
				    dir_vec2.y = (dir_vec1.x + dir_vec1.y)*0.707106781186548;
				}
				else
				{
					//��dir_vec1˳ʱ����ת45�� (Rotate dir_vec1 45�� clockwise)
					dir_vec2.x = (dir_vec1.x + dir_vec1.y)*0.707106781186548; // sqrt(2)/2 = 0.707106781186548
				    dir_vec2.y = (-dir_vec1.x + dir_vec1.y)*0.707106781186548;
				}
				for ( int j = 1; j<=4; j++)
					for ( int k = 1; k<=4; k++)//��4x4���������� (Search within a 4x4 neighborhood)
					{
						xx = (int)(lines[currentLine*8+0]*0.8+j*dir_vec1.x+k*dir_vec2.x);
						yy = (int)(lines[currentLine*8+1]*0.8+j*dir_vec1.y+k*dir_vec2.y);
						if(xx < 0 || xx >= imgx || yy < 0 || yy >= imgy)//Խ�� (out of bounds)
							continue;
						temp = region[yy*imgx+xx];
						if(temp>0)//��ʾ���߶ε�֧��������1~line_num (Indicates the support area of ??the line segment, in 1~line_num)
						{
							region[yy*imgx+xx] = -temp;//ȡ������� (take negative sign)
							for (xx = 0; xx<bincnt; xx++)
							{
								if(votebin[xx].x == temp - 1)//�����ǰͶƱ����ֱ������Ӧ��bin��Ʊ���ϼ�1 (If you have voted before, add 1 directly to the votes of the corresponding bin)
								{
									votebin[xx].y++;
									break;
								}
							}
							if(xx == bincnt)//�����ǰû��ͶƱ�������Ӹ��߶Σ�����¼Ʊ��Ϊ1 (If no votes have been made before, add the line segment and record the vote as 1)
							{
								if(bincnt == line_num)
									error("group ls error3");
								votebin[bincnt].x = temp - 1;
								votebin[bincnt].y = 1;
								bincnt++; //bin��������1 (Add 1 to the total number of bins)
							}
						}
					}
			    //Ѱ��ͶƱ�����߶Σ�������Ҫ������������һ��ֵ
				//(Find the line segment with the most votes, and the number needs to be greater than a certain value)
			    temp = 0;
				for ( int j = 0; j<bincnt; j++)
				{
					if(votebin[j].y>temp)
					{
						temp = votebin[j].y;
						xx = votebin[j].x;//����xx���� (borrow xx variable)
					}
				}
				if ( temp >= 5 && label[xx] == 0 && lines[8*xx+7] == lines[8*i+7])//��ʵ���������ֵ (Parameter values ??to be adjusted for experimentation)
				{
					if(group_down_cnt == line_num)
					   error("group ls error2");
					yy = group_down_cnt-1;//����yy���� (borrow yy variable)
					start_angle = atan2(lines[8*group_down[yy]+5],lines[8*group_down[yy]+4]);
					end_angle = atan2(lines[8*xx+5],lines[8*xx+4]);
					angle_delta = rotateAngle(end_angle,start_angle,(int)lines[8*i+7]);//ע���ʱ��Ҫ����һ�£���Ϊ�Ǵ�β����ʼ���� (Note that it needs to be replaced at this time, because the search starts from the end)
					if(angle_delta < M_3_8_PI)//�������߶ε���ת�н�Ҳ��Ҫ������pi/4��,pi*3/8 = 66.5�� (The rotation angle of two adjacent line segments also needs to be within pi/4, pi*3/8 = 66.5��)
					{
						group_down[group_down_cnt++] = xx; //ѹ���߶� (Push line segment)
						currentLine = xx; //���µ�ǰ�����߶� (Update the current search segment)
					}
					else
						isEnd = 1;
				}
				else
					isEnd = 1;//�������Ѿ��Ҳ������Է�����߶��� (It's over, I can't find any line segments that can be grouped)
			}
			(*groups).push_back(group_temp); //����߶η��� (Add line segment grouping)
			temp = (*groups).size()-1;
			for (int j = group_down_cnt-1; j>= 0; j--)
			{
				(*groups)[temp].push_back(group_down[j]);
			}
			for (int j = 1; j<group_up_cnt; j++)//���ڵ�i���߶���group_up��group_down�������ˣ����Ծʹ�����1��ʼ (Since the i-th line segment is stored in both group_up and group_down, it starts from index 1)
			{
				(*groups)[temp].push_back(group_up[j]);
			}
		}
	}
	free(label);
	free(group_up);
	free(group_down);
	free(votebin);
}
//����groups��ÿ����Ŀ�� (Calculate the span of each group in groups)
//���룺(enter:)
//lines: �����lines_num���߶Σ�ÿ���߶�8��ֵ������x1,y1,x2,y2,dx,dy,length,polarity
//(lines: input lines_num line segments, each line segment has 8 values, storing x1, y1, x2, y2, dx, dy, length, polarity)
//lines_num:
//groups: ���飬ÿ�����鶼�����߶ε����� (Grouping, each grouping stores the index of the line segment)
//���: (outputs)
//coverages: ÿ����Ŀ�ȣ��������߶�ֻ��1��ʱ�����Ϊ0. coverages�ĳ��ȵ���������� = groups.size()
// (The span of each group, when there is only 1 line segment in the group, the span is 0. The length of coverages is equal to the number of groups = groups.size())
//ע�⣬coverages��ǰ����Ҫ�����ڴ棬coverages�������Ҫ�ں������ֶ��ͷ��ڴ棬���ȵ��ڷ�������
//(Note that you do not need to apply for memory before coverages are used. After the coverages are used up, you need to manually release the 
//memory outside the function. The length is equal to the number of groups.)
void calcuGroupCoverage(double * lines, int line_num, vector<vector<int>> groups, double * &coverages)
{
	int groups_num = groups.size();
	int temp;
	double start_angle,end_angle;
	coverages = (double*)malloc(sizeof(double)*groups_num);
	for ( int i = 0; i<groups_num; i++)
	{
		temp = groups[i].size()-1;
		if(groups[i].size() == 0)//��i������ֻ��1���߶Σ�����Ϊ0 (The i-th group has only 1 line segment, then the span is 0)
		{
			coverages[i] = 0;
		}
		else
		{
			start_angle = atan2(lines[8*groups[i][0]+5],lines[8*groups[i][0]+4]);
			end_angle = atan2(lines[8*groups[i][temp]+5],lines[8*groups[i][temp]+4]);
			coverages[i] = rotateAngle(start_angle,end_angle,(int)lines[8*groups[i][0]+7]);
		}
	}
}

//==============================================================================
//====================================================================================================
//================================clustering==========================================================
//���� (clustering)
//��points�е�i����initializations�е�j����ÿ��Ԫ�ص�ƽ�����ܺ�,ÿ��ά�ȶ�ΪnDims
//(Find the sum of the squared differences of each element in the i-th row in points and the j-th row in initializations, each row has dimension nDims)
inline double squaredDifference(int & nDims, double *& points, int & i, double *& initializations, int & j)
{
    double result = 0;
    for (int k = 0; k < nDims; ++k)
		result += pow(points[i*nDims+k] - initializations[j*nDims+k], 2);
    return result;
}
/**
 *���� (enter)
 *points: ����ֵƯ�Ƶĵ㼯���ܹ���nPoints���㣬ÿ������nDimsά�ȣ���һά����
 *(points: The point set to be mean shifted, there are nPoints points in total, each point has nDims dimension, which is a one-dimensional array)
 *initPoints: ��ֵƯ�Ƴ�ʼ��λ�ã���nxd�ռ����Ҿ�ֵƯ�Ƴ�ʼʱ��ʼ������λ�ã��ܹ���initLength���㣬ÿ������nDimsά��
 *(initPoints: The initialization position of the mean shift, in the nxd space to find the position where the mean shift starts to search at the beginning, 
			   there are a total of initLength points, and each point has nDims dimension)
 *sigma = 1
 *window_size: window parameter = distance_tolerance����window parameter = distance_tolerance/2 (or)
 *accuracy_tolerance: �����������1e-6 (Convergence tolerance error 1e-6)
 *iter_times: ��������50 (Iterations 50)
 *��� (outputs)
 *������λ�ã�λ�ø������ʼ������λ�ø���һ��,���ǽ�������µ�initPoints,Ҳ�������������������Ҳ�������������ʡ�ڴ�
 *Convergence position, the number of positions is the same as the number of initial search positions, 
 we update the result to initPoints, that is, it is both an input parameter and an output parameter, saving memory
 */
void meanShift( double * points, int nPoints, int nDims, double * & initPoints, int initLength, double sigma, double window_size, double accuracy_tolerance, int iter_times )
{
//	for (int i = 0; i<initLength; i++)
//		cout<<initPoints[2*i]<<'\t'<<initPoints[2*i+1]<<endl;
    int nQuerries = initLength;
    double * initializations = (double*)malloc(nQuerries * nDims * sizeof(double));
    memcpy(initializations, initPoints , nQuerries * nDims * sizeof(double));//copy

    double sigma2 = sigma*sigma;//sigmaƽ�� (sigma square)
    double radius2 = window_size *window_size;//ƽ�� (square)
    double tolerance = accuracy_tolerance;
    int iters, maxiters = iter_times;//���������� (The maximum number of iterations)
   //�������ʼ�����㼯һ����С�����ն�λ�㼯
   //Returns a final set of anchor points of the same size as the initial set of search points
    double * finals = (double*)malloc(nQuerries * nDims * sizeof(double));;//���ն�λ�㼯��ָ��
    memcpy(finals, initializations, nQuerries * nDims * sizeof(double));
	double * distances = (double*)malloc(nPoints*sizeof(double));
    //printf("meanShift: nPoints:%d \tnDims: %d \tnQuerries:%d \n",nPoints,nDims,nQuerries);//��ӡ (Print)
    for (int loop = 0; loop < nQuerries; ++loop)
    {
        iters = 0;
        while (iters < maxiters)
        {
            bool flag = false;
            double denominator = 0;//��ĸ (denominator)
            for (int i = 0; i < nPoints; ++i)//�����еĵ㼯���б������ҵ���������Բ���ڵĵ� (Traverse all point sets to find the points that fall within the search circle)
            {
                distances[i] = squaredDifference(nDims, points, i, initializations, loop);// (find the square of the distance)
                if (distances[i] <= radius2)//�ڵ�loop���������ĵ���sqrt(radius2)Ϊ�뾶��Բ���� (In the circle domain with sqrt(radius2) as the radius of the search center of the loop)
                {
                    flag = true;
                    denominator += exp(-distances[i] / sigma2);
                }
            }
            if (!flag)
                break;
            for (int j = 0; j < nDims; ++j)
				finals[loop*nDims+j] = 0;//�����ն�λ�㼯�еĵ�loop�����������ֵΪ0 (Assign 0 to the vector of the loop-th point in the final set of positioning points)
            for (int i = 0; i < nPoints; ++i)
                if (distances[i] <= radius2)
                {
                    for (int j = 0; j < nDims; ++j)//ÿ���ڵ���������һ��Ȩֵ�ۼ� (Each interior point vector is accumulated with a certain weight)
						finals[loop*nDims+j] += exp(-distances[i] / sigma2) * points[i*nDims+j];
                }
            for (int j = 0; j < nDims; ++j)//Ȩֵ��һ�� (Weight normalization)
				finals[loop*nDims+j] /= denominator;
            if (sqrt(squaredDifference(nDims, finals, loop, initializations, loop)) < tolerance)//������εĵ���������������ˣ�����Ϊ�Ѿ�������û��Ҫ�ټ������� (If the center of the two consecutive iterations is within the error, it is considered to have converged, and there is no need to continue the iteration)
                break;
            iters = iters + 1;
            for (int j = 0; j < nDims; ++j)//���µ������������� (Update iterative search center)
				initializations[loop*nDims+j] = finals[loop*nDims+j];
        }
    }
	memcpy(initPoints, finals, nQuerries * nDims * sizeof(double));
    free(distances);
    free(initializations);
	free(finals);
}

/***
 *���� (enter)
 *points,������ĵ㼯,Ϊһά����,nPoints���㣬ÿ����ά����nDims
 *(points, the point set to be clustered, is a one-dimensional array, nPoints points, each point dimension is nDims)
 *distance_threshold ��������ľ�����ֵ (distance_threshold determines the distance threshold for clustering)
 *��� outPoints
 *�����ĵ㼯 nOutPoints x nDims (Clustered point set nOutPoints x nDims)
 *�ú���Ҫǧ��ע�⣬�������ú󣬺����ڲ��������nOutPoints��double�͵������ڴ棬�����ʹ����Ϻ��м�free(outPoints).
 *(Pay attention to this function. When it is called, it will apply for nOutPoints double-type array 
 	memory inside the function. After using it outside, remember to free(outPoints).)
 */
void clusterByDistance(double * points, int nPoints, int nDims, double distance_threshold,int number_control, double * & outPoints, int * nOutPoints)
{ 
	double threshold2 = distance_threshold*distance_threshold;
    std::vector<double*> centers;
    std::vector<int> counts;
    centers.clear();
    counts.clear();
	char * labeled = (char*)malloc(sizeof(char)*nPoints);
    memset(labeled, 0, nPoints * sizeof(char));//��ʼ��bool�ͱ�ǩΪ0 (Initialize the bool type label to 0)
	if(nPoints == 1)
	{
		centers.push_back((double*)malloc(sizeof(double)*nDims));
		for (int k = 0; k < nDims; ++k)
			centers[centers.size() - 1][k] = points[k];
        counts.push_back(1);
	}
	else
	{
		for (int i = 0; i < nPoints; ++i)
		{
		    if (!labeled[i])
			{
		        labeled[i] = 1;
				centers.push_back((double*)malloc(sizeof(double)*nDims));
		        counts.push_back(1);
		        for (int k = 0; k < nDims; ++k)
				{
				   centers[centers.size() - 1][k] = points[i*nDims+k];  
				}
		        for (int j = i+1; j < nPoints; ++j)
		        {
		            if (!labeled[j])
		            {
		                double d = 0;
		                for (int k = 0; k < nDims; ++k)
				            d += pow(centers[centers.size() - 1][k] / counts[centers.size() - 1] - points[j*nDims+k], 2);
		                if (d <= threshold2)
		                {
		                    ++counts[centers.size() - 1];
		                    for (int k = 0; k < nDims; ++k)
								centers[centers.size() - 1][k] += points[j*nDims+k];
		                    labeled[j] = 1;
							//(Control the number of clusters to prevent the mean center from drifting too far. 20 for center clustering and 10 for radius clustering)
							if(counts[centers.size() - 1] >= number_control)//�����������ƣ���ֹ��ֵ����Ư��̫Զ  Բ�ľ���ʱ20  �뾶����ʱ10
								break;
		                }
		            }
		        }
		    }
		}
	}
    free(labeled);
    centers.shrink_to_fit();
    counts.shrink_to_fit();
    int m = (int) centers.size();
    outPoints = (double*)malloc(sizeof(double)*m*nDims);
	(*nOutPoints) = m;
    for (unsigned int i = 0; i < centers.size(); ++i)
    {
        for (int j = 0; j < nDims; ++j)
		{
			outPoints[i*nDims+j] = centers[i][j] / counts[i];
//			cout<<out[i*nDims+j]<<'\t';
		}
//		cout<<endl;
        free(centers[i]);
    }
    centers.resize(0);
    counts.resize(0);
//	vector<double*>().swap(centers);//�ͷŻ���vector�ڴ� (Free the reclaimed vector memory)
//	vector<int>().swap(counts);
}

//�����㷨����ֵƯ��
// (Release and recycle vecto clustering algorithm, mean shift r memory)
//�������裬һ��ѡȡ��ʼ�����㣬���Ǿ�ֵƯ�ƣ�����ȥ���ظ��㣬�Ӷ��õ���������
// (There are three steps, one is to select the initial iteration point, the other is to shift the mean value, and the third is to remove duplicate points to obtain the cluster center)
//��ú�ѡԲ�ĵľ�������(xi,yi)
// (Get the cluster center (xi,yi) of the candidate circle center)
//���룺(enter)
//points��һά������,����Ϊpoints_num x 2 (points, one-dimensional point data, the length is points_num x 2)
//distance_tolerance,���ݵ����İ뾶 (distance_tolerance, the radius of the clustering of data points)
//�����(outputs)
//��ά���ݵ�ľ������� centers��һάdouble���飬 ��СΪ centers_num x 2
// (Cluster centers of 2D data points centers is a 1D double array of size centers_num x 2)
//��ȷ����ֵΪ1�����ִ���Ϊ0. ����pointsΪ��
// (The correct return value is 1, and the error is 0. For example, points is empty)
//�м��мǣ����� centersΪ�����ڲ�������ڴ棬��������centers_num����ľ������ģ�ʹ�����һ��Ҫ�ͷţ���סfree(centers)������
// (Remember remember! ! ! The centers is the memory applied by the function, which is used to return the cluster centers of centers_num points. It must be released after use. Remember free(centers)! ! !)
int  cluster2DPoints( double * points, int points_num, double distance_tolerance, double * & centers, int * centers_num)
{
	double xmax,xmin,ymax,ymin,xdelta,ydelta;
	int nbins_x,nbins_y;
	int x,y;
	int i;
	unsigned int addr,addr2;
	xmax = ymax = 0;
	xmin = ymin = DBL_MAX;
	for( i = 0; i< points_num; i++ )
	{
		addr = 2*i;
		if( points[addr] > xmax)
			xmax = points[addr];
		if( points[addr] < xmin)
			xmin = points[addr];
		if( points[addr+1] > ymax)
			ymax = points[addr+1];
		if( points[addr+1] < ymin)
			ymin = points[addr+1];
	}
	xmax += xmax*0.02;//����xdelta��ydeltaΪ0 (Avoid xdelta, ydelta being 0)
	xmin -= xmin*0.02;
	ymax += ymax*0.02;
	ymin -= ymin*0.02;
	xdelta = (xmax-xmin);
	ydelta = (ymax-ymin);//�����⣬������������һ���󣬴˴�Ϊ0 (There is a problem, assuming all data are the same size, here is 0)
	nbins_x = (int)ceil(xdelta/distance_tolerance);
	nbins_y = (int)ceil(ydelta/distance_tolerance);
	if(nbins_x <= 0 )
	{
		nbins_x = 1;//���ٱ���1��bin (Keep at least 1 bin)
		//error("generateCircleCandidates,nbins_x,nbins_y error");
	}
	if(nbins_y <= 0)
	{
		nbins_y = 1;
	}
	point2d1i * center_bins;
	center_bins = (point2d1i *)calloc(nbins_y*nbins_x, sizeof(point2d1i));//(x,y,z),x������sum(xi),y������sum(yi),z���������ڸ���������� ((x, y, z), x is used to record sum(xi), y is used to record sum(yi), and z is used to record the number falling in the grid)
	memset(center_bins,0,sizeof(point2d1i)*nbins_y*nbins_x);
	if(center_bins == NULL)
		error("cluster2DPoints, not enough memory");
//	cout<<"2Dԭʼ����:"<<points_num<<endl; (cout<<"2D raw data:"<<points_num<<endl;)
	for ( i = 0; i< points_num; i++ )//��Բ�ļ�¼���������棬ͬʱ������Ӧ�������������++ (Record the center of the circle in the grid, and the number that falls in the corresponding grid at the same time++)
	{
		addr = 2*i;

//		cout<<points[addr]<<'\t'<<points[addr+1]<<endl;

		x = (int)((points[addr]   - xmin)/xdelta*nbins_x+0.5);//�������� (rounding)
		y = (int)((points[addr+1] - ymin)/ydelta*nbins_y+0.5);
		if( x >= nbins_x)
			x = nbins_x-1;
		if( y >= nbins_y)
			y = nbins_y-1;
		addr2 = y*nbins_x+x;
		center_bins[addr2].x += points[addr];
		center_bins[addr2].y += points[addr+1];
		center_bins[addr2].z ++;
	}
	int initCentersLength = 0;
	for ( y = 0; y<nbins_y; y++)//��vote���0�ĸ��������pointȡ��ֵ��������˳����д��center_bins���棬���ڴ����� (Take the average of the points in the non-zero grids after the vote, and rewrite them into center_bins in order, without memory consumption)
		for ( x = 0; x<nbins_x; x++)
		{
			addr = y*nbins_x+x;
			if(center_bins[addr].z > 0)
			{
				center_bins[initCentersLength].x = center_bins[addr].x/center_bins[addr].z;
				center_bins[initCentersLength].y = center_bins[addr].y/center_bins[addr].z;
				initCentersLength++;
			}
		}
	if(initCentersLength == 0)
	{
		(*centers_num) = 0;
		centers = NULL;
		//cout<<"cluster2DPoints,points number:"<<points_num<<endl;
		//cout<<"cluster2DPoints,initCentersLength equals 0"<<endl;
		return 0;
		//error("generateCircleCandidates,initCentersLength equals 0");
	}
	double * initCenters; //initCentersLength x 2
	initCenters = (double*)malloc(sizeof(double)*initCentersLength*2); 
	//����¼����������ķ������Բ�ľ�ֵ��¼�������������Ϊ��ʼ����о�ֵƯ�� 
	// (Record the mean value of the center of the circle after the partition recorded in the linked list into the array, 
	// which is convenient for the mean shift as the initial point)
	for ( i = 0; i<initCentersLength; i++ )// initCenters ��С�� initCentersLength*2 (initCenters size is initCentersLength*2)
	{
		int addr = 2*i;
		initCenters[addr]   = center_bins[i].x;
		initCenters[addr+1] = center_bins[i].y;
	}
	free((void*)center_bins);//�Ͻ��ͷŸ��ڴ� (Free this memory now)

//	cout<<"2D��ֵƯ��ǰ��ʼ�����㣺"<<endl; (cout<<"Initial iteration point before 2D mean shift:"<<endl;)
//	for (int  i = 0; i<initCentersLength; i++)
//		cout<<initCenters[2*i]<<'\t'<<initCenters[2*i+1]<<endl;
	
	//��ֵƯ�ƵĽ������µ�initCenters����
	// (The result of the mean shift will be updated to initCenters)
	meanShift(points,points_num,2,initCenters,initCentersLength,1,distance_tolerance,1e-6,50);//����20�� (Iterate 20 times)

//	cout<<"2D��ֵƯ�ƺ�ľ�������:"<<endl; (cout<<"Cluster center after 2D mean shift:"<<endl;)
//	for (int  i = 0; i<initCentersLength; i++)
//		cout<<initCenters[2*i]<<'\t'<<initCenters[2*i+1]<<endl;

	//���� (clustering)
	//ǧ��Ҫע��centers_num��int��ָ�룬++--ʱҪ(*centers_num).
	// (Be sure to note that centers_num is an int pointer, and ++-- requires (*centers_num).)
	clusterByDistance(initCenters,initCentersLength,2,distance_tolerance/2,40,centers, centers_num);//�˴����Ʋ���Ҫ�ģ�Ҫ���� (Here the control parameters need to be changed and adjusted)

//	cout<<"2D������࣬ȥ���ظ����ĵ㼯:"<<endl; (cout<<"2D distance clustering, point set after removing duplicate points:"<<endl;)
//	for (int  i = 0; i<(*centers_num); i++)
//		cout<<centers[2*i]<<'\t'<<centers[2*i+1]<<endl;

	if((*centers_num) <= 0)//���� (optional)
	{
		return 0;  //����Ϊʲô���������ĵ���Χȷû��������ĵ� (I don't know why, there is no point closest to it around the cluster center)
		//system("pause");
		//error("cluster2DPoints,(*centers_num)<=0");
	}
	free(initCenters);
	//cout<<"2D���������:"<<(*centers_num)<<endl;
	return 1;
}

//�����㷨����ֵƯ��
// (Clustering Algorithms, Mean Shift)
//�������裬һ��ѡȡ��ʼ�����㣬���Ǿ�ֵƯ�ƣ�����ȥ���ظ��㣬�Ӷ��õ���������
// (There are three steps, one is to select the initial iteration point, the other is to shift the mean value,
// and the third is to remove duplicate points to obtain the cluster center)
//��ú�ѡԲ�ĵľ�������(xi,yi)
// (Get the cluster center (xi,yi) of the candidate circle center)
//���룺(enter)
//datas��һά������,����Ϊdatas_num x 1 (datas, one-dimensional point data, the length is datas_num x 1)
//distance_tolerance,���ݵ����İ뾶 (distance_tolerance, the radius of the clustering of data points)
//�����(outputs)
//һά���ݵ�ľ������� centers��һάdouble���飬 ��СΪ centers_num x 1
// (Cluster centers of 1D data points centers is a 1D double array of size centers_num x 1)
//��ȷ����ֵΪ1�����ִ���Ϊ0. ����pointsΪ�� (The correct return value is 1, and the error is 0. For example, points is empty)
//�м��мǣ����� centersΪ�����ڲ�������ڴ棬��������centers_num����ľ������ģ�ʹ�����һ��Ҫ�ͷţ���סfree(centers)������
// (Remember remember! ! ! The centers is the memory applied by the function, which is used to return the cluster centers of centers_num points. 
// It must be released after use. Remember free(centers)! ! !)
int  cluster1DDatas( double * datas, int datas_num, double distance_tolerance, double * & centers, int * centers_num)
{
	double rmin,rmax,rdelta;
	int r;
	int i;
	rmin = DBL_MAX;
	rmax = 0;
	for( i  = 0; i < datas_num; i++)//���������r���ϸ��Ƶ����� (Copy the r collection in the linked list to the array)
	{
		if(datas[i] < rmin)//����һ�α����У���¼�����Сֵ (In this traversal, record the maximum and minimum values)
			rmin = datas[i];
		if(datas[i] > rmax)
			rmax = datas[i];
	}
	int nbins_r = 0;
	point1d1i * center_bins;
	rmax += rmin*0.02;//����rmax-rmin = 0 (Avoid rmax-rmin=0)
	rmin -= rmin*0.02;
	rdelta = rmax - rmin;
	nbins_r = (int)ceil((rdelta)/distance_tolerance);
	if(nbins_r <= 0)//������һ��bin (at least one bin)
		nbins_r = 1;
	center_bins = (point1d1i *)malloc(sizeof(point1d1i)*nbins_r);
	memset(center_bins,0,sizeof(point1d1i)*nbins_r);//��ʼ��Ϊ0 (initialized to 0)
//	cout<<"1Dԭʼ����:"<<datas_num<<endl; (cout<<"1D raw data:"<<datas_num<<endl;)
	for( i = 0; i<datas_num; i++)//�Է���vote (vote on partition)
	{
//		cout<<datas[i]<<endl;
		r = int((datas[i]-rmin)/rdelta*nbins_r+0.5);
		if(r>=nbins_r)
			r = nbins_r-1;
		center_bins[r].data += datas[i];
		center_bins[r].cnt  ++;			
	}
	int init_r_length = 0;
	for( i = 0; i<nbins_r; i++)
	{
		// (Count non-zero partitions, and take the mean value of each bin, rewrite them to center_bins in order, without memory consumption)
		if(center_bins[i].cnt > 0)//ͳ�Ʒ�0����,���Ҷ�ÿһ��binȡ��ֵ������˳����д��center_bins���棬���ڴ�����
		{
			center_bins[init_r_length].data = center_bins[i].data/center_bins[i].cnt;
			init_r_length++;
		}
	}
	if(init_r_length == 0)
	{
		(*centers_num) = 0;
		centers = NULL;
		//cout<<"cluster1DDatas,points number:"<<datas_num<<endl;
		//cout<<"cluster2DDatas,init_r_length equals 0"<<endl;
		return 0;
		//error("generateCircleCandidates,initCentersLength equals 0");
	}
	double * initCenters; //init_r_length x 1
	initCenters = (double*)malloc(sizeof(double)*init_r_length); 
	//����¼����������ķ������Բ�ľ�ֵ��¼�������������Ϊ��ʼ����о�ֵƯ��
	// Record the mean value of the center of the circle after the partition recorded in the linked list into the array, 
	// which is convenient for the mean shift as the initial point
	for ( i = 0; i<init_r_length; i++ )// initCenters ��С�� init_r_length x 1 (initCenters size is init_r_length x 1)
	{
		initCenters[i] = center_bins[i].data;
	}
	free(center_bins);//�Ͻ��ͷŸ��ڴ� (Free this memory now)

//	cout<<"1D��ֵƯ��ǰ��ʼ�����㣺"<<endl; (cout<<"Initial iteration point before 1D mean shift:"<<endl;)
//	for (int  i = 0; i<init_r_length; i++)
//		cout<<initCenters[i]<<'\t';
//	cout<<endl;

	//���ˣ��õ��˾�ֵƯ�Ƴ�ʼ��initCenters��Ϊһάdouble���飬������init_r_length
	// So far, the initial initCenters of the mean shift has been obtained, which is a one-dimensional double array with a length of init_r_length
	meanShift(datas, datas_num, 1, initCenters, init_r_length, 1, distance_tolerance, 1e-6, 20);//����20�� (Iterate 20 times)

//	cout<<"1D��ֵƯ�ƺ�ľ�������:"<<endl; (cout<<"Cluster center after 1D mean shift:"<<endl;)
//	for (int  i = 0; i<init_r_length; i++)
//		cout<<initCenters[i]<<'\t';
//	cout<<endl;

	//���� (clustering)
	//ǧ��Ҫע��centers_num��int��ָ�룬++--ʱҪ(*centers_num).
	// (Be sure to note that centers_num is an int pointer, and ++-- requires (*centers_num).)
	clusterByDistance(initCenters, init_r_length, 1, distance_tolerance/2, 40, centers, centers_num);//���Ʋ���40�����40����ϳ�1���� (Control parameter 40, up to 40 points can be combined into 1 point)
	
//	cout<<"1D������࣬ȥ���ظ����ĵ㼯:"<<endl; (cout<<"1D distance clustering, point set after removing duplicate points:"<<endl;)
//	for (int  i = 0; i<(*centers_num); i++)
//		cout<<centers[i]<<'\t';
//	cout<<endl;

	if((*centers_num) <= 0)//����
	{
		return 0;  //����Ϊʲô���������ĵ���Χȷû��������ĵ� (I don't know why, there is no point closest to it around the cluster center)
		//system("pause");
		//error("cluster1DDatas,(*centers_num)<=0");
	}
    free(initCenters);
//	cout<<"1D���������::"<<(*centers_num)<<endl; (cout<<"Number after 1D clustering::"<<(*centers_num)<<endl;)
	return 1;
}

//================================Generate Ellipse Candidates=========================================
//ƥ����ԣ���Ե�������������Բ����
// (match group pair, index parameter of group pair, ellipse parameter)
typedef struct PairGroup_s
{
	point2i pairGroupInd;
	point2d center;  //(x0,y0)
	point2d axis;    //(a,b)
	double  phi;     //angle of orientation  
}PairGroup;

//ƥ����Խڵ� (Match group pair nodes)
typedef struct PairGroupNode_s
{
	point2i pairGroupInd;
	point2d center;  //(x0,y0)
	point2d axis;    //(a,b)
	double  phi;     //angle of orientation  
	PairGroupNode_s* next;
}PairGroupNode;

typedef struct  PairGroupList_s
{
	int length;
	PairGroup * pairGroup;
}PairGroupList;

typedef struct Point2dNode_s
{
	point2d point;
	Point2dNode_s * next;
}Point2dNode;

typedef struct Point3dNode_s
{
	point3d point;
	Point3dNode_s * next;
}Point3dNode;

typedef struct Point5dNode_s
{
	point2d center;
	point2d axis;
	double  phi;
	Point5dNode_s * next;
}Point5dNode;

typedef struct Point1dNode_s
{
	double data;
	Point1dNode_s * next;
}Point1dNode;

PairGroupList * pairGroupListInit( int length)
{
	if(length <= 0)
		error("paired groups length less equal than 0");
	PairGroupList * pairGroupList = (PairGroupList*)malloc(sizeof(PairGroupList));
	pairGroupList->length = length;
	pairGroupList->pairGroup = (PairGroup*)malloc(sizeof(PairGroup)*length);
	if(pairGroupList->pairGroup == NULL)
		error("pairGroupListInit,not enough memory");
	return pairGroupList;
}

void freePairGroupList( PairGroupList * list)
{
	if(list == NULL || list->pairGroup == NULL)
		error("freePairGroupList,invalidate free");
	free(list->pairGroup);
    list->pairGroup = NULL;
	free(list);
	list = NULL;
}

//�����ݶȣ�����ģ�ͽǶȣ�ͬʱģֵ̫С�����ص�ֱ�����Ƶ�����ֵΪNOTDEF
// (Calculate the gradient, return the modulus and angle, and directly suppress the pixels whose modulus value is too small, and assign the value to NOTDEF)
//mod��anglesΪ�˴�ֵ���Ƕ���ָ��
// (mod and angles are secondary pointers in order to pass values)
void calculateGradient( double * img_in, unsigned int imgx, unsigned int imgy,image_double * mod, image_double * angles)
{
	if(img_in == NULL || imgx == 0 || imgy == 0)
		error("calculateGradient error!");
	(*mod) = new_image_double(imgx,imgy);
	(*angles) = new_image_double(imgx,imgy);
	double threshold = 2/sin(22.5/180*M_PI);
	unsigned int x,y,adr;
	double com1,com2;
	double gx,gy;
	double norm,norm_square;
	double sum = 0;

	//double max_grad = 0.0;
	//�߽��ʼΪNOTDEF (Boundary is initially NOTDEF)
	for ( x = 0; x<imgx; x++) 
	{
		//(*angles)->data[x]=NOTDEF;
		(*angles)->data[(imgy-1)*imgx+x]=NOTDEF;
		//(*mod)->data[x]=NOTDEF;
		(*mod)->data[(imgy-1)*imgx+x]=NOTDEF;
	}
	for ( y = 0; y<imgy; y++) 
	{
		//(*angles)->data[y*imgx] = NOTDEF;
		(*angles)->data[y*imgx+imgx-1] = NOTDEF;
		//(*mod)->data[y*imgx] = NOTDEF;
		(*mod)->data[y*imgx+imgx-1] = NOTDEF;
	}
	 /* compute gradient on the remaining pixels */
	for(x=0;x<imgx-1;x++)
		for(y=0;y<imgy-1;y++)
		{
			adr = y*imgx+x;
		  /*
		     Norm 2 computation using 2x2 pixel window:
		       A B
		       C D
		     and
		       com1 = D-A,  com2 = B-C.
		     Then
		       gx = B+D - (A+C)   horizontal difference
		       gy = C+D - (A+B)   vertical difference
		     com1 and com2 are just to avoid 2 additions.
		   */
		  com1 = img_in[adr+imgx+1] - img_in[adr];
		  com2 = img_in[adr+1]   - img_in[adr+imgx];

		  gx = com1+com2; /* gradient x component */
		  gy = com1-com2; /* gradient y component */
		  norm_square = gx*gx+gy*gy;

		  norm = sqrt( norm_square / 4.0 ); /* gradient norm */

		  (*mod)->data[adr] = norm; /* store gradient norm */

		  if( norm <= threshold ) /* norm too small, gradient no defined */
		  {
		    (*angles)->data[adr] = NOTDEF; /* gradient angle not defined */
			(*mod)->data[adr] = NOTDEF;
		  }
		  else
		    {
		      /* gradient angle computation */
		      (*angles)->data[adr] = atan2(gx,-gy);
		    }
		}
}

void calculateGradient2( double * img_in, unsigned int imgx, unsigned int imgy, image_double * angles)
{
	if(img_in == NULL || imgx == 0 || imgy == 0)
		error("calculateGradient error!");
	image_double mod = new_image_double(imgx,imgy);
	(*angles) = new_image_double(imgx,imgy);
	unsigned int x,y,adr;
	double com1,com2;
	double gx,gy;
	double norm,norm_square;
	double threshold;
	double sum = 0;
	double value;  
	//double max_grad = 0.0;
	//�߽��ʼΪNOTDEF (Boundary is initially NOTDEF)
	for ( x = 0; x<imgx; x++) 
	{
		(*angles)->data[x]=NOTDEF;
		(*angles)->data[(imgy-1)*imgx+x]=NOTDEF;
		(mod)->data[x]=NOTDEF;
		(mod)->data[(imgy-1)*imgx+x]=NOTDEF;
	}
	for ( y = 0; y<imgy; y++) 
	{
		(*angles)->data[y*imgx] = NOTDEF;
		(*angles)->data[y*imgx+imgx-1] = NOTDEF;
		(mod)->data[y*imgx] = NOTDEF;
		(mod)->data[y*imgx+imgx-1] = NOTDEF;
	}
	 /* compute gradient on the remaining pixels */
	for(x=1;x<imgx-1;x++)
		for(y=1;y<imgy-1;y++)
		{
			adr = y*imgx+x;
		  /*
		     Norm 2 computation using 2x2 pixel window:
		       A B C
		       D E F
			   G H I
		     and
		       com1 = C-G,  com2 = I-A.
		     Then
		       gx = C+2F+I - (A+2D+G)=com1+com2+2(F-D)   horizontal difference
		       gy = G+2H+I - (A+2B+C)=-com1+com2+2(H-B)   vertical difference
		     com1 and com2 are just to avoid 2 additions.
		   */
		  com1 = img_in[adr-imgx+1] - img_in[adr+imgx-1];
		  com2 = img_in[adr+imgx+1] - img_in[adr-imgx-1];

		  gx = (com1+com2+2*(img_in[adr+1] - img_in[adr-1]))/(8.0*255); /* gradient x component */
		  gy = (-com1+com2+2*(img_in[adr+imgx] - img_in[adr-imgx]))/(8.0*255); /* gradient y component */
		  norm_square = gx*gx+gy*gy;
		  sum+=norm_square;

		  norm = sqrt( norm_square); /* gradient norm */

		  (mod)->data[adr] = norm; /* store gradient norm */
		   /* gradient angle computation */
	     (*angles)->data[adr] = atan2(gy,gx);
		}
	threshold = 2*sqrt(sum/(imgx*imgy));//�Զ���ֵ (automatic threshold)
	//non maximum suppression
	for(x=1;x<imgx-1;x++)
		for(y=1;y<imgy-1;y++)
		{
			adr = y*imgx+x;
			value = (*angles)->data[adr];
			if((mod)->data[adr] < threshold )
			{
				(*angles)->data[adr] = NOTDEF;
				continue;
			}
			if( (value > -M_1_8_PI && value<=M_1_8_PI) || (value <= -M_7_8_PI ) || (value > M_7_8_PI))
			{
				if((mod)->data[adr] <= (mod)->data[adr+1] || (mod)->data[adr] <= (mod)->data[adr-1])
					(*angles)->data[adr] = NOTDEF;
			}
			else if( (value> M_1_8_PI && value<= M_3_8_PI) || (value> -M_7_8_PI && value<= -M_5_8_PI) )
			{
				if((mod)->data[adr] <= (mod)->data[adr-imgx-1] || (mod)->data[adr] <= (mod)->data[adr+imgx+1])
					(*angles)->data[adr] = NOTDEF;
			}
			else if((value> M_3_8_PI && value<= M_5_8_PI) || (value> -M_5_8_PI && value<= -M_3_8_PI))
			{
				if((mod)->data[adr] <= (mod)->data[adr-imgx] || (mod)->data[adr] <= (mod)->data[adr+imgx])
					(*angles)->data[adr] = NOTDEF;
			}
			else 
			{
				if((mod)->data[adr] <= (mod)->data[adr-imgx+1] || (mod)->data[adr] <= (mod)->data[adr+imgx-1])
					(*angles)->data[adr] = NOTDEF;
			}
		}
    //Ҳ��ǵ�modͼ���� (Also marked on the mod map)
	//for(x=1;x<imgx-1;x++)
	//	for(y=1;y<imgy-1;y++)
	//	{
	//		if((*angles)->data[y*imgx+x] == NOTDEF)
	//			(mod)->data[y*imgx+x] = NOTDEF;
	//	}
		free_image_double(mod);
}

//=============================================================================
//��Ҫ��������ͷ�ļ� (Need to include the following header files)
//#include <opencv2\opencv.hpp>
//using namespace cv;
void cvCanny3(	const void* srcarr, void* dstarr,
				void* dxarr, void* dyarr,
                int aperture_size )
{
    //cv::Ptr<CvMat> dx, dy;
    cv::AutoBuffer<char> buffer;
    std::vector<uchar*> stack;
    uchar **stack_top = 0, **stack_bottom = 0;

    CvMat srcstub, *src = cvGetMat( srcarr, &srcstub );
    CvMat dststub, *dst = cvGetMat( dstarr, &dststub );

	CvMat dxstub, *dx = cvGetMat( dxarr, &dxstub );
	CvMat dystub, *dy = cvGetMat( dyarr, &dystub );


    CvSize size;
    int flags = aperture_size;
    int low, high;
    int* mag_buf[3];
    uchar* map;
    ptrdiff_t mapstep;
    int maxsize;
    int i, j;
    CvMat mag_row;

    if( CV_MAT_TYPE( src->type ) != CV_8UC1 ||
        CV_MAT_TYPE( dst->type ) != CV_8UC1 ||
		CV_MAT_TYPE( dx->type  ) != CV_16SC1 ||
		CV_MAT_TYPE( dy->type  ) != CV_16SC1 )
        CV_Error( CV_StsUnsupportedFormat, "" );

    if( !CV_ARE_SIZES_EQ( src, dst ))
        CV_Error( CV_StsUnmatchedSizes, "" );
	
    aperture_size &= INT_MAX;
    if( (aperture_size & 1) == 0 || aperture_size < 3 || aperture_size > 7 )
        CV_Error( CV_StsBadFlag, "" );


	size.width = src->cols;
    size.height = src->rows;

	//aperture_size = -1; //SCHARR
    cvSobel( src, dx, 1, 0, aperture_size );
    cvSobel( src, dy, 0, 1, aperture_size );

	Mat1f magGrad(size.height, size.width, 0.f);
	float maxGrad(0);
	float val(0);
	for(i=0; i<size.height; ++i)
	{
		float* _pmag = magGrad.ptr<float>(i);
		const short* _dx = (short*)(dx->data.ptr + dx->step*i);
        const short* _dy = (short*)(dy->data.ptr + dy->step*i);
		for(j=0; j<size.width; ++j)
		{
			val = float(abs(_dx[j]) + abs(_dy[j]));
			_pmag[j] = val;
			maxGrad = (val > maxGrad) ? val : maxGrad;
		}
	}
	
	//% Normalize for threshold selection
	//normalize(magGrad, magGrad, 0.0, 1.0, NORM_MINMAX);

	//% Determine Hysteresis Thresholds
	
	//set magic numbers
	const int NUM_BINS = 64;	
	const double percent_of_pixels_not_edges = 0.9;
	const double threshold_ratio = 0.3;

	//compute histogram
	int bin_size = cvFloor(maxGrad / float(NUM_BINS) + 0.5f) + 1;
	if (bin_size < 1) bin_size = 1;
	int bins[NUM_BINS] = { 0 }; 
	for (i=0; i<size.height; ++i) 
	{
		float *_pmag = magGrad.ptr<float>(i);
		for(j=0; j<size.width; ++j)
		{
			int hgf = int(_pmag[j]);
			bins[int(_pmag[j]) / bin_size]++;
		}
	}	

	
	

	//% Select the thresholds
	float total(0.f);	
	float target = float(size.height * size.width * percent_of_pixels_not_edges);
	int low_thresh, high_thresh(0);
	
	while(total < target)
	{
		total+= bins[high_thresh];
		high_thresh++;
	}
	high_thresh *= bin_size;
	low_thresh = cvFloor(threshold_ratio * float(high_thresh));
	
    if( flags & CV_CANNY_L2_GRADIENT )
    {
        Cv32suf ul, uh;
        ul.f = (float)low_thresh;
        uh.f = (float)high_thresh;

        low = ul.i;
        high = uh.i;
    }
    else
    {
        low = cvFloor( low_thresh );
        high = cvFloor( high_thresh );
    }

    
	buffer.allocate( (size.width+2)*(size.height+2) + (size.width+2)*3*sizeof(int) );
    mag_buf[0] = (int*)(char*)buffer;
    mag_buf[1] = mag_buf[0] + size.width + 2;
    mag_buf[2] = mag_buf[1] + size.width + 2;
    map = (uchar*)(mag_buf[2] + size.width + 2);
    mapstep = size.width + 2;

    maxsize = MAX( 1 << 10, size.width*size.height/10 );
    stack.resize( maxsize );
    stack_top = stack_bottom = &stack[0];

    memset( mag_buf[0], 0, (size.width+2)*sizeof(int) );
    memset( map, 1, mapstep );
    memset( map + mapstep*(size.height + 1), 1, mapstep );

    /* sector numbers
       (Top-Left Origin)

        1   2   3
         *  *  *
          * * *
        0*******0
          * * *
         *  *  *
        3   2   1
    */

    #define CANNY_PUSH(d)    *(d) = (uchar)2, *stack_top++ = (d)
    #define CANNY_POP(d)     (d) = *--stack_top

    mag_row = cvMat( 1, size.width, CV_32F );

    // calculate magnitude and angle of gradient, perform non-maxima supression.
    // fill the map with one of the following values:
    //   0 - the pixel might belong to an edge
    //   1 - the pixel can not belong to an edge
    //   2 - the pixel does belong to an edge
    for( i = 0; i <= size.height; i++ )
    {
        int* _mag = mag_buf[(i > 0) + 1] + 1;
        float* _magf = (float*)_mag;
        const short* _dx = (short*)(dx->data.ptr + dx->step*i);
        const short* _dy = (short*)(dy->data.ptr + dy->step*i);
        uchar* _map;
        int x, y;
        ptrdiff_t magstep1, magstep2;
        int prev_flag = 0;

        if( i < size.height )
        {
            _mag[-1] = _mag[size.width] = 0;

            if( !(flags & CV_CANNY_L2_GRADIENT) )
                for( j = 0; j < size.width; j++ )
                    _mag[j] = abs(_dx[j]) + abs(_dy[j]);

            else
            {
                for( j = 0; j < size.width; j++ )
                {
                    x = _dx[j]; y = _dy[j];
                    _magf[j] = (float)std::sqrt((double)x*x + (double)y*y);
                }
            }
        }
        else
            memset( _mag-1, 0, (size.width + 2)*sizeof(int) );

        // at the very beginning we do not have a complete ring
        // buffer of 3 magnitude rows for non-maxima suppression
        if( i == 0 )
            continue;

        _map = map + mapstep*i + 1;
        _map[-1] = _map[size.width] = 1;

        _mag = mag_buf[1] + 1; // take the central row
        _dx = (short*)(dx->data.ptr + dx->step*(i-1));
        _dy = (short*)(dy->data.ptr + dy->step*(i-1));

        magstep1 = mag_buf[2] - mag_buf[1];
        magstep2 = mag_buf[0] - mag_buf[1];

        if( (stack_top - stack_bottom) + size.width > maxsize )
        {
            int sz = (int)(stack_top - stack_bottom);
            maxsize = MAX( maxsize * 3/2, maxsize + 8 );
            stack.resize(maxsize);
            stack_bottom = &stack[0];
            stack_top = stack_bottom + sz;
        }

        for( j = 0; j < size.width; j++ )
        {
            #define CANNY_SHIFT 15
            #define TG22  (int)(0.4142135623730950488016887242097*(1<<CANNY_SHIFT) + 0.5)

            x = _dx[j];
            y = _dy[j];
            int s = x ^ y;
            int m = _mag[j];

            x = abs(x);
            y = abs(y);
            if( m > low )
            {
                int tg22x = x * TG22;
                int tg67x = tg22x + ((x + x) << CANNY_SHIFT);

                y <<= CANNY_SHIFT;

                if( y < tg22x )
                {
                    if( m > _mag[j-1] && m >= _mag[j+1] )
                    {
                        if( m > high && !prev_flag && _map[j-mapstep] != 2 )
                        {
                            CANNY_PUSH( _map + j );
                            prev_flag = 1;
                        }
                        else
                            _map[j] = (uchar)0;
                        continue;
                    }
                }
                else if( y > tg67x )
                {
                    if( m > _mag[j+magstep2] && m >= _mag[j+magstep1] )
                    {
                        if( m > high && !prev_flag && _map[j-mapstep] != 2 )
                        {
                            CANNY_PUSH( _map + j );
                            prev_flag = 1;
                        }
                        else
                            _map[j] = (uchar)0;
                        continue;
                    }
                }
                else
                {
                    s = s < 0 ? -1 : 1;
                    if( m > _mag[j+magstep2-s] && m > _mag[j+magstep1+s] )
                    {
                        if( m > high && !prev_flag && _map[j-mapstep] != 2 )
                        {
                            CANNY_PUSH( _map + j );
                            prev_flag = 1;
                        }
                        else
                            _map[j] = (uchar)0;
                        continue;
                    }
                }
            }
            prev_flag = 0;
            _map[j] = (uchar)1;
        }

        // scroll the ring buffer
        _mag = mag_buf[0];
        mag_buf[0] = mag_buf[1];
        mag_buf[1] = mag_buf[2];
        mag_buf[2] = _mag;
    }

    // now track the edges (hysteresis thresholding)
    while( stack_top > stack_bottom )
    {
        uchar* m;
        if( (stack_top - stack_bottom) + 8 > maxsize )
        {
            int sz = (int)(stack_top - stack_bottom);
            maxsize = MAX( maxsize * 3/2, maxsize + 8 );
            stack.resize(maxsize);
            stack_bottom = &stack[0];
            stack_top = stack_bottom + sz;
        }

        CANNY_POP(m);

        if( !m[-1] )
            CANNY_PUSH( m - 1 );
        if( !m[1] )
            CANNY_PUSH( m + 1 );
        if( !m[-mapstep-1] )
            CANNY_PUSH( m - mapstep - 1 );
        if( !m[-mapstep] )
            CANNY_PUSH( m - mapstep );
        if( !m[-mapstep+1] )
            CANNY_PUSH( m - mapstep + 1 );
        if( !m[mapstep-1] )
            CANNY_PUSH( m + mapstep - 1 );
        if( !m[mapstep] )
            CANNY_PUSH( m + mapstep );
        if( !m[mapstep+1] )
            CANNY_PUSH( m + mapstep + 1 );
    }

    // the final pass, form the final image
    for( i = 0; i < size.height; i++ )
    {
        const uchar* _map = map + mapstep*(i+1) + 1;
        uchar* _dst = dst->data.ptr + dst->step*i;

        for( j = 0; j < size.width; j++ )
		{
            _dst[j] = (uchar)-(_map[j] >> 1);
		}
	}
};

void Canny3(	InputArray image, OutputArray _edges,
				OutputArray _sobel_x, OutputArray _sobel_y,
                int apertureSize, bool L2gradient )
{
    Mat src = image.getMat();
    _edges.create(src.size(), CV_8U);
	_sobel_x.create(src.size(), CV_16S);
	_sobel_y.create(src.size(), CV_16S);


    CvMat c_src = src, c_dst = _edges.getMat();
	CvMat c_dx = _sobel_x.getMat();
	CvMat c_dy = _sobel_y.getMat();


    cvCanny3(	&c_src, &c_dst, 
				&c_dx, &c_dy,
				apertureSize + (L2gradient ? CV_CANNY_L2_GRADIENT : 0));
};

//canny
void calculateGradient3( double * img_in, unsigned int imgx, unsigned int imgy, image_double * angles)
{
	Mat1b edge;
	Mat1s DX,DY;
	Mat1b gray = Mat::zeros(imgy,imgx,CV_8UC1);
	unsigned int x,y,addr;
	(*angles) = new_image_double(imgx,imgy);
	//copy to gray image
	for ( y = 0; y<imgy; y++)
		for ( x = 0; x<imgx; x++)
		{
			addr = y*imgx+x;
			gray.data[addr] = (uchar)(img_in[addr]);
		}
	//canny
   Canny3(gray,edge,DX,DY,3,false);
   for ( y = 0; y<imgy; y++)
   {
	    short* _dx = DX.ptr<short>(y);
		short* _dy = DY.ptr<short>(y);
		uchar* _e = edge.ptr<uchar>(y);
		for ( x = 0; x<imgx; x++)
		{
			if(_e[x] > 0)//0 or 255
			{
				(*angles)->data[y*imgx+x]  = atan2((double)_dy[x],(double)_dx[x]);//calculate gradient 
			}
			else
				(*angles)->data[y*imgx+x] = NOTDEF;
		}
   }
   edge.release();
   DX.release();
   DY.release();
   gray.release();
}


//=============================================================================
/** Convert ellipse from matrix form to common form:
    ellipse = (centrex,centrey,ax,ay,orientation).
 */
int ellipse2Param(double *p,double param[])
{
	// ax^2 + bxy + cy^2 + dx + ey + f = 0 
  double a,b,c,d,e,f;
  double thetarad,cost,sint,cos_squared,sin_squared,cos_sin,Ao,Au,Av,Auu,Avv,tuCentre,tvCentre,wCentre,uCentre,vCentre,Ru,Rv;
  a = p[0];
  b = p[1];
  c = p[2];
  d = p[3];
  e = p[4];
  f = p[5]; 

  thetarad=0.5*atan2(b,a-c); 
  cost=cos(thetarad);
  sint=sin(thetarad);
  sin_squared=sint*sint;
  cos_squared=cost*cost;
  cos_sin=sint*cost;
  Ao=f;
  Au=d*cost+e* sint;
  Av=-d*sint+e* cost;
  Auu=a*cos_squared+c*sin_squared+b*cos_sin;
  Avv=a*sin_squared+c*cos_squared-b*cos_sin;

  if(Auu==0 || Avv==0){ param[0]=0;param[1]=0;param[2]=0;param[3]=0;param[4]=0;return 0;}
  else
    {
      tuCentre=-Au/(2.*Auu);
      tvCentre=-Av/(2.*Avv);
      wCentre=Ao-Auu*tuCentre*tuCentre-Avv*tvCentre*tvCentre;
      uCentre=tuCentre*cost-tvCentre*sint;
      vCentre=tuCentre*sint+tvCentre*cost;
      Ru=-wCentre/Auu;
      Rv=-wCentre/Avv;
 //     if (Ru>0) Ru=pow(Ru,0.5);
 //     else Ru=-pow(-Ru,0.5);
 //     if (Rv>0) Rv=pow(Rv,0.5);
 //     else Rv=-pow(-Rv,0.5);
	  if (Ru <= 0 || Rv <= 0)//������С��0�����������
		  return 0;
	  Ru = sqrt(Ru);
	  Rv = sqrt(Rv);
      param[0]=uCentre;param[1]=vCentre;
      param[2]=Ru;param[3]=Rv;param[4]=thetarad;
	  //�����Ru < Rv������Ե�һ�� (There will be Ru < Rv situation, swap it)
	  if(Ru < Rv )
	  {
		  param[2] = Rv;
		  param[3] = Ru;
		  if(thetarad < 0)//���������ᣬʹ�õ���������Ϊ���ᣬ���ĸ�Ϊ���� (Swap the long and short axes so that the third parameter is the long axis and the fourth is the short axis)
			  param[4] += M_1_2_PI;
		  else
			  param[4] -= M_1_2_PI;
		  if(thetarad < - M_1_2_PI)//��������޶���-pi/2 ~ pi/2���߱�Ψһ�� (The long-axis inclination is limited to -pi/2 ~ pi/2, which is unique)
			  param[4] += M_PI;
		  if(thetarad > M_1_2_PI)
			  param[4] -= M_PI;
	  }
    }
  return 1;
}
//input : (xi,yi)
//output: x0,y0,a,b,phi,ellipara��Ҫ���������ڴ� (x0,y0,a,b,phi,ellipara need to apply for memory in advance)
//successfull, return 1; else return 0
int fitEllipse(point2d* dataxy, int datanum, double* ellipara)
{
	double* D = (double*)malloc(datanum*6*sizeof(double));
	double S[36]; 
	double C[36];
	memset(D,0,sizeof(double)*datanum);
	memset(S,0,sizeof(double)*36);
	memset(C,0,sizeof(double)*36);
	for ( int i = 0; i<datanum; i++)
	{
		D[i*6]  = dataxy[i].x*dataxy[i].x;
		D[i*6+1]= dataxy[i].x*dataxy[i].y;
		D[i*6+2]= dataxy[i].y*dataxy[i].y;
		D[i*6+3]= dataxy[i].x;
		D[i*6+4]= dataxy[i].y;
		D[i*6+5]= 1;
	}
	for ( int i = 0; i<6; i++)
		for ( int j = i; j<6; j++)
		{
			//S[i*6+j]
			for ( int k = 0; k<datanum; k++)
				S[i*6+j] += D[k*6+i]*D[k*6+j];
		}
	free(D);//�ͷ��ڴ� (free memory)
	//�Գƾ���ֵ (Symmetric Matrix Assignment)
	for ( int i = 0; i<6; i++)
		for ( int j = 0; j<i; j++)
			S[i*6+j]=S[j*6+i];
	C[0*6+2] = 2;
	C[1*6+1] = -1;
	C[2*6+0] = 2;
	// eig(S,C) eig(inv(S)*C)
	double alphar[6],alphai[6],beta[6];
	double vl[36] = {0};//�˴����� (not used here)
	double vr[36] = {0};
	char JOBVL = 'N';
	char JOBVR = 'V';
	ptrdiff_t fitN = 6;
	double fitWork[64];
	ptrdiff_t workLen = 64;
	ptrdiff_t info;
	//info = LAPACKE_dggev(LAPACK_ROW_MAJOR,'N','V',6,S,6,C,6,alphar,alphai,beta,vl,6,vr,6);
	//ע��SΪ�Գƾ��󣬹�ת�ú���ڱ�����������ȣ�S���Բ���
	//(Note that S is a symmetric matrix, so it is equal to itself after transposition, becoming column priority, and S can be unchanged)
	dggev(&JOBVL,&JOBVR,&fitN,S,&fitN,C,&fitN,alphar,alphai,beta,vl,&fitN,vr,&fitN,fitWork,&workLen,&info);
	if(info == 0)
	{
		int index = -1;
		for ( int i = 0; i<6; i++)
			if( (alphar[i]>=-(2.2204460492503131e-014)) && (alphai[i] == 0) && (beta[i] != 0)) // 100*DBL_EPSILON, eigenvalue = (alphar + i*alphai)/beta
				index = i;//vr[:,i],vr��i�ж�Ӧ������������Ϊ��ϲ��� (vr[:,i], the eigenvector corresponding to the i-th column of vr is the fitting parameter)
		if(index == -1)//����һ�Σ��ſ��ʵ��>0��Լ�����ſ�>-0.005 (try again, relax the constraint on the real part > 0 to >-0.005)
		{
			double temp = -0.005;//��������ܹؼ� (//This parameter is critical)
			for ( int i = 0; i<6; i++)
			if( (alphar[i]>=temp) && (alphai[i] == 0) && (beta[i] != 0)) // 100*DBL_EPSILON, eigenvalue = (alphar + i*alphai)/beta
			{
				temp = alphar[i];
				index = i;//vr[:,i],vr��i�ж�Ӧ������������Ϊ��ϲ��� (vr[:,i], the eigenvector corresponding to the i-th column of vr is the fitting parameter)
			}
		}
		if(index != -1)
		{
			//�˴�����beta�����ݲ��� (Borrow beta to pass parameters here)
		    //beta[0] = vr[6*0+index];
		    //beta[1] = vr[6*1+index];
		    //beta[2] = vr[6*2+index];
		    //beta[3] = vr[6*3+index];
		    //beta[4] = vr[6*4+index];
		    //beta[5] = vr[6*5+index];
			  beta[0] = vr[6*index+0];
		      beta[1] = vr[6*index+1];
		      beta[2] = vr[6*index+2];
		      beta[3] = vr[6*index+3];
		      beta[4] = vr[6*index+4];
		      beta[5] = vr[6*index+5];
			ellipse2Param(beta,ellipara);//ax^2 + bxy + cy^2 + dx + ey + f = 0, transform to (x0,y0,a,b,phi)
			return 1;
		}
	}
	return 0;
}

//input: dataxyΪ���ݵ�(xi,yi),�ܹ���datanum�� (dataxy is the data point (xi,yi), there are a total of datanum)
//output: ��Ͼ���S. ע�⣺S��Ҫ���������ڴ棬double S[36]. (Fitting matrix S. Note: S needs to apply for memory in advance, double S[36].)
inline void calcuFitMatrix(point2d* dataxy, int datanum, double * S)
{
	double* D = (double*)malloc(datanum*6*sizeof(double));
	memset(D,0,sizeof(double)*datanum);
	for ( int i = 0; i<datanum; i++)
	{
		D[i*6]  = dataxy[i].x*dataxy[i].x;
		D[i*6+1]= dataxy[i].x*dataxy[i].y;
		D[i*6+2]= dataxy[i].y*dataxy[i].y;
		D[i*6+3]= dataxy[i].x;
		D[i*6+4]= dataxy[i].y;
		D[i*6+5]= 1;
	}
	for ( int i = 0; i<6; i++)
	{
		for ( int j = i; j<6; j++)
		{
			//S[i*6+j]
			for ( int k = 0; k<datanum; k++)
				S[i*6+j] += D[k*6+i]*D[k*6+j];
		}
	}
    free(D);//�ͷ��ڴ� (free memory)
	//�Գƾ���ֵ (Symmetric Matrix Assignment)
	for ( int i = 0; i<6; i++)
		for ( int j = 0; j<i; j++)
			S[i*6+j]=S[j*6+i];
}
//input: fit matrixes S1,S2. length is 36.
//output: fit matrix S_out. S_out = S1 + S2.
//S_out������Ҫ�����ڴ� (S_out needs to apply for memory in advance)
inline void addFitMatrix(double * S1, double * S2, double * S_out)
{
	int ind;
	for ( int i = 0; i<6; i++ )
		for ( int j = i; j<6; j++)
		{
			ind = i*6+j;
			S_out[ind] = S1[ind]+S2[ind];
		}
	//�Գƾ���ֵ (Symmetric Matrix Assignment)
	for ( int i = 0; i<6; i++)
		for ( int j = 0; j<i; j++)
			S_out[i*6+j]=S_out[j*6+i];
}
//input : S����6 x 6 = 36
//output: (A,B,C,D,E,F)��A>0, ellicoeff��Ҫ���������ڴ�. ��Ҫת����(x0,y0,a,b,phi)
//		  ((A,B,C,D,E,F) and A>0, ellicoeff needs to apply for memory in advance. When converting to (x0,y0,a,b,phi))
//ellipse2Param(ellicoeff,ellipara); ax^2 + bxy + cy^2 + dx + ey + f = 0, transform to (x0,y0,a,b,phi)
//successfull, return 1; else return 0
int fitEllipse2(double * S, double* ellicoeff)
{
	double C[36];
	memset(C,0,sizeof(double)*36);
	
	C[0*6+2] = 2;
	C[1*6+1] = -1;
	C[2*6+0] = 2;
	// eig(S,C) eig(inv(S)*C)
	double alphar[6],alphai[6],beta[6];
	double vl[36] = {0};//�˴����� (not used here)
	double vr[36] = {0};
	char JOBVL = 'N';
	char JOBVR = 'V';
	ptrdiff_t fitN = 6;
	double fitWork[64];
	ptrdiff_t workLen = 64;
	ptrdiff_t info;
	//info = LAPACKE_dggev(LAPACK_ROW_MAJOR,'N','V',6,S,6,C,6,alphar,alphai,beta,vl,6,vr,6);
	dggev(&JOBVL,&JOBVR,&fitN,S,&fitN,C,&fitN,alphar,alphai,beta,vl,&fitN,vr,&fitN,fitWork,&workLen,&info);
	if(info == 0)
	{
		int index = -1;
		for ( int i = 0; i<6; i++)
			if( (alphar[i]>=-(2.2204460492503131e-014)) && (alphai[i] == 0) && (beta[i] != 0)) // 100*DBL_EPSILON, eigenvalue = (alphar + i*alphai)/beta
				index = i;//vr[:,i],vr��i�ж�Ӧ������������Ϊ��ϲ��� (vr[:,i], the eigenvector corresponding to the i-th column of vr is the fitting parameter)
		if(index == -1)//����һ�Σ��ſ��ʵ��>0��Լ�����ſ�>-0.005 (try again, relax the constraint on the real part > 0 to >-0.005)
		{
			double temp = -0.005;//��������ܹؼ�
			for ( int i = 0; i<6; i++)
			if( (alphar[i]>=temp) && (alphai[i] == 0) && (beta[i] != 0)) // 100*DBL_EPSILON, eigenvalue = (alphar + i*alphai)/beta
			{
				temp = alphar[i];
				index = i;//vr[:,i],vr��i�ж�Ӧ������������Ϊ��ϲ��� (vr[:,i], the eigenvector corresponding to the i-th column of vr is the fitting parameter)
			}
		}
		if(index != -1)
		{
			//�˴�����beta�����ݲ��� (Borrow beta to pass parameters here)
	        if(vr[6*index+0] < 0)//ע�������� (Pay attention to column first)
			{
				ellicoeff[0] = -vr[6*index+0]; //-vr[6*0+index];
				ellicoeff[1] = -vr[6*index+1]; //-vr[6*1+index];
				ellicoeff[2] = -vr[6*index+2]; //-vr[6*2+index];
				ellicoeff[3] = -vr[6*index+3]; //-vr[6*3+index];
				ellicoeff[4] = -vr[6*index+4]; //-vr[6*4+index];
				ellicoeff[5] = -vr[6*index+5]; //-vr[6*5+index];
			}
			else
			{
				ellicoeff[0] = vr[6*index+0];//vr[6*0+index];
				ellicoeff[1] = vr[6*index+1];//vr[6*1+index];
				ellicoeff[2] = vr[6*index+2];//vr[6*2+index];
				ellicoeff[3] = vr[6*index+3];//vr[6*3+index];
				ellicoeff[4] = vr[6*index+4];//vr[6*4+index];
				ellicoeff[5] = vr[6*index+5];//vr[6*5+index];
			}
			return 1;
		}
	}
	return 0;
}

//��Σ�e1 = (x1,y1,a1,b1,phi1), e2 = (x2,y2,a2,b2,phi2) (Input parameters: e1 = (x1,y1,a1,b1,phi1), e2 = (x2,y2,a2,b2,phi2))
//��������Ϊ1������Ϊ0 (Output: 1 if equal, 0 otherwise)
inline bool isEllipseEqual(double * ellipse1, double * ellipse2, double centers_distance_threshold, double semimajor_errorratio, double semiminor_errorratio, double angle_errorratio, double iscircle_ratio)
{
	bool con1 = ( abs(ellipse1[0] - ellipse2[0]) < centers_distance_threshold && abs(ellipse1[1] - ellipse2[1]) < centers_distance_threshold &&
		abs(ellipse1[2] - ellipse2[2])/MAX(ellipse1[2],ellipse2[2]) < semimajor_errorratio && abs(ellipse1[3] - ellipse2[3])/MIN(ellipse1[3],ellipse2[3]) < semiminor_errorratio );
	bool con2 = ( ellipse1[3]/ellipse1[2] >= iscircle_ratio );//0.9 0.85
	bool con3 = ( ellipse2[3]/ellipse2[2] >= iscircle_ratio );
	bool con4 = ( (con2 && con3) || (con2 == false && con3 == false && abs(ellipse1[4]-ellipse2[4])<= angle_errorratio*M_PI) );
	return (con1 && con4);
}

inline bool regionLimitation( point2d point_g1s, point2d g1s_ls_dir, point2d point_g1e, point2d g1e_ls_dir, point2d point_g2s, point2d g2s_ls_dir, point2d point_g2e, point2d g2e_ls_dir, double polarity, double region_limitation_dis_tolerance)
{
	point2d g1m_ls_dir, g2m_ls_dir;
	point2d g1s_arc_dir,g1e_arc_dir,g1m_arc_dir,g2s_arc_dir,g2e_arc_dir,g2m_arc_dir;
	point2d test_vec1,test_vec2,test_vec3; //��ָ��Բ�ĵ������Ͳ������� (The vector of the arc pointing to the center of the circle and the test vector)
	//���pend<-pstart���ɵ�����Ϊgim_arc_dir (The vector composed of pend<-pstart of the group is gim_arc_dir)
	double xdelta, ydelta, theta;
	xdelta = point_g1e.x - point_g1s.x;
	ydelta = point_g1e.y - point_g1s.y;
	theta = atan2(ydelta,xdelta);
	g1m_ls_dir.x = cos(theta);
	g1m_ls_dir.y = sin(theta);
	xdelta = point_g2e.x - point_g2s.x;
	ydelta = point_g2e.y - point_g2s.y;
	theta = atan2(ydelta,xdelta);
	g2m_ls_dir.x = cos(theta);
	g2m_ls_dir.y = sin(theta);

	if( polarity == 1)// polarity is equal 1, arc vector = (dy,-dx)
	{
		g1s_arc_dir.x = g1s_ls_dir.y;
		g1s_arc_dir.y = -g1s_ls_dir.x;
		g1e_arc_dir.x = g1e_ls_dir.y;
		g1e_arc_dir.y = -g1e_ls_dir.x;
		g1m_arc_dir.x = g1m_ls_dir.y;
		g1m_arc_dir.y = -g1m_ls_dir.x;
		g2s_arc_dir.x = g2s_ls_dir.y;
		g2s_arc_dir.y = -g2s_ls_dir.x;
		g2e_arc_dir.x = g2e_ls_dir.y;
		g2e_arc_dir.y = -g2e_ls_dir.x;
		g2m_arc_dir.x = g2m_ls_dir.y;
		g2m_arc_dir.y = -g2m_ls_dir.x;
	}
	else// polarity is equal -1, arc vector = (-dy,dx)
	{
		g1s_arc_dir.x = -g1s_ls_dir.y;
		g1s_arc_dir.y = g1s_ls_dir.x;
		g1e_arc_dir.x = -g1e_ls_dir.y;
		g1e_arc_dir.y = g1e_ls_dir.x;
		g1m_arc_dir.x = -g1m_ls_dir.y;
		g1m_arc_dir.y = g1m_ls_dir.x;
		g2s_arc_dir.x = -g2s_ls_dir.y;
		g2s_arc_dir.y = g2s_ls_dir.x;
		g2e_arc_dir.x = -g2e_ls_dir.y;
		g2e_arc_dir.y = g2e_ls_dir.x;
		g2m_arc_dir.x = -g2m_ls_dir.y;
		g2m_arc_dir.y = g2m_ls_dir.x;
	}
	test_vec1.x = (point_g2e.x - point_g1s.x);
	test_vec1.y = (point_g2e.y - point_g1s.y);
	test_vec2.x = (point_g2s.x - point_g1e.x);
	test_vec2.y = (point_g2s.y - point_g1e.y);
	test_vec3.x = (test_vec1.x + test_vec2.x)/2;
	test_vec3.y = (test_vec1.y + test_vec2.y)/2;
	double t1,t2,t3,t4,t5,t6;
	t1 = dotProduct(g1s_arc_dir,test_vec1);
	t2 = dotProduct(g1e_arc_dir,test_vec2);
	t3 = dotProduct(g1m_arc_dir,test_vec3);
	t4 = -dotProduct(g2e_arc_dir,test_vec1);
	t5 = -dotProduct(g2s_arc_dir,test_vec2);
	t6 = -dotProduct(g2m_arc_dir,test_vec3);

	if(  dotProduct(g1s_arc_dir,test_vec1)  >= region_limitation_dis_tolerance && \
		 dotProduct(g1e_arc_dir,test_vec2)  >= region_limitation_dis_tolerance && \
		 dotProduct(g1m_arc_dir,test_vec3)  >= region_limitation_dis_tolerance && \
		-dotProduct(g2e_arc_dir,test_vec1) >= region_limitation_dis_tolerance && \
	    -dotProduct(g2s_arc_dir,test_vec2) >= region_limitation_dis_tolerance && \
		-dotProduct(g2m_arc_dir,test_vec3) >= region_limitation_dis_tolerance
		)
		return TRUE;
	return FALSE;
}

/*
void drawEllipse(Mat img, double * ellipara)
{
  Point peliicenter(ellipara[0],ellipara[1]);
  Size  saxis(ellipara[2],ellipara[3]);
  //Mat ellimat = Mat::zeros(img.rows,img.cols,CV_8UC3);
  //ellimat.setTo(255);
  static int ccc = 0;
  static unsigned int cnt = 0;
  if(cnt % 2 == 0 )
	  ccc = 0;
  else
  {
	  ccc = 255;
	  cout<<cnt/2<<'\t'<<ellipara[0]<<'\t'<<ellipara[1]<<"\t"<<ellipara[2]<<'\t'<<ellipara[3]<<'\t'<<ellipara[4]<<endl;
  }
  cnt++;

  Mat imgtemp = img.clone();
  ellipse(imgtemp,peliicenter,saxis,ellipara[4]*180/M_PI,0,360,(Scalar(0,255,ccc)),2);
  namedWindow("w1");
  imshow("w1",imgtemp);
  //waitKey(0);
}
void drawEdge(Mat img, point2d * dataxy, int num)
{
	 static int ccc = 0;
     static int cnt = 0;
     cnt++;
     if(cnt % 2 == 0 )
	     ccc = 0;
     else
	  ccc = 255;
	Mat imgtemp = img.clone();
	for (int i = 0; i<num; i++)
	{
		imgtemp.at<Vec3b>(dataxy[i].y,dataxy[i].x) = (Vec3b(ccc,255,0));
	}
	namedWindow("w2");
    imshow("w2",imgtemp);
}
*/

/*----------------------------------------------------------------------------*/
/** Approximate the distance between a point and an ellipse using Rosin distance.
 */
inline double d_rosin (double *param, double x, double y)
{ 
  double ae2 = param[2]*param[2];
  double be2 = param[3]*param[3];
  x = x - param[0];
  y = y - param[1];
  double xp = x*cos(-param[4])-y*sin(-param[4]);
  double yp = x*sin(-param[4])+y*cos(-param[4]);
  double fe2;
  fe2 = ae2-be2;
  double X = xp*xp;
  double Y = yp*yp;
  double delta = (X+Y+fe2)*(X+Y+fe2)-4*X*fe2;
  double A = (X + Y + fe2 - sqrt(delta))/2.0; 
  double ah = sqrt(A);
  double bh2 = fe2-A;
  double term = (A*be2+ae2*bh2);
  double xi = ah*sqrt(ae2*(be2+bh2)/term);
  double yi = param[3]*sqrt(bh2*(ae2-A)/term);
  double d[4],dmin;


  d[0] = dist(xp,yp,xi,yi);
  d[1] = dist(xp,yp,xi,-yi);
  d[2] = dist(xp,yp,-xi,yi);
  d[3] = dist(xp,yp,-xi,-yi);
  dmin = DBL_MAX;
  for ( int i = 0; i<4; i++)
  {
	  if( d[i] <= dmin)
		  dmin = d[i];
  }
//  if (X+Y>xi*xi+yi*yi)
//    return dmin;
//  else return -dmin; 
  return dmin;
}
/*----------------------------------------------------------------------------*/

//���� (enter)
//lsd�㷨���õ����߶μ���lines������line_num��return�ķ���ֵ��line_nums���߶Σ�Ϊһάdouble������lines������Ϊ8*n��ÿ8��Ϊһ��
// (The number line_num of the line segment set lines detected by the lsd algorithm, the return value of return is line_nums 
// line segments, which is a one-dimensional double-type array lines, with a length of 8*n, and each 8 is a group)
//����x1,y1,x2,y2,dx,dy,length,polarity (Store x1,y1,x2,y2,dx,dy,length,polarity)
//groups: �߶η��飬ÿ����水�ռ��ηֲ�˳��˳ʱ�������ʱ��洢���߶��������߶�������Χ��0~line_num-1. ����������ָ�룬ʹ��ʱҪע��(*group)
// (groups: Line segments are grouped. Each group stores the line segment index clockwise or counterclockwise according to 
// the geometric distribution order. The range of the line segment index is 0~line_num-1. Since it is a pointer, pay attention when using it (*group))
//first_group_ind��second_group_ind��ƥ����ӵ�����������ȡsalient hypothesisʱ��second_group_ind = -1, fit_matrix2 = NULL.
// (first_group_ind, second_group_ind are the indices of matching teams, when extracting the salient hypothesis, second_group_ind = -1, fit_matrix2 = NULL.)
//fit_matrix1, fit_matrix2, �ֱ�����ӵĶ�Ӧ����Ͼ��� (are the corresponding fitting matrices of the team, respectively)
//angles, �Ǳ�Ե��ͼ+�ݶȷ��� �ޱ�Ե��ʱ��NODEF (is the edge point map + gradient direction. NODEF when there are no edge points)
//distance_tolerance:
//group_inliers_num:��¼�Ÿ������֧���ڵ����������飬ʵʱ���£���ʼʱΪ0
// (group_inliers_num: An array that records the number of inliers supported by each group, updated in real time, initially 0)
//��� (output)
//ellipara
bool calcEllipseParametersAndValidate( double * lines, int line_num, vector<vector<int>> * groups, int first_group_ind,int second_group_ind, double * fit_matrix1, double * fit_matrix2, image_double angles, double distance_tolerance, unsigned int * group_inliers_num, point5d *ellipara)
{
	double S[36]; //��Ͼ���S (fit matrix S)
	double Coefficients[6] = {0,0,0,0,0,0};// ax^2 + bxy + cy^2 + dx + ey + f = 0 
	double param[5], param2[5];
	int info,addr;
	rect rec;
	rect_iter* iter;
	int rec_support_cnt,rec_inliers_cnt;
	bool flag1 = TRUE, flag2 = TRUE;
	double point_normalx, point_normaly, point_normal, temp;
	vector<point2i> first_group_inliers, second_group_inliers;
	point2i pixel_temp;
	double semimajor_errorratio,semiminor_errorratio,iscircle_ratio;
	if( fit_matrix2 == NULL || second_group_ind == -1)//ֻ��һ�����ǶȽϴ���������� (Fit only one group with greater coverage)
	{
		for ( int i  = 0; i < 36; i++)
			S[i] = fit_matrix1[i];
	}
	else
	{
		addFitMatrix(fit_matrix1,fit_matrix2,S);//����Խ�����ϣ� S = fit_matrix1 + fit_matrix2 (Fitting pairs of groups)
	}
	info = fitEllipse2(S, Coefficients);// ax^2 + bxy + cy^2 + dx + ey + f = 0, a > 0
	if ( info == 0 )//���ʧ�� (Fit failed)
	{
		ellipara = NULL;
		return FALSE;
	}
	ellipse2Param(Coefficients, param);// (x0,y0,a,b,phi)
	if ( min(param[2],param[3]) < 3*distance_tolerance || max(param[2],param[3]) > min(angles->xsize,angles->ysize) ||  param[0] < 0 || param[0] > angles->xsize || param[1] < 0 || param[1] > angles->ysize )
	{
		ellipara = NULL;
		return FALSE;
	}
	//if ( first_group_ind == 2 && second_group_ind == 8)
	//drawEllipse(img,param);
	//����е� first group�Ƚ����ڵ�׼����֤�����Ҹ������֧���ڵ�����
	// (The first group in the team is verified by the interior point criterion first, and the number of supported interior points of the group is updated)
	for ( unsigned int i = 0; i<(*groups)[first_group_ind].size(); i++)
	{
		addr = (*groups)[first_group_ind][i] * 8; //��first_group_ind����ĵ�i���߶�����*8 (The i-th line segment index of the first_group_ind grouping*8)
		rec.x1 = lines[addr];
		rec.y1 = lines[addr+1];
		rec.x2 = lines[addr+2];
		rec.y2 = lines[addr+3];
		rec.x  = (rec.x1 + rec.x2)/2;
		rec.y  = (rec.y1 + rec.y2)/2;
		rec.dx = lines[addr+4];
		rec.dy = lines[addr+5];
		rec.width = 3*distance_tolerance;
		//line_length[i] = (int)lines[addr+6];//��¼�߶γ��ȵ�����line_length[i] (Record the length of the line segment to the array line_length[i])
		rec_support_cnt = rec_inliers_cnt = 0;//�������Ҫ (clearing is important)
		if ( lines[addr+7] == 1) //����һ�� (same polarity)
		{
			for(iter = ri_ini(&rec);!ri_end(iter);ri_inc(iter))//�߶�1 (line segment 1)
			{
				//��Ӿ��ο��ܻ�Խ�� (The bounding rectangle may be out of bounds)
				if(iter->x >= 0 && iter->y >= 0 && iter->x < angles->xsize && iter->y < angles->ysize)
				{
					temp  = angles->data[iter->y*angles->xsize+iter->x] ;//�ڵ���ݶȷ��� (Gradient direction of interior points)
					if(temp!= NOTDEF )
					{
						//test point's normal is (ax0+by0/2+d/2, cy0+bx0/2+e/2)
						point_normalx = Coefficients[0]*iter->x + (Coefficients[1]*iter->y + Coefficients[3])/2;
						point_normaly = Coefficients[2]*iter->y + (Coefficients[1]*iter->x + Coefficients[4])/2;
						point_normal = atan2(-point_normaly,-point_normalx); //��Ե��ķ��߷���,ָ����Բ�ڲ� (The normal direction of the edge point, pointing to the inside of the ellipse)
						rec_inliers_cnt++;
						if(angle_diff(point_normal,temp) <= M_1_8_PI ) //+- 22.5���� �� || d - r || < 3 dis_t
						{
							rec_support_cnt++;
							pixel_temp.x = iter->x; pixel_temp.y = iter->y;
							first_group_inliers.push_back(pixel_temp);//��Ӹ��߶ζ�Ӧ���ڵ� (Add the interior point corresponding to the line segment)
						}
					} 
				}
			}
		}
		else//�����෴ (opposite polarity)
		{
			for(iter = ri_ini(&rec);!ri_end(iter);ri_inc(iter))//�߶�1 (line segment 1)
			{
				//��Ӿ��ο��ܻ�Խ�� (The bounding rectangle may be out of bounds)
				if(iter->x >= 0 && iter->y >= 0 && iter->x < angles->xsize && iter->y < angles->ysize)
				{
					temp  = angles->data[iter->y*angles->xsize+iter->x] ;//�ڵ���ݶȷ��� (Gradient direction of interior points)
					if(temp!= NOTDEF )
					{
						//test point's normal is (ax0+by0/2+d/2, cy0+bx0/2+e/2)
						point_normalx = Coefficients[0]*iter->x + (Coefficients[1]*iter->y + Coefficients[3])/2;
						point_normaly = Coefficients[2]*iter->y + (Coefficients[1]*iter->x + Coefficients[4])/2;
						point_normal = atan2(point_normaly,point_normalx); //��Ե��ķ��߷���,ָ����Բ��� (The normal direction of the edge point, pointing to the outside of the ellipse)
						rec_inliers_cnt++;
						if(angle_diff(point_normal,temp) <= M_1_8_PI ) //+- 22.5���� �� || d - r || < 3 dis_t (+- within 22.5�� and || d - r || < 3 dis_t)
						{
							rec_support_cnt++;
							pixel_temp.x = iter->x; pixel_temp.y = iter->y;
							first_group_inliers.push_back(pixel_temp);//��Ӹ��߶ζ�Ӧ���ڵ� (Add the interior point corresponding to the line segment)
						}
					} 
				}
			}
		}
		if( !( rec_support_cnt > 0 && ( rec_support_cnt >= 0.8*lines[addr+6] || rec_support_cnt*1.0/rec_inliers_cnt >= 0.6) ) )
		{
			flag1 = FALSE; //flag1 ��ʼ��ʱΪTRUE, һ��������һ���߶β�����Ҫ��ֱ��false, �ڵ�׼����֤��ͨ��
			//(flag1 is TRUE when initialized, once there is a line segment in the group that does not meet the requirements, it is directly false, and the interior point criterion verification fails)
			break;
		}
	}
	if ( flag1 == TRUE && first_group_inliers.size() >= 0.8*group_inliers_num[first_group_ind] )//�������ͳ�ƹ����ڵ�,ͨ����֤ (Close to the largest statistical interior point, pass the verification)
	{
		if( first_group_inliers.size() >= group_inliers_num[first_group_ind])//��������ֹ�������ڵ��� (The maximum number of inliers the update group has ever seen)
			group_inliers_num[first_group_ind] =  first_group_inliers.size();
	}
	else 
		flag1 = FALSE;
	//��һ���������֤ (The first group completes verification)
	if ( second_group_ind == -1 || fit_matrix2 == NULL)//ֻ��һ�����ǶȽϴ���������� (Fit only one group with greater coverage)
	{
		ellipara->x = param[0];//��Ϊ������Σ�����Ҫ����������ǿ����Բ (Because in any case, it is necessary to return a highly significant ellipse)
	    ellipara->y = param[1];
	    ellipara->a = param[2];
	    ellipara->b = param[3];
	    ellipara->phi = param[4];
		if ( flag1 == TRUE)//ͨ���ڵ��ٴ���ϣ�������� (Refit with interior points to improve quality)
		{
			point2d * dataxy = (point2d*)malloc(sizeof(point2d)*first_group_inliers.size());
			for ( unsigned int i = 0; i<first_group_inliers.size(); i++)
			{
				dataxy[i].x = first_group_inliers[i].x;
				dataxy[i].y = first_group_inliers[i].y;
			}
			info = fitEllipse(dataxy,first_group_inliers.size(), param2);
			free(dataxy); //�ͷ��ڴ� (free memory)
			if ( info == 1  && isEllipseEqual(param2,param,3*distance_tolerance,0.1,0.1,0.1,0.9) )
			{
				ellipara->x = param2[0];//������Բ�����Ʒ�� (Update ellipse, improve quality)
			    ellipara->y = param2[1];
			    ellipara->a = param2[2];
			    ellipara->b = param2[3];
			    ellipara->phi = param2[4];
			    //drawEllipse(img,param2);
			}
		}
		return TRUE;//����ֻ��һ�������ȡ��Բ����ʱֱ�ӷ��� (For the extraction ellipse with only one group, return directly at this time)
	}
	//��������������е� second group�����ڵ�׼����֤�����Ҹ������֧���ڵ�����
	// (Next, verify the interior point criterion for the second group in the team, and update the number of supported interior points for the group)
	if (flag1 == FALSE)//����������У������һ���鶼�޷������ڵ�Ҫ��ֱ�ӷ���false (In the team operation, if the first group cannot meet the interior point requirements, return false directly)
		return FALSE;
	for ( unsigned int i = 0; i<(*groups)[second_group_ind].size(); i++)
	{
		addr = (*groups)[second_group_ind][i] * 8; //��first_group_ind����ĵ�i���߶�����*8 (The i-th line segment index of the first_group_ind grouping*8)
		rec.x1 = lines[addr];
		rec.y1 = lines[addr+1];
		rec.x2 = lines[addr+2];
		rec.y2 = lines[addr+3];
		rec.x  = (rec.x1 + rec.x2)/2;
		rec.y  = (rec.y1 + rec.y2)/2;
		rec.dx = lines[addr+4];
		rec.dy = lines[addr+5];
		rec.width = 3*distance_tolerance;
		//line_length[i] = (int)lines[addr+6];//��¼�߶γ��ȵ�����line_length[i] (Record the length of the line segment to the array line_length[i])
		rec_support_cnt = rec_inliers_cnt = 0;//�������Ҫ (clearing is important)
		if ( lines[addr+7] == 1) //����һ�� (same polarity)
		{
			for(iter = ri_ini(&rec);!ri_end(iter);ri_inc(iter))//�߶�1 (�߶�1)
			{
				//��Ӿ��ο��ܻ�Խ�� (The bounding rectangle may be out of bounds)
				if(iter->x >= 0 && iter->y >= 0 && iter->x < angles->xsize && iter->y < angles->ysize)
				{
					temp  = angles->data[iter->y*angles->xsize+iter->x] ;//�ڵ���ݶȷ��� (Gradient direction of interior points)
					if(temp!= NOTDEF )
					{
						//test point's normal is (ax0+by0/2+d/2, cy0+bx0/2+e/2)
						point_normalx = Coefficients[0]*iter->x + (Coefficients[1]*iter->y + Coefficients[3])/2;
						point_normaly = Coefficients[2]*iter->y + (Coefficients[1]*iter->x + Coefficients[4])/2;
						point_normal = atan2(-point_normaly,-point_normalx); //��Ե��ķ��߷���,ָ����Բ�ڲ� (The normal direction of the edge point, pointing to the inside of the ellipse)
						rec_inliers_cnt++;
						if(angle_diff(point_normal,temp) <= M_1_8_PI ) //+- 22.5���� �� || d - r || < 3 dis_t (+- within 22.5�� and || d - r || < 3 dis_t)
						{
							rec_support_cnt++;
							pixel_temp.x = iter->x; pixel_temp.y = iter->y;
							second_group_inliers.push_back(pixel_temp);//��Ӹ��߶ζ�Ӧ���ڵ� (Add the interior point corresponding to the line segment)
						}
					} 
				}
			}
		}
		else//�����෴ (opposite polarity)
		{
			for(iter = ri_ini(&rec);!ri_end(iter);ri_inc(iter))//�߶�1 (line segment 1)
			{
				//��Ӿ��ο��ܻ�Խ�� (The bounding rectangle may be out of bounds)
				if(iter->x >= 0 && iter->y >= 0 && iter->x < angles->xsize && iter->y < angles->ysize)
				{
					temp  = angles->data[iter->y*angles->xsize+iter->x] ;//�ڵ���ݶȷ��� (Gradient direction of interior points)
					if(temp!= NOTDEF )
					{
						//test point's normal is (ax0+by0/2+d/2, cy0+bx0/2+e/2)
						point_normalx = Coefficients[0]*iter->x + (Coefficients[1]*iter->y + Coefficients[3])/2;
						point_normaly = Coefficients[2]*iter->y + (Coefficients[1]*iter->x + Coefficients[4])/2;
						point_normal = atan2(point_normaly,point_normalx); //��Ե��ķ��߷���,ָ����Բ��� (The normal direction of the edge point, pointing to the outside of the ellipse)
						rec_inliers_cnt++;
						if(angle_diff(point_normal,temp) <= M_1_8_PI ) //+- 22.5���� �� || d - r || < 3 dis_t (+- within 22.5�� and || d - r || < 3 dis_t)
						{
							rec_support_cnt++;
							pixel_temp.x = iter->x; pixel_temp.y = iter->y;
							second_group_inliers.push_back(pixel_temp);//��Ӹ��߶ζ�Ӧ���ڵ� (Add the interior point corresponding to the line segment)
						}
					} 
				}
			}
		}
		if( !(rec_support_cnt > 0 && ( rec_support_cnt >= 0.8*lines[addr+6] || rec_support_cnt*1.0/rec_inliers_cnt >= 0.6) ) )
		{
			flag2 = FALSE; //flag1 ��ʼ��ʱΪTRUE, һ��������һ���߶β�����Ҫ��ֱ��false, �ڵ�׼����֤��ͨ��
			// (flag1 is TRUE when initialized, once there is a line segment in the group that does not meet the requirements, it is directly false, and the interior point criterion verification fails)
			break;
		}
	}
	if ( flag2 == TRUE && second_group_inliers.size() >= 0.8*group_inliers_num[second_group_ind] )//�������ͳ�ƹ����ڵ�,ͨ����֤ (Close to the largest statistical interior point, pass the verification)
	{
		if(second_group_inliers.size() >= group_inliers_num[second_group_ind])//��������ֹ�������ڵ��� (The maximum number of inliers the update group has ever seen)
			group_inliers_num[second_group_ind] = second_group_inliers.size();
	}
	else 
		flag2 = FALSE;
	//�ڶ����������֤ (The second group completes the verification)
	if ( flag1 == TRUE && flag2 == TRUE)
	{
		point2d * dataxy = (point2d*)malloc(sizeof(point2d)*(first_group_inliers.size() + second_group_inliers.size()));
		for ( unsigned int i = 0; i<first_group_inliers.size(); i++)
		{
			dataxy[i].x = first_group_inliers[i].x;
			dataxy[i].y = first_group_inliers[i].y;
		}
		addr = first_group_inliers.size();
		for ( unsigned int i = 0; i<second_group_inliers.size(); i++)//������������ʱһ��Ҫע��������Χ (Be sure to pay attention to the index range when concatenating two arrays)
		{
			dataxy[addr+i].x = second_group_inliers[i].x;
			dataxy[addr+i].y = second_group_inliers[i].y;
		}
//		drawEdge(img,dataxy,(first_group_inliers.size() + second_group_inliers.size()));
		info = fitEllipse(dataxy,(first_group_inliers.size() + second_group_inliers.size()), param2);
		free(dataxy); //�ͷ��ڴ� (free memory)
		//С���������Բ��Ҫ�ſ���� (Ellipses with small major and minor axes need to relax the parameters)
		if ( param[2] <= 50 )
			semimajor_errorratio = 0.25;
		else if (param[2] <= 100 )
			semimajor_errorratio = 0.15;
		else
			semimajor_errorratio = 0.1;
		if ( param[3] <= 50 )
			semiminor_errorratio = 0.25;
		else if ( param[3] <= 100)
			semiminor_errorratio = 0.15;
		else
			semiminor_errorratio = 0.1;
		if (param[2] <= 50 && param[3] <= 50 )
			iscircle_ratio = 0.75;
		else if (param[2] >= 50 && param[2] <= 100 &&  param[3] >= 50 && param[3] <= 100 )
			iscircle_ratio = 0.85;
		else
			iscircle_ratio = 0.9;
		if ( info == 1  && isEllipseEqual(param2,param,3*distance_tolerance,semimajor_errorratio,semiminor_errorratio,0.1, iscircle_ratio) )
		{
			ellipara->x = param2[0];//������Բ�����Ʒ�� (������Բ�����Ʒ��)
		    ellipara->y = param2[1];
		    ellipara->a = param2[2];
		    ellipara->b = param2[3];
		    ellipara->phi = param2[4];
		    //drawEllipse(img,param2);
			return TRUE;
		}
	}
	return FALSE;
}


//���� (enter)
//lsd�㷨���õ����߶μ���lines������line_num��return�ķ���ֵ��line_nums���߶Σ�Ϊһάdouble������lines������Ϊ8*n��ÿ8��Ϊһ��
// (The number line_num of the line segment set lines detected by the lsd algorithm, the return value of return is line_nums line segments, 
// which is a one-dimensional double-type array lines, with a length of 8*n, and each 8 is a group)
//����x1,y1,x2,y2,dx,dy,length,polarity (Store x1,y1,x2,y2,dx,dy,length,polarity)
//groups: �߶η��飬ÿ����水�ռ��ηֲ�˳��˳ʱ�������ʱ��洢���߶��������߶�������Χ��0~line_num-1
// (groups: line segment grouping, each group stores the line segment index clockwise or counterclockwise according 
// 			to the geometric distribution order, the line segment index range is 0~line_num-1)
//coverages: ÿ������ĽǶȸ��Ƿ�Χ0~2pi���������ֻ��1���߶Σ����ǽǶ�Ϊ0�����鳤�ȵ��ڷ����������
// (coverages: The angle coverage of each group is 0~2pi. If there is only one line segment in the 
// 			   group, the coverage angle is 0. The array length is equal to the number of groups.)
//angles ���Ե����ݶȷ���gradient direction, �ޱ�Ե��λNOTDEF
// (angles store the gradient direction of the edge point gradient direction, no edge point NOTDEF)
//����ֵ PairedGroupList* list ���ص��ǳ�ʼ��Բ���ϵ����飬����list->length. 
// (Return value PairedGroupList* list Returns an array of initial ellipse sets, with length list->length.)
//�мǣ����ڴ��ں��������룬����ú����ǵ��ͷ��ڴ棬���ú���freePairedSegmentList()�����ͷ�
// (Remember, the memory is applied in the function, remember to release the memory after using the function, call the function freePairedSegmentList() to release)

PairGroupList * getValidInitialEllipseSet( double * lines, int line_num, vector<vector<int>> * groups, double * coverages, image_double angles, double distance_tolerance, int specified_polarity)
{
	//���ټ��� (Accelerated Computing)
	//int* lineInliersIndex = (int*)malloc(sizeof(int)*line_num);//�����i���߶��ҵ����ڵ㣬���¼������Ϊj = length(supportInliers),��supportInliers.at(j)���Ÿ��߶ε�֧���ڵ�,û�ҵ��ڵ���߶ζ�Ӧ����Ϊ��ʼֵ-1.
	// (If the i-th line segment finds an interior point, record its index as j = length(supportInliers), that is, supportInliers.at(j) stores the support interior point of the line segment, 
	// 		and the corresponding index of the line segment where the interior point is not found is the initial value - 1.)
    //vector<vector<point2d>> supportInliers;//������Ӧ�߶ε�֧���ڵ� (Save the supported interior points of the corresponding line segment)
	//memset(lineInliersIndex,-1,sizeof(int)*line_num);//�˴�Ҫʵ��ȷʵ���У������������Գ�ʼ��Ϊ0��-1.���ڸ�������ֻ����Ϊ0.
	// (It is indeed feasible to practice here. For integers, it can be initialized to 0, -1. For floating-point numbers, it can only be 0.)

	PairGroupList * pairGroupList = NULL;
	PairGroupNode *head, *tail;
	int pairlength = 0;
	point2d pointG1s,pointG1e,pointG2s,pointG2e,g1s_ls_dir,g1e_ls_dir,g2s_ls_dir,g2e_ls_dir;
	double polarity;
	point5d ellipara;
    int groupsNum = (*groups).size();//������� (number of groups)
	double * fitMatrixes = (double*)malloc(sizeof(double)*groupsNum*36);//������Ͼ���S_{6 x 6}. ÿ���鶼��һ����Ͼ��� (Define the fit matrix S_{6 x 6}. Each group has a fit matrix)
	unsigned int * supportInliersNum = (unsigned int*)malloc(sizeof(int)*groupsNum);//���ڴ洢ÿ�������������ֵ�֧���ڵ����� (Used to store the maximum number of supported inliers that have ever occurred for each group)
	memset(fitMatrixes,0,sizeof(double)*groupsNum*36);
	memset(supportInliersNum, 0, sizeof(unsigned int)*groupsNum);//��ʼ��Ϊ0. (Initialized to 0.)
	//double distance_tolerance = max( 2.0, 0.005*min(angles->xsize,angles->ysize) ); // 0.005%*min(xsize,ysize)
    int i,j;
	int cnt_temp,ind_start,ind_end;
	bool info;
    
	//ʵ������Ͼ���Si (Instantiate the fit matrix Si)
	point2d * dataxy = (point2d*)malloc(sizeof(point2d)*line_num*2);//�����㹻���ڴ�, line_num���߶Σ�����2line_num���˵� (Apply for a large enough memory, line_num line segments, a total of 2line_num endpoints)
	for ( i = 0; i<groupsNum; i++)
	{
		cnt_temp = 0;//ǧ��ע��Ҫ��0 (Be careful to clear 0)
		for ( j = 0; j<(*groups)[i].size(); j++)
		{
			//ÿһ���߶���2���˵� (Each line segment has 2 endpoints)
			dataxy[cnt_temp].x = lines[(*groups)[i][j]*8];
			dataxy[cnt_temp++].y = lines[(*groups)[i][j]*8+1];
			dataxy[cnt_temp].x = lines[(*groups)[i][j]*8+2];
			dataxy[cnt_temp++].y = lines[(*groups)[i][j]*8+3];
		}
		calcuFitMatrix(dataxy,cnt_temp, fitMatrixes+i*36);
	}
	free(dataxy);//�ͷ��ڴ� (free memory)

	head = tail = NULL;//����ʼ��Բ���ϴ洢�������� (Store the initial set of ellipses into a linked list)
	//selection of salient elliptic hypothesis
	for ( i = 0; i<groupsNum; i++)
	{
		if(coverages[i] >= M_4_9_PI )//����ĸ��ǽǶ�>= 4pi/9 = 80��, ������Ϊ���кܴ�������ԣ���ֱ�������ȡ
		// (When the coverage angle of the group is >= 4pi/9 = 80��, we think it has great significance and can be directly fitted and extracted)
		{
			//���뼫���ж�,ֻ��ȡָ�����Ե���Բ (Add polarity judgment, only extract ellipse with specified polarity)
			if (specified_polarity == 0 || (lines[(*groups)[i][0]*8+7] == specified_polarity))
			{
				//�����Դ�ĳ�ʼ��Բ��ȡ��һ���᷵��TRUE�����û��Ҫ���ж� (The initial ellipse extraction with large significance will definitely return TRUE, so there is no need to judge again)
				info = calcEllipseParametersAndValidate(lines,line_num,groups,i,-1,(fitMatrixes+i*36),NULL,angles,distance_tolerance,supportInliersNum,&ellipara);
				if (info == FALSE) 
				{
					continue;
					error("getValidInitialEllipseSet, selection of salient ellipses failed!");//�����������֣���,��54.jpg���ָ����� (Will this happen? ? , the problem occurs when running 54.jpg)
				}
				PairGroupNode * node = (PairGroupNode*)malloc(sizeof(PairGroupNode));
				node->center.x = ellipara.x;
				node->center.y = ellipara.y;
				node->axis.x   = ellipara.a;
				node->axis.y   = ellipara.b;
				node->phi      = ellipara.phi;
				node->pairGroupInd.x = i;
				node->pairGroupInd.y = -1;//�� (without)
				if(head != NULL)
				{
					tail->next = node;
					tail = node;
				}
				else
				{
					head = tail = node;
				}
				pairlength++;
			}
		}
	}
    //selection of pair group hypothesis
	for ( i = 0; i<groupsNum-1; i++)
		for ( j = i+1; j<groupsNum; j++)
			{
				//���뼫���ж�,ֻ��ȡָ�����Ե���Բ (Add polarity judgment, only extract ellipse with specified polarity)
			   if (specified_polarity == 0 || (lines[(*groups)[i][0]*8+7] == specified_polarity))
			    {
					//group i 's polarity is the same as group j; and the number of two paired groups should be >= 3.
					if( lines[(*groups)[i][0]*8+7] == lines[(*groups)[j][0]*8+7] && ((*groups)[i].size() + (*groups)[j].size()) >= 3)
					{
						ind_start = (*groups)[i][0];//��i����ʼһ���߶����� (The first line segment index of the i-th group)
						ind_end   = (*groups)[i][(*groups)[i].size()-1];//��i������һ���߶����� (last line segment index of group i)
						pointG1s.x = lines[ind_start*8];
						pointG1s.y = lines[ind_start*8+1];
						g1s_ls_dir.x = lines[ind_start*8+4];
						g1s_ls_dir.y = lines[ind_start*8+5];
						pointG1e.x = lines[ind_end*8+2];
						pointG1e.y = lines[ind_end*8+3];
						g1e_ls_dir.x = lines[ind_end*8+4];
						g1e_ls_dir.y = lines[ind_end*8+5];
						ind_start = (*groups)[j][0];//��j����ʼһ���߶����� (The first line segment index of the jth group)
						ind_end   = (*groups)[j][(*groups)[j].size()-1];//��j������һ���߶����� (last line segment index of the jth group)
						pointG2s.x = lines[ind_start*8];
						pointG2s.y = lines[ind_start*8+1];
						g2s_ls_dir.x = lines[ind_start*8+4];
						g2s_ls_dir.y = lines[ind_start*8+5];
						pointG2e.x = lines[ind_end*8+2];
						pointG2e.y = lines[ind_end*8+3];
						g2e_ls_dir.x = lines[ind_end*8+4];
						g2e_ls_dir.y = lines[ind_end*8+5];
						polarity = lines[ind_start*8+7]; //i,j����ļ��� (The polarities of the two groups i, j)
						if(regionLimitation(pointG1s,g1s_ls_dir,pointG1e,g1e_ls_dir,pointG2s,g2s_ls_dir,pointG2e,g2e_ls_dir,polarity,-3*distance_tolerance))//���ڱ˴˵�����������
						{
							//if ( i == 2)
							//	drawPairGroup(img,lines,(*groups),i,j);

							if(calcEllipseParametersAndValidate(lines,line_num,groups,i,j,(fitMatrixes+i*36),(fitMatrixes+j*36),angles,distance_tolerance,supportInliersNum,&ellipara))//����һ�㷽��������⣬�߶ε��ڵ�֧�ֱ���
							{
								PairGroupNode * node = (PairGroupNode*)malloc(sizeof(PairGroupNode));
								node->center.x = ellipara.x;
								node->center.y = ellipara.y;
								node->axis.x   = ellipara.a;
								node->axis.y   = ellipara.b;
								node->phi      = ellipara.phi;
								node->pairGroupInd.x = i;
								node->pairGroupInd.y = -1;//�� (without)
								if(head != NULL)
								{
									tail->next = node;
									tail = node;
								}
								else
								{
									head = tail = node;
								}
								pairlength++;
							}
						}
						
					}
			   }
			}
	if(pairlength > 0)
	{
		PairGroupNode *p;
		p = head;
		pairGroupList = pairGroupListInit(pairlength);
		for( int i = 0; i<pairGroupList->length; i++)
		{
			pairGroupList->pairGroup[i].center.x = p->center.x;
			pairGroupList->pairGroup[i].center.y = p->center.y;
			pairGroupList->pairGroup[i].axis.x = p->axis.x;
			pairGroupList->pairGroup[i].axis.y = p->axis.y;
			pairGroupList->pairGroup[i].phi = p->phi;
			pairGroupList->pairGroup[i].pairGroupInd.x = p->pairGroupInd.x;//��¼���(i,j),��groups�еĵ�i����͵�j���鹹�ɵ�ƥ�����������Ч��Բ���� (Record the group pair (i, j), the matching group consisting of the i-th group and the j-th group in groups produces the valid ellipse parameter)
			pairGroupList->pairGroup[i].pairGroupInd.y = p->pairGroupInd.y;
			p = p->next;
		}
		tail->next = NULL;
		while (head != NULL)
		{
			p = head;
			head = head->next;
			free(p);
		}
	}
	//supportInliers.resize(0);
	//free(lineInliersIndex);//�ͷ��߶��ڵ������ (release the index of the point within the segment)
	free(supportInliersNum);//�ͷŴ洢�������֧���ڵ����������� (Free the array that stores the number of supported interior points for each group)
	free(fitMatrixes);//�ͷŴ洢���������Ͼ��� (release the fit matrix that stores the individual groups)
	return pairGroupList;
}


void generateEllipseCandidates( PairGroupList * pairGroupList, double distance_tolerance, double * & ellipse_candidates, int * candidates_num)
{
	if( pairGroupList->length <= 0 )//��⣬����Ҫ��1����������������ѡ (detection, at least 1 sample must be used to generate candidates)
	{
		ellipse_candidates = NULL;
		(*candidates_num) = 0;
		return;
	}
	double * centers;
	int center_num; //��Բ����(xi,yi)�ľ������� (Number of clusters at the center of the ellipse (xi,yi))
	double * phis;
	int phi_num;    //���ÿһ����Բ����(xi,yi)����б�Ƕ�phi�ľ������� (For each ellipse center (xi,yi), the number of clusters at the tilt angle phi)
	double * axises;
	int axis_num;   //���ÿһ����Բ���ĺ����(xi,yi,phi),���̰���(a,b)�ľ������� (For each ellipse center and inclination (xi, yi, phi), the number of clusters of the major and minor semi-axes (a, b))
	double * bufferXY = (double*)calloc(pairGroupList->length*2,sizeof(double));
	double * bufferPhi = (double*)calloc(pairGroupList->length,sizeof(double));
	double * bufferAB = (double*)calloc(pairGroupList->length*2,sizeof(double));
	point2i * bufferIndexes = (point2i *)calloc(pairGroupList->length,sizeof(point2i));//point[i].x��¼��i��������bufferXX�е���ʼ����λ�ã�point[i].y��¼��i��������bufferXX�еĳ���
																					   // (point[i].x records the starting index position of the i-th category in bufferXX, point[i].y records the length of the i-th category in bufferXX)
	double  * buffer2AB = (double*)calloc(pairGroupList->length*2,sizeof(double));
	point2i * buffer2Indexes = (point2i *)calloc(pairGroupList->length,sizeof(point2i));//point[i].x��¼��i��������bufferXX�е���ʼ����λ�ã�point[i].y��¼��i��������bufferXX�еĳ���
																						// (point[i].x records the starting index position of the i-th category in bufferXX, point[i].y records the length of the i-th category in bufferXX)
	int     * buffer_temp = (int*)calloc(pairGroupList->length,sizeof(int));
	int addr,addr2,info,ind;
	double dis_min,dis_temp;
	if ( bufferXY == NULL || bufferPhi == NULL || bufferAB == NULL || bufferIndexes == NULL ||
		 buffer2AB == NULL || buffer2Indexes == NULL || buffer_temp == NULL
		)
	{
		ellipse_candidates = NULL;
		(*candidates_num) = 0;
		error("generateEllipseCandidates, not enough memory");
	}
	(*candidates_num) = 0; //��ѡ��Բ��������ʼ��Ϊ0,�ǳ���Ҫ (The number of candidate ellipses, initialized to 0, very important)
	//copy
	for ( int i = 0; i<pairGroupList->length; i++)
	{
		addr = 2*i;
		bufferXY[addr] = pairGroupList->pairGroup[i].center.x;
		bufferXY[addr+1] = pairGroupList->pairGroup[i].center.y;
	}
	//cluster the ellipses' centers
	info = cluster2DPoints(bufferXY,pairGroupList->length,distance_tolerance,centers,&center_num);
	if( info == 0)
	{
		ellipse_candidates = NULL;
		(*candidates_num) = 0;
		error("generateEllipseCandidates, cluster2DPoints, error in clustering elliptic centers");
	}
	//classification,Ѱ��ÿ��������ľ������� (classification, find the cluster center to which each point belongs)
	for ( int i = 0; i<pairGroupList->length; i++)
	{
		dis_min = DBL_MAX;
		ind = -1;
		for ( int j = 0; j<center_num; j++)
		{
			addr = 2*j;
			dis_temp = (pairGroupList->pairGroup[i].center.x - centers[addr])*(pairGroupList->pairGroup[i].center.x - centers[addr]) + (pairGroupList->pairGroup[i].center.y - centers[addr+1])*(pairGroupList->pairGroup[i].center.y - centers[addr+1]);
			if(dis_temp < dis_min)
			{
				dis_min = dis_temp;
				ind = j; //record the nearest center's index
			}
		}
		buffer_temp[i] = ind; //�˴�����buffer2�����µ�i����ʼ��Բ��Ӧ��ind����Բ�������� (Here, buffer2 is used to record the i-th initial ellipse corresponding to the ind-th ellipse cluster center)
	}
	//����������˳��浽bufferXY,bufferPhi,bufferAB�У���bufferIndexes[i]���ŵ�i���������ĵ���ʼ����λ�úͳ���
	// (Store the classification results in bufferXY, bufferPhi, bufferAB in order, and bufferIndexes[i] 
	//  stores the starting index position and length of the i-th cluster center)
	memset(bufferIndexes,0,sizeof(point2i)*pairGroupList->length);
	ind = 0;//���㣬��������ʼλ�ã�����λ����ind*2,�����Ļ�ַ (Cleared, the starting position of the sample point, the index position is ind*2, the base address of the partition)
	for ( int i = 0; i<center_num; i++)
	{
		bufferIndexes[i].x = ind; 
		for ( int j = 0; j<pairGroupList->length; j++)
		{
			if ( buffer_temp[j] == i)
			{
				addr = ind*2;//�мǳ��̰�����һ��һ��索�ģ���Ҫ x 2 (Remember that the long and short semi-axes are stored in groups of inches, and you need x 2)
				addr2 = bufferIndexes[i].y*2;
				bufferPhi[ind+bufferIndexes[i].y] = pairGroupList->pairGroup[j].phi;
				bufferAB[addr+addr2] = pairGroupList->pairGroup[j].axis.x;
				bufferAB[addr+addr2+1] = pairGroupList->pairGroup[j].axis.y;
				bufferIndexes[i].y++;//��i������������Χ�ĵ�������1 (Add 1 to the number of points around the i-th cluster center)
			}
		}
		if(bufferIndexes[i].y == 0)//����������Χû�п����ĵ� (There are no close points around the cluster center)
		{
			error("generateEllipseCandidates, no XY points near to the clustering center");
		}
		ind += bufferIndexes[i].y;
	}
	//cout<<"2D cluster centers over"<<endl;
	//��ÿһ����Բ���ĵ���Χ�ĵ������Ǿ��� (Dip clustering of points around the center of each ellipse)
	//��i����Բ�������ģ����ڽ����������Χ�ǣ�bufferIndexs[i].x ~ (bufferIndex[i].x + bufferIndex[i].y-1)
	// (The i-th ellipse cluster center, the index range of its neighboring points is: bufferIndexs[i].x ~ (bufferIndex[i].x + bufferIndex[i].y-1))
	for ( int i = 0; i<center_num; i++)
	{
		

		double * phi_pointer_temp = bufferPhi+bufferIndexes[i].x;//���ָ�� (Tilt pointer)
		double * ab_pointer_temp = bufferAB+bufferIndexes[i].x*2;//���̰����ָ��,��ס x 2 (The pointer of the long and short semi-axis, remember x 2)
		info = cluster1DDatas(phi_pointer_temp, bufferIndexes[i].y, 0.0873, phis, &phi_num);//��phi����, pi/180*5 = 0.0873, 5����� (For phi clustering, pi/180*5 = 0.0873, 5�� error)
		if (info == 0) //����Ϊʲô����������centers[i]����Χ����û��������ĵ�,����bufferIndexes[i].y = 0 (I don't know why, the cluster center centers[i] may not have the closest point around it, the number bufferIndexes[i].y = 0)
		{ 
			//cout<<"generateEllipseCandidates, cluster2DPoints, error in clustering elliptic phis"<<endl;
			continue;
			//error("generateEllipseCandidates, cluster2DPoints, error in clustering elliptic phis");
		}
		//classification,Ѱ��ÿ��������ľ������� (classification, find the cluster center to which each point belongs)
		for ( int j = 0; j<bufferIndexes[i].y; j++ )
		{
			dis_min = DBL_MAX;
			ind = -1;
			for ( int k = 0; k<phi_num; k++)
			{
				dis_temp = (*(phi_pointer_temp+j)-phis[k]) * (*(phi_pointer_temp+j)-phis[k]);
				if(dis_temp < dis_min)
				{
					dis_min = dis_temp;
					ind = k;//record the nearest phi's index
				}
			}
			buffer_temp[j] = ind;
		}
		//����������˳��洢��buffer2AB�У���buffer2Indexes[j].x��Ӧ��i��phi�ľ���������ʼ�㣬buffer2Indexes[j].y��Ӧ����(����)
		// (Store the classification results in buffer2AB in order, and buffer2Indexes[j].x corresponds to the starting 
		//  point of the cluster center of the i-th phi, and buffer2Indexes[j].y corresponds to the number (length))
		memset(buffer2Indexes,0,sizeof(point2i)*bufferIndexes[i].y);
		ind = 0;
		for ( int j = 0; j<phi_num; j++)
		{
			buffer2Indexes[j].x = ind;//��ʼ�� (starting point)
			for ( int k = 0; k<bufferIndexes[i].y; k++)
			{
				if ( buffer_temp[k] == j)
				{
					addr = ind*2;
					addr2 = buffer2Indexes[j].y*2;
					buffer2AB[addr+addr2] = *(ab_pointer_temp+k*2);
					buffer2AB[addr+addr2+1] = *(ab_pointer_temp+k*2+1);
					buffer2Indexes[j].y++;//���ȼ�1 (length plus 1)
				}
			}
			ind += buffer2Indexes[j].y;
		}
		for ( int j = 0; j<phi_num; j++ )
		{
			double * ab_pointer_temp2 = buffer2AB+buffer2Indexes[j].x*2; //���̰����ָ��,��ס x 2 (The pointer of the long and short semi-axis, remember x 2)
			info = cluster2DPoints(ab_pointer_temp2, buffer2Indexes[j].y, distance_tolerance, axises, &axis_num);
			if (info == 0) //����Ϊʲô����������phi_j����Χ����û��������ĵ�,����buffer2Indexes[j].y = 0 (I don't know why, the cluster center phi_j may not have the closest point around it, the number buffer2Indexes[j].y = 0)
			{   
				//cout<<"generateEllipseCandidates, cluster2DPoints, error in clustering elliptic axises"<<endl;
				continue;
				//error("generateEllipseCandidates, cluster2DPoints, error in clustering elliptic axises");
			}
			//����ѡ��Բ��д��bufferXY,bufferPhi,bufferAB����, ��ѡ��Բ����(*candidates_num)++
			// (Rewrite the candidate ellipse into bufferXY, bufferPhi, bufferAB, the number of candidate ellipses (*candidates_num)++)
			for ( int k = 0; k<axis_num; k++)
			{
				addr = (*candidates_num)*2;
				bufferXY[addr] = centers[i*2];
				bufferXY[addr+1] = centers[i*2+1];
				bufferPhi[(*candidates_num)] = phis[j];
				bufferAB[addr] = axises[k*2];
				bufferAB[addr+1] = axises[k*2+1];
				(*candidates_num)++;
			}
			free(axises);//cluster2DPoints�ϸ�Ҫ������axises����Ҫ�ͷź����ڲ�������ڴ�
						 // (Cluster2DPoints is strictly required. After axises are used up, the memory allocated inside the function needs to be released)
		}
		free(phis);//cluster1DDatas�ϸ�Ҫ������phis����Ҫ�ͷź����ڲ�������ڴ�
				   // (Cluster1DDatas is strictly required. After using phis, you need to release the memory requested by the function.)
	}
	free(centers);//cluster2DPoints�ϸ�Ҫ������centers����Ҫ�ͷź����ڲ�������ڴ�
				  // (Cluster2DPoints is strictly required. After running out of centers, you need to release the memory requested by the function.)
	//�ͷ��ں�����ͷ����Ĳ����ڴ� (Free part of the memory allocated at the beginning of the function)
	free(buffer_temp); //�˴��ͷų����� (issue here)
	free(buffer2Indexes);
	free(buffer2AB);
	free(bufferIndexes);
	ellipse_candidates = (double*)malloc(sizeof(double)*(*candidates_num)*5);
	for ( int i = 0; i < (*candidates_num); i++ )
	{
		addr = 2*i;
		ellipse_candidates[i*5]  = bufferXY[addr];
		ellipse_candidates[i*5+1]= bufferXY[addr+1];
		ellipse_candidates[i*5+2]= bufferAB[addr];
		ellipse_candidates[i*5+3]= bufferAB[addr+1];
		ellipse_candidates[i*5+4]= bufferPhi[i];
	}
	//�ͷ��ں�����ͷ������ڴ� (Free the memory allocated at the beginning of the function)
	free(bufferAB);
	free(bufferPhi);
	free(bufferXY);
	if((*candidates_num)<= 0)
	{
		*candidates_num = 0;
		ellipse_candidates = NULL;
		//cout<<"no any candidates generated!"<<endl;
	}
}







//==========================================END=======================================================================
/**
���룺(enter)
prhs[0]: ����ĻҶ�ͼ�񣬵�ͨ������С��imgy x imgx (input grayscale image, single channel, size imgy x imgx)
prhs[1]: ��Ե��ȡѡ��1 canny; 2 sobel (edge extraction selection, 1 canny; 2 sobel)
prhs[2]: ���ָ������Բ���� (Detects the specified ellipse polarity)
�����(output)
plhs[0]: ��ѡ��Բ���(xi,yi,ai,bi,phi_i)', 5 x m (Candidate ellipse combination (xi,yi,ai,bi,phi_i)', 5 x m)
plhs[1]: ��Եͼ����С��imgy x imgx�����Ե������Ϊ edgepix_n. ��ֵ����0 ���� 255
		 (Edge map, the size is imgy x imgx, let the total number of edge points be edgepix_n. Binarization, 0 or 255)
plhs[2]: ��Ե����ݶ��������󣬴�С�� 2 x edgepix_n, (cos(theta_rad),sin(theta_rad))'...
		 (Matrix of gradient vectors of edge points of size 2 x edgepix_n, (cos(theta_rad),sin(theta_rad))'...)
plhs[3]: �߶�ͼ����С��imgy x imgx (Line segment plot, size is imgy x imgx)
*/
/*
compile��
mex generateEllipseCandidates.cpp -IF:\OpenCV\opencv2.4.9\build\include -IF:\OpenCV\opencv2.4.9\build\include\opencv -IF:\OpenCV\opencv2.4.9\build\include\opencv2 -LF:\OpenCV\opencv2.4.9\build\x64\vc11\lib -IF:\Matlab\settlein\extern\include -LF:\Matlab\settlein\extern\lib\win64\microsoft -lopencv_core249 -lopencv_highgui249 -lopencv_imgproc249 -llibmwlapack.lib
*/
//======================================MEX function==================================================================

void mexFunction( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	if(nrhs!=3) 
      mexErrMsgIdAndTxt( "MATLAB:revord:invalidNumInputs","One input required.");
    else if(nlhs > 4) 
      mexErrMsgIdAndTxt( "MATLAB:revord:maxlhs","Too many output arguments.");
	uchar * inputimg = (uchar*)mxGetData(prhs[0]);
	int imgy,imgx;
	int edge_process_select = (int)mxGetScalar(prhs[1]);//��Ե��ȡѡ��1 canny; 2 sobel (edge extraction selection, 1 canny; 2 sobel)
	int specified_polarity  = (int)mxGetScalar(prhs[2]);//1,ָ��������Բ����ҪΪ��; -1ָ������Ϊ��; 0��ʾ���ּ�����Բ�����
														// (1, the polarity of the specified ellipse to be detected must be positive; -1 specifies that the polarity is negative; 0 means that both polar ellipses are detected)
	imgy = (int)mxGetM(prhs[0]);
	imgx = (int)mxGetN(prhs[0]);
	double *data=(double*)malloc(imgy*imgx*sizeof(double));//����������е�ͼ������ת�浽һά������ (Dump the image data in the input matrix into a 1D array)
    for(int c=0;c<imgx;c++)
    {
        for(int r=0;r<imgy;r++)
        {
           data[c+r*imgx]=inputimg[r+c*imgy];              
        }    
    }
	int n;//�߶����� (Number of line segments)
	//int new_n;
	vector<vector<int>> groups;
	double * coverages;
	int * reg;
	int reg_x;
	int reg_y;
    double* out=mylsd(&n, data,imgx,imgy,&reg,&reg_x,&reg_y);
	groupLSs(out,n,reg,reg_x,reg_y,&groups);//���� (grouping)
	free(reg); //�ͷ��ڴ� (free memory)
	calcuGroupCoverage(out,n,groups,coverages);//����ÿ����ĸ��ǽǶ� (Calculate the coverage angle for each group)

    printf("The number of output arc-support line segments: %i\n",n);
	printf("The number of arc-support groups:%i\n",groups.size());
	/*int groups_t = 0;
	for (int i = 0; i<groups.size(); i++)
	{ 
		groups_t+= groups[i].size();
	}
	printf("Groups' total ls num:%i\n",groups_t);*/

	 image_double angles;
	 if(edge_process_select == 1)
		calculateGradient2(data,imgx,imgy,&angles); //version2, sobel; version 3 canny
	 else 
		 calculateGradient3(data,imgx,imgy,&angles); //version2, sobel; version 3 canny
	 PairGroupList * pairGroupList;
	 double distance_tolerance = 2;//max( 2.0, 0.005*min(angles->xsize,angles->ysize) ); // 0.005%*min(xsize,ysize)
	 double * candidates; //��ѡ��Բ (candidate ellipse)
	 double * candidates_out;//�����ѡ��Բָ�� (output candidate ellipse pointer)
	 int  candidates_num = 0;//��ѡ��Բ���� (Number of candidate ellipses)
	 //rejectShortLines(out,n,&new_n);
	 pairGroupList = getValidInitialEllipseSet(out,n,&groups,coverages,angles,distance_tolerance,specified_polarity);
	 if(pairGroupList != NULL)
	 {
		printf("The number of initial ellipses��%i \n",pairGroupList->length);
		generateEllipseCandidates(pairGroupList, distance_tolerance, candidates, &candidates_num);
		printf("The number of ellipse candidates: %i \n",candidates_num);
		
		plhs[0] = mxCreateDoubleMatrix(5,candidates_num,mxREAL);
		candidates_out = (double*)mxGetPr(plhs[0]);
		//��ѡԲ���(xi,yi,ai,bi,phi_i)', 5 x candidates_num, ���Ƶ�����candidates_out��
		// (Candidate circle combination (xi,yi,ai,bi,phi_i)', 5 x candidates_num, copied to matrix candidates_out)
		memcpy(candidates_out,candidates,sizeof(double)*5*candidates_num);

		freePairGroupList(pairGroupList);
		free(candidates);
	 }
	 else
	 {
		 printf("The number of initial ellipses��%i \n",0);
		 double *candidates_out;
		 plhs[0] = mxCreateDoubleMatrix(5,1,mxREAL);
		 candidates_out = (double*)mxGetPr(plhs[0]);
		 candidates_out[0] = candidates_out[1] = candidates_out[2] = candidates_out[3] = candidates_out[4] = 0;
	 }
	 uchar *edgeimg_out;
	 unsigned long edge_pixels_total_num = 0;//��Ե������ (total edge pixels)
	 double *gradient_vec_out;
	 plhs[1] = mxCreateNumericMatrix(imgy,imgx,mxUINT8_CLASS,mxREAL);
	 edgeimg_out = (uchar*)mxGetData(plhs[1]);
	 //����Եͼ���Ƶ�����edgeimg_out�� (Copy the edge map into the matrix edgeimg_out)
	 //���ݶ������浽����gradient_vec_out�� (Store the gradient vector in the matrix gradient_vec_out)
	 unsigned long addr,g_cnt = 0;
	 for ( int c = 0; c < imgx; c++ )
		 for ( int r = 0; r < imgy; r++)
		 {
			 addr = r*imgx+c;
			 if(angles->data[addr] == NOTDEF)
				 edgeimg_out[c*imgy+r] = 0;
			 else
			 {
				 edgeimg_out[c*imgy+r] = 255;//Ϊ��Ե�㣬��ֵΪ��ɫ (is the edge point, assigned as white)
				 //------------------------------------------------
				 edge_pixels_total_num++;
			 }
		 }
	 printf("edge pixel number: %i\n",edge_pixels_total_num);
	//����edge_pixels_total_num x 2 ������ÿһ����Ե����ݶ�����������Ϊ���ȣ�����matlab��ϰ��
	// (Apply edge_pixels_total_num x 2 to save the gradient vector of each edge point, listed first, in line with the habit of matlab)
	 plhs[2] = mxCreateDoubleMatrix(2,edge_pixels_total_num,mxREAL);
	 gradient_vec_out = (double*)mxGetPr(plhs[2]);
	  for ( int c = 0; c < imgx; c++ )
		 for ( int r = 0; r < imgy; r++)
		 {
			 addr = r*imgx+c;
			 if(angles->data[addr] != NOTDEF)
			 {
				 gradient_vec_out[g_cnt++] = cos(angles->data[addr]);
				 gradient_vec_out[g_cnt++] = sin(angles->data[addr]);
			 }
		 }
	 //---------------------------------------------------------------------
	//����߶μ���ͼ�� (Output image for line segment detection)
	if(nlhs == 4)
	{
		Mat ls_mat = Mat::zeros(imgy,imgx,CV_8UC1);
		for ( int i = 0; i<n ; i++)//draw lines
		{
		  Point2d p1(out[8*i],out[8*i+1]),p2(out[8*i+2],out[8*i+3]);
		  line(ls_mat,p1,p2,Scalar(255,0,0));
		}
		if(candidates_num > 0)//draw ellipses
		{
			for ( int i = 0; i<candidates_num; i++)
				ellipse(ls_mat,cv::Point((int)candidates_out[i*5],(int)candidates_out[i*5+1]),cv::Size(candidates_out[i*5+2],candidates_out[i*5+3]),candidates_out[i*5+4]*180/M_PI,0,360,(Scalar(255,0,0)),1);
		}
		plhs[3] = mxCreateDoubleMatrix(imgy,imgx,mxREAL);
		double * ls_img_out = (double*)mxGetPr(plhs[3]);
		//memcpy(ls_out_mat,ls_mat.data ,sizeof(unsigned char)*M*N);
		for (int i = 0; i<imgx; i++)
			for (int j = 0; j<imgy;j++)
				ls_img_out[i*imgy+j]=ls_mat.data[j*imgx+i];
	}
	//---------------------------------------------------------------------
	//�����free���ͷų��������ڲ�����ѡԲ���õ���һϵ���ڴ�
	// (The free here is to release a series of memory used in the program to generate candidate circles)
	free(data);
	free(coverages);
	free(out);
	free_image_double(angles);

}










/*
void mexFunction(int nlhs, mxArray * plhs[], int nrhs, const mxArray * prhs[])
{
	int m = mxGetM(prhs[0]);
	int n = mxGetN(prhs[0]);
	double * p = (double*)mxGetData(prhs[0]);
	int sum = 0;
	for (int c = 0; c<n; c++)
		for ( int r = 0; r<m; r++)
			sum += p[c*m+r];
	plhs[0] = mxCreateDoubleMatrix(1,1,mxREAL);
	double *pout = mxGetPr(plhs[0]);
	*pout = sum;

}
*/
