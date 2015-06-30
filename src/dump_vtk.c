#include "copyright.h"
/*============================================================================*/
/*! \file dump_vtk.c
 *  \brief Function to write a dump in VTK "legacy" format.
 *
 * PURPOSE: Function to write a dump in VTK "legacy" format.  With SMR,
 *   dumps are made for all levels and domains, unless nlevel and ndomain are
 *   specified in <output> block.  Works for BOTH conserved and primitives.
 *
 * CONTAINS PUBLIC FUNCTIONS:
 * - dump_vtk() - writes VTK dump (all variables).                            */
/*============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "athena.h"
#include "globals.h"
#include "prototypes.h"
#ifdef PARTICLES
#include "particles/particle.h"
#endif

/*----------------------------------------------------------------------------*/
/*! \fn void dump_vtk(MeshS *pM, OutputS *pOut)
 *  \brief Writes VTK dump (all variables).                                   */

void dump_vtk(MeshS *pM, OutputS *pOut)
{
  GridS *pGrid;
  PrimS ***W;
  FILE *pfile;
  char *fname,*plev=NULL,*pdom=NULL;
  char levstr[8],domstr[8];
/* Upper and Lower bounds on i,j,k for data dump */
  int i,j,k,il,iu,jl,ju,kl,ku,nl,nd;
  int big_end = ath_big_endian();
  int ndata0,ndata1,ndata2;
  float *data;   /* points to 3*ndata0 allocated floats */
  double x1, x2, x3;
#ifdef POINT_SOURCE
  int ipf;
#endif
#if defined (RADIATION_TRANSFER) || defined (FULL_RADIATION_TRANSFER)
  RadGridS *pRG;
  int ifr,nf;
  int irl,iru,jrl,jru,krl,kru;
#ifdef WRITE_GHOST_CELLS
  int inkloop, injloop, iniloop;
  int ir,jr,kr;
#endif
#endif
#if (NSCALARS > 0)
  int n;
#endif

/* Loop over all Domains in Mesh, and output Grid data */

  for (nl=0; nl<(pM->NLevels); nl++){
    for (nd=0; nd<(pM->DomainsPerLevel[nl]); nd++){
      if (pM->Domain[nl][nd].Grid != NULL){

/* write files if domain and level match input, or are not specified (-1) */
        if ((pOut->nlevel == -1 || pOut->nlevel == nl) &&
            (pOut->ndomain == -1 || pOut->ndomain == nd)){
          pGrid = pM->Domain[nl][nd].Grid;

          il = pGrid->is, iu = pGrid->ie;
          jl = pGrid->js, ju = pGrid->je;
          kl = pGrid->ks, ku = pGrid->ke;

#ifdef RADIATION_TRANSFER
          if ( (radt_mode == 0) || (radt_mode == 2) ) {
            pRG = pM->Domain[nl][nd].RadGrid;
            nf = pRG->nf;
            irl = pRG->is, iru = pRG->ie;
            jrl = pRG->js, jru = pRG->je;
            krl = pRG->ks, kru = pRG->ke;
          }
#endif
#ifdef FULL_RADIATION_TRANSFER
          pRG = pM->Domain[nl][nd].RadGrid;
          nf = pRG->nf;
          irl = pRG->is, iru = pRG->ie;
          jrl = pRG->js, jru = pRG->je;
          krl = pRG->ks, kru = pRG->ke;
#endif
#ifdef WRITE_GHOST_CELLS
          iu = pGrid->ie + nghost;
          il = pGrid->is - nghost;

          if(pGrid->Nx[1] > 1) {
            ju = pGrid->je + nghost;
            jl = pGrid->js - nghost;
          }

          if(pGrid->Nx[2] > 1) {
            ku = pGrid->ke + nghost;
            kl = pGrid->ks - nghost;
          }
#endif /* WRITE_GHOST_CELLS */

          ndata0 = iu-il+1;
          ndata1 = ju-jl+1;
          ndata2 = ku-kl+1;


/* calculate primitive variables, if needed */

          if(strcmp(pOut->out,"prim") == 0) {
            if((W = (PrimS***)calloc_3d_array(ndata2,ndata1,ndata0,sizeof(PrimS)))
               == NULL) ath_error("[dump_vtk]: failed to allocate Prim array\n");

            for (k=kl; k<=ku; k++) {
              for (j=jl; j<=ju; j++) {
                for (i=il; i<=iu; i++) {
                  W[k-kl][j-jl][i-il] = Cons_to_Prim(&(pGrid->U[k][j][i]));
                }}}
          }

/* construct filename, open file */
          if (nl>0) {
            plev = &levstr[0];
            sprintf(plev,"lev%d",nl);
          }
          if (nd>0) {
            pdom = &domstr[0];
            sprintf(pdom,"dom%d",nd);
          }
          if((fname = ath_fname(plev,pM->outfilename,plev,pdom,num_digit,
                                pOut->num,NULL,"vtk")) == NULL){
            ath_error("[dump_vtk]: Error constructing filename\n");
          }

          if((pfile = fopen(fname,"w")) == NULL){
            ath_error("[dump_vtk]: Unable to open vtk dump file\n");
            return;
          }
          free(fname);

/* Allocate memory for temporary array of floats */

          if((data = (float *)malloc(20*ndata0*sizeof(float))) == NULL){
            ath_error("[dump_vtk]: malloc failed for temporary array\n");
            return;
          }

/* There are five basic parts to the VTK "legacy" file format.  */
/*  1. Write file version and identifier */

          fprintf(pfile,"# vtk DataFile Version 2.0\n");

/*  2. Header */

          if (strcmp(pOut->out,"cons") == 0){
            fprintf(pfile,"CONSERVED vars at time= %e, level= %i, domain= %i\n",
                    pGrid->time,nl,nd);
          } else if(strcmp(pOut->out,"prim") == 0) {
            fprintf(pfile,"PRIMITIVE vars at time= %e, level= %i, domain= %i\n",
                    pGrid->time,nl,nd);
          }

/*  3. File format */

          fprintf(pfile,"BINARY\n");

/*  4. Dataset structure */

/* Set the Grid origin */

          fc_pos(pGrid, il, jl, kl, &x1, &x2, &x3);

          fprintf(pfile,"DATASET STRUCTURED_POINTS\n");
          if (pGrid->Nx[1] == 1) {
            fprintf(pfile,"DIMENSIONS %d %d %d\n",iu-il+2,1,1);
          } else {
            if (pGrid->Nx[2] == 1) {
              fprintf(pfile,"DIMENSIONS %d %d %d\n",iu-il+2,ju-jl+2,1);
            } else {
              fprintf(pfile,"DIMENSIONS %d %d %d\n",iu-il+2,ju-jl+2,ku-kl+2);
            }
          }
          fprintf(pfile,"ORIGIN %e %e %e \n",x1,x2,x3);
          fprintf(pfile,"SPACING %e %e %e \n",pGrid->dx1,pGrid->dx2,pGrid->dx3);

/*  5. Data  */

          fprintf(pfile,"CELL_DATA %d \n", (iu-il+1)*(ju-jl+1)*(ku-kl+1));

/* Write density */

          fprintf(pfile,"SCALARS density float\n");
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                if (strcmp(pOut->out,"cons") == 0){
                  data[i-il] = (float)pGrid->U[k][j][i].d;
                } else if(strcmp(pOut->out,"prim") == 0) {
                  data[i-il] = (float)W[k-kl][j-jl][i-il].d;
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }

/* Write momentum or velocity */

          if (strcmp(pOut->out,"cons") == 0){
            fprintf(pfile,"\nVECTORS momentum float\n");
          } else if(strcmp(pOut->out,"prim") == 0) {
            fprintf(pfile,"\nVECTORS velocity float\n");
          }
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                if (strcmp(pOut->out,"cons") == 0){
                  data[3*(i-il)  ] = (float)pGrid->U[k][j][i].M1;
                  data[3*(i-il)+1] = (float)pGrid->U[k][j][i].M2;
                  data[3*(i-il)+2] = (float)pGrid->U[k][j][i].M3;
                } else if(strcmp(pOut->out,"prim") == 0) {
                  data[3*(i-il)  ] = (float)W[k-kl][j-jl][i-il].V1;
                  data[3*(i-il)+1] = (float)W[k-kl][j-jl][i-il].V2;
                  data[3*(i-il)+2] = (float)W[k-kl][j-jl][i-il].V3;
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
            }
          }

/* Write total energy or pressure */

#ifndef BAROTROPIC
          if (strcmp(pOut->out,"cons") == 0){
            fprintf(pfile,"\nSCALARS total_energy float\n");
          } else if(strcmp(pOut->out,"prim") == 0) {
            fprintf(pfile,"\nSCALARS pressure float\n");
          }
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                if (strcmp(pOut->out,"cons") == 0){
                  data[i-il] = (float)pGrid->U[k][j][i].E;
                } else if(strcmp(pOut->out,"prim") == 0) {
                  data[i-il] = (float)W[k-kl][j-jl][i-il].P;
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }
#endif

/* Write cell centered B */

#if defined(MHD) || defined(RADIATION_MHD)
          fprintf(pfile,"\nVECTORS cell_centered_B float\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                data[3*(i-il)] = (float)pGrid->U[k][j][i].B1c;
                data[3*(i-il)+1] = (float)pGrid->U[k][j][i].B2c;
                data[3*(i-il)+2] = (float)pGrid->U[k][j][i].B3c;
              }
              if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
            }
          }
#endif

#if defined(RADIATION_HYDRO) || defined(RADIATION_MHD)
          fprintf(pfile,"\nSCALARS rad_energy float\n");
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                if (strcmp(pOut->out,"cons") == 0){
                  data[i-il] = (float)pGrid->U[k][j][i].Er;
                } else if(strcmp(pOut->out,"prim") == 0) {
                  data[i-il] = (float)W[k-kl][j-jl][i-il].Er;
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }

          fprintf(pfile,"\nVECTORS rad_flux float\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                data[3*(i-il)] = (float)pGrid->U[k][j][i].Fr1;
                data[3*(i-il)+1] = (float)pGrid->U[k][j][i].Fr2;
                data[3*(i-il)+2] = (float)pGrid->U[k][j][i].Fr3;
              }
              if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
            }
          }

#ifdef RADIATION_TRANSFER
          fprintf(pfile,"\nTENSORS Edd_tensor float\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                data[9*(i-il)] = (float)pGrid->U[k][j][i].Edd_11;
                data[9*(i-il)+1] = (float)pGrid->U[k][j][i].Edd_21;
                data[9*(i-il)+2] = (float)pGrid->U[k][j][i].Edd_31;
                data[9*(i-il)+3] = (float)pGrid->U[k][j][i].Edd_21;
                data[9*(i-il)+4] = (float)pGrid->U[k][j][i].Edd_22;
                data[9*(i-il)+5] = (float)pGrid->U[k][j][i].Edd_32;
                data[9*(i-il)+6] = (float)pGrid->U[k][j][i].Edd_31;
                data[9*(i-il)+7] = (float)pGrid->U[k][j][i].Edd_32;
                data[9*(i-il)+8] = (float)pGrid->U[k][j][i].Edd_33;
              }
              if(!big_end) ath_bswap(data,sizeof(float),9*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
            }
          }

#endif


#endif

/* Write gravitational potential */

#ifdef SELF_GRAVITY
          fprintf(pfile,"\nSCALARS gravitational_potential float\n");
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                data[i-il] = (float)pGrid->Phi[k][j][i];
              }
              if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }
#endif

/* Write binned particle grid */

#ifdef PARTICLES
          if (pOut->out_pargrid) {
            fprintf(pfile,"\nSCALARS particle_density float\n");
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=kl; k<=ku; k++) {
              for (j=jl; j<=ju; j++) {
                for (i=il; i<=iu; i++) {
                  data[i-il] = pGrid->Coup[k][j][i].grid_d;
                }
                if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
            fprintf(pfile,"\nVECTORS particle_momentum float\n");
            for (k=kl; k<=ku; k++) {
              for (j=jl; j<=ju; j++) {
                for (i=il; i<=iu; i++) {
                  data[3*(i-il)] = pGrid->Coup[k][j][i].grid_v1;
                  data[3*(i-il)+1] = pGrid->Coup[k][j][i].grid_v2;
                  data[3*(i-il)+2] = pGrid->Coup[k][j][i].grid_v3;
                }
                if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
                fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
              }
            }
          }
#endif

#ifdef FULL_RADIATION_TRANSFER
#ifdef WRITE_GHOST_CELLS
/* Grid has more ghost cells than RadGrid so these need to be accounted for */
          jrl=jl;
          jru=ju;
          krl=kl;
          kru=ku;
          irl=il+nghost-Radghost;
          iru=iu-nghost+Radghost;

          if(pGrid->Nx[1] > 1) {
            jrl += nghost-Radghost;
            jru -= (nghost-Radghost);
          }
          if(pGrid->Nx[2] > 1) {
            krl += nghost-Radghost;
            kru -= (nghost-Radghost);
          }
/* Write frequency integrated moments of the intensities */
/* Write 0th moment integrated over frequency */
          fprintf(pfile,"\nSCALARS rad_J float\n");
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=kl; k<=ku; k++) {
            inkloop = (k >= krl) && (k <= kru);
            kr = k - krl;
            for (j=jl; j<=ju; j++) {
              injloop = (j >= jrl) && (j <= jru);
              jr = j - jrl;
              for (i=il; i<=iu; i++) {
                iniloop = (i >= irl) && (i <= iru);
                ir = i - irl;
                data[i-il] = 0.0;
                if(inkloop && injloop && iniloop) {
                  for (ifr=0; ifr<nf; ifr++) {
                    data[i-il] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].J);
                  }
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }
/* Write components of 1st moment integrated over frequency */
          fprintf(pfile,"\nVECTORS rad_H float\n");
          for (k=kl; k<=ku; k++) {
            inkloop = (k >= krl) && (k <= kru);
            kr = k - krl;
            for (j=jl; j<=ju; j++) {
              injloop = (j >= jrl) && (j <= jru);
              jr = j - jrl;
              for (i=il; i<=iu; i++) {
                iniloop = (i >= irl) && (i <= iru);
                ir = i - irl;
                data[3*(i-il)  ] = 0.0;
                data[3*(i-il)+1] = 0.0;
                data[3*(i-il)+2] = 0.0;
                if(inkloop && injloop && iniloop) {
                  for (ifr=0; ifr<nf; ifr++) {
                    data[3*(i-il)  ] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].H[0]);
                    data[3*(i-il)+1] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].H[1]);
                    data[3*(i-il)+2] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].H[2]);
                  }
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
            }
          }
/* Write components of 2nd moment integrated over frequency */
          fprintf(pfile,"\nTENSORS rad_K float\n");
          for (k=kl; k<=ku; k++) {
            inkloop = (k >= krl) && (k <= kru);
            kr = k - krl;
            for (j=jl; j<=ju; j++) {
              injloop = (j >= jrl) && (j <= jru);
              jr = j - jrl;
              for (i=il; i<=iu; i++) {
                iniloop = (i >= irl) && (i <= iru);
                ir = i - irl;
                data[9*(i-il)  ] = 0.0;
                data[9*(i-il)+1] = 0.0;
                data[9*(i-il)+2] = 0.0;
                data[9*(i-il)+3] = 0.0;
                data[9*(i-il)+4] = 0.0;
                data[9*(i-il)+5] = 0.0;
                data[9*(i-il)+6] = 0.0;
                data[9*(i-il)+7] = 0.0;
                data[9*(i-il)+8] = 0.0;
                if(inkloop && injloop && iniloop) {
                  for (ifr=0; ifr<nf; ifr++) {
                    data[9*(i-il)  ] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[0]);
                    data[9*(i-il)+1] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[1]);
                    data[9*(i-il)+2] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[3]);
                    data[9*(i-il)+3] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[1]);
                    data[9*(i-il)+4] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[2]);
                    data[9*(i-il)+5] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[4]);
                    data[9*(i-il)+6] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[3]);
                    data[9*(i-il)+7] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[4]);
                    data[9*(i-il)+8] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[kr][jr][ir][ifr].K[5]);
                  }
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),9*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
            }
          }


#else
/* Write frequency integrated moments of the intensities */
/* Write 0th moment integrated over frequency */
          fprintf(pfile,"\nSCALARS rad_J float\n");
          fprintf(pfile,"LOOKUP_TABLE default\n");
          for (k=krl; k<=kru; k++) {
            for (j=jrl; j<=jru; j++) {
              for (i=irl; i<=iru; i++) {
                data[i-irl] = 0.0;
                for (ifr=0; ifr<nf; ifr++) {
                  data[i-irl] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].J);
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),iru-irl+1);
              fwrite(data,sizeof(float),(size_t)ndata0,pfile);
            }
          }
/* Write components of 1st moment integrated over frequency */
          fprintf(pfile,"\nVECTORS rad_H float\n");
          for (k=krl; k<=kru; k++) {
            for (j=jrl; j<=jru; j++) {
              for (i=irl; i<=iru; i++) {
                data[3*(i-irl)  ] = 0.0;
                data[3*(i-irl)+1] = 0.0;
                data[3*(i-irl)+2] = 0.0;
                for (ifr=0; ifr<nf; ifr++) {
                  data[3*(i-irl)  ] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].H[0]);
                  data[3*(i-irl)+1] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].H[1]);
                  data[3*(i-irl)+2] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].H[2]);
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),3*(iru-irl+1));
              fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
            }
          }
/* Write components of 2nd moment integrated over frequency */
          fprintf(pfile,"\nTENSORS rad_K float\n");
          for (k=krl; k<=kru; k++) {
            for (j=jrl; j<=jru; j++) {
              for (i=irl; i<=iru; i++) {
                data[9*(i-irl)  ] = 0.0;
                data[9*(i-irl)+1] = 0.0;
                data[9*(i-irl)+2] = 0.0;
                data[9*(i-irl)+3] = 0.0;
                data[9*(i-irl)+4] = 0.0;
                data[9*(i-irl)+5] = 0.0;
                data[9*(i-irl)+6] = 0.0;
                data[9*(i-irl)+7] = 0.0;
                data[9*(i-irl)+8] = 0.0;
                for (ifr=0; ifr<nf; ifr++) {
                  data[9*(i-irl)  ] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[0]);
                  data[9*(i-irl)+1] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[1]);
                  data[9*(i-irl)+2] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[3]);
                  data[9*(i-irl)+3] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[1]);
                  data[9*(i-irl)+4] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[2]);
                  data[9*(i-irl)+5] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[4]);
                  data[9*(i-irl)+6] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[3]);
                  data[9*(i-irl)+7] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[4]);
                  data[9*(i-irl)+8] += (float)(4.0*PI*pRG->wnu[ifr]*pRG->R[k][j][i][ifr].K[5]);
                }
              }
              if(!big_end) ath_bswap(data,sizeof(float),9*(iru-irl+1));
              fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
            }
          }


#endif /* WRITE_GHOST_CELLS */
#endif /* FULL_RADIATION_TRANSFER */

#ifdef RADIATION_TRANSFER
/*Only dump J, H, K if RadGrid is used for integration */
          if ( (radt_mode == 0) || (radt_mode == 2) ) {
#ifdef WRITE_GHOST_CELLS
/* Grid has more ghost cells than RadGrid so these need to be accounted for */
            irl=il+nghost-1;
            iru=iu-nghost+1;
            jrl=jl;
            jru=ju;
            krl=kl;
            kru=ku;
            if(pGrid->Nx[1] > 1) {
              jrl += nghost-1;
              jru -= nghost-1;
            }
            if(pGrid->Nx[2] > 1) {
              krl += nghost-1;
              kru -= nghost-1;
            }
/* Write frequency integrated moments of the intensities */
/* Write 0th moment integrated over frequency */
            fprintf(pfile,"\nSCALARS rad_J float\n");
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=kl; k<=ku; k++) {
              inkloop = (k >= krl) && (k <= kru);
              kr = k - krl;
              for (j=jl; j<=ju; j++) {
                injloop = (j >= jrl) && (j <= jru);
                jr = j - jrl;
                for (i=il; i<=iu; i++) {
                  iniloop = (i >= irl) && (i <= iru);
                  ir = i - irl;
                  data[i-il] = 0.0;
                  if(inkloop && injloop && iniloop) {
                    for (ifr=0; ifr<nf; ifr++) {
                      data[i-il] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].J);
                    }
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
/* Write components of 1st moment integrated over frequency */
            fprintf(pfile,"\nVECTORS rad_H float\n");
            for (k=kl; k<=ku; k++) {
              inkloop = (k >= krl) && (k <= kru);
              kr = k - krl;
              for (j=jl; j<=ju; j++) {
                injloop = (j >= jrl) && (j <= jru);
                jr = j - jrl;
                for (i=il; i<=iu; i++) {
                  iniloop = (i >= irl) && (i <= iru);
                  ir = i - irl;
                  data[3*(i-il)  ] = 0.0;
                  data[3*(i-il)+1] = 0.0;
                  data[3*(i-il)+2] = 0.0;
                  if(inkloop && injloop && iniloop) {
                    for (ifr=0; ifr<nf; ifr++) {
                      data[3*(i-il)  ] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].H[0]);
                      data[3*(i-il)+1] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].H[1]);
                      data[3*(i-il)+2] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].H[2]);
                    }
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
                fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
              }
            }
#if !defined(RADIATION_HYDRO) &&  !defined(RADIATION_MHD)
/* Write components of 2nd moment integrated over frequency if
 * Eddington tensor is not written above. */
            fprintf(pfile,"\nTENSORS rad_K float\n");
            for (k=kl; k<=ku; k++) {
              inkloop = (k >= krl) && (k <= kru);
              kr = k - krl;
              for (j=jl; j<=ju; j++) {
                injloop = (j >= jrl) && (j <= jru);
                jr = j - jrl;
                for (i=il; i<=iu; i++) {
                  iniloop = (i >= irl) && (i <= iru);
                  ir = i - irl;
                  data[9*(i-il)  ] = 0.0;
                  data[9*(i-il)+1] = 0.0;
                  data[9*(i-il)+2] = 0.0;
                  data[9*(i-il)+3] = 0.0;
                  data[9*(i-il)+4] = 0.0;
                  data[9*(i-il)+5] = 0.0;
                  data[9*(i-il)+6] = 0.0;
                  data[9*(i-il)+7] = 0.0;
                  data[9*(i-il)+8] = 0.0;
                  if(inkloop && injloop && iniloop) {
                    for (ifr=0; ifr<nf; ifr++) {
                      data[9*(i-il)  ] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[0]);
                      data[9*(i-il)+1] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[1]);
                      data[9*(i-il)+2] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[3]);
                      data[9*(i-il)+3] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[1]);
                      data[9*(i-il)+4] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[2]);
                      data[9*(i-il)+5] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[4]);
                      data[9*(i-il)+6] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[3]);
                      data[9*(i-il)+7] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[4]);
                      data[9*(i-il)+8] += (float)(pRG->wnu[ifr]*pRG->R[ifr][kr][jr][ir].K[5]);
                    }
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),9*(iu-il+1));
                fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
              }
            }
#endif /* !defined(RADIATION_HYDRO) &&  !defined(RADIATION_MHD) */
#else /* WRITE_GHOST_CELLS */
/* Write frequency integrated moments of the intensities */
/* Write 0th moment integrated over frequency */
            fprintf(pfile,"\nSCALARS rad_J float\n");
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=krl; k<=kru; k++) {
              for (j=jrl; j<=jru; j++) {
                for (i=irl; i<=iru; i++) {
                  data[i-irl] = 0.0;
                  for (ifr=0; ifr<nf; ifr++) {
                    data[i-irl] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].J);
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),iru-irl+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
/* Write components of 1st moment integrated over frequency */
            fprintf(pfile,"\nVECTORS rad_H float\n");
            for (k=krl; k<=kru; k++) {
              for (j=jrl; j<=jru; j++) {
                for (i=irl; i<=iru; i++) {
                  data[3*(i-irl)  ] = 0.0;
                  data[3*(i-irl)+1] = 0.0;
                  data[3*(i-irl)+2] = 0.0;
                  for (ifr=0; ifr<nf; ifr++) {
                    data[3*(i-irl)  ] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].H[0]);
                    data[3*(i-irl)+1] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].H[1]);
                    data[3*(i-irl)+2] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].H[2]);
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),3*(iru-irl+1));
                fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
              }
            }
#if !defined(RADIATION_HYDRO) &&  !defined(RADIATION_MHD)
/* Write components of 2nd moment integrated over frequency if
 * Eddington tensor is not written above. */
            fprintf(pfile,"\nTENSORS rad_K float\n");
            for (k=krl; k<=kru; k++) {
              for (j=jrl; j<=jru; j++) {
                for (i=irl; i<=iru; i++) {
                  data[9*(i-irl)  ] = 0.0;
                  data[9*(i-irl)+1] = 0.0;
                  data[9*(i-irl)+2] = 0.0;
                  data[9*(i-irl)+3] = 0.0;
                  data[9*(i-irl)+4] = 0.0;
                  data[9*(i-irl)+5] = 0.0;
                  data[9*(i-irl)+6] = 0.0;
                  data[9*(i-irl)+7] = 0.0;
                  data[9*(i-irl)+8] = 0.0;
                  for (ifr=0; ifr<nf; ifr++) {
                    data[9*(i-irl)  ] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[0]);
                    data[9*(i-irl)+1] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[1]);
                    data[9*(i-irl)+2] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[3]);
                    data[9*(i-irl)+3] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[1]);
                    data[9*(i-irl)+4] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[2]);
                    data[9*(i-irl)+5] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[4]);
                    data[9*(i-irl)+6] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[3]);
                    data[9*(i-irl)+7] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[4]);
                    data[9*(i-irl)+8] += (float)(pRG->wnu[ifr]*pRG->R[ifr][k][j][i].K[5]);
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),9*(iru-irl+1));
                fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
              }
            }
#endif  /* !defined(RADIATION_HYDRO) &&  !defined(RADIATION_MHD) */
#ifdef RAY_TRACING
/* Write frequency integrated flux from ray tracing */
            fprintf(pfile,"\nSCALARS ray_tracing_H float\n");
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=krl; k<=kru; k++) {
              for (j=jrl; j<=jru; j++) {
                for (i=irl; i<=iru; i++) {
                  data[i-irl] = 0.0;
                  for (ifr=0; ifr<pRG->nf_rt; ifr++) {
                    data[i-irl] += (float)(pRG->wnu_rt[ifr]*pRG->H[ifr][k][j][i]);
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),iru-irl+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
#endif /* RAY_TRACING */

#endif /* WRITE_GHOST_CELLS */
          } /* ( (radt_mode == 0) || (radt_mode == 2) */
#endif /* RADIATION_TRANSFER */

#ifdef POINT_SOURCE
/* Write frequency integrated flux from point source ray tracing */
	  for(ipf=0; ipf<pGrid->npf; ipf++) {
	    if (ipf == 0) 
	      fprintf(pfile,"\nSCALARS point_source_J float\n");
	    else
	      fprintf(pfile,"\nSCALARS point_source_J%d float\n",ipf);
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=kl; k<=ku; k++) {
              for (j=jl; j<=ju; j++) {
                for (i=il; i<=iu; i++) {
                  data[i-il] = (float)(pGrid->Jps[ipf][k][j][i]);
                }
                if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
	  }
/* Write components of 1st moment integrated over frequency */
	  for(ipf=0; ipf<pGrid->npf; ipf++) {
	    if (ipf == 0)
	      fprintf(pfile,"\nVECTORS point_source_H float\n");
	    else
	      fprintf(pfile,"\nVECTORS point_source_H%d float\n",ipf);
	    for (k=kl; k<=ku; k++) {
	      for (j=jl; j<=ju; j++) {
		for (i=il; i<=iu; i++) {
		  data[3*(i-il)  ] = (float)(pGrid->Hps[ipf][k][j][i][0]);
		  data[3*(i-il)+1] = (float)(pGrid->Hps[ipf][k][j][i][1]);
		  data[3*(i-il)+2] = (float)(pGrid->Hps[ipf][k][j][i][2]);
		}
		if(!big_end) ath_bswap(data,sizeof(float),3*(iu-il+1));
		fwrite(data,sizeof(float),(size_t)(3*ndata0),pfile);
	      }
	    }
	  }
/* Write components of 2nd moment integrated over frequency */
	  /*         fprintf(pfile,"\nTENSORS point_source_K float\n");
          for (k=kl; k<=ku; k++) {
            for (j=jl; j<=ju; j++) {
              for (i=il; i<=iu; i++) {
                data[9*(i-il)  ] = (float)(pGrid->Kps[k][j][i][0]);
                data[9*(i-il)+1] = (float)(pGrid->Kps[k][j][i][1]);
                data[9*(i-il)+2] = (float)(pGrid->Kps[k][j][i][3]);
                data[9*(i-il)+3] = (float)(pGrid->Kps[k][j][i][1]);
                data[9*(i-il)+4] = (float)(pGrid->Kps[k][j][i][2]);
                data[9*(i-il)+5] = (float)(pGrid->Kps[k][j][i][4]);
                data[9*(i-il)+6] = (float)(pGrid->Kps[k][j][i][3]);
                data[9*(i-il)+7] = (float)(pGrid->Kps[k][j][i][4]);
                data[9*(i-il)+8] = (float)(pGrid->Kps[k][j][i][5]);                
              }
              if(!big_end) ath_bswap(data,sizeof(float),9*(iu-il+1));
              fwrite(data,sizeof(float),(size_t)(9*ndata0),pfile);
            }
          }*/
#endif /* POINT_SOURCE */


/* Write passive scalars */

#if (NSCALARS > 0)
          for (n=0; n<NSCALARS; n++){
            if (strcmp(pOut->out,"cons") == 0){
              fprintf(pfile,"\nSCALARS scalar[%d] float\n",n);
            } else if(strcmp(pOut->out,"prim") == 0) {
              fprintf(pfile,"\nSCALARS specific_scalar[%d] float\n",n);
            }
            fprintf(pfile,"LOOKUP_TABLE default\n");
            for (k=kl; k<=ku; k++) {
              for (j=jl; j<=ju; j++) {
                for (i=il; i<=iu; i++) {
                  if (strcmp(pOut->out,"cons") == 0){
                    data[i-il] = (float)pGrid->U[k][j][i].s[n];
                  } else if(strcmp(pOut->out,"prim") == 0) {
                    data[i-il] = (float)W[k-kl][j-jl][i-il].r[n];
                  }
                }
                if(!big_end) ath_bswap(data,sizeof(float),iu-il+1);
                fwrite(data,sizeof(float),(size_t)ndata0,pfile);
              }
            }
          }
#endif

/* close file and free memory */

          fclose(pfile);
          free(data);
          if(strcmp(pOut->out,"prim") == 0) free_3d_array(W);
        }}
    }
  }
  return;
}
