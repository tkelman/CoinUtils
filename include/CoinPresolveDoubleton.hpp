// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.

#ifndef CoinPresolveDoubleton_H
#define CoinPresolveDoubleton_H

#define	DOUBLETON	5

class doubleton_action : public CoinPresolveAction {
 public:
  struct action {
    int icolx;
    int row;

    double clox;
    double cupx;
    double costx;
    
    int icoly;
    double cloy;
    double cupy;
    double costy;

    double rlo;
    double rup;

    double coeffx;
    double coeffy;

    int ncolx;
    double *colel;

    int ncoly;
  };

  const int nactions_;
  const action *const actions_;

 private:
  doubleton_action(int nactions,
		      const action *actions,
		      const CoinPresolveAction *next) :
    CoinPresolveAction(next),
    nactions_(nactions), actions_(actions)
{}

 public:
  const char *name() const { return ("doubleton_action"); }

  static const CoinPresolveAction *presolve(CoinPresolveMatrix *,
					 const CoinPresolveAction *next);
  
  void postsolve(CoinPostsolveMatrix *prob) const;

  ~doubleton_action();
};
#endif


