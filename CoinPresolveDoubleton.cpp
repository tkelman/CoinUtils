// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#include <stdio.h>
#include <math.h>

#include "CoinHelperFunctions.hpp"
#include "CoinPresolveMatrix.hpp"

#include "CoinPresolveEmpty.hpp"	// for DROP_COL/DROP_ROW
#include "CoinPresolveZeros.hpp"
#include "CoinPresolveFixed.hpp"
#include "CoinPresolveDoubleton.hpp"

#include "CoinPresolvePsdebug.hpp"
#include "CoinMessage.hpp"

// #define DEBUG_PRESOLVE 1
// #define PRESOLVE_SUMMARY 1

// This one saves in one go to save [] memory and deletes row
static double * presolve_duparray(const double * element, const int * index,
			   int length, int offset,int row)
{
  int n;
  length--;
  if (sizeof(double)==2*sizeof(int)) 
    n = (3*length+1)>>1;
  else
    n = 2*length;
  double * dArray = new double [n];
  int * iArray = (int *) (dArray+length);
  length++;
  int i,j=0;
  index += offset;
  element += offset;
  for (i=0;i<length;i++) {
    int iRow = index[i];
    if (iRow!=row) {
      dArray[j]=element[i];
      iArray[j++]=index[i];
    }
  }
  return dArray;
}


void compact_rep(double *elems, int *indices, CoinBigIndex *starts, const int *lengths, int n,
			const presolvehlink *link)
{
#if	PRESOLVE_SUMMARY
  printf("****COMPACTING****\n");
#endif

  // for now, just look for the first element of the list
  int i = n;
  while (link[i].pre != NO_LINK)
    i = link[i].pre;

  int j = 0;
  for (; i != n; i = link[i].suc) {
    CoinBigIndex s = starts[i];
    CoinBigIndex e = starts[i] + lengths[i];

    // because of the way link is organized, j <= s
    starts[i] = j;
    for (CoinBigIndex k = s; k < e; k++) {
      elems[j] = elems[k];
      indices[j] = indices[k];
      j++;
   }
  }
}

// returns true if ran out of memory
static bool expand_col(CoinBigIndex *mcstrt, 
		       double *colels,
		       int *hrow,
		       int *hincol,
		       presolvehlink *clink, int ncols,

		       int icolx)
{
  CoinBigIndex kcsx = mcstrt[icolx];
  CoinBigIndex kcex = kcsx + hincol[icolx];

  const int maxk = mcstrt[ncols];	// (22)

  // update col rep - need to expand the column, though.
  int nextcol = clink[icolx].suc;

  // (22)
  if (kcex + 1 < mcstrt[nextcol] || nextcol == ncols) {
    if (! (kcex + 1 < mcstrt[nextcol])) {
      // nextcol==ncols and no space - must compact
      compact_rep(colels, hrow, mcstrt, hincol, ncols, clink);

      // update vars
      kcsx = mcstrt[icolx];
      kcex = kcsx + hincol[icolx];

      if (! (kcex + 1 < mcstrt[nextcol])) {
	return (true);
      }
    }
  } else {
    //printf("EXPAND_COL\n");

    // this is not the last col 
    // fetch last non-empty col (presolve_make_memlists-1)
    int lastcol = clink[ncols].pre;
    // (clink[icolx].suc != ncols) ==> (icolx != lastcol)

    // put it directly after the last column 
    int newkcsx = mcstrt[lastcol] + hincol[lastcol];

    if (newkcsx + hincol[icolx] + 1 >= maxk) {
      compact_rep(colels, hrow, mcstrt, hincol, ncols, clink);

      // update vars
      kcsx = mcstrt[icolx];
      kcex = kcsx + hincol[icolx];

      newkcsx = mcstrt[lastcol] + hincol[lastcol];

      if (newkcsx + hincol[icolx] + 1 >= maxk) {
	return (true);
      }
      // have to adjust various induction variables
      kcsx = mcstrt[icolx];
      kcex = mcstrt[icolx] + hincol[icolx];
    }

    // move the column - 1:  copy the entries
    memcpy((void*)&hrow[newkcsx], (void*)&hrow[kcsx], hincol[icolx] * sizeof(int));
    memcpy((void*)&colels[newkcsx], (void*)&colels[kcsx], hincol[icolx] * sizeof(double));

    // move the column - 2:  update the memory-order linked list
    PRESOLVE_REMOVE_LINK(clink, icolx);
    PRESOLVE_INSERT_LINK(clink, icolx, lastcol);

    // move the column - 3:  update loop variables to maintain invariant
    mcstrt[icolx] = newkcsx;
    kcsx = newkcsx;
    kcex = newkcsx + hincol[icolx];
  }

  return (false);
}

#if 0
static bool reject_doubleton(int *mcstrt, 
			     double *colels,
			     int *hrow,
			     int *hincol,
			     double coeff_factor,
			     double max_coeff_ratio,
			     int row0, int icolx, int icoly)
{
  CoinBigIndex kcs = mcstrt[icoly];
  CoinBigIndex kce = kcs + hincol[icoly];
  CoinBigIndex kcsx = mcstrt[icolx];
  CoinBigIndex kcex = kcsx + hincol[icolx];

  for (CoinBigIndex kcoly=kcs; kcoly<kce; kcoly++) {
    int row = hrow[kcoly];

    if (row != row0) {
      // see if row appears in colx
      CoinBigIndex kcolx = presolve_find_row1(row, kcsx, kcex, hrow);

      if (kcolx<kcex) {
	// we will add these two terms
	// they they are of different magnitudes,
	// then their difference will be approximately the size of the largest.
	double orig  = fabs(colels[kcoly] * coeff_factor);
	double addin = fabs(colels[kcolx]);

	if (max_coeff_ratio * CoinMin(orig,addin) < CoinMax(orig,addin)) {
#if	DEBUG_PRESOLVE
	  printf("REJECTED %d %g %g\n", row0, orig, addin);
#endif
	  return (true);
	}

	// cancellation is bad, too
	double newval = fabs((colels[kcoly] * coeff_factor) + colels[kcolx]);

	if (max_coeff_ratio * newval < orig) {
#if	DEBUG_PRESOLVE
	  printf("REJECTED1 %d %g %g\n", row0,
		 (colels[kcoly] * coeff_factor),
		 colels[kcolx]);
#endif
	  return (true);
	}
	  
      }
    }
  }
  return (false);
}
#endif


/*
 * Substituting y away:
 *
 *	 y = (c - a x) / b
 *
 * so adjust bounds by:   c/b
 *           and x  by:  -a/b
 *
 * This affects both the row and col representations.
 *
 * mcstrt only modified if the column must be moved.
 *
 * for every row in icoly
 *	if icolx is also has an entry for row
 *		modify the icolx entry for row
 *		drop the icoly entry from row and modify the icolx entry
 *	else 
 *		add a new entry to icolx column
 *		create a new icolx entry
 *		(this may require moving the column in memory)
 *		replace icoly entry from row and replace with icolx entry
 *
 * The row and column reps are inconsistent during the routine,
 * because icolx in the column rep is updated, and the entries corresponding
 * to icolx in the row rep are updated, but nothing concerning icoly
 * in the col rep is changed.  icoly entries in the row rep are deleted,
 * and icolx entries in both reps are consistent.
 * At the end, we set the length of icoly to be zero, so the reps would
 * be consistent if the row were deleted from the row rep.
 * Both the row and icoly must be removed from both reps.
 * In the col rep, icoly will be eliminated entirely, at the end of the routine;
 * irow occurs in just two columns, one of which (icoly) is eliminated
 * entirely, the other is icolx, which is not deleted here.
 * In the row rep, irow will be eliminated entirely, but not here;
 * icoly is removed from the rows it occurs in.
 */
static bool elim_doubleton(const char *msg,
			   CoinBigIndex *mcstrt, 
			   double *rlo, double *rup,
			   double *colels,
			   int *hrow, int *hcol,
			   int *hinrow, int *hincol,
			   presolvehlink *clink, int ncols,
			   CoinBigIndex *mrstrt, double *rowels,
			   //double a, double b, double c,
			   double coeff_factor,
			   double bounds_factor,
			   int row0, int icolx, int icoly)
{
  CoinBigIndex kcs = mcstrt[icoly];
  CoinBigIndex kce = kcs + hincol[icoly];
  CoinBigIndex kcsx = mcstrt[icolx];
  CoinBigIndex kcex = kcsx + hincol[icolx];

  //printf("%s %d x=%d y=%d cf=%g bf=%g nx=%d\n", msg,
  // row0, icolx, icoly, coeff_factor, bounds_factor, hincol[icolx]);
#if	DEBUG_PRESOLVE
  printf("%s %d x=%d y=%d cf=%g bf=%g nx=%d yrows=(", msg,
	 row0, icolx, icoly, coeff_factor, bounds_factor, hincol[icolx]);
#endif
  for (CoinBigIndex kcoly=kcs; kcoly<kce; kcoly++) {
    int row = hrow[kcoly];

    // even though these values are updated, they remain consistent
    PRESOLVEASSERT(kcex == kcsx + hincol[icolx]);

    // we don't need to update the row being eliminated 
    if (row != row0/* && hinrow[row] > 0*/) {
      // see if row appears in colx
      CoinBigIndex kcolx = presolve_find_row1(row, kcsx, kcex, hrow);

      if (bounds_factor != 0.0) {
	// (1)
	if (-PRESOLVE_INF < rlo[row])
	  rlo[row] -= colels[kcoly] * bounds_factor;

	// (2)
	if (rup[row] < PRESOLVE_INF)
	  rup[row] -= colels[kcoly] * bounds_factor;
      }

#if	DEBUG_PRESOLVE
      printf("%d%s ", row, (kcolx<kcex) ? "+" : "");
#endif

      if (kcolx<kcex) {
	// before:  both x and y are in the row
	// after:   only x is in the row
	// so: number of elems in col x unchanged, and num elems in row is one less

	// update col rep - just modify coefficent
	// column y is deleted as a whole at the end of the loop
	colels[kcolx] += colels[kcoly] * coeff_factor;

	// update row rep
	// first, copy new value for col x into proper place in rowels
	CoinBigIndex k2 = presolve_find_row(icolx, mrstrt[row], mrstrt[row]+hinrow[row], hcol);
	rowels[k2] = colels[kcolx];

	// now delete col y from the row; this changes hinrow[row]
	presolve_delete_from_row(row, icoly, mrstrt, hinrow, hcol, rowels);
      } else {
	// before:  only y is in the row
	// after:   only x is in the row
	// so: number of elems in col x is one greater, but num elems in row remains same
	// update entry corresponding to icolx in row rep 
	// by just overwriting the icoly entry
	{
	  CoinBigIndex k2 = presolve_find_row(icoly, mrstrt[row], mrstrt[row]+hinrow[row], hcol);
	  hcol[k2] = icolx;
	  rowels[k2] = colels[kcoly] * coeff_factor;
	}

	{
	  bool no_mem = expand_col(mcstrt, colels, hrow, hincol, clink, ncols,
				   icolx);
	  if (no_mem)
	    return (true);

	  // have to adjust various induction variables
	  kcoly = mcstrt[icoly] + (kcoly - kcs);
	  kcs = mcstrt[icoly];			// do this for ease of debugging
	  kce = mcstrt[icoly] + hincol[icoly];
	    
	  kcolx = mcstrt[icolx] + (kcolx - kcs);	// don't really need to do this
	  kcsx = mcstrt[icolx];
	  kcex = mcstrt[icolx] + hincol[icolx];
	}

	// there is now an unused entry in the memory after the column - use it
	// mcstrt[ncols] == penultimate index of arrays hrow/colels
	hrow[kcex] = row;
	colels[kcex] = colels[kcoly] * coeff_factor;	// y factor is 0.0 
	hincol[icolx]++, kcex++;	// expand the col
      }
    }
  }

#if	DEBUG_PRESOLVE
  printf(")\n");
#endif

  // delete the whole column
  hincol[icoly] = 0;

  return (false);
}


void update_other_rep_quick(int col,
			    const int *mcstrt, const int *hrow, const double *colels,
			    const int *hincol,

			    const int *mrstrt, int *hcol, double *rowels, int *hinrow)
{
  CoinBigIndex kcs = mcstrt[col];
  CoinBigIndex kce = kcs + hincol[col];

  for (CoinBigIndex k=kcs; k<kce; ++k) {
    int row = hrow[k];
    double coeff = colels[k];

    CoinBigIndex krs = mrstrt[row];
    CoinBigIndex kre = krs + hinrow[row];

    // the "quick" refers to the assumption that there will be enough room,
    // and that col does not already occur in the row.
    hcol[kre] = col;
    rowels[kre] = coeff;
    ++hinrow[row];
  }
}    



/*
 * It is always the case that one of the variables of a doubleton
 * will be (implied) free, but neither will necessarily be a singleton.
 * Since in the case of a doubleton the number of non-zero entries
 * will never increase, though, it makes sense to always eliminate them.
 *
 * The col rep and row rep must be consistent.
 */
const CoinPresolveAction *doubleton_action::presolve(CoinPresolveMatrix *prob,
						  const CoinPresolveAction *next)
{
  double *colels	= prob->colels_;
  int *hrow		= prob->hrow_;
  CoinBigIndex *mcstrt		= prob->mcstrt_;
  int *hincol		= prob->hincol_;
  int ncols		= prob->ncols_;

  double *clo	= prob->clo_;
  double *cup	= prob->cup_;

  double *rowels	= prob->rowels_;
  int *hcol		= prob->hcol_;
  CoinBigIndex *mrstrt		= prob->mrstrt_;
  int *hinrow		= prob->hinrow_;
  int nrows		= prob->nrows_;

  double *rlo	= prob->rlo_;
  double *rup	= prob->rup_;

  presolvehlink *clink = prob->clink_;
  presolvehlink *rlink = prob->rlink_;

  const char *integerType = prob->integerType_;

  double *cost	= prob->cost_;

  int numberLook = prob->numberRowsToDo_;
  int iLook;
  int * look = prob->rowsToDo_;
  const double ztolzb	= prob->ztolzb_;

  action * actions = new action [nrows];
  int nactions = 0;

  int *zeros	= new int[ncols];
  int nzeros	= 0;

  int *fixed	= new int[ncols];
  int nfixed	= 0;

  // If rowstat exists then all do
  unsigned char *rowstat	= prob->rowstat_;
  double *acts	= prob->acts_;
  double * sol = prob->sol_;
  //  unsigned char * colstat = prob->colstat_;


#if	CHECK_CONSISTENCY
  presolve_links_ok(clink, mcstrt, hincol, ncols);
#endif

  // wasfor (int irow=0; irow<nrows; irow++)
  for (iLook=0;iLook<numberLook;iLook++) {
    int irow = look[iLook];
    if (hinrow[irow] == 2 &&
	fabs(rup[irow] - rlo[irow]) <= ZTOLDP) {
      double rhs = rlo[irow];
      CoinBigIndex krs = mrstrt[irow];
      CoinBigIndex kre = krs + hinrow[irow];
      int icolx, icoly;
      CoinBigIndex k;
      
      /* locate first column */
      for (k=krs; k<kre; k++) {
	if (hincol[hcol[k]] > 0) {
	  break;
	}
      }
      PRESOLVEASSERT(k<kre);
      if (fabs(rowels[k]) < ZTOLDP)
	continue;
      icolx = hcol[k];
      if (prob->colProhibited(icolx))
	continue;
      
      /* locate second column */
      for (k++; k<kre; k++) {
	if (hincol[hcol[k]] > 0) {
	  break;
	}
      }
      PRESOLVEASSERT(k<kre);
      if (fabs(rowels[k]) < ZTOLDP)
	continue;
      icoly = hcol[k];
      if (prob->colProhibited(icoly))
	continue;
      
      // don't bother with fixed variables
      if (!(fabs(cup[icolx] - clo[icolx]) < ZTOLDP) &&
	  !(fabs(cup[icoly] - clo[icoly]) < ZTOLDP)) {
	double coeffx, coeffy;
	/* find this row in each of the columns */
	CoinBigIndex krowx = presolve_find_row(irow, mcstrt[icolx], mcstrt[icolx] + hincol[icolx], hrow);
	CoinBigIndex krowy = presolve_find_row(irow, mcstrt[icoly], mcstrt[icoly] + hincol[icoly], hrow);

	/* don't do if both integers for now - unless a variant
	   of x=y and 0-1 variables */
	// 0 not, 1 x integer, 2 y integer, 3 both (but okay), -1 skip
	int integerStatus=0;
	if (integerType[icolx]) {
	  if (integerType[icoly]) {
	    // both integer
	    int good = 0;
	    double rhs2 = rhs;
	    double value;
	    value=colels[krowx];
	    if (value<0.0) {
	      value = - value;
	      rhs2 += 1;
	    }
	    if (cup[icolx]==1.0&&clo[icolx]==0.0&&fabs(value-1.0)<1.0e-7)
	      good =1;
	    value=colels[krowy];
	    if (value<0.0) {
	      value = - value;
	      rhs2 += 1;
	    }
	    if (cup[icoly]==1.0&&clo[icoly]==0.0&&fabs(value-1.0)<1.0e-7)
	      good  |= 2;
	    if (good==3&&fabs(rhs2-1.0)<1.0e-7)
	      integerStatus = 3;
	    else
	      integerStatus=-1;
	  } else {
	    integerStatus = 1;
	  }
	} else if (integerType[icoly]) {
	  integerStatus = 2;
	}
	if (integerStatus<0)
	  continue;
	if (integerStatus == 2) {
	  swap(icoly,icolx);
	  swap(krowy,krowx);
	}

	// HAVE TO JIB WITH ABOVE swapS
	// if x's coefficient is something like 1000, but y's only something like -1,
	// then when we postsolve, if x's is close to being out of tolerance,
	// then y is very likely to be (because y==1000x) . (55)
	// It it interesting that the number of doubletons found may depend
	// on which column is substituted away (this is true of baxter.mps).
	if (!integerStatus) {
	  if (fabs(colels[krowy]) < fabs(colels[krowx])) {
	    swap(icoly,icolx);
	    swap(krowy,krowx);
	  }
	}

#if 0
	//?????
	if (integerType[icolx] &&
	    clo[icoly] != -PRESOLVE_INF &&
	    cup[icoly] != PRESOLVE_INF) {
	  continue;
	}
#endif

	{
	  CoinBigIndex kcs = mcstrt[icoly];
	  CoinBigIndex kce = kcs + hincol[icoly];
	  for (k=kcs; k<kce; k++) {
	    if (hinrow[hrow[k]] == 1) {
	      break;
	    }
	  }
	  // let singleton rows be taken care of first
	  if (k<kce)
	    continue;
	}

	coeffx = colels[krowx];
	coeffy = colels[krowy];

	// it is possible that both x and y are singleton columns
	// that can cause problems
	if (hincol[icolx] == 1 && hincol[icoly] == 1)
	  continue;

	// BE CAUTIOUS and avoid very large relative differences
	// if this is not done in baxter, then the computed solution isn't optimal,
	// but gets it in 11995 iterations; the postsolve goes to iteration 16181.
	// with this, the solution is optimal, but takes 18825 iters; postsolve 18871.
#if 0
	if (fabs(coeffx) * max_coeff_factor <= fabs(coeffy))
	  continue;
#endif

#if 0
	if (only_zero_rhs && rhs != 0)
	  continue;

	if (reject_doubleton(mcstrt, colels, hrow, hincol,
			     -coeffx / coeffy,
			     max_coeff_ratio,
			     irow, icolx, icoly))
	  continue;
#endif

	// common equations are of the form ax + by = 0, or x + y >= lo
	{
	  action *s = &actions[nactions];	  
	  nactions++;
	  
	  s->row = irow;
	  s->icolx = icolx;
	  
	  s->clox = clo[icolx];
	  s->cupx = cup[icolx];
	  s->costx = cost[icolx];
	  
	  s->icoly = icoly;
	  s->costy = cost[icoly];
	  
	  s->rlo = rlo[irow];
	  
	  s->coeffx = coeffx;
	  
	  s->coeffy = coeffy;
	  
	  s->ncolx	= hincol[icolx];
	  
	  s->ncoly	= hincol[icoly];
	  if (s->ncoly<s->ncolx) {
	    // Take out row 
	    s->colel	= presolve_duparray(colels, hrow, hincol[icoly],
					    mcstrt[icoly],irow);
	    s->ncolx=0;
	  } else {
	    s->colel	= presolve_duparray(colels, hrow, hincol[icolx],
					    mcstrt[icolx],irow);
	    s->ncoly=0;
	  }
	}

	/*
	 * This moves the bounds information for y onto x,
	 * making y free and allowing us to substitute it away.
	 *
	 * a x + b y = c
	 * l1 <= x <= u1
	 * l2 <= y <= u2	==>
	 *
	 * l2 <= (c - a x) / b <= u2
	 * b/-a > 0 ==> (b l2 - c) / -a <= x <= (b u2 - c) / -a
	 * b/-a < 0 ==> (b u2 - c) / -a <= x <= (b l2 - c) / -a
	 */
	{
	  double lo1 = -PRESOLVE_INF;
	  double up1 = PRESOLVE_INF;
	  
	  //PRESOLVEASSERT((coeffx < 0) == (coeffy/-coeffx < 0));
	  // (coeffy/-coeffx < 0) == (coeffy<0 == coeffx<0) 
	  if (-PRESOLVE_INF < clo[icoly]) {
	    if (coeffx * coeffy < 0)
	      lo1 = (coeffy * clo[icoly] - rhs) / -coeffx;
	    else 
	      up1 = (coeffy * clo[icoly] - rhs) / -coeffx;
	  }
	  
	  if (cup[icoly] < PRESOLVE_INF) {
	    if (coeffx * coeffy < 0)
	      up1 = (coeffy * cup[icoly] - rhs) / -coeffx;
	    else 
	      lo1 = (coeffy * cup[icoly] - rhs) / -coeffx;
	  }
	  
	  // costy y = costy ((c - a x) / b) = (costy c)/b + x (costy -a)/b
	  // the effect of maxmin cancels out
	  cost[icolx] += cost[icoly] * (-coeffx / coeffy);
	  
	  prob->change_bias(cost[icoly] * rhs / coeffy);
	  
	  if (0    /*integerType[icolx]*/) {
	    abort();
	    /* no change possible for now */
#if 0
	    lo1 = trunc(lo1);
	    up1 = trunc(up1);
	    
	    /* trunc(3.5) == 3.0 */
	    /* trunc(-3.5) == -3.0 */
	    
	    /* I think this is ok */
	    if (lo1 > clo[icolx]) {
	      (clo[icolx] <= 0.0)
		clo[icolx] =  ? ilo

		clo[icolx] = ilo;
	      cup[icolx] = iup;
	    }
#endif
	  } else {
	    double lo2 = CoinMax(clo[icolx], lo1);
	    double up2 = CoinMin(cup[icolx], up1);
	    if (lo2 > up2) {
	      if (lo2 <= up2 + prob->feasibilityTolerance_) {
		// If close to integer then go there
		double nearest = floor(lo2+0.5);
		if (fabs(nearest-lo2)<2.0*prob->feasibilityTolerance_) {
		  lo2 = nearest;
		  up2 = nearest;
		} else {
		  lo2 = up2;
		}
	      } else {
		prob->status_ |= 1;
		prob->messageHandler()->message(COIN_PRESOLVE_COLINFEAS,
							 prob->messages())
							   <<icolx
							   <<lo2
							   <<up2
							   <<CoinMessageEol;
		break;
	      }
	    }
	    clo[icolx] = lo2;
	    cup[icolx] = up2;

	    if (rowstat) {
	      // update solution and basis
              int basisChoice=0;
	      int numberBasic=0;
	      double movement = 0 ;
	      if (prob->columnIsBasic(icolx))
		numberBasic++;
	      if (prob->columnIsBasic(icoly))
		numberBasic++;
	      if (prob->rowIsBasic(irow))
		numberBasic++;
              if (sol[icolx]<=lo2+ztolzb) {
		movement = lo2-sol[icolx] ;
		sol[icolx] = lo2;
		prob->setColumnStatus(icolx,CoinPrePostsolveMatrix::atLowerBound);
	      } else if (sol[icolx]>=up2-ztolzb) {
		movement = up2-sol[icolx] ;
		sol[icolx] = up2;
		prob->setColumnStatus(icolx,CoinPrePostsolveMatrix::atUpperBound);
	      } else {
		basisChoice=1;
	      }
	      if (numberBasic>1)
		prob->setColumnStatus(icolx,CoinPrePostsolveMatrix::basic);
/*
  We need to compensate if x was forced to move. Beyond that, even if x didn't
  move, we've forced y = (c-ax)/b, and that might not have been true before. So
  even if x didn't move, y may have moved. Note that the constant term c/b is
  subtracted out as the constraints are modified, so we don't include it when
  calculating movement for y.
*/
	      if (movement)
	      { CoinBigIndex k;
		for (k = mcstrt[icolx] ; k < mcstrt[icolx]+hincol[icolx] ; k++)
		{ int row = hrow[k];
		  if (hinrow[row])
		    acts[row] += movement*colels[k]; } }
	      movement = (-coeffx*sol[icolx]/coeffy)-sol[icoly] ;
	      if (movement)
	      { for (k = mcstrt[icoly] ;
		     k < mcstrt[icoly]+hincol[icoly] ;
		     k++)
		{ int row = hrow[k];
		  if (hinrow[row])
		    acts[row] += movement*colels[k]; } }
	    }
	    if (lo2 == up2)
	      fixed[nfixed++] = icolx;
	  }
	}

	// Update next set of actions
	{
	  prob->addCol(icolx);
	  int i,kcs,kce;
	  kcs = mcstrt[icoly];
	  kce = kcs + hincol[icoly];
	  for (i=kcs;i<kce;i++) {
	    int row = hrow[i];
	    prob->addRow(row);
	  }
	  kcs = mcstrt[icolx];
	  kce = kcs + hincol[icolx];
	  for (i=kcs;i<kce;i++) {
	    int row = hrow[i];
	    prob->addRow(row);
	  }
	}

	/* transfer the colx factors to coly */
	bool no_mem = elim_doubleton("ELIMD",
				     mcstrt, rlo, rup, colels,
				     hrow, hcol, hinrow, hincol,
				     clink, ncols, 
				     mrstrt, rowels,
				     -coeffx / coeffy,
				     rhs / coeffy,
				     irow, icolx, icoly);
	if (no_mem) 
	  throwCoinError("out of memory",
			 "doubleton_action::presolve");

	// now remove irow from icolx in the col rep
	// better if this were first.
	presolve_delete_from_row(icolx, irow, mcstrt, hincol, hrow, colels);

	// eliminate irow entirely from the row rep
	hinrow[irow] = 0;

	// eliminate irow entirely from the row rep
	PRESOLVE_REMOVE_LINK(rlink, irow);

	// eliminate coly entirely from the col rep
	PRESOLVE_REMOVE_LINK(clink, icoly);
	cost[icoly] = 0.0;

	rlo[irow] = 0.0;
	rup[irow] = 0.0;

	zeros[nzeros++] = icolx;	// check for zeros

	// strictly speaking, since we didn't adjust {clo,cup}[icoly]
	// or {rlo,rup}[irow], this col/row may be infeasible,
	// because the solution/activity would be 0, whereas the
	// bounds may be non-zero.
      }
      
#if 0
      presolve_links_ok(clink, mcstrt, ncols);
      presolve_links_ok(rlink, mrstrt, nrows);
      prob->consistent();
#endif
    }
  }

  if (nactions) {
#if	PRESOLVE_SUMMARY
    printf("NDOUBLETONS:  %d\n", nactions);
#endif
    action *actions1 = new action[nactions];
    CoinMemcpyN(actions, nactions, actions1);

    next = new doubleton_action(nactions, actions1, next);

    if (nzeros)
      next = drop_zero_coefficients_action::presolve(prob, zeros, nzeros, next);
    if (nfixed)
      next = remove_fixed_action::presolve(prob, fixed, nfixed, next);
  }

  delete[]zeros;
  delete[]fixed;
  deleteAction(actions,action*);

  return (next);
}


void doubleton_action::postsolve(CoinPostsolveMatrix *prob) const
{
  const action *const actions = actions_;
  const int nactions = nactions_;

  double *colels	= prob->colels_;
  int *hrow		= prob->hrow_;
  CoinBigIndex *mcstrt		= prob->mcstrt_;
  int *hincol		= prob->hincol_;
  int *link		= prob->link_;

  double *clo	= prob->clo_;
  double *cup	= prob->cup_;

  double *rlo	= prob->rlo_;
  double *rup	= prob->rup_;

  double *dcost	= prob->cost_;

  double *sol	= prob->sol_;
  double *rcosts	= prob->rcosts_;

  double *acts	= prob->acts_;
  double *rowduals = prob->rowduals_;

  unsigned char *colstat	= prob->colstat_;
  unsigned char *rowstat	= prob->rowstat_;

  const double maxmin	= prob->maxmin_;

  char *cdone	= prob->cdone_;
  char *rdone	= prob->rdone_;

  CoinBigIndex free_list = prob->free_list_;

  const double ztolzb	= prob->ztolzb_;
  const double ztoldj	= prob->ztoldj_;

  // Space for accumulating two columns
  int nrows = prob->nrows_;
  int * index1 = new int[nrows];
  double * element1 = new double[nrows];
  memset(element1,0,nrows*sizeof(double));

  for (const action *f = &actions[nactions-1]; actions<=f; f--) {
    int irow = f->row;
    double lo0 = f->clox;
    double up0 = f->cupx;


    double coeffx = f->coeffx;
    double coeffy = f->coeffy;
    int jcolx = f->icolx;
    int jcoly = f->icoly;

    // needed?
    double rhs = f->rlo;

    /* the column was in the reduced problem */
    PRESOLVEASSERT(cdone[jcolx] && rdone[irow]==DROP_ROW);
    PRESOLVEASSERT(cdone[jcoly]==DROP_COL);

    // probably don't need this
    rlo[irow] = f->rlo;
    rup[irow] = f->rlo;

    clo[jcolx] = lo0;
    cup[jcolx] = up0;

    dcost[jcolx] = f->costx;
    dcost[jcoly] = f->costy;

#if	DEBUG_PRESOLVE
    // I've forgotten what this is about
    if ((rhs < 0) == ((coeffx * sol[jcolx]) < 0) &&
	fabs(rhs - coeffx * sol[jcolx]) * 100 < rhs &&
	fabs(rhs - coeffx * sol[jcolx]) * 100 < (coeffx * sol[jcolx]))
      printf("DANGEROUS RHS??? %g %g %g\n",
	     rhs, coeffx * sol[jcolx],
	     (rhs - coeffx * sol[jcolx]));
#endif
    // this is why we want coeffx < coeffy (55)
    sol[jcoly] = (rhs - coeffx * sol[jcolx]) / coeffy;
	  
    // since this row is fixed 
    acts[irow] = rhs;

    // acts[irow] always ok, since slack is fixed
    if (rowstat)
      prob->setRowStatus(irow,CoinPrePostsolveMatrix::atLowerBound);


    // CLAIM:
    // if the new pi value is chosen to keep the reduced cost
    // of col x at its prior value, then the reduced cost of
    // col y will be 0.
    
    // also have to update row activities and bounds for rows affected by jcoly
    //
    // sol[jcolx] was found for coeffx that
    // was += colels[kcoly] * coeff_factor;
    // where coeff_factor == -coeffx / coeffy
    //
    // its contribution to activity was
    // (colels[kcolx] + colels[kcoly] * coeff_factor) * sol[jcolx]	(1)
    //
    // After adjustment, the two columns contribute:
    // colels[kcoly] * sol[jcoly] + colels[kcolx] * sol[jcolx]
    // == colels[kcoly] * ((rhs - coeffx * sol[jcolx]) / coeffy) + colels[kcolx] * sol[jcolx]
    // == colels[kcoly] * rhs/coeffy + colels[kcoly] * coeff_factor * sol[jcolx] + colels[kcolx] * sol[jcolx]
    // colels[kcoly] * rhs/coeffy + the expression (1)
    //
    // therefore, we must increase the row bounds by colels[kcoly] * rhs/coeffy,
    // which is similar to the bias
    double djy = maxmin * dcost[jcoly];
    double djx = maxmin * dcost[jcolx];
    double bounds_factor = rhs/coeffy;
    if (f->ncoly) {
      // y is shorter so was saved - need to reconstruct x
      int ncoly=f->ncoly-1; // as row taken out
      double multiplier = coeffx/coeffy;
      //printf("Current colx %d\n",jcolx);
      int * indy = (int *) (f->colel+ncoly);
      int ystart = NO_LINK;
      int nX=0;
      int i,iRow;
      for (i=0; i<ncoly; ++i) {
	int iRow = indy[i];
	double yValue = f->colel[i];
	CoinBigIndex k = free_list;
	free_list = link[free_list];

	check_free_list(free_list);
	// are these tests always true???
	
	// undo elim_doubleton(1)
	if (-PRESOLVE_INF < rlo[iRow])
	  rlo[iRow] += yValue * bounds_factor;
	
	// undo elim_doubleton(2)
	if (rup[iRow] < PRESOLVE_INF)
	  rup[iRow] += yValue * bounds_factor;
	
	acts[iRow] += yValue * bounds_factor;
	
	djy -= rowduals[iRow] * yValue;

	hrow[k] = iRow;
	PRESOLVEASSERT(rdone[hrow[k]] || hrow[k] == irow);
	colels[k] = yValue;
	link[k] = ystart;
	ystart = k;
	yValue *= multiplier;
	element1[iRow]=yValue;
	index1[nX++]=iRow;
      }
      // And coeffy
      {
	double yValue = coeffy;
	CoinBigIndex k = free_list;
	free_list = link[free_list];
	
	check_free_list(free_list);
	
	hrow[k] = irow;
	colels[k] = yValue;
	link[k] = ystart;
	ystart = k;
	yValue *= multiplier;
	element1[irow]=yValue;
	index1[nX++]=irow;
      }
      mcstrt[jcoly] = ystart;
      hincol[jcoly] = f->ncoly;
      // find the tail
      CoinBigIndex k=mcstrt[jcolx];
      CoinBigIndex last = NO_LINK;
      int numberInColumn = hincol[jcolx];
      int numberToDo=numberInColumn;
      for (i=0; i<numberToDo; ++i) {
	iRow = hrow[k];
	assert (iRow>=0&&iRow<nrows);
	double value = colels[k]+element1[iRow];
	element1[iRow]=0.0;
	if (fabs(value)>=1.0e-15) {
	  colels[k]=value;
	  last=k;
	  k = link[k];
	  if (iRow != irow) 
	    djx -= rowduals[iRow] * value;
	} else {
	  numberInColumn--;
	  // add to free list
	  int nextk = link[k];
	  assert(free_list>=0);
	  link[k]=free_list;
	  free_list=k;
	  assert (k>=0);
	  k=nextk;
	  if (last!=NO_LINK)
	    link[last]=k;
	  else
	    mcstrt[jcolx]=k;
	}
      }
      for (i=0;i<nX;i++) {
	int iRow = index1[i];
	double xValue = element1[iRow];
	element1[iRow]=0.0;
	if (fabs(xValue)>=1.0e-15) {
	  if (iRow != irow)
	    djx -= rowduals[iRow] * xValue;
	  numberInColumn++;
	  CoinBigIndex k = free_list;
	  free_list = link[free_list];
	  
	  check_free_list(free_list);
	  
	  hrow[k] = iRow;
	  PRESOLVEASSERT(rdone[hrow[k]] || hrow[k] == irow);
	  colels[k] = xValue;
	  link[last] = k;
	  last = k;
	}
      }
      link[last]=NO_LINK;
      assert(numberInColumn);
      hincol[jcolx] = numberInColumn;
    } else {
      // will use x
      int ncolx=f->ncolx-1; // as row taken out
      double multiplier = -coeffy/coeffx;
      int * indx = (int *) (f->colel+ncolx);
      //printf("Current colx %d\n",jcolx);
      // find the tail 
      CoinBigIndex k=mcstrt[jcolx];
      int nX=0;
      int i,iRow;
      for (i=0; i<hincol[jcolx]-1; ++i) {
	if (colels[k]) {
	  iRow = hrow[k];
	  index1[nX++]=iRow;
	  element1[iRow]=multiplier*colels[k];
	}
	k = link[k];
      }
      iRow = hrow[k];
      index1[nX++]=iRow;
      element1[iRow]=multiplier*colels[k];
      multiplier = - multiplier;
      link[k] = free_list;
      free_list = mcstrt[jcolx];
      int xstart = NO_LINK;
      for (i=0; i<ncolx; ++i) {
	int iRow = indx[i];
	double xValue = f->colel[i];
	//printf("x %d %d %g\n",i,indx[i],f->colel[i]);
	CoinBigIndex k = free_list;
	free_list = link[free_list];
	
	check_free_list(free_list);
	
	hrow[k] = iRow;
	PRESOLVEASSERT(rdone[hrow[k]] || hrow[k] == irow);
	colels[k] = xValue;
	link[k] = xstart;
	xstart = k;
	xValue *= multiplier;
	if (!element1[iRow]) {
	  element1[iRow]=xValue;
	  index1[nX++]=iRow;
	} else {
	  element1[iRow]+=xValue;
	}
      }
      // And coeffx
      {
	double xValue = coeffx;
	CoinBigIndex k = free_list;
	free_list = link[free_list];
	
	check_free_list(free_list);
	
	hrow[k] = irow;
	colels[k] = xValue;
	link[k] = xstart;
	xstart = k;
	xValue *= multiplier;
	if (!element1[irow]) {
	  element1[irow]=xValue;
	  index1[nX++]=irow;
	} else {
	  element1[irow]+=xValue;
	}
      }
      mcstrt[jcolx] = xstart;
      hincol[jcolx] = f->ncolx;
      int ystart = NO_LINK;
      int n=0;
      for (i=0;i<nX;i++) {
	int iRow = index1[i];
	double yValue = element1[iRow];
	element1[iRow]=0.0;
	if (fabs(yValue)>=1.0e-12) {
	  n++;
	  CoinBigIndex k = free_list;
	  free_list = link[free_list];
	  
	  check_free_list(free_list);
	  
	  hrow[k] = iRow;
	  PRESOLVEASSERT(rdone[hrow[k]] || hrow[k] == irow);
	  colels[k] = yValue;
	  link[k] = ystart;
	  ystart = k;
	}
      }
      mcstrt[jcoly] = ystart;
      assert(n);
      hincol[jcoly] = n;
      
      k = mcstrt[jcoly];
      int ny = hincol[jcoly];
      // this probably doesn't work (???)
      for (i=0; i<ny; ++i) {
	int row = hrow[k];
	double coeff = colels[k];
	k = link[k];
	
	if (row != irow) {
	  // are these tests always true???
	  
	  // undo elim_doubleton(1)
	  if (-PRESOLVE_INF < rlo[row])
	    rlo[row] += coeff * bounds_factor;
	  
	  // undo elim_doubleton(2)
	  if (rup[row] < PRESOLVE_INF)
	    rup[row] += coeff * bounds_factor;
	  
	  acts[row] += coeff * bounds_factor;
	  
	  djy -= rowduals[row] * coeff;
	}
      }
      k = mcstrt[jcolx];
      int nx = hincol[jcolx];
      
      for ( i=0; i<nx; ++i) {
	int row = hrow[k];
	double coeff = colels[k];
	k = link[k];
	
	if (row != irow) {
	  djx -= rowduals[row] * coeff;
	}
      }
    }
    assert (fabs(coeffx-f->coeffx)<1.0e-6&&fabs(coeffy-f->coeffy)<1.0e-6);
    
    
    // The only problem with keeping the reduced costs the way they were
    // was that the variable's bound may have moved, requiring it
    // to become basic.
    //printf("djs x - %g (%g), y - %g (%g)\n",djx,coeffx,djy,coeffy);
    if (colstat) {
      if (prob->columnIsBasic(jcolx) ||
	  (fabs(lo0 - sol[jcolx]) < ztolzb && rcosts[jcolx] >= -ztoldj) ||
	  (fabs(up0 - sol[jcolx]) < ztolzb && rcosts[jcolx] <= ztoldj)) {
	// colx is fine as it is - make coly basic
	
	prob->setColumnStatus(jcoly,CoinPrePostsolveMatrix::basic);
	// this is the coefficient we need to force col y's reduced cost to 0.0;
	// for example, this is obviously true if y is a singleton column
	rowduals[irow] = djy / coeffy;
	rcosts[jcolx] = djx - rowduals[irow] * coeffx;
#if 0
	if (prob->columnIsBasic(jcolx))
	  assert (fabs(rcosts[jcolx])<1.0e-5);
#endif
	rcosts[jcoly] = 0.0;
      } else {
	prob->setColumnStatus(jcolx,CoinPrePostsolveMatrix::basic);
	prob->setColumnStatusUsingValue(jcoly);
	
	// change rowduals[jcolx] enough to cancel out rcosts[jcolx]
	rowduals[irow] = djx / coeffx;
	rcosts[jcoly] = djy - rowduals[irow] * coeffy;
	rcosts[jcolx] = 0.0;
      }
    } else {
      // No status array
      // this is the coefficient we need to force col y's reduced cost to 0.0;
      // for example, this is obviously true if y is a singleton column
      rowduals[irow] = djy / coeffy;
      rcosts[jcoly] = 0.0;
    }
    
    // DEBUG CHECK
#if	DEBUG_PRESOLVE
    {
      CoinBigIndex k = mcstrt[jcolx];
      int nx = hincol[jcolx];
      double dj = maxmin * dcost[jcolx];
      
      for (int i=0; i<nx; ++i) {
	int row = hrow[k];
	double coeff = colels[k];
	k = link[k];
	
	dj -= rowduals[row] * coeff;
      }
      if (! (fabs(rcosts[jcolx] - dj) < 100*ZTOLDP))
	printf("BAD DOUBLE X DJ:  %d %d %g %g\n",
	       irow, jcolx, rcosts[jcolx], dj);
      rcosts[jcolx]=dj;
    }
    {
      CoinBigIndex k = mcstrt[jcoly];
      int ny = hincol[jcoly];
      double dj = maxmin * dcost[jcoly];
      
      for (int i=0; i<ny; ++i) {
	int row = hrow[k];
	double coeff = colels[k];
	k = link[k];
	
	dj -= rowduals[row] * coeff;
	//printf("b %d coeff %g dual %g dj %g\n",
	// row,coeff,rowduals[row],dj);
      }
      if (! (fabs(rcosts[jcoly] - dj) < 100*ZTOLDP))
	printf("BAD DOUBLE Y DJ:  %d %d %g %g\n",
	       irow, jcoly, rcosts[jcoly], dj);
      rcosts[jcoly]=dj;
      //exit(0);
    }
#endif
    
    cdone[jcoly] = DOUBLETON;
    rdone[irow] = DOUBLETON;
  }
  delete [] index1;
  delete [] element1;
  prob->free_list_ = free_list;
}


doubleton_action::~doubleton_action()
{
  for (int i=nactions_-1; i>=0; i--) {
    delete[]actions_[i].colel;
  }
  deleteAction(actions_,action*);
}



static double *doubleton_mult;
static int *doubleton_id;
void check_doubletons(const CoinPresolveAction * paction)
{
  const CoinPresolveAction * paction0 = paction;
  
  if (paction) {
    check_doubletons(paction->next);
    
    if (strcmp(paction0->name(), "doubleton_action") == 0) {
      const doubleton_action *daction = (const doubleton_action *)paction0;
      for (int i=daction->nactions_-1; i>=0; --i) {
	int icolx = daction->actions_[i].icolx;
	int icoly = daction->actions_[i].icoly;
	double coeffx = daction->actions_[i].coeffx;
	double coeffy = daction->actions_[i].coeffy;
	
	doubleton_mult[icoly] = -coeffx/coeffy;
	doubleton_id[icoly] = icolx;
      }
    }
  }
}

void check_doubletons1(const CoinPresolveAction * paction,
		       int ncols)
{
#if	DEBUG_PRESOLVE
  doubleton_mult = new double[ncols];
  doubleton_id = new int[ncols];
  int i;
  for ( i=0; i<ncols; ++i)
    doubleton_id[i] = i;
  check_doubletons(paction);
  double minmult = 1.0;
  int minid = -1;
  for ( i=0; i<ncols; ++i) {
    double mult = 1.0;
    int j = i;
    if (doubleton_id[j] != j) {
      printf("MULTS (%d):  ", j);
      while (doubleton_id[j] != j) {
	printf("%d %g, ", doubleton_id[j], doubleton_mult[j]);
	mult *= doubleton_mult[j];
	j = doubleton_id[j];
      }
      printf(" == %g\n", mult);
      if (minmult > fabs(mult)) {
	minmult = fabs(mult);
	minid = i;
      }
    }
  }
  if (minid != -1)
    printf("MIN MULT:  %d %g\n", minid, minmult);
#endif
}
