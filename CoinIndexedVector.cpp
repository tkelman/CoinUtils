// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif

#include <cassert>
#include <cstdio>

#include "CoinHelperFunctions.hpp"
#include "CoinIndexedVector.hpp"
//#############################################################################

void
CoinIndexedVector::clear()
{
  if (!packedMode_) {
    if (3*nElements_<capacity_) {
      int i=0;
      if ((nElements_&1)!=0) {
	elements_[indices_[0]]=0.0;
	i=1;
      }
      for (;i<nElements_;i+=2) {
	int i0 = indices_[i];
	int i1 = indices_[i+1];
	elements_[i0]=0.0;
	elements_[i1]=0.0;
      }
    } else {
      memset(elements_,0,capacity_*sizeof(double));
    }
  } else {
    memset(elements_,0,nElements_*sizeof(double));
  }
  nElements_ = 0;
  packedMode_=false;
}

//#############################################################################

void
CoinIndexedVector::empty()
{
  delete [] indices_;
  indices_=NULL;
  if (elements_)
    delete [] (elements_-offset_);
  elements_=NULL;
  nElements_ = 0;
  capacity_=0;
  packedMode_=false;
}

//#############################################################################

CoinIndexedVector &
CoinIndexedVector::operator=(const CoinIndexedVector & rhs)
{
  if (this != &rhs) {
    clear();
    packedMode_=rhs.packedMode_;
    gutsOfSetVector(rhs.capacity_,rhs.nElements_, rhs.indices_, rhs.elements_);
  }
  return *this;
}

//#############################################################################

CoinIndexedVector &
CoinIndexedVector::operator=(const CoinPackedVectorBase & rhs)
{
  clear();
  gutsOfSetVector(rhs.getNumElements(), 
			    rhs.getIndices(), rhs.getElements());
  return *this;
}

//#############################################################################

void
CoinIndexedVector::borrowVector(int size, int numberIndices, int* inds, double* elems)
{
  empty();
  capacity_=size;
  nElements_ = numberIndices;
  indices_ = inds;  
  elements_ = elems;
  
  // whole point about borrowvector is that it is lightweight so no testing is done
}

//#############################################################################

void
CoinIndexedVector::returnVector()
{
  indices_=NULL;
  elements_=NULL;
  nElements_ = 0;
  capacity_=0;
  packedMode_=false;
}

//#############################################################################

void
CoinIndexedVector::setVector(int size, const int * inds, const double * elems)
{
  clear();
  gutsOfSetVector(size, inds, elems);
}
//#############################################################################


void 
CoinIndexedVector::setVector(int size, int numberIndices, const int * inds, const double * elems)
{
  clear();
  gutsOfSetVector(size, numberIndices, inds, elems);
}
//#############################################################################

void
CoinIndexedVector::setConstant(int size, const int * inds, double value)
{
  clear();
  gutsOfSetConstant(size, inds, value);
}

//#############################################################################

void
CoinIndexedVector::setFull(int size, const double * elems)
{
  // Clear out any values presently stored
  clear();
  
  if (size<0)
    throw CoinError("negative number of indices", "setFull", "CoinIndexedVector");
  
  reserve(size);
  nElements_ = 0;
  // elements_ array is all zero
  int i;
  for (i=0;i<size;i++) {
    int indexValue=i;
    if (fabs(elems[i])>=COIN_INDEXED_TINY_ELEMENT) {
      elements_[indexValue]=elems[i];
      indices_[nElements_++]=indexValue;
    }
  }
}
//#############################################################################

/** Access the i'th element of the full storage vector.  */
double &
CoinIndexedVector::operator[](int index) const
{
  if ( index >= capacity_ ) 
    throw CoinError("index >= capacity()", "[]", "CoinIndexedVector");
  if ( index < 0 ) 
    throw CoinError("index < 0" , "[]", "CoinIndexedVector");
  double * where = elements_ + index;
  return *where;
  
}
//#############################################################################

void
CoinIndexedVector::setElement(int index, double element)
{
  if ( index >= nElements_ ) 
    throw CoinError("index >= size()", "setElement", "CoinIndexedVector");
  if ( index < 0 ) 
    throw CoinError("index < 0" , "setElement", "CoinIndexedVector");
  elements_[indices_[index]] = element;
}

//#############################################################################

void
CoinIndexedVector::insert( int index, double element )
{
  if ( index < 0 ) 
    throw CoinError("index < 0" , "setElement", "CoinIndexedVector");
  if (index >= capacity_)
    reserve(index+1);
  if (elements_[index])
    throw CoinError("Index already exists", "insert", "CoinIndexedVector");
  indices_[nElements_++] = index;
  elements_[index] = element;
}

//#############################################################################

void
CoinIndexedVector::add( int index, double element )
{
  if ( index < 0 ) 
    throw CoinError("index < 0" , "setElement", "CoinIndexedVector");
  if (index >= capacity_)
    reserve(index+1);
  if (elements_[index]) {
    element += elements_[index];
    if (fabs(element)>= COIN_INDEXED_TINY_ELEMENT) {
      elements_[index] = element;
    } else {
      elements_[index] = 1.0e-100;
    }
  } else if (fabs(element)>= COIN_INDEXED_TINY_ELEMENT) {
    indices_[nElements_++] = index;
    assert (nElements_<=capacity_);
    elements_[index] = element;
   }
}

//#############################################################################

int
CoinIndexedVector::clean( double tolerance )
{
  int number = nElements_;
  int i;
  nElements_=0;
  assert(!packedMode_);
  for (i=0;i<number;i++) {
    int indexValue = indices_[i];
    if (fabs(elements_[indexValue])>=tolerance) {
      indices_[nElements_++]=indexValue;
    } else {
      elements_[indexValue]=0.0;
    }
  }
  return nElements_;
}

//#############################################################################
// For debug check vector is clear i.e. no elements
void CoinIndexedVector::checkClear()
{
#ifndef NDEBUG
  assert(!nElements_);
  assert(!packedMode_);
  int i;
  for (i=0;i<capacity_;i++) {
    assert(!elements_[i]);
  }
  // check mark array zeroed
  char * mark = (char *) (indices_+capacity_);
  for (i=0;i<capacity_;i++) {
    assert(!mark[i]);
  }
#else
  if(nElements_) {
    printf("%d nElements_ - checkClear\n",nElements_);
    abort();
  }
  if(packedMode_) {
    printf("packed mode when empty - checkClear\n");
    abort();
  }
  int i;
  int n=0;
  int k=-1;
  for (i=0;i<capacity_;i++) {
    if(elements_[i]) {
      n++;
      if (k<0)
	k=i;
    }
  }
  if(n) {
    printf("%d elements, first %d - checkClear\n",n,k);
    abort();
  }
#endif
}
// For debug check vector is clean i.e. elements match indices
void CoinIndexedVector::checkClean()
{
  int i;
  if (packedMode_) {
    for (i=0;i<nElements_;i++) 
      assert(elements_[i]);
    for (;i<capacity_;i++) 
      assert(!elements_[i]);
  } else {
    double * copy = new double[capacity_];
    CoinMemcpyN(elements_,capacity_,copy);
    for (i=0;i<nElements_;i++) {
      int indexValue = indices_[i];
      copy[indexValue]=0.0;
    }
    for (i=0;i<capacity_;i++) 
      assert(!copy[i]);
    delete [] copy;
  }
#ifndef NDEBUG
  // check mark array zeroed
  char * mark = (char *) (indices_+capacity_);
  for (i=0;i<capacity_;i++) {
    assert(!mark[i]);
  }
#endif
}

//#############################################################################

void
CoinIndexedVector::append(const CoinPackedVectorBase & caboose) 
{
  const int cs = caboose.getNumElements();
  
  const int * cind = caboose.getIndices();
  const double * celem = caboose.getElements();
  int maxIndex=-1;
  int i;
  for (i=0;i<cs;i++) {
    int indexValue = cind[i];
    if (indexValue<0)
      throw CoinError("negative index", "append", "CoinIndexedVector");
    if (maxIndex<indexValue)
      maxIndex = indexValue;
  }
  reserve(maxIndex+1);
  bool needClean=false;
  int numberDuplicates=0;
  for (i=0;i<cs;i++) {
    int indexValue=cind[i];
    if (elements_[indexValue]) {
      numberDuplicates++;
      elements_[indexValue] += celem[i] ;
      if (fabs(elements_[indexValue])<COIN_INDEXED_TINY_ELEMENT) 
	needClean=true; // need to go through again
    } else {
      if (fabs(celem[i])>=COIN_INDEXED_TINY_ELEMENT) {
	elements_[indexValue]=celem[i];
	indices_[nElements_++]=indexValue;
      }
    }
  }
  if (needClean) {
    // go through again
    int size=nElements_;
    nElements_=0;
    for (i=0;i<size;i++) {
      int indexValue=indices_[i];
      double value=elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
  }
  if (numberDuplicates)
    throw CoinError("duplicate index", "append", "CoinIndexedVector");
}

//#############################################################################

void
CoinIndexedVector::swap(int i, int j) 
{
  if ( i >= nElements_ ) 
    throw CoinError("index i >= size()","swap","CoinIndexedVector");
  if ( i < 0 ) 
    throw CoinError("index i < 0" ,"swap","CoinIndexedVector");
  if ( j >= nElements_ ) 
    throw CoinError("index j >= size()","swap","CoinIndexedVector");
  if ( j < 0 ) 
    throw CoinError("index j < 0" ,"swap","CoinIndexedVector");
  
  // Swap positions i and j of the
  // indices array
  
  int isave = indices_[i];
  indices_[i] = indices_[j];
  indices_[j] = isave;
}

//#############################################################################

void
CoinIndexedVector::truncate( int n ) 
{
  reserve(n);
}

//#############################################################################

void
CoinIndexedVector::operator+=(double value) 
{
  int i,indexValue;
  for (i=0;i<nElements_;i++) {
    indexValue = indices_[i];
    elements_[indexValue] += value;
  }
}

//-----------------------------------------------------------------------------

void
CoinIndexedVector::operator-=(double value) 
{
  int i,indexValue;
  for (i=0;i<nElements_;i++) {
    indexValue = indices_[i];
    elements_[indexValue] -= value;
  }
}

//-----------------------------------------------------------------------------

void
CoinIndexedVector::operator*=(double value) 
{
  int i,indexValue;
  for (i=0;i<nElements_;i++) {
    indexValue = indices_[i];
    elements_[indexValue] *= value;
  }
}

//-----------------------------------------------------------------------------

void
CoinIndexedVector::operator/=(double value) 
{
  int i,indexValue;
  for (i=0;i<nElements_;i++) {
    indexValue = indices_[i];
    elements_[indexValue] /= value;
  }
}
//#############################################################################

void
CoinIndexedVector::reserve(int n) 
{
  int i;
  // don't make allocated space smaller but do take off values
  if ( n < capacity_ ) {
    if (n<0) 
      throw CoinError("negative capacity", "reserve", "CoinIndexedVector");
    
    int nNew=0;
    for (i=0;i<nElements_;i++) {
      int indexValue=indices_[i];
      if (indexValue<n) {
        indices_[nNew++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
    nElements_=nNew;
  } else if (n>capacity_) {
    
    // save pointers to existing data
    int * tempIndices = indices_;
    double * tempElements = elements_;
    double * delTemp = elements_-offset_;
    
    // allocate new space
    int nPlus;
    if (sizeof(int)==4*sizeof(char))
      nPlus=(n+3)>>2;
    else
      nPlus=(n+7)>>4;
    indices_ = new int [n+nPlus];
    memset(indices_+n,0,nPlus*sizeof(int));
    // align on 64 byte boundary
    double * temp = new double [n+7];
    offset_ = 0;
#ifndef __64BIT__
    int xx = (int) temp;
    int iBottom = xx & 63;
    if (iBottom)
      offset_ = (64-iBottom)>>3;
#else
    long xx = (long) temp;
    long iBottom = xx & 63;
    if (iBottom)
      offset_ = (64-iBottom)>>3;
#endif
    elements_ = temp + offset_;;
    
    // copy data to new space
    // and zero out part of array
    if (nElements_ > 0) {
      CoinMemcpyN(tempIndices, nElements_, indices_);
      CoinMemcpyN(tempElements, capacity_, elements_);
      CoinZeroN(elements_+capacity_,n-capacity_);
    } else {
      CoinZeroN(elements_,n);
    }
    capacity_ = n;
    
    // free old data
    if (tempElements)
      delete [] delTemp;
    delete [] tempIndices;
  }
}

//#############################################################################

CoinIndexedVector::CoinIndexedVector () :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(int size,
				     const int * inds, const double * elems)  :
  indices_(NULL),
  elements_(NULL),
  nElements_(0),
  capacity_(0),
  offset_(0),
  packedMode_(false)
{
  gutsOfSetVector(size, inds, elems);
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(int size,
  const int * inds, double value) :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{
gutsOfSetConstant(size, inds, value);
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(int size, const double * element) :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{
  setFull(size, element);
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(const CoinPackedVectorBase & rhs) :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{  
  gutsOfSetVector(rhs.getNumElements(), 
			    rhs.getIndices(), rhs.getElements());
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(const CoinIndexedVector & rhs) :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{  
  gutsOfSetVector(rhs.capacity_,rhs.nElements_, rhs.indices_, rhs.elements_);
}

//-----------------------------------------------------------------------------

CoinIndexedVector::CoinIndexedVector(const CoinIndexedVector * rhs) :
indices_(NULL),
elements_(NULL),
nElements_(0),
capacity_(0),
offset_(0),
packedMode_(false)
{  
  gutsOfSetVector(rhs->capacity_,rhs->nElements_, rhs->indices_, rhs->elements_);
}

//-----------------------------------------------------------------------------

CoinIndexedVector::~CoinIndexedVector ()
{
  delete [] indices_;
  if (elements_)
    delete [] (elements_-offset_);
}
//#############################################################################
//#############################################################################

/// Return the sum of two indexed vectors
CoinIndexedVector 
CoinIndexedVector::operator+(
                            const CoinIndexedVector& op2)
{
  int i;
  int nElements=nElements_;
  int capacity = CoinMax(capacity_,op2.capacity_);
  CoinIndexedVector newOne(*this);
  newOne.reserve(capacity);
  bool needClean=false;
  // new one now can hold everything so just modify old and add new
  for (i=0;i<op2.nElements_;i++) {
    int indexValue=op2.indices_[i];
    double value=op2.elements_[indexValue];
    double oldValue=elements_[indexValue];
    if (!oldValue) {
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.elements_[indexValue]=value;
	newOne.indices_[nElements++]=indexValue;
      }
    } else {
      value += oldValue;
      newOne.elements_[indexValue]=value;
      if (fabs(value)<COIN_INDEXED_TINY_ELEMENT) {
	needClean=true;
      }
    }
  }
  newOne.nElements_=nElements;
  if (needClean) {
    // go through again
    nElements_=0;
    for (i=0;i<nElements;i++) {
      int indexValue=newOne.indices_[i];
      double value=newOne.elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.indices_[nElements_++]=indexValue;
      } else {
        newOne.elements_[indexValue]=0.0;
      }
    }
  }
  return newOne;
}

/// Return the difference of two indexed vectors
CoinIndexedVector 
CoinIndexedVector::operator-(
                            const CoinIndexedVector& op2)
{
  int i;
  int nElements=nElements_;
  int capacity = CoinMax(capacity_,op2.capacity_);
  CoinIndexedVector newOne(*this);
  newOne.reserve(capacity);
  bool needClean=false;
  // new one now can hold everything so just modify old and add new
  for (i=0;i<op2.nElements_;i++) {
    int indexValue=op2.indices_[i];
    double value=op2.elements_[indexValue];
    double oldValue=elements_[indexValue];
    if (!oldValue) {
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.elements_[indexValue]=-value;
	newOne.indices_[nElements++]=indexValue;
      }
    } else {
      value = oldValue-value;
      newOne.elements_[indexValue]=value;
      if (fabs(value)<COIN_INDEXED_TINY_ELEMENT) {
	needClean=true;
      }
    }
  }
  newOne.nElements_=nElements;
  if (needClean) {
    // go through again
    nElements_=0;
    for (i=0;i<nElements;i++) {
      int indexValue=newOne.indices_[i];
      double value=newOne.elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.indices_[nElements_++]=indexValue;
      } else {
        newOne.elements_[indexValue]=0.0;
      }
    }
  }
  return newOne;
}

/// Return the element-wise product of two indexed vectors
CoinIndexedVector 
CoinIndexedVector::operator*(
                            const CoinIndexedVector& op2)
{
  int i;
  int nElements=nElements_;
  int capacity = CoinMax(capacity_,op2.capacity_);
  CoinIndexedVector newOne(*this);
  newOne.reserve(capacity);
  bool needClean=false;
  // new one now can hold everything so just modify old and add new
  for (i=0;i<op2.nElements_;i++) {
    int indexValue=op2.indices_[i];
    double value=op2.elements_[indexValue];
    double oldValue=elements_[indexValue];
    if (oldValue) {
      value *= oldValue;
      newOne.elements_[indexValue]=value;
      if (fabs(value)<COIN_INDEXED_TINY_ELEMENT) {
	needClean=true;
      }
    }
  }

// I don't see why this is necessary. Multiplication cannot add new values.
//newOne.nElements_=nElements;
  assert(newOne.nElements_ == nElements) ;

  if (needClean) {
    // go through again
    nElements_=0;
    for (i=0;i<nElements;i++) {
      int indexValue=newOne.indices_[i];
      double value=newOne.elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.indices_[nElements_++]=indexValue;
      } else {
        newOne.elements_[indexValue]=0.0;
      }
    }
  }
  return newOne;
}

/// Return the element-wise ratio of two indexed vectors
CoinIndexedVector 
CoinIndexedVector::operator/ (const CoinIndexedVector& op2) 
{
  // I am treating 0.0/0.0 as 0.0
  int i;
  int nElements=nElements_;
  int capacity = CoinMax(capacity_,op2.capacity_);
  CoinIndexedVector newOne(*this);
  newOne.reserve(capacity);
  bool needClean=false;
  // new one now can hold everything so just modify old and add new
  for (i=0;i<op2.nElements_;i++) {
    int indexValue=op2.indices_[i];
    double value=op2.elements_[indexValue];
    double oldValue=elements_[indexValue];
    if (oldValue) {
      if (!value)
        throw CoinError("zero divisor", "/", "CoinIndexedVector");
      value = oldValue/value;
      newOne.elements_[indexValue]=value;
      if (fabs(value)<COIN_INDEXED_TINY_ELEMENT) {
	needClean=true;
      }
    }
  }

// I don't see why this is necessary. Division can only modify existing.
//newOne.nElements_=nElements;
  assert(newOne.nElements_ == nElements) ;

  if (needClean) {
    // go through again
    nElements_=0;
    for (i=0;i<nElements;i++) {
      int indexValue=newOne.indices_[i];
      double value=newOne.elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	newOne.indices_[nElements_++]=indexValue;
      } else {
        newOne.elements_[indexValue]=0.0;
      }
    }
  }
  return newOne;
}
//#############################################################################
void 
CoinIndexedVector::sortDecrIndex()
{ 
  // Should replace with std sort
  double * elements = new double [nElements_];
  memset (elements,0,nElements_*sizeof(double));
  CoinSort_2(indices_, indices_ + nElements_, elements,
	     CoinFirstGreater_2<int, double>());
  delete [] elements;
}

void 
CoinIndexedVector::sortIncrElement()
{ 
  double * elements = new double [nElements_];
  int i;
  for (i=0;i<nElements_;i++) 
    elements[i] = elements_[indices_[i]];
  CoinSort_2(elements, elements + nElements_, indices_,
    CoinFirstLess_2<double, int>());
  delete [] elements;
}

void 
CoinIndexedVector::sortDecrElement()
{ 
  double * elements = new double [nElements_];
  int i;
  for (i=0;i<nElements_;i++) 
    elements[i] = elements_[indices_[i]];
  CoinSort_2(elements, elements + nElements_, indices_,
    CoinFirstGreater_2<double, int>());
  delete [] elements;
}
//#############################################################################

void
CoinIndexedVector::gutsOfSetVector(int size,
				   const int * inds, const double * elems)
{
  if (size<0)
    throw CoinError("negative number of indices", "setVector", "CoinIndexedVector");
  assert(!packedMode_);  
  // find largest
  int i;
  int maxIndex=-1;
  for (i=0;i<size;i++) {
    int indexValue = inds[i];
    if (indexValue<0)
      throw CoinError("negative index", "setVector", "CoinIndexedVector");
    if (maxIndex<indexValue)
      maxIndex = indexValue;
  }
  reserve(maxIndex+1);
  nElements_ = 0;
  // elements_ array is all zero
  bool needClean=false;
  int numberDuplicates=0;
  for (i=0;i<size;i++) {
    int indexValue=inds[i];
    if (elements_[indexValue] == 0)
    {
      if (fabs(elems[i])>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
	elements_[indexValue]=elems[i];
      }
    }
    else
    {
      numberDuplicates++;
      elements_[indexValue] += elems[i] ;
      if (fabs(elements_[indexValue])<COIN_INDEXED_TINY_ELEMENT) 
	needClean=true; // need to go through again
    }
  }
  if (needClean) {
    // go through again
    size=nElements_;
    nElements_=0;
    for (i=0;i<size;i++) {
      int indexValue=indices_[i];
      double value=elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
  }
  if (numberDuplicates)
    throw CoinError("duplicate index", "setVector", "CoinIndexedVector");
}

//#############################################################################

void
CoinIndexedVector::gutsOfSetVector(int size, int numberIndices, 
				   const int * inds, const double * elems)
{
  assert(!packedMode_);  
  
  int i;
  reserve(size);
  if (numberIndices<0)
    throw CoinError("negative number of indices", "setVector", "CoinIndexedVector");
  nElements_ = 0;
  // elements_ array is all zero
  bool needClean=false;
  int numberDuplicates=0;
  for (i=0;i<numberIndices;i++) {
    int indexValue=inds[i];
    if (indexValue<0) 
      throw CoinError("negative index", "setVector", "CoinIndexedVector");
    else if (indexValue>=size) 
      throw CoinError("too large an index", "setVector", "CoinIndexedVector");
    if (elements_[indexValue]) {
      numberDuplicates++;
      elements_[indexValue] += elems[indexValue] ;
      if (fabs(elements_[indexValue])<COIN_INDEXED_TINY_ELEMENT) 
	needClean=true; // need to go through again
    } else {
      if (fabs(elems[indexValue])>=COIN_INDEXED_TINY_ELEMENT) {
	elements_[indexValue]=elems[indexValue];
	indices_[nElements_++]=indexValue;
      }
    }
  }
  if (needClean) {
    // go through again
    size=nElements_;
    nElements_=0;
    for (i=0;i<size;i++) {
      int indexValue=indices_[i];
      double value=elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
  }
  if (numberDuplicates)
    throw CoinError("duplicate index", "setVector", "CoinIndexedVector");
}

//-----------------------------------------------------------------------------

void
CoinIndexedVector::gutsOfSetConstant(int size,
				     const int * inds, double value)
{

  assert(!packedMode_);  
  if (size<0)
    throw CoinError("negative number of indices", "setConstant", "CoinIndexedVector");
  
  // find largest
  int i;
  int maxIndex=-1;
  for (i=0;i<size;i++) {
    int indexValue = inds[i];
    if (indexValue<0)
      throw CoinError("negative index", "setConstant", "CoinIndexedVector");
    if (maxIndex<indexValue)
      maxIndex = indexValue;
  }
  
  reserve(maxIndex+1);
  nElements_ = 0;
  int numberDuplicates=0;
  // elements_ array is all zero
  bool needClean=false;
  for (i=0;i<size;i++) {
    int indexValue=inds[i];
    if (elements_[indexValue] == 0)
    {
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	elements_[indexValue] += value;
	indices_[nElements_++]=indexValue;
      }
    }
    else
    {
      numberDuplicates++;
      elements_[indexValue] += value ;
      if (fabs(elements_[indexValue])<COIN_INDEXED_TINY_ELEMENT) 
	needClean=true; // need to go through again
    }
  }
  if (needClean) {
    // go through again
    size=nElements_;
    nElements_=0;
    for (i=0;i<size;i++) {
      int indexValue=indices_[i];
      double value=elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
  }
  if (numberDuplicates)
    throw CoinError("duplicate index", "setConstant", "CoinIndexedVector");
}

//#############################################################################
// Append a CoinIndexedVector to the end
void 
CoinIndexedVector::append(const CoinIndexedVector & caboose)
{
  const int cs = caboose.getNumElements();
  
  const int * cind = caboose.getIndices();
  const double * celem = caboose.denseVector();
  int maxIndex=-1;
  int i;
  for (i=0;i<cs;i++) {
    int indexValue = cind[i];
    if (indexValue<0)
      throw CoinError("negative index", "append", "CoinIndexedVector");
    if (maxIndex<indexValue)
      maxIndex = indexValue;
  }
  reserve(maxIndex+1);
  bool needClean=false;
  int numberDuplicates=0;
  for (i=0;i<cs;i++) {
    int indexValue=cind[i];
    if (elements_[indexValue]) {
      numberDuplicates++;
      elements_[indexValue] += celem[indexValue] ;
      if (fabs(elements_[indexValue])<COIN_INDEXED_TINY_ELEMENT) 
	needClean=true; // need to go through again
    } else {
      if (fabs(celem[indexValue])>=COIN_INDEXED_TINY_ELEMENT) {
	elements_[indexValue]=celem[indexValue];
	indices_[nElements_++]=indexValue;
      }
    }
  }
  if (needClean) {
    // go through again
    int size=nElements_;
    nElements_=0;
    for (i=0;i<size;i++) {
      int indexValue=indices_[i];
      double value=elements_[indexValue];
      if (fabs(value)>=COIN_INDEXED_TINY_ELEMENT) {
	indices_[nElements_++]=indexValue;
      } else {
        elements_[indexValue]=0.0;
      }
    }
  }
  if (numberDuplicates)
    throw CoinError("duplicate index", "append", "CoinIndexedVector");
}

/* Equal. Returns true if vectors have same length and corresponding
   element of each vector is equal. */
bool 
CoinIndexedVector::operator==(const CoinPackedVectorBase & rhs) const
{
  const int cs = rhs.getNumElements();
  
  const int * cind = rhs.getIndices();
  const double * celem = rhs.getElements();
  if (nElements_!=cs)
    return false;
  int i;
  bool okay=true;
  for (i=0;i<cs;i++) {
    int iRow = cind[i];
    if (celem[i]!=elements_[iRow]) {
      okay=false;
      break;
    }
  }
  return okay;
}
// Not equal
bool 
CoinIndexedVector::operator!=(const CoinPackedVectorBase & rhs) const
{
  const int cs = rhs.getNumElements();
  
  const int * cind = rhs.getIndices();
  const double * celem = rhs.getElements();
  if (nElements_!=cs)
    return true;
  int i;
  bool okay=false;
  for (i=0;i<cs;i++) {
    int iRow = cind[i];
    if (celem[i]!=elements_[iRow]) {
      okay=true;
      break;
    }
  }
  return okay;
}
/* Equal. Returns true if vectors have same length and corresponding
   element of each vector is equal. */
bool 
CoinIndexedVector::operator==(const CoinIndexedVector & rhs) const
{
  const int cs = rhs.nElements_;
  
  const int * cind = rhs.indices_;
  const double * celem = rhs.elements_;
  if (nElements_!=cs)
    return false;
  int i;
  bool okay=true;
  for (i=0;i<cs;i++) {
    int iRow = cind[i];
    if (celem[iRow]!=elements_[iRow]) {
      okay=false;
      break;
    }
  }
  return okay;
}
/// Not equal
bool 
CoinIndexedVector::operator!=(const CoinIndexedVector & rhs) const
{
  const int cs = rhs.nElements_;
  
  const int * cind = rhs.indices_;
  const double * celem = rhs.elements_;
  if (nElements_!=cs)
    return true;
  int i;
  bool okay=false;
  for (i=0;i<cs;i++) {
    int iRow = cind[i];
    if (celem[iRow]!=elements_[iRow]) {
      okay=true;
      break;
    }
  }
  return okay;
}
// Get value of maximum index
int 
CoinIndexedVector::getMaxIndex() const
{
  int maxIndex = INT_MIN;
  int i;
  for (i=0;i<nElements_;i++)
    maxIndex = CoinMax(maxIndex,indices_[i]);
  return maxIndex;
}
// Get value of minimum index
int 
CoinIndexedVector::getMinIndex() const
{
  int minIndex = INT_MAX;
  int i;
  for (i=0;i<nElements_;i++)
    minIndex = min(minIndex,indices_[i]);
  return minIndex;
}
// Scan dense region and set up indices
int
CoinIndexedVector::scan()
{
  nElements_=0;
  return scan(0,capacity_);
}
// Scan dense region from start to < end and set up indices
int
CoinIndexedVector::scan(int start, int end)
{
  assert(!packedMode_);
  end = min(end,capacity_);
  start = CoinMax(start,0);
  int i;
  int number = 0;
  int * indices = indices_+nElements_;
  for (i=start;i<end;i++) 
    if (elements_[i])
      indices[number++] = i;
  nElements_ += number;
  return number;
}
// Scan dense region and set up indices with tolerance
int
CoinIndexedVector::scan(double tolerance)
{
  nElements_=0;
  return scan(0,capacity_,tolerance);
}
// Scan dense region from start to < end and set up indices with tolerance
int
CoinIndexedVector::scan(int start, int end, double tolerance)
{
  assert(!packedMode_);
  end = min(end,capacity_);
  start = CoinMax(start,0);
  int i;
  int number = 0;
  int * indices = indices_+nElements_;
  for (i=start;i<end;i++) {
    double value = elements_[i];
    if (value) {
      if (fabs(value)>=tolerance) 
	indices[number++] = i;
      else
	elements_[i]=0.0;
    }
  }
  nElements_ += number;
  return number;
}
// These pack down
int
CoinIndexedVector::cleanAndPack( double tolerance )
{
  int number = nElements_;
  int i;
  nElements_=0;
  assert(!packedMode_);
  for (i=0;i<number;i++) {
    int indexValue = indices_[i];
    double value = elements_[indexValue];
    elements_[indexValue]=0.0;
    if (fabs(value)>=tolerance) {
      elements_[nElements_]=value;
      indices_[nElements_++]=indexValue;
    }
  }
  packedMode_=true;
  return nElements_;
}
// These pack down
int
CoinIndexedVector::cleanAndPackSafe( double tolerance )
{
  int number = nElements_;
  if (number) {
    int i;
    nElements_=0;
    assert(!packedMode_);
    double * temp=NULL;
    bool gotMemory;
    if (number*3<capacity_-3-9999999) {
      // can find room without new
      gotMemory=false;
      // But may need to align on 8 byte boundary
      char * tempC = (char *) (indices_+number);
#ifndef __64BIT__
      int xx = (int) tempC;
      int iBottom = xx & 7;
#else
      long xx = (long) tempC;
      long iBottom = xx & 7;
#endif
      if (iBottom)
	tempC += 8-iBottom;
      temp = (double *) tempC;
#ifndef __64BIT__
      xx = (int) temp;
#else
      xx = (long) temp;
#endif
      iBottom = xx & 7;
      assert(!iBottom);
    } else {
      // might be better to do complete scan
      gotMemory=true;
      temp = new double[number];
    }
    for (i=0;i<number;i++) {
      int indexValue = indices_[i];
      double value = elements_[indexValue];
      elements_[indexValue]=0.0;
      if (fabs(value)>=tolerance) {
	temp[nElements_]=value;
	indices_[nElements_++]=indexValue;
      }
    }
    memcpy(elements_,temp,nElements_*sizeof(double));
    if (gotMemory)
      delete [] temp;
    packedMode_=true;
  }
  return nElements_;
}
// Scan dense region and set up indices
int
CoinIndexedVector::scanAndPack()
{
  nElements_=0;
  return scanAndPack(0,capacity_);
}
// Scan dense region from start to < end and set up indices
int
CoinIndexedVector::scanAndPack(int start, int end)
{
  assert(!packedMode_);
  end = min(end,capacity_);
  start = CoinMax(start,0);
  int i;
  int number = 0;
  int * indices = indices_+nElements_;
  for (i=start;i<end;i++) {
    double value = elements_[i];
    elements_[i]=0.0;
    if (value) {
      elements_[number]=value;
      indices[number++] = i;
    }
  }
  nElements_ += number;
  packedMode_=true;
  return number;
}
// Scan dense region and set up indices with tolerance
int
CoinIndexedVector::scanAndPack(double tolerance)
{
  nElements_=0;
  return scanAndPack(0,capacity_,tolerance);
}
// Scan dense region from start to < end and set up indices with tolerance
int
CoinIndexedVector::scanAndPack(int start, int end, double tolerance)
{
  assert(!packedMode_);
  end = min(end,capacity_);
  start = CoinMax(start,0);
  int i;
  int number = 0;
  int * indices = indices_+nElements_;
  for (i=start;i<end;i++) {
    double value = elements_[i];
    elements_[i]=0.0;
    if (fabs(value)>=tolerance) {
      elements_[number]=value;
	indices[number++] = i;
    }
  }
  nElements_ += number;
  packedMode_=true;
  return number;
}
// This is mainly for testing - goes from packed to indexed
void 
CoinIndexedVector::expand()
{
  if (nElements_&&packedMode_) {
    double * temp = new double[capacity_];
    int i;
    for (i=0;i<nElements_;i++) 
      temp[indices_[i]]=elements_[i];
    memset(elements_,0,nElements_*sizeof(double));
    for (i=0;i<nElements_;i++) {
      int iRow = indices_[i];
      elements_[iRow]=temp[iRow];
    }
    delete [] temp;
  }
  packedMode_=false;
}
// Create packed array
void 
CoinIndexedVector::createPacked(int number, const int * indices, 
		    const double * elements)
{
  nElements_=number;
  packedMode_=true;
  memcpy(indices_,indices,number*sizeof(int));
  memcpy(elements_,elements,number*sizeof(double));
}
//  Print out
void 
CoinIndexedVector::print() const
{
  printf("Vector has %d elements (%spacked mode)\n",nElements_,packedMode_ ? "" : "un");
  for (int i=0;i<nElements_;i++) {
    if (i&&(i%5==0))
      printf("\n");
    int index = indices_[i];
    double value = packedMode_ ? elements_[i] : elements_[index];
    printf(" (%d,%g)",index,value);
  }
  printf("\n");
}

