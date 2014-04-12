/*********************************************************************
This file is part of QtAppEdit.

    QtAppEdit is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    QtAppEdit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with QtAppEdit.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#include <math.h>
#include <iostream>
#include <functional>
#include "nrutil.h"

#define TOL 2.0e-5
#define ITMAX 100
#define GOLD 1.618034 
#define GLIMIT 100.0 
#define TINY 1.0e-20 
#define SHFT(a,b,c,d) (a)=(b);(b)=(c);(c)=(d); 
#define CGOLD 0.3819660 
#define ZEPS 1.0e-10 
#define EPS 3.0e-8

static int ncom;
static float *pcom, *xicom, (*nrfunc)( float []); 

static void linmin( float p[], float xi[], int n, float *fret, 
		float (*func)( float [] ) );
float brent( float ax, float bx, float cx, float (*f)( float ), float tol, float *xmin ); 
static float f1dim( float x ); 
void mnbrak( float *ax, float *bx, float *cx, float *fa, float *fb, float *fc, float (*func)( float ) ); 
float zbrent(std::function<float (float)> func, float x1, float x2, float tol);

/*****************************************************************************
*****************************************************************************/
bool powell( float p[], float **xi, int n, float ftol, int *iter, 
		float *fret, float (*func)( float [] ) )
{ 

  int i, ibig, j;
  float del, fp, fptt, t, *pt, *ptt, *xit;

  pt = vector( 1, n );
  ptt = vector( 1, n );
  xit = vector( 1, n );
  *fret=(*func)( p );

  for ( j=1; j<=n; j++ ) 
	pt[j] = p[j];

  for ( *iter=1; ;++(*iter) ) 
  { fp = (*fret);
    ibig = 0;
    del = 0.0;

    for ( i=1; i<=n; i++ ) 
	{ for ( j=1; j<=n; j++ ) 
	    xit[j] = xi[j][i];

      fptt = (*fret);
	  linmin(p,xit,n,fret,func);

      if ( fptt-(*fret) > del ) 
	  { del = fptt - (*fret);
        ibig = i;
      }
    }

    if ( 2.0 * fabs( fp - (*fret) ) <= ftol*( fabs(fp) + fabs(*fret)) + TINY ) 
	{ //printf( "%d %f %f\n", *iter, fp, *fret );
	  free_vector( xit, 1, n ); 
      free_vector( ptt, 1, n );
      free_vector( pt, 1, n );
	  
      return true;
    }

    if ( *iter == ITMAX )
      return false;

    for ( j=1; j<=n; j++ ) 
	{ ptt[j] = 2.0 * p[j] - pt[j];
      xit[j] = p[j] - pt[j];
      pt[j] = p[j];
    }

    fptt = (*func)( ptt );

    if ( fptt < fp ) 
	{ t = 2.0 * ( fp - 2.0 * (*fret) + fptt ) * TSQR( fp - (*fret) - del ) - del * TSQR( fp - fptt );

      if ( t < 0.0 ) 
	  { linmin( p, xit, n, fret, func );
	    for ( j=1; j<=n; j++ ) 
		{ xi[j][ibig] = xi[j][n];
          xi[j][n] = xit[j];
        }
      }
    }
  } 
}


static void linmin( float p[], float xi[], int n, float *fret, float (*func)( float [] ) )

/*Given an n-dimensional point p[1..n] and an n-dimensional direction xi[1..n], moves and resets p to where the function func(p) takes 
on a minimum along the direction xi from p, and replaces xi by the actual vector displacement that p was moved. Also returns as fret
 the value of func at the returned location p. This is actually all accomplished by calling the routines mnbrak and brent.*/

{ 

  int j; 
  float xx, xmin, fx, fb, fa, bx, ax; 

  ncom = n; 
  pcom = vector( 1, n ); 
  xicom = vector( 1, n ); 
  nrfunc = func; 

  for ( j=1; j<=n; j++ ) 
  { pcom[j] = p[j]; 
    xicom[j] = xi[j]; 
  } 

  ax = 0.0; 
  xx=1.0; 
  mnbrak( &ax, &xx, &bx, &fa, &fx, &fb, f1dim ); 
  *fret = brent( ax, xx, bx, f1dim, TOL, &xmin ); 

  for ( j=1; j<=n; j++ ) 
  { xi[j] *= xmin; 
    p[j] += xi[j]; 
  } 

  free_vector( xicom, 1, n ); 
  free_vector( pcom, 1, n ); 
}


static float f1dim( float x )
{ int j; 
  float f, *xt; 

  xt = vector( 1, ncom ); 

  for ( j=1; j<=ncom; j++ ) 
    xt[j] = pcom[j] + x * xicom[j] ; 

  f = (*nrfunc)( xt); 
  free_vector( xt, 1, ncom ); 

  return f; 
}


void mnbrak( float *ax, float *bx, float *cx, float *fa, float *fb, float *fc, float (*func)( float ) ) 

/*Given a function func, and given distinct initial points ax and bx, this routine searches in the downhill direction (defined by the
 function as evaluated at the initial points) and returns new points ax, bx, cx that bracket a minimum of the function. Also returned
 are the function values at the three points, fa, fb, and fc.*/

{ float ulim, u, r, q, fu, dum; 
  *fa = (*func)( *ax); 
  *fb = (*func)( *bx);

  if ( *fb > *fa ) 
  { SHFT( dum, *ax, *bx, dum ) 
    SHFT( dum, *fb, *fa, dum ) 
  } 

  *cx = (*bx) + GOLD * ( *bx - *ax ); 
  *fc = (*func)( *cx ); 
  
  while (*fb > *fc) 
  { r=(*bx-*ax)*(*fb-*fc); 
    q=(*bx-*cx)*(*fb-*fa); 
    u=(*bx)-((*bx-*cx)*q-(*bx-*ax)*r)/(2.0*TSIGN(FMAX(fabs(q-r),TINY),q-r)); 
    ulim=(*bx)+GLIMIT*(*cx-*bx); 

    if ((*bx-u)*(u-*cx) > 0.0) 
    { fu=(*func)(u); 

      if (fu < *fc) 
      { *ax=(*bx); 
        *bx=u; 
        *fa=(*fb); 
        *fb=fu; 
        return; 
      } 

      else if (fu > *fb) 
      { *cx=u; 
        *fc=fu; 
        return; 
      }

	  u=(*cx)+GOLD*(*cx-*bx); 
      fu=(*func)(u); 
    } 

	else if ((*cx-u)*(u-ulim) > 0.0) 
    { fu=(*func)(u); 

      if (fu < *fc) 
      { SHFT(*bx,*cx,u,*cx+GOLD*(*cx-*bx)) 
        SHFT(*fb,*fc,fu,(*func)(u)) 
      } 
    }     

    else if ((u-ulim)*(ulim-*cx) >= 0.0) 
    { u=ulim; 
      fu=(*func)(u); 
    } 

    else 
    { u=(*cx)+GOLD*(*cx-*bx); 
      fu=(*func)(u); 
    } 

    SHFT(*ax,*bx,*cx,u) 
    SHFT(*fa,*fb,*fc,fu) 

  } 
}


float brent( float ax, float bx, float cx, float (*f)( float ), float tol, float *xmin )

/*Given a function f, and given a bracketing triplet of abscissas ax, bx, cx (such that bx is between ax and cx, and f(bx) is less than 
both f(ax) and f(cx)), this routine isolates the minimum to a fractional precision of about tol using Brent s method. The abscissa of 
the minimum is returned as xmin, and the minimum function value is returned as brent, the returned function value.*/

{ int iter; 
  float a,b,d,etemp,fu,fv,fw,fx,p,q,r,tol1,tol2,u,v,w,x,xm; 
  float e=0.0; 

  a=(ax < cx ? ax : cx); 
  b=(ax > cx ? ax : cx); 
  x=w=v=bx; 
  fw=fv=fx=(*f)(x); 

  for (iter=1;iter<=ITMAX;iter++) 
  { xm=0.5*(a+b); 
    tol2=2.0*(tol1=tol*fabs(x)+ZEPS); 

    if (fabs(x-xm) <= (tol2-0.5*(b-a))) 
    { *xmin=x; 
      return fx; 
    } 

    if (fabs(e) > tol1) 
    { r=(x-w)*(fx-fv); 
      q=(x-v)*(fx-fw); 
      p=(x-v)*q-(x-w)*r; 
      q=2.0*(q-r);

      if (q > 0.0) 
        p = -p; 

      q=fabs(q);
      etemp=e; 
      e=d; 

      if (fabs(p) >= fabs(0.5*q*etemp) || p <= q*(a-x) || p >= q*(b-x)) 
        d=CGOLD*(e=(x >= xm ? a-x : b-x));

      else 
      { d=p/q; 
        u=x+d; 

		if (u-a < tol2 || b-u < tol2) 
          d=TSIGN(tol1,xm-x); 
      } 
    } 

    else 
    { d=CGOLD*(e=(x >= xm ? a-x : b-x)); 
    } 

    u=(fabs(d) >= tol1 ? x+d : x+TSIGN(tol1,d)); 
    fu=(*f)(u); 

    if (fu <= fx) 
    { if (u >= x) a=x; 
      else b=x; 
      SHFT(v,w,x,u) 
      SHFT(fv,fw,fx,fu) 
    } 

    else 
    { if (u < x) a=u; 
      else b=u; 

      if (fu <= fw || w == x) 
      { v=w; 
        w=u; 
        fv=fw; 
        fw=fu; 
      } 

      else if (fu <= fv || v == x || v == w) 
      { v=u; 
        fv=fu; 
      } 
    } 
  } 

  *xmin=x; 

  return fx; 
}

float zbrent(std::function<float (float)> func, float x1, float x2, float tol) {
	// Using Brent's method, find the root of the function func known to lie
	// between x1 and x2. The root, returned as zbrent, will be refined until
	// its accuracy is tol.
	if (!func)
		nrerror("Must pass a function!");

	int iter;
	float a=x1, b=x2, c=x2, d, e, min1, min2;
	float fa=func(a), fb=func(b), fc, p, q, r, s, tol1, xm;

	if ((fa > 0.0 && fb > 0.0) || (fa < 0.0 && fb < 0.0))
		nrerror("Root must be bracketed in zbrent");
	fc=fb;
	for (iter = 1; iter <= ITMAX; iter++) {
		if ((fb > 0.0 && fc > 0.0) || (fb < 0.0 && fc < 0.0)) {
			// Rename a, b, c and adjust bounding interval d
			c = a;
			fc = fa;
			e = d = b-a;
		}
		if (fabs(fc) < fabs(fb)) {
			a = b;
			b = c;
			c = a;
			fa = fb;
			fb = fc;
			fc = fa;
		}
		tol1 = 2.0 * EPS * fabs(b) + 0.5 * tol;		// Convergence check
		xm = 0.5 * (c-b);
		if (fabs(xm) <= tol1 || fb == 0.0) return b;
		if (fabs(e) >= tol1 && fabs(fa) > fabs(fb)) {
			// Attempt inverse quadratic interpolation
			s = fb / fa;
			if (a == c) {
				p = 2.0 * xm * s;
				q = 1.0 - s;
			} else {
				q = fa / fc;
				r = fb / fc;
				p = s * (2.0 * xm * q * (q-r) - (b-a) * (r - 1.0));
				q = (q - 1.0) * (r - 1.0) * (s - 1.0);
			}
			if (p > 0.0) q = -q;	// Check whether in bounds
			p = fabs(p);
			min1 = 3.0 * xm * q - fabs(tol1 * q);
			min2 = fabs(e * q);
			if (2.0 * p < (min1 < min2 ? min1 : min2)) {
				e = d;		// Accept interpolation
				d = p / q;
			} else {
				d = xm;		// Interpolation failed, use bisection
				e = d;
			}
		} else {			// Bounds decreasing too slowly, use bisection
			d = xm;
			e = d;
		}
		a = b;		// Move last best guess to a
		fa = fb;
		if (fabs(d) > tol1)		// Evaluate new trial root
			b += d;
		else
			b += TSIGN(tol1, xm);
		fb = func(b);
	}
	nrerror("Maximum number of iterations exceeded in zbrent");
	return 0.0;			// Never get here
}