/*$Id: beuler.c,v 1.41 2000/01/11 21:02:59 bsmith Exp bsmith $*/
/*
       Code for Timestepping with implicit backwards Euler.
*/
#include "src/ts/tsimpl.h"                /*I   "ts.h"   I*/

typedef struct {
  Vec  update;      /* work vector where new solution is formed */
  Vec  func;        /* work vector where F(t[i],u[i]) is stored */
  Vec  rhs;         /* work vector for RHS; vec_sol/dt */
} TS_BEuler;

/*------------------------------------------------------------------------------*/

/*
    Version for linear PDE where RHS does not depend on time. Has built a
  single matrix that is to be used for all timesteps.
*/
#undef __FUNC__  
#define __FUNC__ "TSStep_BEuler_Linear_Constant_Matrix"
static int TSStep_BEuler_Linear_Constant_Matrix(TS ts,int *steps,double *time)
{
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  Vec       sol = ts->vec_sol,update = beuler->update;
  Vec       rhs = beuler->rhs;
  int       ierr,i,max_steps = ts->max_steps,its;
  Scalar    mdt = 1.0/ts->time_step;
  
  PetscFunctionBegin;
  *steps = -ts->steps;
  ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);

  /* set initial guess to be previous solution */
  ierr = VecCopy(sol,update);CHKERRQ(ierr);

  for (i=0; i<max_steps; i++) {
    ierr = VecCopy(sol,rhs);CHKERRQ(ierr);
    ierr = VecScale(&mdt,rhs);CHKERRQ(ierr);
    /* apply user-provided boundary conditions (only needed if they are time dependent) */
    ierr = TSComputeRHSBoundaryConditions(ts,ts->ptime,rhs);CHKERRQ(ierr);

    ts->ptime += ts->time_step;
    if (ts->ptime > ts->max_time) break;
    ierr = SLESSolve(ts->sles,rhs,update,&its);CHKERRQ(ierr);
    ts->linear_its += PetscAbsInt(its);
    ierr = VecCopy(update,sol);CHKERRQ(ierr);
    ts->steps++;
    ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);
  }

  *steps += ts->steps;
  *time  = ts->ptime;
  PetscFunctionReturn(0);
}
/*
      Version where matrix depends on time 
*/
#undef __FUNC__  
#define __FUNC__ "TSStep_BEuler_Linear_Variable_Matrix"
static int TSStep_BEuler_Linear_Variable_Matrix(TS ts,int *steps,double *time)
{
  TS_BEuler    *beuler = (TS_BEuler*)ts->data;
  Vec          sol = ts->vec_sol,update = beuler->update,rhs = beuler->rhs;
  int          ierr,i,max_steps = ts->max_steps,its;
  Scalar       mdt = 1.0/ts->time_step,mone = -1.0;
  MatStructure str;

  PetscFunctionBegin;
  *steps = -ts->steps;
  ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);

  /* set initial guess to be previous solution */
  ierr = VecCopy(sol,update);CHKERRQ(ierr);

  for (i=0; i<max_steps; i++) {
    ierr = VecCopy(sol,rhs);CHKERRQ(ierr);
    ierr = VecScale(&mdt,rhs);CHKERRQ(ierr);
    /* apply user-provided boundary conditions (only needed if they are time dependent) */
    ierr = TSComputeRHSBoundaryConditions(ts,ts->ptime,rhs);CHKERRQ(ierr);

    ts->ptime += ts->time_step;
    if (ts->ptime > ts->max_time) break;
    /*
        evaluate matrix function 
    */
    ierr = (*ts->rhsmatrix)(ts,ts->ptime,&ts->A,&ts->B,&str,ts->jacP);CHKERRQ(ierr);
    if (!ts->Ashell) {
      ierr = MatScale(&mone,ts->A);CHKERRQ(ierr);
      ierr = MatShift(&mdt,ts->A);CHKERRQ(ierr);
    }
    if (ts->B != ts->A && ts->Ashell != ts->B && str != SAME_PRECONDITIONER) {
      ierr = MatScale(&mone,ts->B);CHKERRQ(ierr);
      ierr = MatShift(&mdt,ts->B);CHKERRQ(ierr);
    }
    ierr = SLESSetOperators(ts->sles,ts->A,ts->B,str);CHKERRQ(ierr);
    ierr = SLESSolve(ts->sles,rhs,update,&its);CHKERRQ(ierr);
    ts->linear_its += PetscAbsInt(its);
    ierr = VecCopy(update,sol);CHKERRQ(ierr);
    ts->steps++;
    ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);
  }

  *steps += ts->steps;
  *time  = ts->ptime;
  PetscFunctionReturn(0);
}
/*
    Version for nonlinear PDE.
*/
#undef __FUNC__  
#define __FUNC__ "TSStep_BEuler_Nonlinear"
static int TSStep_BEuler_Nonlinear(TS ts,int *steps,double *time)
{
  Vec       sol = ts->vec_sol;
  int       ierr,i,max_steps = ts->max_steps,its,lits;
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  
  PetscFunctionBegin;
  *steps = -ts->steps;
  ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);

  for (i=0; i<max_steps; i++) {
    ts->ptime += ts->time_step;
    if (ts->ptime > ts->max_time) break;
    ierr = VecCopy(sol,beuler->update);CHKERRQ(ierr);
    ierr = SNESSolve(ts->snes,beuler->update,&its);CHKERRQ(ierr);
    ierr = SNESGetNumberLinearIterations(ts->snes,&lits);CHKERRQ(ierr);
    ts->nonlinear_its += PetscAbsInt(its); ts->linear_its += lits;
    ierr = VecCopy(beuler->update,sol);CHKERRQ(ierr);
    ts->steps++;
    ierr = TSMonitor(ts,ts->steps,ts->ptime,sol);CHKERRQ(ierr);
  }

  *steps += ts->steps;
  *time  = ts->ptime;
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNC__  
#define __FUNC__ "TSDestroy_BEuler"
static int TSDestroy_BEuler(TS ts)
{
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  int       ierr;

  PetscFunctionBegin;
  if (beuler->update) {ierr = VecDestroy(beuler->update);CHKERRQ(ierr);}
  if (beuler->func) {ierr = VecDestroy(beuler->func);CHKERRQ(ierr);}
  if (beuler->rhs) {ierr = VecDestroy(beuler->rhs);CHKERRQ(ierr);}
  if (ts->Ashell) {ierr = MatDestroy(ts->A);CHKERRQ(ierr);}
  ierr = PetscFree(beuler);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


/*------------------------------------------------------------*/
/*
    This matrix shell multiply where user provided Shell matrix
*/

#undef __FUNC__  
#define __FUNC__ "TSBEulerMatMult"
int TSBEulerMatMult(Mat mat,Vec x,Vec y)
{
  TS     ts;
  Scalar mdt,mone = -1.0;
  int    ierr;

  PetscFunctionBegin;
  ierr = MatShellGetContext(mat,(void **)&ts);CHKERRQ(ierr);
  mdt = 1.0/ts->time_step;

  /* apply user provided function */
  ierr = MatMult(ts->Ashell,x,y);CHKERRQ(ierr);
  /* shift and scale by 1/dt - F */
  ierr = VecAXPBY(&mdt,&mone,x,y);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* 
    This defines the nonlinear equation that is to be solved with SNES

              U^{n+1} - dt*F(U^{n+1}) - U^{n}
*/
#undef __FUNC__  
#define __FUNC__ "TSBEulerFunction"
int TSBEulerFunction(SNES snes,Vec x,Vec y,void *ctx)
{
  TS     ts = (TS) ctx;
  Scalar mdt = 1.0/ts->time_step,*unp1,*un,*Funp1;
  int    ierr,i,n;

  PetscFunctionBegin;
  /* apply user-provided function */
  ierr = TSComputeRHSFunction(ts,ts->ptime,x,y);CHKERRQ(ierr);
  /* (u^{n+1} - U^{n})/dt - F(u^{n+1}) */
  ierr = VecGetArray(ts->vec_sol,&un);CHKERRQ(ierr);
  ierr = VecGetArray(x,&unp1);CHKERRQ(ierr);
  ierr = VecGetArray(y,&Funp1);CHKERRQ(ierr);
  ierr = VecGetLocalSize(x,&n);CHKERRQ(ierr);

  for (i=0; i<n; i++) {
    Funp1[i] = mdt*(unp1[i] - un[i]) - Funp1[i];
  }
  ierr = VecRestoreArray(ts->vec_sol,&un);CHKERRQ(ierr);
  ierr = VecRestoreArray(x,&unp1);CHKERRQ(ierr);
  ierr = VecRestoreArray(y,&Funp1);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
   This constructs the Jacobian needed for SNES 

             J = I/dt - J_{F}   where J_{F} is the given Jacobian of F.
*/
#undef __FUNC__  
#define __FUNC__ "TSBEulerJacobian"
int TSBEulerJacobian(SNES snes,Vec x,Mat *AA,Mat *BB,MatStructure *str,void *ctx)
{
  TS      ts = (TS) ctx;
  int     ierr;
  Scalar  mone = -1.0,mdt = 1.0/ts->time_step;
  MatType mtype;

  PetscFunctionBegin;
  /* construct user's Jacobian */
  ierr = TSComputeRHSJacobian(ts,ts->ptime,x,AA,BB,str);CHKERRQ(ierr);

  /* shift and scale Jacobian, if not matrix-free */
  ierr = MatGetType(*AA,&mtype,PETSC_NULL);CHKERRQ(ierr);
  if (mtype != MATSHELL) {
    ierr = MatScale(&mone,*AA);CHKERRQ(ierr);
    ierr = MatShift(&mdt,*AA);CHKERRQ(ierr);
  }
  ierr = MatGetType(*BB,&mtype,PETSC_NULL);CHKERRQ(ierr);
  if (*BB != *AA && *str != SAME_PRECONDITIONER && mtype != MATSHELL) {
    ierr = MatScale(&mone,*BB);CHKERRQ(ierr);
    ierr = MatShift(&mdt,*BB);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------*/
#undef __FUNC__  
#define __FUNC__ "TSSetUp_BEuler_Linear_Constant_Matrix"
static int TSSetUp_BEuler_Linear_Constant_Matrix(TS ts)
{
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  int       ierr,M,m;
  Scalar    mdt = 1.0/ts->time_step,mone = -1.0;

  PetscFunctionBegin;
  ierr = VecDuplicate(ts->vec_sol,&beuler->update);CHKERRQ(ierr);  
  ierr = VecDuplicate(ts->vec_sol,&beuler->rhs);CHKERRQ(ierr);  
    
  /* build linear system to be solved */
  if (!ts->Ashell) {
    ierr = MatScale(&mone,ts->A);CHKERRQ(ierr);
    ierr = MatShift(&mdt,ts->A);CHKERRQ(ierr);
  } else {
    /* construct new shell matrix */
    ierr = VecGetSize(ts->vec_sol,&M);CHKERRQ(ierr);
    ierr = VecGetLocalSize(ts->vec_sol,&m);CHKERRQ(ierr);
    ierr = MatCreateShell(ts->comm,m,M,M,M,ts,&ts->A);CHKERRQ(ierr);
    ierr = MatShellSetOperation(ts->A,MATOP_MULT,(void *)TSBEulerMatMult);CHKERRQ(ierr);
  }
  if (ts->A != ts->B && ts->Ashell != ts->B) {
    ierr = MatScale(&mone,ts->B);CHKERRQ(ierr);
    ierr = MatShift(&mdt,ts->B);CHKERRQ(ierr);
  }
  ierr = SLESSetOperators(ts->sles,ts->A,ts->B,SAME_NONZERO_PATTERN);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "TSSetUp_BEuler_Linear_Variable_Matrix"
static int TSSetUp_BEuler_Linear_Variable_Matrix(TS ts)
{
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  int       ierr,M,m;

  PetscFunctionBegin;
  ierr = VecDuplicate(ts->vec_sol,&beuler->update);CHKERRQ(ierr);  
  ierr = VecDuplicate(ts->vec_sol,&beuler->rhs);CHKERRQ(ierr);  
  if (ts->Ashell) { /* construct new shell matrix */
    ierr = VecGetSize(ts->vec_sol,&M);CHKERRQ(ierr);
    ierr = VecGetLocalSize(ts->vec_sol,&m);CHKERRQ(ierr);
    ierr = MatCreateShell(ts->comm,m,M,M,M,ts,&ts->A);CHKERRQ(ierr);
    ierr = MatShellSetOperation(ts->A,MATOP_MULT,(void *)TSBEulerMatMult);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "TSSetUp_BEuler_Nonlinear"
static int TSSetUp_BEuler_Nonlinear(TS ts)
{
  TS_BEuler *beuler = (TS_BEuler*)ts->data;
  int       ierr,M,m;

  PetscFunctionBegin;
  ierr = VecDuplicate(ts->vec_sol,&beuler->update);CHKERRQ(ierr);  
  ierr = VecDuplicate(ts->vec_sol,&beuler->func);CHKERRQ(ierr);  
  ierr = SNESSetFunction(ts->snes,beuler->func,TSBEulerFunction,ts);CHKERRQ(ierr);
  if (ts->Ashell) { /* construct new shell matrix */
    ierr = VecGetSize(ts->vec_sol,&M);CHKERRQ(ierr);
    ierr = VecGetLocalSize(ts->vec_sol,&m);CHKERRQ(ierr);
    ierr = MatCreateShell(ts->comm,m,M,M,M,ts,&ts->A);CHKERRQ(ierr);
    ierr = MatShellSetOperation(ts->A,MATOP_MULT,(void *)TSBEulerMatMult);CHKERRQ(ierr);
  }
  ierr = SNESSetJacobian(ts->snes,ts->A,ts->B,TSBEulerJacobian,ts);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
/*------------------------------------------------------------*/

#undef __FUNC__  
#define __FUNC__ "TSSetFromOptions_BEuler_Linear"
static int TSSetFromOptions_BEuler_Linear(TS ts)
{
  int ierr;

  PetscFunctionBegin;
  ierr = SLESSetFromOptions(ts->sles);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "TSSetFromOptions_BEuler_Nonlinear"
static int TSSetFromOptions_BEuler_Nonlinear(TS ts)
{
  int ierr;

  PetscFunctionBegin;
  ierr = SNESSetFromOptions(ts->snes);CHKERRQ(ierr);
  
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "TSPrintHelp_BEuler"
static int TSPrintHelp_BEuler(TS ts,char *p)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "TSView_BEuler"
static int TSView_BEuler(TS ts,Viewer viewer)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------ */
EXTERN_C_BEGIN
#undef __FUNC__  
#define __FUNC__ "TSCreate_BEuler"
int TSCreate_BEuler(TS ts)
{
  TS_BEuler *beuler;
  int       ierr;
  KSP       ksp;
  MatType   mtype;

  PetscFunctionBegin;
  ts->destroy         = TSDestroy_BEuler;
  ts->printhelp       = TSPrintHelp_BEuler;
  ts->view            = TSView_BEuler;

  if (ts->problem_type == TS_LINEAR) {
    if (!ts->A) {
      SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Must set rhs matrix for linear problem");
    }
    ierr = MatGetType(ts->A,&mtype,PETSC_NULL);CHKERRQ(ierr);
    if (!ts->rhsmatrix) {
      if (mtype == MATSHELL) {
        ts->Ashell = ts->A;
      }
      ts->setup  = TSSetUp_BEuler_Linear_Constant_Matrix;
      ts->step   = TSStep_BEuler_Linear_Constant_Matrix;
    } else {
      if (mtype == MATSHELL) {
        ts->Ashell = ts->A;
      }
      ts->setup  = TSSetUp_BEuler_Linear_Variable_Matrix;  
      ts->step   = TSStep_BEuler_Linear_Variable_Matrix;
    }
    ts->setfromoptions  = TSSetFromOptions_BEuler_Linear;
    ierr = SLESCreate(ts->comm,&ts->sles);CHKERRQ(ierr);
    ierr = SLESGetKSP(ts->sles,&ksp);CHKERRQ(ierr);
    ierr = KSPSetInitialGuessNonzero(ksp);CHKERRQ(ierr);
  } else if (ts->problem_type == TS_NONLINEAR) {
    if (!ts->A) {
      SETERRQ(PETSC_ERR_ARG_WRONGSTATE,0,"Must set Jacobian for nonlinear problem");
    }
    ierr = MatGetType(ts->A,&mtype,PETSC_NULL);CHKERRQ(ierr);
    if (mtype == MATSHELL) {
      ts->Ashell = ts->A;
    }
    ts->setup           = TSSetUp_BEuler_Nonlinear;  
    ts->step            = TSStep_BEuler_Nonlinear;
    ts->setfromoptions  = TSSetFromOptions_BEuler_Nonlinear;
    ierr = SNESCreate(ts->comm,SNES_NONLINEAR_EQUATIONS,&ts->snes);CHKERRQ(ierr);
  } else SETERRQ(PETSC_ERR_ARG_OUTOFRANGE,0,"No such problem");

  beuler   = PetscNew(TS_BEuler);CHKPTRQ(beuler);
  PLogObjectMemory(ts,sizeof(TS_BEuler));
  ierr     = PetscMemzero(beuler,sizeof(TS_BEuler));CHKERRQ(ierr);
  ts->data = (void*)beuler;

  PetscFunctionReturn(0);
}
EXTERN_C_END





