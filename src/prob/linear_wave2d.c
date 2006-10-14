#include "copyright.h"
/*==============================================================================
 * FILE: linear_wave2d.c
 *
 * PURPOSE: Problem generator for linear wave convergence tests in 2D.  In 2D,
 *   the angle the wave propagates to the grid is automatically computed
 *   to be tan^{-1} (Y/X), so that periodic boundary conditions can be used,
 *   and the size of the box is automatically rescaled so that the fast wave
 *   crossing time (across a diagonal) is one-half, the Alfven wave crossing
 *   time is one, and the slow wave crossing time is two.
 *
 *   Note angle=0 or 90 [grid aligned waves] is not allowed in this routine.
 *   Use linear_wave1d for grid aligned wave on 1D/2D/3D grids.
 *
 *   Can be used for either standing (problem/vflow=1.0) or travelling
 *   (problem/vflow=0.0) waves.
 *
 * USERWORK_AFTER_LOOP function computes L1 error norm in solution by comparing
 *   to initial conditions.  Problem must be evolved for an integer number of
 *   wave periods for this to work.
 *============================================================================*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "athena.h"
#include "globals.h"
#include "prototypes.h"

/* Initial solution, shared with Userwork_after_loop to compute L1 error */
static Gas ***Soln=NULL;
static int wave_flag;

/*----------------------------------------------------------------------------*/
/* problem:   */

void problem(Grid *pGrid)
{
  int i=0,j=0,k=0;
  int is,ie,iu,js,je,ju,ks,ke,n,m,nx1,nx2,nx3,Nx1,Nx2;
  Real amp,vflow,angle;
  Real d0,p0,u0,v0,w0,h0;
  Real x1,x2,x3,r,ev[NWAVE],rem[NWAVE][NWAVE],lem[NWAVE][NWAVE];
  Real x1max,x2max,lambda;
#ifdef MHD
  Real bx0,by0,bz0,xfact,yfact;
  Real **az;
#endif /* MHD */
  is = pGrid->is; ie = pGrid->ie;
  js = pGrid->js; je = pGrid->je;
  ks = pGrid->ks; ke = pGrid->ke;
  nx1 = (ie-is)+1 + 2*nghost;
  nx2 = (je-js)+1 + 2*nghost;
  nx3 = (ke-ks)+1 + 2*nghost;

/* NOTE: For parallel calculations Nx1 != nx1 and Nx2 != nx2 */

  Nx1 = par_geti("grid","Nx1");
  Nx2 = par_geti("grid","Nx2");
  if (Nx1 == 1 || Nx2 == 1) {
    ath_error("[linear_wave2d]: this test only works with Nx1 & Nx2 > 1\n");
  }

/* allocate memory for solution and vector potential */

#ifdef MHD
  if ((az = (Real**)calloc_2d_array(nx2, nx1, sizeof(Real))) == NULL)
    ath_error("[linear_wave2d]: Error allocating memory for \"az\"\n");
#endif /* MHD */

  if ((Soln = (Gas***)calloc_3d_array(nx3,nx2,nx1,sizeof(Gas))) == NULL)
    ath_error("[linear_wave2d]: Error allocating memory\n");

/* Read initial conditions */

  wave_flag = par_geti("problem","wave_flag");
  amp = par_getd("problem","amp");
  vflow = par_getd("problem","vflow");

/* Set angle, dynamically resize grid so length of diagonal is one */

  angle = atan((Nx1*pGrid->dx1)/(Nx2*pGrid->dx2));
  x1max = sin(angle);
  x2max = cos(angle);
  lambda = x2max*sin(angle);

  pGrid->dx1 = x1max/(Real)Nx1;
  pGrid->dx2 = x2max/(Real)Nx2;

/* Get eigenmatrix, where the quantities u0 and bx0 are parallel to the
 * wavevector, and v0,w0,by0,bz0 are perpendicular. */

  d0 = 1.0;
#ifndef ISOTHERMAL
  p0 = 1.0/Gamma;
  u0 = vflow*sqrt(Gamma*p0/d0);
#else
  u0 = vflow*Iso_csound;
#endif
  v0 = 0.0;
  w0 = 0.0;
#ifdef MHD
  bx0 = 1.0;
  by0 = sqrt(2.0);
  bz0 = 0.5;
  xfact = 0.0;
  yfact = 1.0;
#endif

  for (n=0; n<NWAVE; n++) {
    for (m=0; m<NWAVE; m++) {
      rem[n][m] = 0.0;
      lem[n][m] = 0.0;
    }
  }

#ifdef HYDRO
#ifdef ISOTHERMAL
  esys_roe_iso_hyd(u0,v0,w0,   ev,rem,lem);
#else
  h0 = ((p0/Gamma_1 + 0.5*d0*(u0*u0+v0*v0+w0*w0)) + p0)/d0;
  esys_roe_adb_hyd(u0,v0,w0,h0,ev,rem,lem);
  printf("Ux - Cs = %e, %e\n",ev[0],rem[0][wave_flag]);
  printf("Ux      = %e, %e\n",ev[1],rem[1][wave_flag]);
  printf("Ux + Cs = %e, %e\n",ev[4],rem[4][wave_flag]);
#endif /* ISOTHERMAL */
#endif /* HYDRO */

#ifdef MHD
#if defined(ISOTHERMAL)
  esys_roe_iso_mhd(d0,u0,v0,w0,bx0,by0,bz0,xfact,yfact,ev,rem,lem);
  printf("Ux - Cf = %e, %e\n",ev[0],rem[0][wave_flag]);
  printf("Ux - Ca = %e, %e\n",ev[1],rem[1][wave_flag]);
  printf("Ux - Cs = %e, %e\n",ev[2],rem[2][wave_flag]);
  printf("Ux + Cs = %e, %e\n",ev[3],rem[3][wave_flag]);
  printf("Ux + Ca = %e, %e\n",ev[4],rem[4][wave_flag]);
  printf("Ux + Cf = %e, %e\n",ev[5],rem[5][wave_flag]);
#else
  h0 = ((p0/Gamma_1+0.5*(bx0*bx0+by0*by0+bz0*bz0)+0.5*d0*(u0*u0+v0*v0+w0*w0))
               + (p0+0.5*(bx0*bx0+by0*by0+bz0*bz0)))/d0;
  esys_roe_adb_mhd(d0,u0,v0,w0,h0,bx0,by0,bz0,xfact,yfact,ev,rem,lem);
  printf("Ux - Cf = %e, %e\n",ev[0],rem[0][wave_flag]);
  printf("Ux - Ca = %e, %e\n",ev[1],rem[1][wave_flag]);
  printf("Ux - Cs = %e, %e\n",ev[2],rem[2][wave_flag]);
  printf("Ux      = %e, %e\n",ev[3],rem[3][wave_flag]);
  printf("Ux + Cs = %e, %e\n",ev[4],rem[4][wave_flag]);
  printf("Ux + Ca = %e, %e\n",ev[5],rem[5][wave_flag]);
  printf("Ux + Cf = %e, %e\n",ev[6],rem[6][wave_flag]);
#endif /* ISOTHERMAL */
#endif /* MHD */

/* Now initialize 2D solution */
/* Fields are initialized using vector potential in 2D (except B3) */

#ifdef MHD
  for (j=js; j<=je+1; j++) {
    for (i=is; i<=ie+1; i++) {
      cc_pos(pGrid,i,j,k,&x1,&x2,&x3);
      x1 = x1 - 0.5*pGrid->dx1;
      x2 = x2 - 0.5*pGrid->dx2;
      r = (x1*cos(angle) + x2*sin(angle))/lambda;

      az[j][i] = -bx0*(x1*sin(angle) - x2*cos(angle))
                - by0*(x1*cos(angle) + x2*sin(angle))
                + amp*lambda*cos(2.0*PI*r)*rem[NWAVE-2][wave_flag]/(2.0*PI);
    }
  }
#endif /* MHD */

/* The initial solution is stored into global 2D arrays so the error norm can
 * be computed at the end of the run.  After calculating these arrays, the
 * conserved variables are set equal to them */

  for (k=ks; k<=ke; k++) {
  for (j=js; j<=je; j++) {
  for (i=is; i<=ie; i++) {
    cc_pos(pGrid,i,j,k,&x1,&x2,&x3);
    r = (x1*cos(angle) + x2*sin(angle))/lambda;

    Soln[k][j][i].d = d0 + amp*sin(2.0*PI*r)*rem[0][wave_flag];

#ifndef ISOTHERMAL
#ifdef HYDRO
    Soln[k][j][i].E = p0/Gamma_1 + 0.5*d0*u0*u0 
                    + amp*sin(2.0*PI*r)*rem[4][wave_flag];
#else
    Soln[k][j][i].E = p0/Gamma_1 + 0.5*d0*u0*u0 + 0.5*(bx0*bx0+by0*by0+bz0*bz0)
                    + amp*sin(2.0*PI*r)*rem[4][wave_flag];
#endif /* HYDRO */
#endif /* ISOTHERMAL */

    Soln[k][j][i].M1 = d0*vflow*cos(angle)
                    + amp*sin(2.0*PI*r)*rem[1][wave_flag]*cos(angle)
                    - amp*sin(2.0*PI*r)*rem[2][wave_flag]*sin(angle);
    Soln[k][j][i].M2 = d0*vflow*sin(angle)
                    + amp*sin(2.0*PI*r)*rem[1][wave_flag]*sin(angle)
                    + amp*sin(2.0*PI*r)*rem[2][wave_flag]*cos(angle);

#ifdef MHD
    pGrid->B1i[k][j][i] = (az[j+1][i] - az[j][i])/pGrid->dx2;
    pGrid->B2i[k][j][i] =-(az[j][i+1] - az[j][i])/pGrid->dx1;
#endif /* MHD */

    Soln[k][j][i].M3 = amp*sin(2.0*PI*r)*rem[3][wave_flag];

#ifdef MHD
    Soln[k][j][i].B3c = bz0 + amp*sin(2.0*PI*r)*rem[NWAVE-1][wave_flag];
#endif /* MHD */

  }}}

  for (k=ks; k<=ke; k++) {
    for (j=js; j<=je; j++) {
      pGrid->B1i[k][j][ie+1] = pGrid->B1i[k][j][is];
    }
    for (i=is; i<=ie; i++) {
      pGrid->B2i[k][je+1][i] = pGrid->B2i[k][js][i];
    }
  }

/* compute cell-centered fields for 2D problems */

#ifdef MHD
  for (k=ks; k<=ke; k++) {
  for (j=js; j<=je; j++) {
  for (i=is; i<=ie; i++) {
     Soln[k][j][i].B1c = 0.5*(pGrid->B1i[k][j][i] + pGrid->B1i[k][j][i+1]);
     Soln[k][j][i].B2c = 0.5*(pGrid->B2i[k][j][i] + pGrid->B2i[k][j+1][i]);
  }}}
#endif /* MHD */

/* Now set initial conditions to 2d wave solution */ 

  for (k=ks; k<=ke; k++) {
  for (j=js; j<=je; j++) {
  for (i=is; i<=ie; i++) {
    pGrid->U[k][j][i].d = Soln[k][j][i].d;
#ifndef ISOTHERMAL
    pGrid->U[k][j][i].E = Soln[k][j][i].E;
#endif /* ISOTHERMAL */
    pGrid->U[k][j][i].M1 = Soln[k][j][i].M1;
    pGrid->U[k][j][i].M2 = Soln[k][j][i].M2;
    pGrid->U[k][j][i].M3 = Soln[k][j][i].M3;
#ifdef MHD
    pGrid->U[k][j][i].B1c = Soln[k][j][i].B1c;
    pGrid->U[k][j][i].B2c = Soln[k][j][i].B2c;
    pGrid->U[k][j][i].B3c = Soln[k][j][i].B3c;
#endif /* MHD */
  }}}
#ifdef MHD
  if (pGrid->Nx3 > 1) {
    for (k=ks; k<=ke+1; k++) {
      for (j=js; j<=je; j++) {
        for (i=is; i<=ie; i++) {
          pGrid->B3i[k][j][i] = Soln[k][j][i].B3c;
	}
      }
    }
  }

  free_2d_array((void**)az);
#endif /* MHD */

  return;
}

/*==============================================================================
 * PROBLEM USER FUNCTIONS:
 * problem_write_restart() - writes problem-specific user data to restart files
 * problem_read_restart()  - reads problem-specific user data from restart files
 * get_usr_expr()          - sets pointer to expression for special output data
 * Userwork_in_loop        - problem specific work IN     main loop
 * Userwork_after_loop     - problem specific work AFTER  main loop
 *----------------------------------------------------------------------------*/

void problem_write_restart(Grid *pG, FILE *fp)
{
  return;
}

void problem_read_restart(Grid *pG, FILE *fp)
{
  return;
}

Gasfun_t get_usr_expr(const char *expr)
{
  return NULL;
}

void Userwork_in_loop(Grid *pGrid)
{
}

/*---------------------------------------------------------------------------
 * Userwork_after_loop: computes L1-error in linear waves,
 * ASSUMING WAVE HAS PROPAGATED AN INTEGER NUMBER OF PERIODS
 * Must set parameters in input file appropriately so that this is true
 */

void Userwork_after_loop(Grid *pGrid)
{
  int i=0,j=0,k=0;
  int is,ie,js,je,ks,ke;
  Real rms_error=0.0;
  Gas error,total_error;
  FILE *fp;
  char *fname;
  int Nx1, Nx2, Nx3, count;
#if defined MPI_PARALLEL
  double err[8], tot_err[8];
  int mpi_err;
#endif

  total_error.d = 0.0;
  total_error.M1 = 0.0;
  total_error.M2 = 0.0;
  total_error.M3 = 0.0;
#ifdef MHD
  total_error.B1c = 0.0;
  total_error.B2c = 0.0;
  total_error.B3c = 0.0;
#endif /* MHD */
#ifndef ISOTHERMAL
  total_error.E = 0.0;
#endif /* ISOTHERMAL */

/* compute L1 error in each variable, and rms total error */

  is = pGrid->is; ie = pGrid->ie;
  js = pGrid->js; je = pGrid->je;
  ks = pGrid->ks; ke = pGrid->ke;
  for (k=ks; k<=ke; k++) {
  for (j=js; j<=je; j++) {
    error.d = 0.0;
    error.M1 = 0.0;
    error.M2 = 0.0;
    error.M3 = 0.0;
#ifdef MHD
    error.B1c = 0.0;
    error.B2c = 0.0;
    error.B3c = 0.0;
#endif /* MHD */
#ifndef ISOTHERMAL
    error.E = 0.0;
#endif /* ISOTHERMAL */

    for (i=is; i<=ie; i++) {
      error.d   += fabs(pGrid->U[k][j][i].d   - Soln[k][j][i].d );
      error.M1  += fabs(pGrid->U[k][j][i].M1  - Soln[k][j][i].M1);
      error.M2  += fabs(pGrid->U[k][j][i].M2  - Soln[k][j][i].M2);
      error.M3  += fabs(pGrid->U[k][j][i].M3  - Soln[k][j][i].M3); 
#ifdef MHD
      error.B1c += fabs(pGrid->U[k][j][i].B1c - Soln[k][j][i].B1c);
      error.B2c += fabs(pGrid->U[k][j][i].B2c - Soln[k][j][i].B2c);
      error.B3c += fabs(pGrid->U[k][j][i].B3c - Soln[k][j][i].B3c);
#endif /* MHD */
#ifndef ISOTHERMAL
      error.E   += fabs(pGrid->U[k][j][i].E   - Soln[k][j][i].E );
#endif /* ISOTHERMAL */
    }

    total_error.d += error.d;
    total_error.M1 += error.M1;
    total_error.M2 += error.M2;
    total_error.M3 += error.M3;
#ifdef MHD
    total_error.B1c += error.B1c;
    total_error.B2c += error.B2c;
    total_error.B3c += error.B3c;
#endif /* MHD */
#ifndef ISOTHERMAL
    total_error.E += error.E;
#endif /* ISOTHERMAL */
  }}

#if defined MPI_PARALLEL
  Nx1 = pDomain.ixe - pDomain.ixs + 1;
  Nx2 = pDomain.jxe - pDomain.jxs + 1;
  Nx3 = pDomain.kxe - pDomain.kxs + 1;
#else
  Nx1 = ie - is + 1;
  Nx2 = je - js + 1;
  Nx3 = ke - ks + 1;
#endif

  count = Nx1*Nx2*Nx3;

#ifdef MPI_PARALLEL 

/* Now we have to use an All_Reduce to get the total error over all the MPI
 * grids.  Begin by copying the error into the err[] array */

  err[0] = total_error.d;
  err[1] = total_error.M1;
  err[2] = total_error.M2;
  err[3] = total_error.M3;
#ifdef MHD
  err[4] = total_error.B1c;
  err[5] = total_error.B2c;
  err[6] = total_error.B3c;
#endif /* MHD */
#ifndef ISOTHERMAL
  err[7] = total_error.E;
#endif /* ISOTHERMAL */

/* Sum up the Computed Error */
  mpi_err = MPI_Reduce(err, tot_err, 8,
		       MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  if(mpi_err)
    ath_error("[Userwork_after_loop]: MPI_Reduce call returned error = %d\n",
	      mpi_err);

/* If I'm the parent, copy the sum back to the total_error variable */
  if(pGrid->my_id == 0){ /* I'm the parent */
    total_error.d   = tot_err[0];
    total_error.M1  = tot_err[1];
    total_error.M2  = tot_err[2];
    total_error.M3  = tot_err[3];
#ifdef MHD
    total_error.B1c = tot_err[4];
    total_error.B2c = tot_err[5];
    total_error.B3c = tot_err[6];
#endif /* MHD */
#ifndef ISOTHERMAL
    total_error.E   = tot_err[7];
#endif /* ISOTHERMAL */
  }
  else return; /* The child grids do not do any of the following code */

#endif /* MPI_PARALLEL */

/* Compute RMS error over all variables, and print out */

  rms_error = SQR(total_error.d) + SQR(total_error.M1) + SQR(total_error.M2)
                + SQR(total_error.M3);
#ifdef MHD
  rms_error += SQR(total_error.B1c) + SQR(total_error.B2c) 
               + SQR(total_error.B3c);
#endif /* MHD */
#ifndef ISOTHERMAL
  rms_error += SQR(total_error.E);
#endif /* ISOTHERMAL */
  rms_error = sqrt(rms_error)/(double)count;


/* Print error to file "LinWave-errors.#.dat", where #=wave_flag  */

  fname = fname_construct("LinWave-errors",1,wave_flag,NULL,"dat");

/* The file exists -- reopen the file in append mode */
  if((fp=fopen(fname,"r")) != NULL){
    if((fp = freopen(fname,"a",fp)) == NULL){
      ath_error("[Userwork_after_loop]: Unable to reopen file.\n");
      free(fname);
      return;
    }
  }
/* The file does not exist -- open the file in write mode */
  else{
    if((fp = fopen(fname,"w")) == NULL){
      ath_error("[Userwork_after_loop]: Unable to open file.\n");
      free(fname);
      return;
    }
/* Now write out some header information */
    fprintf(fp,"# Nx1  Nx2  Nx3  RMS-Error  d  M1  M2  M3");
#ifndef ISOTHERMAL
    fprintf(fp,"  E");
#endif /* ISOTHERMAL */
#ifdef MHD
    fprintf(fp,"  B1c  B2c  B3c");
#endif /* MHD */
    fprintf(fp,"\n#\n");
  }

  fprintf(fp,"%d  %d  %d  %e",Nx1,Nx2,Nx3,rms_error);

  fprintf(fp,"  %e  %e  %e  %e",
	  (total_error.d/(double) count),
	  (total_error.M1/(double)count),
	  (total_error.M2/(double)count),
	  (total_error.M3/(double)count));

#ifndef ISOTHERMAL
  fprintf(fp,"  %e",(total_error.E/(double)count));
#endif /* ISOTHERMAL */

#ifdef MHD
  fprintf(fp,"  %e  %e  %e",
	  (total_error.B1c/(double)count),
	  (total_error.B2c/(double)count),
	  (total_error.B3c/(double)count));
#endif /* MHD */

  fprintf(fp,"\n");

  fclose(fp);
  free(fname);

  return;
}