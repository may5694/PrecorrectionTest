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

#ifndef _nrutil_already_included
#define _nrutil_already_included

#ifdef __cplusplus
extern "C" {
#endif

static float sqrarg;
#define TSQR(a) ((sqrarg=(a)) == 0.0 ? 0.0 : sqrarg*sqrarg)  //used to be SOR

static float maxarg1,maxarg2;
#define FMAX(a,b) (maxarg1=(a),maxarg2=(b),(maxarg1) > (maxarg2) ?\
        (maxarg1) : (maxarg2))

#define TSIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

static int iminarg1,iminarg2;
#define IMIN(a,b) (iminarg1=(a),iminarg2=(b),(iminarg1) < (iminarg2) ?\
        (iminarg1) : (iminarg2))


float *vector(int,int);
float **matrix(int,int,int,int);
float **convert_matrix();
double *dvector();
double **dmatrix();
int *ivector();
int **imatrix();
float **submatrix();
void free_vector(float *,int,int);
void free_dvector();
void free_ivector();
void free_matrix(float **,int,int,int,int);
void free_dmatrix();
void free_imatrix();
void free_submatrix();
void free_convert_matrix();
void nrerror(char []);

#ifdef __cplusplus
};
#endif

#endif
