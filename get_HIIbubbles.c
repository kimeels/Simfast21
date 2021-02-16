
/************************************************************************************************************
SimFast21
Module: get_HIIbubbles
Description: Generates a 3d box with global_N_smooth^3 cells witho 0s (neutral) and 1s (ionized)
Input: the base directory where we find the parameter file simfast21.ini
Output: creates base_dir/Ionization if needed
For further details see:
M. G. Santos, L. Ferramacho, M. B. Silva, A. Amblard, A. Cooray, MNRAS 2010, http://arxiv.org/abs/0911.2219
*************************************************************************************************************/

#ifdef _OMPTHREAD_
#include <omp.h>
#endif
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>   /* header for complex numbers in c */
#include <fftw3.h>     /* headers for FFTW library */
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "Input_variables.h"
#include "auxiliary.h"

#define FFTWflag FFTW_MEASURE  /* PATIENT is too slow... */
//#define FFTWflag FFTW_PATIENT  /* PATIENT is too slow... */
#define CM_PER_MPC 3.0857e24
#define MASSFRAC 0.76

double Rion(float hmass, double redshift, double Cion, double Dion);
double Rrec(float overdensity, double redshift);
double G_H(double redshift);
double XHI(double ratio);
double Qion(double z);


int main(int argc, char *argv[]) {

  char fname[300];
  FILE *fid;
  DIR* dir;
  long int nhalos;
  Halo_t *halo_v;
  size_t elem;
  long int ii,ij,ik, ii_c, ij_c, ik_c, a, b, c;
  long int ncells_1D;
  long int i,j,p,indi,indj,ind;
  int flag_bub,iz;
  double redshift,tmp;
  double kk;
  double bfactor; /* value by which to divide bubble size R */
  double neutral,*xHI;
  float *halo_map, *top_hat_r, *density_map,*bubblef, *bubble, *fresid, *halo_map1;
  fftwf_complex *halo_map_c, *top_hat_c, *collapsed_mass_c, *density_map_c, *total_mass_c, *bubble_c;
  fftwf_plan pr2c1,pr2c2,pr2c3,pr2c4,pc2r1,pc2r2,pc2r3;
  double zmin,zmax,dz;
  double Cion,Dion;
  double R;


  if(argc != 2) {
    printf("Generates boxes with ionization fraction for a range of redshifts\n");
    printf("usage: get_HIIbubbles base_dir\n");
    printf("base_dir contains simfast21.ini and directory structure\n");
    exit(1);
  }
  get_Simfast21_params(argv[1]);
  zmin=global_Zminsim;
  zmax=global_Zmaxsim;
  dz=global_Dzsim;
  Cion=global_Cion;
  Dion=global_Dion;

#ifdef _OMPTHREAD_
  omp_set_num_threads(global_nthreads);
  fftwf_init_threads();
  fftwf_plan_with_nthreads(global_nthreads);
  printf("Using %d threads\n",global_nthreads);fflush(0);
#endif
  /* Create Ionization folder */
  sprintf(fname,"%s/Ionization",argv[1]);
  if((dir=opendir(fname))==NULL) {
    printf("Creating Ionization directory\n");
    if(mkdir(fname,(S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))!=0) {
      printf("Error creating directory!\n");
      exit(1);
    }
  }
  sprintf(fname,"%s/Output_text_files",argv[1]);
  if((dir=opendir(fname))==NULL) {
    printf("Creating Output_text_files directory\n");
    if(mkdir(fname,(S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))!=0) {
      printf("Error creating directory!\n");
      exit(1);
    }
  }

 /* Memory allocation - we could do some of the FFTs inline... */
 /*************************************************************/

  /* density_map mass */
  if(!(density_map=(float *) fftwf_malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem1...\n");
    exit(1);
  }
  if(!(density_map_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem2...\n");
    exit(1);
  }
  if(!(pr2c1=fftwf_plan_dft_r2c_3d(global_N_smooth, global_N_smooth, global_N_smooth, density_map, density_map_c, FFTWflag))) {
    printf("Memory Problem3...\n");
    exit(1);
  }
  /* halo_map mass */
  if(!(halo_map1=(float *) fftwf_malloc(global_N3_halo*sizeof(float)))) {
    printf("Memory Problem4...\n");
    exit(1);
  }

  if(!(halo_map=(float *) fftwf_malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem5...\n");
    exit(1);
  }
  if(!(halo_map_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem6...\n");
    exit(1);
  }
  if(!(pr2c2=fftwf_plan_dft_r2c_3d(global_N_smooth, global_N_smooth, global_N_smooth, halo_map, halo_map_c, FFTWflag))) {
    printf("FFTW Problem1...\n");
    exit(1);
  }
  /* total mass */
  if(!(total_mass_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem7...\n");
    exit(1);
  }
  if(!(pc2r1=fftwf_plan_dft_c2r_3d(global_N_smooth, global_N_smooth, global_N_smooth, total_mass_c, density_map, FFTWflag))) {
    printf("FFTW Problem2...\n");
    exit(1);
  }
  /* collapsed mass */
  if(!(collapsed_mass_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem8...\n");
    exit(1);
  }
  if(!(pc2r2=fftwf_plan_dft_c2r_3d(global_N_smooth, global_N_smooth, global_N_smooth, collapsed_mass_c, halo_map, FFTWflag))) {
    printf("FFTW Problem3...\n");
    exit(1);
  }
  /* top hat window */
  if(!(top_hat_r=(float *) fftwf_malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem9...\n");
    exit(1);
  }
  if(!(top_hat_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem10...\n");
    exit(1);
  }
  if(!(pr2c3=fftwf_plan_dft_r2c_3d(global_N_smooth, global_N_smooth, global_N_smooth, top_hat_r, top_hat_c, FFTWflag))) {
    printf("FFTW Problem4...\n");
    exit(1);
  }
  /* bubble boxes */
  if(!(bubble=(float *) fftwf_malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem11...\n");
    exit(1);
  }
  if(!(bubble_c=(fftwf_complex *) fftwf_malloc(global_N_smooth*global_N_smooth*(global_N_smooth/2+1)*sizeof(fftwf_complex)))) {
    printf("Memory Problem12...\n");
    exit(1);
  }
  if(!(bubblef=(float *) malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem13...\n");
    exit(1);
  }
  if(!(pr2c4=fftwf_plan_dft_r2c_3d(global_N_smooth, global_N_smooth, global_N_smooth, bubble, bubble_c, FFTWflag))) {
    printf("FFTW Problem5...\n");
    exit(1);
  }
  if(!(pc2r3=fftwf_plan_dft_c2r_3d(global_N_smooth, global_N_smooth, global_N_smooth, bubble_c, bubble, FFTWflag))) {
    printf("FFTW Problem6...\n");
    exit(1);
  }
  if(!(xHI=(double *) malloc((int)((zmax-zmin)/dz+2)*sizeof(double)))) {
    printf("Memory Problem14...\n");
    exit(1);
  }
  if(!(fresid=(float *) fftwf_malloc(global_N3_smooth*sizeof(float)))) {
    printf("Memory Problem15...\n");
    exit(1);
  }
  bfactor=pow(10.0,log10(global_bubble_Rmax/global_dx_smooth)/global_bubble_Nbins);
  printf("Bubble radius ratio (bfactor): %f\n", bfactor);
  printf("Number of bubble steps: %d\n",(int)((log10(global_bubble_Rmax)-log10(global_dx_smooth))/log10(bfactor)));  /**!!! need to check...!!!*/
  printf("Redshift cycle...\n");fflush(0);




  /****************************************************/
  /****************************************************/
  /****************************************************/
  /***************** Redshift cycle *******************/
  iz=0;
  neutral=0.;
  for(redshift=zmin;redshift<(zmax+dz/10) && (neutral < global_xHlim);redshift+=dz){
    printf("z = %f\n",redshift);fflush(0);

    sprintf(fname, "%s/delta/deltanl_z%.3f_N%ld_L%.1f.dat",argv[1],redshift,global_N_smooth,global_L/global_hubble);
    fid=fopen(fname,"rb");
    if (fid==NULL) {printf("\nError reading deltanl file... Check path or if the file exists..."); exit (1);}
    elem=fread(density_map,sizeof(float),global_N3_smooth,fid);
    fclose(fid);

    /* Calculate the residual neutral fraction and recombination rate */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_smooth, density_map, fresid, bubblef, redshift) private(i)
#endif
    for(i=0;i<(global_N3_smooth);i++){
      fresid[i] = (1. + density_map[i])*1.881e-7*pow(1.+redshift,3.0)*G_H(redshift); // ratio between hydrogen recombination coefficient and uniform ionising background (Haardt & Madau (2012)).
      fresid[i] = XHI(fresid[i]); // Residual neutral fraction following Popping et al. (2009).
      density_map[i]= Rrec(1.0+density_map[i], redshift); // Rrec from the CIC nonlinear density field.

      bubblef[i]=0.0;
    }

    /********************************************/
    /* calculate Rion if there is no SFRD file */
    if(global_use_SFR==0) {  /* assume there are no SFRD boxes... */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_halo, halo_map1) private(i)
#endif
      for(i=0;i<(global_N3_halo);i++){
	      halo_map1[i] =0.0;
      }
      sprintf(fname, "%s/Halos/halonl_z%.3f_N%ld_L%.1f.dat.catalog",argv[1],redshift,global_N_halo,global_L/global_hubble);
      fid=fopen(fname,"rb");
      if (fid==NULL) {printf("\nError reading %s file... Check path or if the file exists...",fname); exit (1);}
      elem=fread(&nhalos,sizeof(long int),1,fid);
      printf("Reading %ld halos...\n",nhalos);fflush(0);
      if(!(halo_v=(Halo_t *) malloc(nhalos*sizeof(Halo_t)))) {
	      printf("Memory Problem - halo...\n");
	      exit(1);
      }
      elem=fread(halo_v,sizeof(Halo_t),nhalos,fid);
      fclose(fid);

      // CIC Rion//
      for(i=0;i<nhalos;i++){
	      CIC(halo_v[i].x, halo_v[i].y, halo_v[i].z, Rion(halo_v[i].Mass, redshift, Cion, Dion), halo_map1, global_N_halo);  /* distributes Rion over cells */
      }
      free(halo_v);
      smooth_box(halo_map1, halo_map, global_N_halo, global_N_smooth);  /* Sums Rion over cells to reduce resolution - smooth_box averages down the box */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_smooth, halo_map, global_dx_halo, global_dx_smooth) private(i)
#endif
      for(i=0;i<(global_N3_smooth);i++){
	      halo_map[i] = halo_map[i]*pow(global_dx_smooth/global_dx_halo,3)*global_fesc; /* corrects for the fact that we averaged instead of summing... */
      }
      /******************************/
      /* uses SFRD boxes instead: */
    } else {
      sprintf(fname, "%s/SFR/sfrd_z%.3f_N%ld_L%.1f.dat",argv[1],redshift,global_N_smooth,global_L/global_hubble);
      fid=fopen(fname,"rb");
      if (fid==NULL) {printf("\nError reading %s file... Check path or if the file exists...",fname); exit (1);}
      elem=fread(halo_map,sizeof(float),global_N3_smooth,fid);
      fclose(fid);
      /* converts SFRD to Rion */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_smooth, halo_map) private(i)
#endif
      for(i=0;i<(global_N3_smooth);i++){
	      halo_map[i] = halo_map[i]*global_dx_smooth*global_dx_smooth*global_dx_smooth*Qion(redshift)*global_fesc;
      }
    }

    /* Quick fill of single cells before going to bubble cycle */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_smooth,halo_map,density_map,bubblef) private(i,tmp)
#endif
    for(i=0;i<global_N3_smooth;i++) {
      if(halo_map[i]>0.) {
	if(density_map[i]>0.)
	  tmp=(double)halo_map[i]/density_map[i];
	else tmp=1.0;
      }else tmp=0.;
      if(tmp>=1.0) bubblef[i]=1.0; else bubblef[i]=tmp;
    }
    /* FFT density and halos */
    fftwf_execute(pr2c1);    /* FFT density map */
    fftwf_execute(pr2c2);   /* FFT halo map */



    /****************************************************/
    /****************************************************/
    /************** going over the bubble sizes for each z ****************/
    R=global_bubble_Rmax;    /* Maximum bubble size...*/
    while(R>=2*global_dx_smooth){ /* only tries to find bubbles using this method down to a certain minimum size because of resolution effects */

      //      printf("bubble radius R= %lf\n", R);fflush(0);
      //      printf("Filtering halo and density boxes...\n");fflush(0);

      /* this cycle smoothes "recombinations" and "ionisations" over a given sphere size */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(collapsed_mass_c,halo_map_c,total_mass_c,density_map_c,global_N_smooth,global_dk,R) private(i,j,p,indi,indj,kk)
#endif
      for(i=0;i<global_N_smooth;i++) {
	if(i>global_N_smooth/2) {
	  indi=-(global_N_smooth-i);
	}else indi=i;
	for(j=0;j<global_N_smooth;j++) {
	  if(j>global_N_smooth/2) {
	    indj=-(global_N_smooth-j);
	  }else indj=j;
	  for(p=0;p<=global_N_smooth/2;p++) {
	    kk=global_dk*sqrt(indi*indi+indj*indj+p*p);
	    total_mass_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]=density_map_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]*W_filter(kk*R);
	    collapsed_mass_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]=halo_map_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]*W_filter(kk*R);
	  }
	}
      }

      fftwf_execute(pc2r1);     /* FFT back filtered density map */
      fftwf_execute(pc2r2);     /* FFT back filtered halo map */

      flag_bub=0;

      //      printf("Starting to find and fill bubbles...\n");fflush(0);

      /* this cycle finds ionisation bubbles and signals the center of those bubbles */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(halo_map,density_map, bubble, global_N_smooth,flag_bub) private(ii,ij,ik,ind)
#endif
      for(ii=0;ii<global_N_smooth;ii++){
	      for(ij=0;ij<global_N_smooth;ij++){
	        for(ik=0;ik<global_N_smooth;ik++){
	          ind=ii*global_N_smooth*global_N_smooth+ij*global_N_smooth+ik;
	          if(halo_map[ind]>0.) {
	            if(density_map[ind]>0.) {
		            if((double)halo_map[ind]/density_map[ind]>=1.0) {
		              flag_bub=1;
		              bubble[ind]=1.0;
		            }else bubble[ind]=0;
	            }else {
		            flag_bub=1;
		            bubble[ind]=1.0;
	            }
	          }else bubble[ind]=0;
	        }
	      }
      }

      /* the next section uses a special technique to try to signal all points in the final bubble box as 0 or 1. It calculates the value in each cell as a convolution between a top hat window of radius R (0s and 1s) and the bubble box above of 0s and 1s which signals the center of bubbles of radius R. The convolution will either give zero if there are no bubbles over the radius or some number greater than zero otherwise. Maximum value would be the integral over the top-hat window, e.g. 4/3*pi*R^3. For only one bubble contributing, the result of the convolution would be dx^3 */

      /* this cycle generates a spherical window function  in real space for a given R (top hat window) - 0s and 1s */
      /* it could be done outside the redshift cycle but would require saving the window for different sizes */
      if(flag_bub>0){
	//	printf("Found bubble...\n");fflush(0);
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(top_hat_r,R,global_dx_smooth,global_N_smooth) private(i,j,p)
#endif
	for(i=0;i<global_N_smooth;i++){
	  for(j=0;j<global_N_smooth;j++){
	    for(p=0;p<global_N_smooth;p++){
	      if(sqrt(i*i+j*j+p*p)*global_dx_smooth<=R || sqrt(i*i+(j-global_N_smooth)*(j-global_N_smooth)+p*p)*global_dx_smooth<=R  || sqrt(i*i+(j-global_N_smooth)*(j-global_N_smooth)+(p-global_N_smooth)*(p-global_N_smooth))*global_dx_smooth <=R || sqrt(i*i+(p-global_N_smooth)*(p-global_N_smooth)+j*j)*global_dx_smooth<=R || sqrt((i-global_N_smooth)*(i-global_N_smooth)+j*j+p*p)*global_dx_smooth<=R ||  sqrt((i-global_N_smooth)*(i-global_N_smooth)+(j-global_N_smooth)*(j-global_N_smooth)+p*p)*global_dx_smooth<=R ||  sqrt((i-global_N_smooth)*(i-global_N_smooth)+(j-global_N_smooth)*(j-global_N_smooth)+(p-global_N_smooth)*(p-global_N_smooth))*global_dx_smooth<=R ||  sqrt((i-global_N_smooth)*(i-global_N_smooth)+j*j+(p-global_N_smooth)*(p-global_N_smooth))*global_dx_smooth<=R ) {
		top_hat_r[i*global_N_smooth*global_N_smooth+j*global_N_smooth+p]=1.0;
	      }else top_hat_r[i*global_N_smooth*global_N_smooth+j*global_N_smooth+p]=0.0;
	    }
	  }
	}
	fftwf_execute(pr2c3); /* FFT window above (top_hat_r) - dx^3 missing */
	fftwf_execute(pr2c4); /* FFT ionisation/bubble centers box above (bubble) - dx^3 missing */

	/* Make convolution between the two boxes above */
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(bubble_c,top_hat_c,global_N_smooth) private(i)
#endif
	for(i=0;i<global_N_smooth*global_N_smooth*(global_N_smooth/2+1);i++) {
	  bubble_c[i]*=top_hat_c[i];
	}
	fftwf_execute(pc2r3);     /* FFT back to real ionisation (bubble) box  - output in bubble - dk^3 missing */

	/* so missing factors are dx^6*dk^3 = (L/N)^6*(1/L)^3 = L^3/N^6 = dx^3/N^3 */
	/* after correcting by the missing factors, the values will be between zero and 4/3*pi*R^3. The minimum non-zero value will be dx^3, with dx=L/N */

#ifdef _OMPTHREAD_
#pragma omp parallel for shared(bubble,bubblef,global_N3_smooth) private(i)
#endif
	for (i=0; i<global_N3_smooth; i++){
	  bubble[i]=bubble[i]*pow(global_dx_smooth,3)/global_N3_smooth;
	  if (bubble[i]>pow(global_dx_smooth,3)/100) bubblef[i]=1.0; /* neutral should be zero. use dx^3/100 for minimum positive value...*/
	}

      } /* ends filling out bubbles in box for R */

      R/=bfactor;
    } /* ends R cycle */

    /* just to check smallest bubbles through older method */
    printf("Going to smaller R cycle...\n"); fflush(0);
    while(R>=global_dx_smooth){

      //      printf("bubble radius R= %lf\n", R);fflush(0);
      flag_bub=0;
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(collapsed_mass_c,halo_map_c,total_mass_c,density_map_c,global_N_smooth,global_dx_smooth,global_dk,R) private(i,j,p,indi,indj,kk)
#endif
      for(i=0;i<global_N_smooth;i++) {
	if(i>global_N_smooth/2) {
	  indi=-(global_N_smooth-i);
	}else indi=i;
	for(j=0;j<global_N_smooth;j++) {
	  if(j>global_N_smooth/2) {
	    indj=-(global_N_smooth-j);
	  }else indj=j;
	  for(p=0;p<=global_N_smooth/2;p++) {
	    kk=global_dk*sqrt(indi*indi+indj*indj+p*p);
	    total_mass_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]=density_map_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]*W_filter(kk*R);
	    collapsed_mass_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]=halo_map_c[i*global_N_smooth*(global_N_smooth/2+1)+j*(global_N_smooth/2+1)+p]*W_filter(kk*R);
	  }
	}
      }
      fftwf_execute(pc2r1);  /* FFT convolved density field - gives smoothed real density field */
      fftwf_execute(pc2r2);  /* FFT convolved halo filed - gives smoothed real halo field */

      /* fill smaller bubbles in box */
      ncells_1D=(long int)(R/global_dx_smooth);
      //      printf("Starting to find and fill bubbles...\n");fflush(0);
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(halo_map,density_map,global_N_smooth,global_dx_smooth,R,ncells_1D,bubblef,flag_bub) private(ii_c,ij_c,ik_c,ii,ij,ik,a,b,c,ind)
#endif
      for(ii_c=0;ii_c<global_N_smooth;ii_c++){
	for(ij_c=0;ij_c<global_N_smooth;ij_c++){
	  for(ik_c=0;ik_c<global_N_smooth;ik_c++){
	    ind=ii_c*global_N_smooth*global_N_smooth+ij_c*global_N_smooth+ik_c;
	    if(halo_map[ind]>0.) {
	      if(!(density_map[ind]>0.) || ((double)halo_map[ind]/density_map[ind]>=1.0)) {
		flag_bub=1;
		for(ii=-(ncells_1D+1);ii<=ncells_1D+1;ii++){
		  a=check_borders(ii_c+ii,global_N_smooth);
		  for(ij=-(ncells_1D+1);ij<=ncells_1D+1;ij++){
		    if(sqrt(ii*ii+ij*ij)*global_dx_smooth <= R){
		      b=check_borders(ij_c+ij,global_N_smooth);
		      for(ik=-(ncells_1D+1);ik<=ncells_1D+1;ik++){
			c=check_borders(ik_c+ik,global_N_smooth);
			if(sqrt(ii*ii+ij*ij+ik*ik)*global_dx_smooth <= R){
			  bubblef[a*global_N_smooth*global_N_smooth+b*global_N_smooth+c]=1.0;
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
      //     if(flag_bub>0){printf("Found bubble...\n");fflush(0);}

      R/=bfactor;
    } /* ends small bubbles R cycle */
    /****************************************************/
    /****************************************************/

#ifdef _OMPTHREAD_
#pragma omp parallel for shared(global_N3_smooth, bubblef, fresid) private(i)
#endif
    for (i=0; i<global_N3_smooth; i++){
      if(bubblef[i] > 1.0 - fresid[i]) bubblef[i] = 1.0 - fresid[i];
    }

    neutral=0.;
    for (i=0; i<global_N3_smooth; i++){
      neutral+=1.0-bubblef[i];
    }
    neutral/=global_N3_smooth;
    printf("neutral fraction=%lf\n",neutral);fflush(0);
    xHI[iz]=neutral;
    sprintf(fname, "%s/Ionization/xHII_z%.3f_N%ld_L%.1f.dat",argv[1],redshift,global_N_smooth,global_L/global_hubble);
    if((fid = fopen(fname,"wb"))==NULL) {
      printf("Error opening file:%s\n",fname);
      exit(1);
    }
    elem=fwrite(bubblef,sizeof(float),global_N3_smooth,fid);
    fclose(fid);
    iz++;
  } /* ends redshift cycle */
    /****************************************************/
    /****************************************************/
    /****************************************************/


  /* z cycle for neutral>=xHlim */
  printf("Writing high redshift xHII boxes...\n");
#ifdef _OMPTHREAD_
#pragma omp parallel for shared(bubblef,global_N3_smooth) private(i)
#endif
    for(i=0;i<global_N3_smooth;i++) bubblef[i]=0.0;

  while(redshift<(zmax+dz/10)) {
    //    printf("z(>%f) = %f\n",global_xHlim,redshift);fflush(0);
    xHI[iz]=1.0;
    sprintf(fname, "%s/Ionization/xHII_z%.3f_N%ld_L%.1f.dat",argv[1],redshift,global_N_smooth,global_L/global_hubble);
    if((fid = fopen(fname,"wb"))==NULL) {
      printf("Error opening file:%s\n",fname);
      exit(1);
    }
    elem=fwrite(bubblef,sizeof(float),global_N3_smooth,fid);
    fclose(fid);
    iz++;
    redshift+=dz;
  }

  sprintf(fname, "%s/Output_text_files/zsim.txt",argv[1]);
  if((fid = fopen(fname,"a"))==NULL) {
    printf("Error opening file:%s\n",fname);
    exit(1);
  }
  for(redshift=zmax;redshift>(zmin-dz/10);redshift-=dz) fprintf(fid,"%f\n",redshift); /* first line should be highest redshift */
  fclose(fid);
  sprintf(fname, "%s/Output_text_files/x_HI_N%ld_L%.1f.dat",argv[1],global_N_smooth,global_L/global_hubble);
  if((fid = fopen(fname,"a"))==NULL) {
    printf("Error opening file:%s\n",fname);
    exit(1);
  }
  for(i=iz-1;i>=0;i--) fprintf(fid,"%lf\n",xHI[i]); /* first line should be highest redshift */
  fclose(fid);

  free(xHI);
  free(bubblef);
  fftwf_free(top_hat_r);
  fftwf_free(top_hat_c);
  fftwf_free(collapsed_mass_c);
  fftwf_free(density_map);
  fftwf_free(density_map_c);
  fftwf_free(halo_map);
  fftwf_free(halo_map_c);
  fftwf_free(total_mass_c);
  fftwf_free(bubble);
  fftwf_free(bubble_c);


  exit(0);
}
