// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#include <stdio.h>
#include <math.h>

#include "CoinHelperFunctions.hpp"
#include "CoinPresolveMatrix.hpp"
#include "CoinPresolveZeros.hpp"


// searches the cols in checkcols for zero entries.
// creates a dropped_zero entry for each one; doesn't check for out-of-memory.
// returns number of zeros found.
static int drop_col_zeros(int ncheckcols, int *checkcols,
			   const CoinBigIndex *mcstrt, double *colels, int *hrow,
			   int *hincol,
			   dropped_zero *actions)
{
  typedef dropped_zero action;
  int nactions = 0;
  int i;

  for (i=0; i<ncheckcols; i++) {
    int col = checkcols[i];
    CoinBigIndex kcs = mcstrt[col];
    CoinBigIndex kce = mcstrt[col] + hincol[col];
    CoinBigIndex k;

    for (k=kcs; k<kce; ++k) {
      if (fabs(colels[k]) < ZTOLDP) {
	actions[nactions].col = col;
	actions[nactions].row = hrow[k];
	nactions++;

#if	DEBUG_PRESOLVE
	if (nactions == 1)
	  printf("ZEROS:  ");
	printf("(%d,%d) ", hrow[k], col);
#endif

	colels[k] = colels[kce-1];
	hrow[k]   = hrow[kce-1];
	kce--;
	hincol[col]--;

	--k;	// redo this position
      }
    }
  }

#if	DEBUG_PRESOLVE
  if (nactions)
    printf("\n");
#endif

  return (nactions);
}

// very similar to col, but without the buffer and reads zeros
// didn't bother to change the param names
void drop_row_zeros(int nzeros, const dropped_zero *zeros,
		    const CoinBigIndex *mcstrt, double *colels, int *hrow,
		    int *hincol)
{
  int i;
  for (i=0; i<nzeros; i++) {
    int col = zeros[i].row;
    CoinBigIndex kcs = mcstrt[col];
    CoinBigIndex kce = mcstrt[col] + hincol[col];
    CoinBigIndex k;

    for (k=kcs; k<kce; k++) {
      if (fabs(colels[k]) < ZTOLDP) {
	colels[k] = colels[kce-1];
	hrow[k]   = hrow[kce-1];
	kce--;
	hincol[col]--;

	--k;	// redo this position
      }
    }
  }
}


const CoinPresolveAction *drop_zero_coefficients_action::presolve(CoinPresolveMatrix *prob,
							       int *checkcols,
							       int ncheckcols,
							       const CoinPresolveAction *next)
{
  double *colels	= prob->colels_;
  int *hrow		= prob->hrow_;
  CoinBigIndex *mcstrt		= prob->mcstrt_;
  int *hincol		= prob->hincol_;
  int ncols		= prob->ncols_;
  int nrows		= prob->nrows_;

  //  int i;
  dropped_zero * zeros = new dropped_zero[ncols+nrows];

  int nzeros = drop_col_zeros(ncheckcols, checkcols,
			      mcstrt, colels, hrow, hincol,
			      zeros);

  if (nzeros == 0) {
    delete [] zeros;
    return (next);
  } else {
    double *rowels	= prob->rowels_;
    int *hcol		= prob->hcol_;
    CoinBigIndex *mrstrt		= prob->mrstrt_;
    int *hinrow		= prob->hinrow_;
    //    int nrows		= prob->nrows_;

#if	PRESOLVE_SUMMARY
    printf("NZEROS:  %d\n", nzeros);
#endif

    // make the row rep consistent
    drop_row_zeros(nzeros, zeros, mrstrt, rowels, hcol, hinrow);

    dropped_zero *zeros1 = new dropped_zero[nzeros];
    CoinDisjointCopyN(zeros, nzeros, zeros1);

    delete [] zeros;
    return (new drop_zero_coefficients_action(nzeros, zeros1, next));
  }
}


const CoinPresolveAction *drop_zero_coefficients(CoinPresolveMatrix *prob,
					      const CoinPresolveAction *next)
{
  int ncheck		= prob->ncols_;
  int *checkcols	= new int[ncheck];

  if (!prob->anyProhibited()) {
    for (int i=0; i<ncheck; i++)
      checkcols[i] = i;
  } else {
    // some prohibited
    ncheck=0;
    for (int i=0; i<prob->ncols_; i++)
      if (!prob->colProhibited(i))
	checkcols[ncheck++] = i;
  }

  const CoinPresolveAction *retval = drop_zero_coefficients_action::presolve(prob,
							 checkcols, ncheck,
							 next);
  delete[]checkcols;
  return (retval);
}

void drop_zero_coefficients_action::postsolve(CoinPostsolveMatrix *prob) const
{
  const int nzeros	= nzeros_;
  const dropped_zero *const zeros = zeros_;

  double *colels	= prob->colels_;
  int *hrow		= prob->hrow_;
  CoinBigIndex *mcstrt		= prob->mcstrt_;
  int *hincol		= prob->hincol_;
  int *link		= prob->link_;
  CoinBigIndex free_list		= prob->free_list_;

  for (const dropped_zero *z = &zeros[nzeros-1]; zeros<=z; z--) {
    int irow	= z->row;
    int jcol	= z->col;

    {
      CoinBigIndex k = free_list;
      free_list = link[free_list];
      check_free_list(free_list);

      hrow[k] = irow;
      colels[k] = 0.0;
      link[k] = mcstrt[jcol];
      mcstrt[jcol] = k;
    }
    
    hincol[jcol]++;
  }

  prob->free_list_ = free_list;
} 
