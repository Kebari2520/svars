#include <RcppArmadillo.h>

// [[Rcpp::depends(RcppArmadillo)]]



double LikelihoodCV(arma::vec& S, double& Tob, double& TB,  arma::mat& Sigma_hat1, int& k,
                    arma::mat& Sigma_hat2, arma::mat& RestrictionMatrix, int& restrictions){


  RestrictionMatrix.elem(find_nonfinite(RestrictionMatrix)) = S.elem(find_nonfinite(RestrictionMatrix));
  arma::mat W = RestrictionMatrix;

  arma::mat Psi = diagmat(S.subvec(((k * k) - restrictions), (k * k + (k-1) - restrictions)));

  arma::mat MMM = W * arma::trans(W);
  arma::mat MMM2 = W * (Psi * arma::trans(W));
  double MW = arma::det(MMM);
  double MW2 = arma::det(MMM2);


  if(any(vectorise(Psi) < 0.0) || MW < 0.01 ||  MW2 < 0.01){
    return 1e25;
  }

  double L = -(((TB - 1) / 2) * (log(MW) + arma::sum(diagvec((Sigma_hat1 * arma::inv(MMM)))))) -
    (((Tob - TB + 1) / 2) * (log(MW2) + arma::sum(diagvec((Sigma_hat2 * arma::inv(MMM2))))));

  return -L;

}

// [[Rcpp::export]]
Rcpp::List IdentifyVolatilityNew(int& counter, arma::mat& Bhat, arma::mat& LambdaHat,
                    int& Exit, int& crit, int& MaxIter, arma::mat& Zt, double& TB,
                    arma::mat& y, int& p, int& k, arma::mat& RestrictionMatrix,
                    int& restrictions, arma::vec& S, double& Tob){

  arma::mat GLS21(k*k*p+k, TB-1);
  arma::mat GLS22(k*k*p+k, y.n_cols);
  GLS21.zeros();
  GLS22.zeros();
  arma::mat term1(k, Zt.n_cols);
  term1.zeros();
  arma::mat kMat(k,k);
  kMat.eye();
  Rcpp::List BhatList = Rcpp::List::create(Bhat);
  Rcpp::List LambdaHatList = Rcpp::List::create(LambdaHat);
  arma::mat BhatInd = BhatList[0];
  arma::mat LambdaInd = LambdaHatList[0];
  arma::mat Sig1 = arma::inv(BhatInd * arma::trans(BhatInd));
  arma::mat Sig2 = arma::inv(BhatInd * (LambdaInd * arma::trans(BhatInd)));
  arma::mat GLS11 = arma::kron((Zt.cols(0, TB-2) * arma::trans(Zt.cols(0, TB-2))), Sig1);
  arma::mat GLS12 = arma::kron(Zt.cols(TB-1, Zt.n_cols-1) * arma::trans(Zt.cols(TB-1, Zt.n_cols-1)), Sig2);
  arma::mat GLS1 = arma::inv(GLS11 + GLS12);

  for(int i = 0; i < TB-1; ++i){
    GLS21.col(i) = arma::kron(Zt.col(i), Sig1) * y.col(i);
  }
  for(int i = TB-1; i < Zt.n_cols; ++i){
    GLS22.col(i) = arma::kron(Zt.col(i), Sig2) * y.col(i);
  }


  arma::mat GLS21sums = arma::sum(GLS21, 1);
  arma::mat GLS22sums = arma::sum(GLS22, 1);
  arma::mat GLS2 = GLS21sums + GLS22sums;
  arma::mat GLSHat = GLS1 * GLS2;
  term1 = reshape(kron(arma::trans(Zt), kMat) * GLSHat, k, Zt.n_cols);
  arma::mat ugls = arma::trans(y) - arma::trans(term1);
  arma::mat resid1gls = ugls.rows(0, TB-2);
  arma::mat resid2gls = ugls.rows(TB-1, Zt.n_cols-1);
  arma::mat Sigma_hat1gls =  (arma::trans(resid1gls) * resid1gls) / (TB-1);
  arma::mat Sigma_hat2gls = (arma::trans(resid2gls) * resid2gls) / (Zt.n_cols-TB+1);



  Rcpp::Environment stats("package:stats");
  Rcpp::Function nlm = stats["nlm"];


  Rcpp::List MLEgls = nlm(Rcpp::_["f"] = Rcpp::InternalFunction(LikelihoodCV),
                            Rcpp::_["p"] = S,
                            //Rcpp::_["method"] = "Nelder-Mead",
                            Rcpp::_["hessian"] = "T",
                            Rcpp::_["iterlim"] = 150,
                            Rcpp::_["Tob"] = Tob,
                            Rcpp::_["TB"] = TB,
                            Rcpp::_["Sigma_hat1"] = Sigma_hat1gls,
                            Rcpp::_["k"] = k,
                            Rcpp::_["Sigma_hat2"] = Sigma_hat2gls,
                            Rcpp::_["RestrictionMatrix"] = RestrictionMatrix,
                            Rcpp::_["restrictions"] = restrictions);

  //  arma::vec MLEestimate = Rcpp::as<arma::vec>(MLEgls[0]);
  //   return bfgs(S, LikelihoodCV, );
  return MLEgls;
}

/*** R
# Stest = c(0.54044505,  0.77713731, -0.01117899, -0.57977230,  1.07856594,  0.10094522,  0.21125397,
#           0.14164634,  0.73146666,1,1,1)
#   SigmaHat1Test = matrix(c(0.672838988449117,-0.175394202330158,0.0899592354678499,-0.175394202330158,1.78732514711838,
#                            0.203802480028035,0.0899592354678499,0.203802480028035,0.545361197859883), ncol = 3, nrow =3)
#   SigmaHat2Test = matrix(c(0.296478489821277,0.0479470127381236,0.157288545425978,0.0479470127381236,0.672927000646214,
#                            0.148479337390872,0.157288545425978,0.148479337390872,0.619919632740953), ncol = 3, nrow =3)
#
#   likelihood_cv <- function(S, Tob, TB, Sigma_hat1, k, Sigma_hat2, restriction_matrix, restrictions) {
#
#     if(!is.null(restriction_matrix)){
#       if(!is.matrix(restriction_matrix)){
#         stop("Please provide a valid input matrix")
#       }
#       naElements <- is.na(restriction_matrix)
#         toFillMatrix <- restriction_matrix
#         toFillMatrix[naElements] <- S[1:sum(naElements)]
#       W <- toFillMatrix
#     }else{
#       W <- matrix(S[1:(k*k)], nrow = k)
#
#       restrictions <- 0
#     }
#
#     Psi <- diag(S[((k*k+1) - restrictions):((k*k+k)-restrictions)])
#
#       MW <- det(tcrossprod(W))
#       MW2 <- det(W %*% tcrossprod(Psi, W))
#       MMM <- tcrossprod(W)
#       MMM2 <- W %*% tcrossprod(Psi, W)
#
#       if(any(Psi < 0) | MW < 0.01 |  MW2 < 0.01){
#         return(1e25)
#       }
#
#       L <- suppressWarnings(-(((TB - 1) / 2) * (log(MW) + sum(diag((Sigma_hat1 %*% solve(MMM)))))) -
#         (((Tob - TB + 1) / 2) * (log(MW2) + sum(diag((Sigma_hat2 %*% solve(MMM2)))))))
#         return(-L)
#
#   }
#
#
# MLEgls_R = nlm(f = likelihood_cv, p = Stest, k = 3, TB = 54, Sigma_hat1 = SigmaHat1Test,
#                Sigma_hat2 = SigmaHat2Test, Tob = 169, hessian = T, restriction_matrix = NULL,
#                restrictions = 0, iterlim = 150)
#
#   MLEgls_Roptim = optim(par = Stest, fn = likelihood_cv,k = 3, TB = 54, Sigma_hat1 = SigmaHat1Test,
#                         Sigma_hat2 = SigmaHat2Test, Tob = 169,restriction_matrix = NULL,
#                         restrictions = 0,
#                         method = "Nelder-Mead",
#                         hessian = TRUE)
#
#
#   MLEgls_rccp = nlm_rcpp(Stest, 169, 54, SigmaHat1Test, 3, SigmaHat2Test, matrix(rep(NA,9), ncol = 3), 0)
#
#   library(testthat)
#   all.equal(MLEgls_R$minimum, MLEgls_rccp$minimum)
#   all.equal(MLEgls_R$estimate, MLEgls_rccp$estimate)


  */

