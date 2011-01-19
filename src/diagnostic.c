/*
*				diagnostic.c
*
* PSF diagnostics.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
*	This file part of:	PSFEx
*
*	Copyright:		(C) 2006-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
*
*	License:		GNU General Public License
*
*	PSFEx is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
* 	(at your option) any later version.
*	PSFEx is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*	You should have received a copy of the GNU General Public License
*	along with PSFEx.  If not, see <http://www.gnu.org/licenses/>.
*
*	Last modified:		19/01/2011
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#ifdef HAVE_CONFIG_H
#include        "config.h"
#endif

#include	<math.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"define.h"
#include	"types.h"
#include	"globals.h"
#include	"fits/fitscat.h"
#include	"levmar/lm.h"
#include	"diagnostic.h"
#include	"prefs.h"
#include	"poly.h"
#include	"psf.h"

/****** psf_diagnostic *******************************************************
PROTO	void	psf_diagnostic(psfstruct *psf)
PURPOSE	Free a PSF structure and everything it contains.
INPUT	Pointer to the PSF structure.
OUTPUT  -.
NOTES   -.
AUTHOR  E. Bertin (IAP, Leiden observatory & ESO)
VERSION 20/11/2010
 ***/
void	psf_diagnostic(psfstruct *psf)
  {
   moffatstruct		*moffat, *pfmoffat;
   double		lm_opts[5],
			dpos[POLY_MAXDIM],
			dparam[PSF_DIAGNPARAM],
			*dresi;
   float		param[PSF_DIAGNPARAM],
			dstep,dstart, fwhm, ellip,ellip1,ellip2;
   int			i,m,n, w,h, npc,nt, nmed, niter;

  nmed = 0;
  npc = psf->poly->ndim;
  for (nt=1, i=npc; (i--)>0;)
    {
    nt *= psf->nsnap;
    nmed += nmed*psf->nsnap + (psf->nsnap-1)/2;
    }
  psf->nmed = nmed;

  w = psf->size[0];
  h = psf->size[1];
  m = w*h;
  QMALLOC(dresi, double, m);
  dstep = 1.0/psf->nsnap;
  dstart = (1.0-dstep)/2.0;

/*-------------------- "Normal" Moffat profile-fitting ---------------------*/

  moffat = psf->moffat;
  psf->nsubpix = 1;
  memset(dpos, 0, POLY_MAXDIM*sizeof(float));
  for (i=0; i<npc; i++)
     dpos[i] = -dstart;

  psf->moffat_fwhm_min = psf->moffat_ellipticity_min
		= psf->moffat_ellipticity1_min = psf->moffat_ellipticity2_min
		= psf->moffat_beta_min
		= psf->moffat_residuals_min = psf->sym_residuals_min
		= psf->noiseqarea_min = BIG;
  psf->moffat_fwhm_max = psf->moffat_ellipticity_max
		= psf->moffat_ellipticity1_max = psf->moffat_ellipticity2_max
		= psf->moffat_beta_max
		= psf->moffat_residuals_max = psf->sym_residuals_max
		= psf->noiseqarea_max = -BIG;

  lm_opts[0] = 1.0e-2;
  lm_opts[1] = 1.0e-12;
  lm_opts[2] = 1.0e-12;
  lm_opts[3] = 1.0e-12;
  lm_opts[4] = 1.0e-4;

/* For each snapshot of the PSF */ 
  for (n=0; n<nt; n++)
    {
    if (psf->samples_accepted)
      {
      psf_build(psf, dpos);
/*---- Initialize PSF parameters */
      fwhm = psf->fwhm / psf->pixstep;
/*---- Amplitude */
      param[0] = 1.0/(psf->fwhm*psf->fwhm);
      moffat_parammin[0] = param[0]/10.0;
      moffat_parammax[0] = param[0]*10.0;
/*---- Xcenter */
      param[1] = (w-1)/2.0;
      moffat_parammin[1] = 0.0;
      moffat_parammax[1] = w - 1.0;
/*---- Ycenter */
      param[2] = (h-1)/2.0;
      moffat_parammin[2] = 0.0;
      moffat_parammax[2] = h - 1.0;
/*---- Major axis FWHM (pixels) */
      param[3] = fwhm;
      moffat_parammin[3] = fwhm/3.0;
      moffat_parammax[3] = fwhm*3.0;
/*---- Major axis FWHM (pixels) */
      param[4] = fwhm;
      moffat_parammin[4] = fwhm/3.0;
      moffat_parammax[4] = fwhm*3.0;
/*---- Position angle (deg)  */
      param[5] = 0.0;
      moffat_parammin[5] = moffat_parammax[5] = 90.0;
/*---- Moffat beta */
      param[6] = 3.0;
      moffat_parammin[6] = PSF_BETAMIN;
      moffat_parammax[6] = 10.0;
      psf_boundtounbound(param, dparam);
      memset(dresi, 0, m*sizeof(double));
      niter = dlevmar_dif(psf_diagresi, dparam, dresi,
	PSF_DIAGNPARAM, m, 
	PSF_DIAGMAXITER, 
	lm_opts, NULL, NULL, NULL, psf);
      psf_unboundtobound(dparam,param);
      }
    else
      memset(param, 0, PSF_DIAGNPARAM*sizeof(float));

    moffat[n].nsubpix = psf->nsubpix;
    moffat[n].amplitude = param[0]/(psf->pixstep*psf->pixstep);
    moffat[n].xc[0] = param[1];
    moffat[n].xc[1] = param[2];
    if (param[3] > param[4])
      {
      moffat[n].fwhm_max = param[3]*psf->pixstep;
      moffat[n].fwhm_min = param[4]*psf->pixstep;
      moffat[n].theta = (fmod(param[5]+360.0, 180.0));
      }
    else
      {
      moffat[n].fwhm_max = param[4]*psf->pixstep;
      moffat[n].fwhm_min = param[3]*psf->pixstep;
      moffat[n].theta = (fmod(param[5]+450.0, 180.0));
      }
    if (moffat[n].theta > 90.0)
      moffat[n].theta -= 180.0;

    moffat[n].beta = param[6];

    for (i=0; i<npc; i++)
      moffat[n].context[i] = dpos[i]*psf->contextscale[i]+psf->contextoffset[i];

    moffat[n].residuals = psf_normresi(param, psf);
    moffat[n].symresiduals = (float)psf_symresi(psf);
    moffat[n].noiseqarea = (float)psf_noiseqarea(psf);

    if ((fwhm=0.5*(psf->moffat[n].fwhm_min+psf->moffat[n].fwhm_max))
		< psf->moffat_fwhm_min)
      psf->moffat_fwhm_min = fwhm;
    if (fwhm > psf->moffat_fwhm_max)
      psf->moffat_fwhm_max = fwhm;

    if ((ellip = moffat[n].fwhm_max + moffat[n].fwhm_min) > 0.0)
      ellip = (moffat[n].fwhm_max - moffat[n].fwhm_min) / ellip;

    if (ellip < psf->moffat_ellipticity_min)
      psf->moffat_ellipticity_min = ellip;
    if (ellip > psf->moffat_ellipticity_max)
      psf->moffat_ellipticity_max = ellip;

    ellip1 = ellip*cosf(2.0*moffat[n].theta*DEG);
    ellip2 = ellip*sinf(2.0*moffat[n].theta*DEG);

    if (ellip1 < psf->moffat_ellipticity1_min)
      psf->moffat_ellipticity1_min = ellip1;
    if (ellip1 > psf->moffat_ellipticity1_max)
      psf->moffat_ellipticity1_max = ellip1;
    if (ellip2 < psf->moffat_ellipticity2_min)
      psf->moffat_ellipticity2_min = ellip2;
    if (ellip2 > psf->moffat_ellipticity2_max)
      psf->moffat_ellipticity2_max = ellip2;

    if (moffat[n].beta < psf->moffat_beta_min)
      psf->moffat_beta_min = moffat[n].beta;
    if (moffat[n].beta > psf->moffat_beta_max)
      psf->moffat_beta_max = moffat[n].beta;

    if (moffat[n].residuals < psf->moffat_residuals_min)
      psf->moffat_residuals_min = moffat[n].residuals;
    if (moffat[n].residuals > psf->moffat_residuals_max)
      psf->moffat_residuals_max = moffat[n].residuals;

    if (moffat[n].symresiduals < psf->sym_residuals_min)
      psf->sym_residuals_min = moffat[n].symresiduals;
    if (moffat[n].symresiduals > psf->sym_residuals_max)
      psf->sym_residuals_max = moffat[n].symresiduals;
    if (moffat[n].noiseqarea < psf->noiseqarea_min)
      psf->noiseqarea_min = moffat[n].noiseqarea;
    if (moffat[n].noiseqarea > psf->noiseqarea_max)
      psf->noiseqarea_max = moffat[n].noiseqarea;

    for (i=0; i<npc; i++)
      if (dpos[i]<dstart-0.01)
        {
        dpos[i] += dstep;
        break;
        }
      else
        dpos[i] = -dstart;
    }

  psf->moffat_fwhm = 0.5*(moffat[nmed].fwhm_min + moffat[nmed].fwhm_max);

  ellip = moffat[nmed].fwhm_max + moffat[nmed].fwhm_min;
  psf->moffat_ellipticity = (ellip > 0.0)?
	(moffat[nmed].fwhm_max - moffat[nmed].fwhm_min) / ellip : 0.0;
  psf->moffat_ellipticity1 = psf->moffat_ellipticity
				* cosf(2.0*moffat[nmed].theta*DEG);
  psf->moffat_ellipticity2 = psf->moffat_ellipticity
				* sinf(2.0*moffat[nmed].theta*DEG);

  psf->moffat_beta = moffat[nmed].beta;
  psf->moffat_residuals = moffat[nmed].residuals;
  psf->sym_residuals = moffat[nmed].symresiduals;
  psf->noiseqarea = moffat[nmed].noiseqarea;

/*------------------ "Pixel-free" Moffat profile-fitting -------------------*/

  pfmoffat = psf->pfmoffat;
  psf->nsubpix = PSF_NSUBPIX;
  memset(dpos, 0, POLY_MAXDIM*sizeof(float));
  for (i=0; i<npc; i++)
     dpos[i] = -dstart;

  psf->pfmoffat_fwhm_min = psf->pfmoffat_ellipticity_min
		= psf->pfmoffat_ellipticity1_min
		= psf->pfmoffat_ellipticity2_min
		= psf->pfmoffat_beta_min
		= psf->pfmoffat_residuals_min = BIG;
  psf->pfmoffat_fwhm_max = psf->pfmoffat_ellipticity_max
		= psf->pfmoffat_ellipticity1_max
		= psf->pfmoffat_ellipticity2_max
		= psf->pfmoffat_beta_max
		= psf->pfmoffat_residuals_max = -BIG;

/* For each snapshot of the PSF */ 
  for (n=0; n<nt; n++)
    {
    if (psf->samples_accepted)
      {
      psf_build(psf, dpos);
/*---- Initialize PSF parameters */
      fwhm = psf->fwhm / psf->pixstep;
/*---- Amplitude */
      param[0] = 1.0/(psf->fwhm*psf->fwhm);
      moffat_parammin[0] = param[0]/10.0;
      moffat_parammax[0] = param[0]*10.0;
/*---- Xcenter */
      param[1] = (w-1)/2.0;
      moffat_parammin[1] = 0.0;
      moffat_parammax[1] = w - 1.0;
/*---- Ycenter */
      param[2] = (h-1)/2.0;
      moffat_parammin[2] = 0.0;
      moffat_parammax[2] = h - 1.0;
/*---- Major axis FWHM (pixels) */
      param[3] = fwhm;
      moffat_parammin[3] = fwhm/3.0;
      moffat_parammax[3] = fwhm*3.0;
/*---- Major axis FWHM (pixels) */
      param[4] = fwhm;
      moffat_parammin[4] = fwhm/3.0;
      moffat_parammax[4] = fwhm*3.0;
/*---- Position angle (deg)  */
      param[5] = 0.0;
      moffat_parammin[5] = moffat_parammax[5] = 90.0;
/*---- Moffat beta */
      param[6] = 3.0;
      moffat_parammin[6] = PSF_BETAMIN;
      moffat_parammax[6] = 10.0;
      psf_boundtounbound(param, dparam);
      memset(dresi, 0, m*sizeof(double));
      niter = dlevmar_dif(psf_diagresi, dparam, dresi,
	PSF_DIAGNPARAM, m, 
	PSF_DIAGMAXITER, 
	lm_opts, NULL, NULL, NULL, psf);
      psf_unboundtobound(dparam, param);
      }
    else
      memset(param, 0, PSF_DIAGNPARAM*sizeof(float));

    pfmoffat[n].nsubpix = psf->nsubpix;
    pfmoffat[n].amplitude = param[0]/(psf->pixstep*psf->pixstep);
    pfmoffat[n].xc[0] = param[1];
    pfmoffat[n].xc[1] = param[2];
    if (param[3] > param[4])
      {
      pfmoffat[n].fwhm_max = param[3]*psf->pixstep;
      pfmoffat[n].fwhm_min = param[4]*psf->pixstep;
      pfmoffat[n].theta = (fmod(param[5]+360.0, 180.0));
      }
    else
      {
      pfmoffat[n].fwhm_max = param[4]*psf->pixstep;
      pfmoffat[n].fwhm_min = param[3]*psf->pixstep;
      pfmoffat[n].theta = (fmod(param[5]+450.0, 180.0));
      }
    if (pfmoffat[n].theta > 90.0)
      pfmoffat[n].theta -= 180.0;

    pfmoffat[n].beta = param[6];

    for (i=0; i<npc; i++)
      pfmoffat[n].context[i] = dpos[i]*psf->contextscale[i]+psf->contextoffset[i];

    pfmoffat[n].residuals = psf_normresi(param, psf);

    if ((fwhm=0.5*(pfmoffat[n].fwhm_min+pfmoffat[n].fwhm_max))
		< psf->pfmoffat_fwhm_min)
      psf->pfmoffat_fwhm_min = fwhm;
    if (fwhm > psf->pfmoffat_fwhm_max)
      psf->pfmoffat_fwhm_max = fwhm;

    if ((ellip = pfmoffat[n].fwhm_max + pfmoffat[n].fwhm_min) > 0.0)
      ellip = (pfmoffat[n].fwhm_max - pfmoffat[n].fwhm_min) / ellip;

    ellip1 = ellip*cosf(2.0*pfmoffat[n].theta*DEG);
    ellip2 = ellip*sinf(2.0*pfmoffat[n].theta*DEG);

    if (ellip < psf->pfmoffat_ellipticity_min)
      psf->pfmoffat_ellipticity_min = ellip;
    if (ellip > psf->pfmoffat_ellipticity_max)
      psf->pfmoffat_ellipticity_max = ellip;

    if (ellip1 < psf->pfmoffat_ellipticity1_min)
      psf->pfmoffat_ellipticity1_min = ellip1;
    if (ellip1 > psf->pfmoffat_ellipticity1_max)
      psf->pfmoffat_ellipticity1_max = ellip1;
    if (ellip2 < psf->pfmoffat_ellipticity2_min)
      psf->pfmoffat_ellipticity2_min = ellip2;
    if (ellip2 > psf->pfmoffat_ellipticity2_max)
      psf->pfmoffat_ellipticity2_max = ellip2;

    if (pfmoffat[n].beta < psf->pfmoffat_beta_min)
      psf->pfmoffat_beta_min = pfmoffat[n].beta;
    if (pfmoffat[n].beta > psf->pfmoffat_beta_max)
      psf->pfmoffat_beta_max = pfmoffat[n].beta;

    if (pfmoffat[n].residuals < psf->pfmoffat_residuals_min)
      psf->pfmoffat_residuals_min = pfmoffat[n].residuals;
    if (pfmoffat[n].residuals > psf->pfmoffat_residuals_max)
      psf->pfmoffat_residuals_max = pfmoffat[n].residuals;

    for (i=0; i<npc; i++)
      if (dpos[i]<dstart-0.01)
        {
        dpos[i] += dstep;
        break;
        }
      else
        dpos[i] = -dstart;
    }

  psf->pfmoffat_fwhm = 0.5*(pfmoffat[nmed].fwhm_min + pfmoffat[nmed].fwhm_max);

  ellip = pfmoffat[nmed].fwhm_max + pfmoffat[nmed].fwhm_min;
  psf->pfmoffat_ellipticity = (ellip > 0.0)?
	(pfmoffat[nmed].fwhm_max - pfmoffat[nmed].fwhm_min) / ellip : 0.0;
  psf->pfmoffat_ellipticity1 = psf->pfmoffat_ellipticity
		*cosf(2.0*pfmoffat[nmed].theta*DEG);
  psf->pfmoffat_ellipticity2 = psf->pfmoffat_ellipticity
		*sinf(2.0*pfmoffat[nmed].theta*DEG);

  psf->pfmoffat_beta = pfmoffat[nmed].beta;
  psf->pfmoffat_residuals = pfmoffat[nmed].residuals;

  return;
  }


/****** psf_diagresi *********************************************************
PROTO	void psf_diagresi(double *par, double *fvec, int m, int n, void *adata)
PURPOSE	Provide a function returning residuals to lmfit.
INPUT	Pointer to the vector of parameters,
	pointer to the vector of residuals (output),
	number of parameters,
	number of data points,
	pointer to the PSF structure.
OUTPUT	-.
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	26/07/2010
 ***/
void	psf_diagresi(double *dparam, double *fvec, int m, int n, void *adata)
  {
   psfstruct	*psf;
   double	*fvect;
   float	par[PSF_DIAGNPARAM],
		*loc,
		dx,dy,dy2, ct,st, fac,inva2,invb2, cxx,cyy,cxy, a, beta,
		dx0,dy0, dxstep,dystep;
   int		i, x,y, xd,yd, w,h, nsubpix;

//printf("--%g %g %g %g %g %g %g\n", par[0],par[1],par[2],par[3],par[4],par[5],par[6]);
  psf = (psfstruct *)adata;
  nsubpix = psf->nsubpix;
  psf_unboundtobound(dparam, par);
  w = psf->size[0];
  h = psf->size[1];
  ct = cosf(par[5]*PI/180.0);
  st = sinf(par[5]*PI/180.0);
  fac = 4.0*(powf(2.0, par[6]>PSF_BETAMIN? 1.0/par[6] : 1.0/PSF_BETAMIN) - 1.0);
  inva2 = fac/(par[3]>PSF_FWHMMIN? par[3]*par[3] : PSF_FWHMMIN*PSF_FWHMMIN);
  invb2 = fac/(par[4]>PSF_FWHMMIN? par[4]*par[4] : PSF_FWHMMIN*PSF_FWHMMIN);
  cxx = inva2*ct*ct + invb2*st*st;
  cyy = inva2*st*st + invb2*ct*ct;
  cxy = 2.0*ct*st*(inva2 - invb2);
  a = par[0] / (nsubpix*nsubpix);
  beta = -par[6];
  dxstep = psf->pixsize[0]/(nsubpix*psf->pixstep);
  dystep = psf->pixsize[1]/(nsubpix*psf->pixstep);
  dy0 = -par[2] - 0.5*(nsubpix - 1.0)*dystep;
  loc = psf->loc;
  fvect = fvec;
  for (i=w*h; i--;)
    *(fvect++) = -(double)*(loc++);
  for (yd=nsubpix; yd--; dy0+=dystep)
    {
    dx0 = -par[1] - 0.5*(psf->nsubpix - 1.0)*dxstep;
    for (xd=nsubpix; xd--; dx0+=dxstep)
      {
      fvect = fvec;
      dy = dy0;
      for (y=h; y--; dy+=1.0)
        {
        dy2 = dy*dy;
        dx = dx0;
        for (x=w; x--; dx+=1.0)
          *(fvect++) += (double)(a*powf(1.0+cxx*dx*dx+cyy*dy2+cxy*dx*dy, beta));
        }
      }
    }


  psf_boundtounbound(par, dparam);

  return;
  }


/****** psf_normresi *********************************************************
PROTO	double psf_normresi(double *par, psfstruct *psf)
PURPOSE	Compute a normalized estimate of residuals w.r.t. a Moffat function.
INPUT	Pointer to the vector of fitted parameters,
	pointer to the PSF structure.
OUTPUT	Normalized residuals.
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	14/10/2009
 ***/
double	psf_normresi(float *par, psfstruct *psf)
  {
   double	norm, resi;
   float	*loc, *fvec,*fvect,
		dx,dy,dy2, ct,st, fac,inva2,invb2, cxx,cyy,cxy, a, beta,
		dx0,dy0, dxstep,dystep, val;
   int		i, x,y, xd,yd, w,h, nsubpix;

  w = psf->size[0];
  h = psf->size[1];
  ct = cosf(par[5]*PI/180.0);
  st = sinf(par[5]*PI/180.0);
  nsubpix = psf->nsubpix;
  QMALLOC(fvec, float, w*h*sizeof(float));
  fac = 4.0*(pow(2.0, par[6]>PSF_BETAMIN? 1.0/par[6] : 1.0/PSF_BETAMIN) - 1.0);
  inva2 = fac/(par[3]>PSF_FWHMMIN? par[3]*par[3] : PSF_FWHMMIN*PSF_FWHMMIN);
  invb2 = fac/(par[4]>PSF_FWHMMIN? par[4]*par[4] : PSF_FWHMMIN*PSF_FWHMMIN);
  cxx = inva2*ct*ct + invb2*st*st;
  cyy = inva2*st*st + invb2*ct*ct;
  cxy = 2.0*ct*st*(inva2 - invb2);
  a = par[0] / (nsubpix*nsubpix);
  beta = -par[6];
  dxstep = psf->pixsize[0]/(nsubpix*psf->pixstep);
  dystep = psf->pixsize[1]/(nsubpix*psf->pixstep);
  dy0 = -par[2] - 0.5*(nsubpix - 1.0)*dystep;
  fvect = fvec;
  for (i=w*h; i--;)
    *(fvect++) = 0.0;
  for (yd=nsubpix; yd--; dy0+=dystep)
    {
    dx0 = -par[1] - 0.5*(nsubpix - 1.0)*dxstep;
    for (xd=nsubpix; xd--; dx0+=dxstep)
      {
      fvect = fvec;
      dy = dy0;
      for (y=h; y--; dy+=1.0)
        {
        dy2 = dy*dy;
        dx = dx0;
        for (x=w; x--; dx+=1.0)
          *(fvect++) += a*powf(1.0+cxx*dx*dx + cyy*dy2 + cxy*dx*dy, beta);
        }
      }
    }

  resi = norm = 0.0;
  fvect = fvec;
  loc = psf->loc;
  for (i=w*h; i--;)
    {
    val = *loc+*fvect;
    resi += val*fabsf(*(fvect++) - *(loc++));
    norm += val*val;
    }

  free(fvec);

  return norm > 0.0? 2.0 * resi / norm : 1.0;
  }


/****** psf_symresi *********************************************************
PROTO	double psf_symresi(psfstruct *psf)
PURPOSE	Compute a normalized estimate of PSF asymmetry.
INPUT	Pointer to the PSF structure.
OUTPUT	Normalized residuals.
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	27/04/2007
 ***/
double	psf_symresi(psfstruct *psf)
  {
   double	resi,val,valsym,valmean,norm;
   float	*loc,*locsym;
   int		i;

  loc = psf->loc;
  locsym = loc + psf->size[0]*psf->size[1];
  resi = norm = 0.0;
  for (i=psf->size[0]*psf->size[1]; i--;)
    {
    val = (double)*(loc++);
    valsym = (double)*(--locsym);
    valmean = val + valsym;
    norm += valmean*valmean;
    resi += valmean * fabs((val - valsym));
    }

  return norm > 0.0? 2.0 * resi / norm : 1.0;
  }


/****** psf_noiseqarea *********************************************************
PROTO	double psf_noiseqarea(psfstruct *psf)
PURPOSE	Compute the noise equivalent area (in pixels^2).
INPUT	Pointer to the PSF structure.
OUTPUT	Noise equivalent area (in pix^2).
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	19/01/2011
 ***/
double	psf_noiseqarea(psfstruct *psf)
  {
   double	sum,sum2,val;
   float	*loc;
   int		i;

  loc = psf->loc;
  sum = sum2 = 0.0;
  for (i=psf->size[0]*psf->size[1]; i--;)
    {
    val = (double)*(loc++);
    sum += val;
    sum2 += val*val;
    }

  return sum2 > 0.0? psf->pixstep*psf->pixstep*sum*sum/sum2 : 0.0;
  }


/****** psf_moffat *********************************************************
PROTO	void	psf_moffat(psfstruct *psf, moffatstruct *moffat)
PURPOSE	Generate an image of a Moffat PSF.
INPUT	Pointer to the PSF structure,
	pointer to the Moffat structure.
OUTPUT	-.
NOTES	-.
AUTHOR	E. Bertin (IAP)
VERSION	03/11/2009
 ***/
void	psf_moffat(psfstruct *psf, moffatstruct *moffat)
  {
   float	*loc,
		dx,dy,dy2, ct,st, fac,inva2,invb2, cxx,cyy,cxy, a, beta,
		dx0,dy0, dxstep,dystep, xc,yc;
   int		i, x,y, xd,yd, w,h, nsubpix;

  w = psf->size[0];
  h = psf->size[1];
  xc = moffat->xc[0];
  yc = moffat->xc[1];
  nsubpix = moffat->nsubpix;
  a = moffat->amplitude*psf->pixstep*psf->pixstep / (nsubpix*nsubpix);
  beta = -moffat->beta;
  ct = cos(moffat->theta*PI/180.0);
  st = sin(moffat->theta*PI/180.0);
  fac = 4*(pow(2, -1.0/beta) - 1.0);
  inva2 = fac/(moffat->fwhm_max*moffat->fwhm_max)*psf->pixstep*psf->pixstep;
  invb2 = fac/(moffat->fwhm_min*moffat->fwhm_min)*psf->pixstep*psf->pixstep;
  cxx = inva2*ct*ct + invb2*st*st;
  cyy = inva2*st*st + invb2*ct*ct;
  cxy = 2.0*ct*st*(inva2 - invb2);
  dxstep = psf->pixsize[0]/(nsubpix*psf->pixstep);
  dystep = psf->pixsize[1]/(nsubpix*psf->pixstep);
  dy0 = -yc - 0.5*(nsubpix - 1.0)*dystep;
  loc = psf->loc;
  for (i=w*h; i--;)
    *(loc++) = 0.0;
  for (yd=nsubpix; yd--; dy0+=dystep)
    {
    dx0 = -xc - 0.5*(nsubpix - 1.0)*dxstep;
    for (xd=nsubpix; xd--; dx0+=dxstep)
      {
      loc = psf->loc;
      dy = dy0;
      for (y=h; y--; dy+=1.0)
        {
        dy2 = dy*dy;
        dx = dx0;
        for (x=w; x--; dx+=1.0)
          *(loc++) += a*powf(1.0+cxx*dx*dx + cyy*dy2 + cxy*dx*dy, beta);
        }
      }
    }

  return;
  }


/****** psf_boundtounbound **************************************************
PROTO	void psf_boundtounbound(profitstruct *profit)
PURPOSE	Convert parameters from bounded to unbounded space.
INPUT	Pointer to the input vector of parameters,
	pointer to the output vector of parameters.
OUTPUT	-.
AUTHOR	E. Bertin (IAP)
VERSION	26/07/2010
 ***/
void    psf_boundtounbound(float *param, double *dparam)
  {
   double       num,den;
   int          p;

  for (p=0; p<PSF_DIAGNPARAM; p++)
    if (moffat_parammin[p]!=moffat_parammax[p])
      {
      num = param[p] - moffat_parammin[p];
      den = moffat_parammax[p] - param[p];
      dparam[p] = num>1e-50? (den>1e-50? log(num/den): 50.0) : -50.0;
      }
    else if (moffat_parammax[p] > 0.0 || moffat_parammax[p] < 0.0)
        dparam[p] = param[p] / moffat_parammax[p];

  return;

  }


/****** psf_unboundtobound **************************************************
PROTO	void profit_unboundtobound(profitstruct *profit)
PURPOSE	Convert parameters from unbounded to bounded space.
INPUT	Pointer to the input vector of parameters,
	pointer to the output vector of parameters.
OUTPUT	-.
AUTHOR	E. Bertin (IAP)
VERSION	26/07/2010
 ***/
void    psf_unboundtobound(double *dparam, float *param)
  {
   int          p;

  for (p=0; p<PSF_DIAGNPARAM; p++)
    param[p] = (moffat_parammin[p]!=moffat_parammax[p])?
		(moffat_parammax[p] - moffat_parammin[p])
			/ (1.0 + exp(-(dparam[p]>50.0? 50.0
				: (dparam[p]<-50.0? -50.0: dparam[p]))))
			+ moffat_parammin[p]
		: dparam[p]*moffat_parammax[p];

  return;
  }

