// Copyright (C) 2000, International Business Machines
// Corporation and others.  All Rights Reserved.
#ifndef CoinMpsIO_H
#define CoinMpsIO_H

#include <vector>
#include <string>

#include "CoinPackedMatrix.hpp"
#include "CoinMessageHandler.hpp"
#ifdef COIN_USE_ZLIB
#include "zlib.h"
#else
/* just to make the code much nicer (no need for so many ifdefs */
typedef void* gzFile;
#endif

// Plus infinity
#ifndef COIN_DBL_MAX
#define COIN_DBL_MAX DBL_MAX
#endif

/// The following lengths are in decreasing order (for 64 bit etc)
/// Large enough to contain element index
/// This is already defined as CoinBigIndex
/// Large enough to contain column index
typedef int COINColumnIndex;

/// Large enough to contain row index (or basis)
typedef int COINRowIndex;

// We are allowing free format - but there is a limit!
#define MAX_FIELD_LENGTH 100
#define MAX_CARD_LENGTH 5*MAX_FIELD_LENGTH+80

enum COINSectionType { COIN_NO_SECTION, COIN_NAME_SECTION, COIN_ROW_SECTION,
  COIN_COLUMN_SECTION,
  COIN_RHS_SECTION, COIN_RANGES_SECTION, COIN_BOUNDS_SECTION,
  COIN_ENDATA_SECTION, COIN_EOF_SECTION, COIN_QUADRATIC_SECTION, 
		       COIN_CONIC_SECTION,COIN_UNKNOWN_SECTION
};

enum COINMpsType { COIN_N_ROW, COIN_E_ROW, COIN_L_ROW, COIN_G_ROW,
  COIN_BLANK_COLUMN, COIN_S1_COLUMN, COIN_S2_COLUMN, COIN_S3_COLUMN,
  COIN_INTORG, COIN_INTEND, COIN_SOSEND, COIN_UNSET_BOUND,
  COIN_UP_BOUND, COIN_FX_BOUND, COIN_LO_BOUND, COIN_FR_BOUND,
  COIN_MI_BOUND, COIN_PL_BOUND, COIN_BV_BOUND, COIN_UI_BOUND,
  COIN_SC_BOUND, COIN_UNKNOWN_MPS_TYPE
};
class CoinMpsIO;
/// Very simple code for reading MPS data
class CoinMpsCardReader {

public:

  /**@name Constructor and destructor */
  //@{
  /// Constructor expects file to be open 
  /// This one takes gzFile if fp null
  CoinMpsCardReader ( FILE * fp, gzFile gzfp, CoinMpsIO * reader );

  /// Destructor
  ~CoinMpsCardReader (  );
  //@}


  /**@name card stuff */
  //@{
  /// Read to next section
  COINSectionType readToNextSection (  );
  /// Gets next field and returns section type e.g. COIN_COLUMN_SECTION
  COINSectionType nextField (  );
  /// Returns current section type
  inline COINSectionType whichSection (  ) const {
    return section_;
  };
  /// Only for first field on card otherwise BLANK_COLUMN
  /// e.g. COIN_E_ROW
  inline COINMpsType mpsType (  ) const {
    return mpsType_;
  };
  /// Reads and cleans card - taking out trailing blanks - return 1 if EOF
  int cleanCard();
  /// Returns row name of current field
  inline const char *rowName (  ) const {
    return rowName_;
  };
  /// Returns column name of current field
  inline const char *columnName (  ) const {
    return columnName_;
  };
  /// Returns value in current field
  inline double value (  ) const {
    return value_;
  };
  /// Whole card (for printing)
  inline const char *card (  ) const {
    return card_;
  };
  /// Returns card number
  inline CoinBigIndex cardNumber (  ) const {
    return cardNumber_;
  };
  /// Returns file pointer
  inline FILE * filePointer (  ) const {
    return fp_;
  };
  //@}

////////////////// data //////////////////
private:

  /**@name data */
  //@{
  /// Current value
  double value_;
  /// Current card image
  char card_[MAX_CARD_LENGTH];
  /// Current position within card image
  char *position_;
  /// End of card
  char *eol_;
  /// Current COINMpsType
  COINMpsType mpsType_;
  /// Current row name
  char rowName_[MAX_FIELD_LENGTH];
  /// Current column name
  char columnName_[MAX_FIELD_LENGTH];
  /// File pointer
  FILE *fp_;
  /// Compressed file object
  gzFile gzfp_;
  /// Which section we think we are in
  COINSectionType section_;
  /// Card number
  CoinBigIndex cardNumber_;
  /// Whether free format.  Just for blank RHS etc
  bool freeFormat_;
  /// If all names <= 8 characters then allow embedded blanks
  bool eightChar_;
  /// MpsIO
  CoinMpsIO * reader_;
  /// Message handler
  CoinMessageHandler * handler_;
  /// Messages
  CoinMessages messages_;
  //@}
};

//#############################################################################

/** MPS IO Interface

    This class can be used to read in mps files without a solver.  After
    reading the file, the CoinMpsIO object contains all relevant data, which
    may be more than a particular OsiSolverInterface allows for.  Items may
    be deleted to allow for flexibility of data storage.

    The implementation makes the CoinMpsIO object look very like a dummy solver,
    as the same conventions are used.
*/

class CoinMpsIO {
   friend void CoinMpsIOUnitTest(const std::string & mpsDir);

public:

/** @name Methods to retrieve problem information

   These methods return information about the problem held by the CoinMpsIO
   object.
   
   Querying an object that has no data associated with it result in zeros for
   the number of rows and columns, and NULL pointers from the methods that
   return vectors.  Const pointers returned from any data-query method are
   always valid
*/
//@{
    /// Get number of columns
    int getNumCols() const;

    /// Get number of rows
    int getNumRows() const;

    /// Get number of nonzero elements
    int getNumElements() const;

    /// Get pointer to array[getNumCols()] of column lower bounds
    const double * getColLower() const;

    /// Get pointer to array[getNumCols()] of column upper bounds
    const double * getColUpper() const;

    /** Get pointer to array[getNumRows()] of constraint senses.
	<ul>
	<li>'L': <= constraint
	<li>'E': =  constraint
	<li>'G': >= constraint
	<li>'R': ranged constraint
	<li>'N': free constraint
	</ul>
    */
    const char * getRowSense() const;

    /** Get pointer to array[getNumRows()] of constraint right-hand sides.

	Given constraints with upper (rowupper) and/or lower (rowlower) bounds,
	the constraint right-hand side (rhs) is set as
	<ul>
	  <li> if rowsense()[i] == 'L' then rhs()[i] == rowupper()[i]
	  <li> if rowsense()[i] == 'G' then rhs()[i] == rowlower()[i]
	  <li> if rowsense()[i] == 'R' then rhs()[i] == rowupper()[i]
	  <li> if rowsense()[i] == 'N' then rhs()[i] == 0.0
	</ul>
    */
    const double * getRightHandSide() const;

    /** Get pointer to array[getNumRows()] of row ranges.

	Given constraints with upper (rowupper) and/or lower (rowlower) bounds, 
	the constraint range (rowrange) is set as
	<ul>
          <li> if rowsense()[i] == 'R' then
                  rowrange()[i] == rowupper()[i] - rowlower()[i]
          <li> if rowsense()[i] != 'R' then
                  rowrange()[i] is 0.0
        </ul>
	Put another way, only range constraints have a nontrivial value for
	rowrange.
    */
    const double * getRowRange() const;

    /// Get pointer to array[getNumRows()] of row lower bounds
    const double * getRowLower() const;

    /// Get pointer to array[getNumRows()] of row upper bounds
    const double * getRowUpper() const;

    /// Get pointer to array[getNumCols()] of objective function coefficients
    const double * getObjCoefficients() const;

    /// Get pointer to row-wise copy of the coefficient matrix
    const CoinPackedMatrix * getMatrixByRow() const;

    /// Get pointer to column-wise copy of the coefficient matrix
    const CoinPackedMatrix * getMatrixByCol() const;

    /// Return true if column is a continuous variable
    bool isContinuous(int colNumber) const;

    /** Return true if a column is an integer variable

        Note: This function returns true if the the column
        is a binary or general integer variable.
    */
    bool isInteger(int columnNumber) const;
  
    /** Returns array[getNumCols()] specifying if a variable is integer.

	At present, simply coded as zero (continuous) and non-zero (integer)
	May be extended at a later date.
    */
    const char * integerColumns() const;

    /** Returns the row name for the specified index.

	Returns 0 if the index is out of range.
    */
    const char * rowName(int index) const;

    /** Returns the column name for the specified index.

	Returns 0 if the index is out of range.
    */
    const char * columnName(int index) const;

    /** Returns the index for the specified row name
  
	Returns -1 if the name is not found.
        Returns numberRows for the objective row and > numberRows for
	dropped free rows.
    */
    int rowIndex(const char * name) const;

    /** Returns the index for the specified column name
  
	Returns -1 if the name is not found.
    */
    int columnIndex(const char * name) const;

    /** Returns the (constant) objective offset
    
	This is the RHS entry for the objective row
    */
    double objectiveOffset() const;

    /// Return the problem name
    const char * getProblemName() const;

    /// Return the objective name
    const char * getObjectiveName() const;

    /// Return the RHS vector name
    const char * getRhsName() const;

    /// Return the range vector name
    const char * getRangeName() const;

    /// Return the bound vector name
    const char * getBoundName() const;
//@}


/** @name Methods to set problem information

    Methods to load a problem into the CoinMpsIO object.
*/
//@{
  
    /// Set the problem data
    void setMpsData(const CoinPackedMatrix& m, const double infinity,
		     const double* collb, const double* colub,
		     const double* obj, const char* integrality,
		     const double* rowlb, const double* rowub,
		     char const * const * const colnames,
		     char const * const * const rownames);
    void setMpsData(const CoinPackedMatrix& m, const double infinity,
		     const double* collb, const double* colub,
		     const double* obj, const char* integrality,
		     const double* rowlb, const double* rowub,
		     const std::vector<std::string> & colnames,
		     const std::vector<std::string> & rownames);
    void setMpsData(const CoinPackedMatrix& m, const double infinity,
		     const double* collb, const double* colub,
		     const double* obj, const char* integrality,
		     const char* rowsen, const double* rowrhs,
		     const double* rowrng,
		     char const * const * const colnames,
		     char const * const * const rownames);
    void setMpsData(const CoinPackedMatrix& m, const double infinity,
		     const double* collb, const double* colub,
		     const double* obj, const char* integrality,
		     const char* rowsen, const double* rowrhs,
		     const double* rowrng,
		     const std::vector<std::string> & colnames,
		     const std::vector<std::string> & rownames);

    /** Pass in an array[getNumCols()] specifying if a variable is integer.

	At present, simply coded as zero (continuous) and non-zero (integer)
	May be extended at a later date.
    */
    void copyInIntegerInformation(const char * integerInformation);
//@}

/** @name Parameter set/get methods

  Methods to set and retrieve MPS IO parameters.
*/

//@{
    /// Set infinity
    void setInfinity(double value);

    /// Get infinity
    double getInfinity() const;

    /// Set default upper bound for integer variables
    void setDefaultBound(int value);

    /// Get default upper bound for integer variables
    int getDefaultBound() const;
//@}


/** @name Methods for problem input and output

  Methods to read and write MPS format problem files.
   
  The read and write methods return the number of errors that occurred during
  the IO operation, or -1 if no file is opened.

  \note
  If the CoinMpsIO class was compiled with support for libz then
  readMps will automatically try to append .gz to the file name and open it as
  a compressed file if the specified file name cannot be opened.
  (Automatic append of the .bz2 suffix when libbz is used is on the TODO list.)

  \todo
  Allow for file pointers and positioning
*/

//@{
    /// Set the current file name for the CoinMpsIO object
    void setFileName(const char * name);

    /// Get the current file name for the CoinMpsIO object
    const char * getFileName() const;

    /// Test if a file with the currrent file name exists and is readable
    const bool fileReadable() const;

    /** Read a problem in MPS format from the given filename.

      Use "stdin" or "-" to read from stdin.
    */
    int readMps(const char *filename, const char *extension = "mps");

    /** Read a problem in MPS format from a previously opened file

      More precisely, read a problem using a CoinMpsCardReader object already
      associated with this CoinMpsIO object.

      \todo
      Provide an interface that will allow a client to associate a
      CoinMpsCardReader object with a CoinMpsIO object by setting the
      cardReader_ field.
    */
    int readMps();

    /** Write the problem in MPS format to a file with the given filename.

	\param compression can be set to three values to indicate what kind
	of file should be written
	<ul>
	  <li> 0: plain text (default)
	  <li> 1: gzip compressed (.gz is appended to \c filename)
	  <li> 2: bzip2 compressed (.bz2 is appended to \c filename) (TODO)
	</ul>
	If the library was not compiled with the requested compression then
	writeMps falls back to writing a plain text file.

	\param formatType specifies the precision to used for values in the
	MPS file
	<ul>
	  <li> 0: normal precision (default)
	  <li> 1: extra accuracy
	  <li> 2: IEEE hex (TODO)
	</ul>

	\param numberAcross specifies whether 1 or 2 (default) values should be
	specified on every data line in the MPS file.
    */
    int writeMps(const char *filename, int compression = 0,
		 int formatType = 0, int numberAcross = 2) const;

    /** Read in a quadratic objective from the given filename.

      If filename is NULL (or the same as the currently open file) then
      reading continues from the current file.
      If not, the file is closed and the specified file is opened.
      
      Code should be added to
      general MPS reader to read this if QSECTION
      Data is assumed to be Q and objective is c + 1/2 xT Q x
      No assumption is made for symmetry, positive definite, etc.
      No check is made for duplicates or non-triangular if checkSymmetry==0.
      If 1 checks lower triangular (so off diagonal should be 2*Q)
      if 2 makes lower triangular and assumes full Q (but adds off diagonals)
      
      Arrays should be deleted by delete []

      Returns number of errors:
      <ul>
	<li> -1: bad file
	<li> -2: no Quadratic section
	<li> -3: an empty section
        <li> +n: then matching errors etc (symmetry forced)
        <li> -4: no matching errors but fails triangular test
		 (triangularity forced)
      </ul>
      columnStart is numberColumns+1 long, others numberNonZeros
    */
    int readQuadraticMps(const char * filename,
			 int * &columnStart, int * &column, double * &elements,
			 int checkSymmetry);

    /** Read in a list of cones from the given filename.  

      If filename is NULL (or the same as the currently open file) then
      reading continues from the current file.
      If not, the file is closed and the specified file is opened.

      Code should be added to
      general MPS reader to read this if CSECTION
      No checking is done that in unique cone

      Arrays should be deleted by delete []

      Returns number of errors, -1 bad file, -2 no conic section,
      -3 empty section

      columnStart is numberCones+1 long, other number of columns in matrix
    */
    int readConicMps(const char * filename,
		     int * &columnStart, int * &column, int & numberCones);
  //@}

/** @name Constructors and destructors */
//@{
    /// Default Constructor
    CoinMpsIO(); 
      
    /// Copy constructor 
    CoinMpsIO (const CoinMpsIO &);
  
    /// Assignment operator 
    CoinMpsIO & operator=(const CoinMpsIO& rhs);
  
    /// Destructor 
    ~CoinMpsIO ();
//@}


/**@name Message handling */
//@{
  /** Pass in Message handler
  
      Supply a custom message handler. It will not be destroyed when the
      CoinMpsIO object is destroyed.
  */
  void passInMessageHandler(CoinMessageHandler * handler);

  /// Set the language for messages.
  void newLanguage(CoinMessages::Language language);

  /// Set the language for messages.
  void setLanguage(CoinMessages::Language language) {newLanguage(language);};

  /// Return the message handler
  CoinMessageHandler * messageHandler() const {return handler_;};

  /// Return the messages
  CoinMessages messages() {return messages_;};
//@}


/**@name Methods to release storage

  These methods allow the client to reduce the storage used by the CoinMpsIO
  object be selectively releasing unneeded problem information.
*/
//@{
    /** Release all information which can be re-calculated.
    
	E.g., row sense, copies of rows, hash tables for names.
    */
    void releaseRedundantInformation();

    /// Release all row information (lower, upper)
    void releaseRowInformation();

    /// Release all column information (lower, upper, objective)
    void releaseColumnInformation();

    /// Release integer information
    void releaseIntegerInformation();

    /// Release row names
    void releaseRowNames();

    /// Release column names
    void releaseColumnNames();

    /// Release matrix information
    void releaseMatrixInformation();
  //@}

private:
  
/**@name Miscellaneous helper functions */
  //@{

    /// Utility method used several times to implement public methods
    void
    setMpsDataWithoutRowAndColNames(
		      const CoinPackedMatrix& m, const double infinity,
		      const double* collb, const double* colub,
		      const double* obj, const char* integrality,
		      const double* rowlb, const double* rowub);
    void
    setMpsDataColAndRowNames(
		      const std::vector<std::string> & colnames,
		      const std::vector<std::string> & rownames);
    void
    setMpsDataColAndRowNames(
		      char const * const * const colnames,
		      char const * const * const rownames);

  
    /// Does the heavy lifting for destruct and assignment.
    void gutsOfDestructor();

    /// Does the heavy lifting for copy and assignment.
    void gutsOfCopy(const CoinMpsIO &);
  
    /// Clears problem data from the CoinMpsIO object.
    void freeAll();

    /** A quick inlined function to convert from lb/ub style constraint
	definition to sense/rhs/range style */
    inline void
    convertBoundToSense(const double lower, const double upper,
			char& sense, double& right, double& range) const;
    /** A quick inlined function to convert from sense/rhs/range stryle
	constraint definition to lb/ub style */
    inline void
    convertSenseToBound(const char sense, const double right,
			const double range,
			double& lower, double& upper) const;

  /** Deal with a filename
  
    As the name says.
    Returns +1 if the file name is new, 0 if it's the same as before
    (i.e., matches fileName_), and -1 if there's an error and the file
    can't be opened.
    Handles automatic append of .gz suffix when compiled with libz.

    \todo
    Add automatic append of .bz2 suffix when compiled with libbz.
  */

  int dealWithFileName(const char * filename,  const char * extension,
		       FILE * & fp, gzFile  & gzfp); 
  //@}

  
  // for hashing
  typedef struct {
    int index, next;
  } CoinHashLink;

  /**@name Hash table methods */
  //@{
  /// Creates hash list for names (section = 0 for rows, 1 columns)
  void startHash ( char **names, const int number , int section );
  /// This one does it when names are already in
  void startHash ( int section ) const;
  /// Deletes hash storage
  void stopHash ( int section );
  /// Finds match using hash,  -1 not found
  int findHash ( const char *name , int section ) const;
  //@}

    /**@name Cached problem information */
    //@{
      /// Problem name
      char * problemName_;

      /// Objective row name
      char * objectiveName_;

      /// Right-hand side vector name
      char * rhsName_;

      /// Range vector name
      char * rangeName_;

      /// Bounds vector name
      char * boundName_;

      /// Number of rows
      int numberRows_;

      /// Number of columns
      int numberColumns_;

      /// Number of coefficients
      CoinBigIndex numberElements_;

      /// Pointer to dense vector of row sense indicators
      mutable char    *rowsense_;
  
      /// Pointer to dense vector of row right-hand side values
      mutable double  *rhs_;
  
      /** Pointer to dense vector of slack variable upper bounds for range 
          constraints (undefined for non-range rows)
      */
      mutable double  *rowrange_;
   
      /// Pointer to row-wise copy of problem matrix coefficients.
      mutable CoinPackedMatrix *matrixByRow_;  

      /// Pointer to column-wise copy of problem matrix coefficients.
      CoinPackedMatrix *matrixByColumn_;  

      /// Pointer to dense vector of row lower bounds
      double * rowlower_;

      /// Pointer to dense vector of row upper bounds
      double * rowupper_;

      /// Pointer to dense vector of column lower bounds
      double * collower_;

      /// Pointer to dense vector of column upper bounds
      double * colupper_;

      /// Pointer to dense vector of objective coefficients
      double * objective_;

      /// Constant offset for objective value (i.e., RHS value for OBJ row)
      double objectiveOffset_;


      /** Pointer to dense vector specifying if a variable is continuous
	  (0) or integer (1).
      */
      char * integerType_;

      /** Row and column names
	  Linked to hash table sections (0 - row names, 1 column names)
      */
      char **names_[2];
    //@}

    /** @name Hash tables */
    //@{
      /// Current file name
      char * fileName_;

      /// Number of entries in a hash table section
      int numberHash_[2];

      /// Hash tables (two sections, 0 - row names, 1 - column names)
      mutable CoinHashLink *hash_[2];
    //@}

    /** @name CoinMpsIO object parameters */
    //@{
      /// Upper bound when no bounds for integers
      int defaultBound_; 

      /// Value to use for infinity
      double infinity_;

      /// Message handler
      CoinMessageHandler * handler_;
      /** Flag to say if the message handler is the default handler.

          If true, the handler will be destroyed when the CoinMpsIO
	  object is destroyed; if false, it will not be destroyed.
      */
      bool defaultHandler_;
      /// Messages
      CoinMessages messages_;
      /// Card reader
      CoinMpsCardReader * cardReader_;
    //@}

};

//#############################################################################
/** A function that tests the methods in the CoinMpsIO class. The
    only reason for it not to be a member method is that this way it doesn't
    have to be compiled into the library. And that's a gain, because the
    library should be compiled with optimization on, but this method should be
    compiled with debugging. Also, if this method is compiled with
    optimization, the compilation takes 10-15 minutes and the machine pages
    (has 256M core memory!)... */
void
CoinMpsIOUnitTest(const std::string & mpsDir);

#endif
