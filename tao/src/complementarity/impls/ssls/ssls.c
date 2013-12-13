#include "ssls.h"


/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TaoSetFromOptions_SSLS"
PetscErrorCode TaoSetFromOptions_SSLS(TaoSolver tao)
{
  TAO_SSLS *ssls = (TAO_SSLS *)tao->data;
  PetscErrorCode ierr;
  PetscBool flg;

  PetscFunctionBegin;
  ierr = PetscOptionsHead("Semismooth method with a linesearch for "
  		        "complementarity problems"); CHKERRQ(ierr);
  ierr = PetscOptionsReal("-ssls_delta", "descent test fraction", "",
                         ssls->delta, &(ssls->delta), &flg);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-ssls_rho", "descent test power", "",
                         ssls->rho, &(ssls->rho), &flg);CHKERRQ(ierr);
  ierr = TaoLineSearchSetFromOptions(tao->linesearch);CHKERRQ(ierr);
  ierr = KSPSetFromOptions(tao->ksp); CHKERRQ(ierr);
  ierr = PetscOptionsTail(); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "TaoView_SSLS"
PetscErrorCode TaoView_SSLS(TaoSolver tao, PetscViewer pv)
{
  /*PetscErrorCode ierr; */

  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "Tao_SSLS_Function"
PetscErrorCode Tao_SSLS_Function(TaoLineSearch ls, Vec X, PetscReal *fcn, void *ptr) 
{
  TaoSolver tao = (TaoSolver)ptr;
  TAO_SSLS *ssls = (TAO_SSLS *)tao->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  
  ierr = TaoComputeConstraints(tao, X, tao->constraints); CHKERRQ(ierr);
  ierr = VecFischer(X,tao->constraints,tao->XL,tao->XU,ssls->ff); CHKERRQ(ierr);
  ierr = VecNorm(ssls->ff,NORM_2,&ssls->merit); CHKERRQ(ierr);
  *fcn = 0.5*ssls->merit*ssls->merit;
  PetscFunctionReturn(0);
}

/*------------------------------------------------------------*/
#undef __FUNCT__  
#define __FUNCT__ "Tao_SSLS_FunctionGradient"
PetscErrorCode Tao_SSLS_FunctionGradient(TaoLineSearch ls, Vec X, PetscReal *fcn,  Vec G, void *ptr)
{
  TaoSolver tao = (TaoSolver)ptr;
  TAO_SSLS *ssls = (TAO_SSLS *)tao->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;

  ierr = TaoComputeConstraints(tao, X, tao->constraints); CHKERRQ(ierr);
  ierr = VecFischer(X,tao->constraints,tao->XL,tao->XU,ssls->ff); CHKERRQ(ierr);
  ierr = VecNorm(ssls->ff,NORM_2,&ssls->merit); CHKERRQ(ierr);
  *fcn = 0.5*ssls->merit*ssls->merit;

  ierr = TaoComputeJacobian(tao, tao->solution, &tao->jacobian, &tao->jacobian_pre, &ssls->matflag); CHKERRQ(ierr);
  
  ierr = D_Fischer(tao->jacobian, tao->solution, tao->constraints, 
		   tao->XL, tao->XU, ssls->t1, ssls->t2, 
		   ssls->da, ssls->db); CHKERRQ(ierr);
  ierr = MatDiagonalScale(tao->jacobian,ssls->db,PETSC_NULL); CHKERRQ(ierr);
  ierr = MatDiagonalSet(tao->jacobian,ssls->da,ADD_VALUES); CHKERRQ(ierr);
  ierr = MatMultTranspose(tao->jacobian,ssls->ff,G); CHKERRQ(ierr);

  PetscFunctionReturn(0);
}

