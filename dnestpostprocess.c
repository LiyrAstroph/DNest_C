/*
 * C version of Diffusive Nested Sampling (DNest4) by Brendon J. Brewer
 *
 * Yan-Rong Li, liyanrong@mail.ihep.ac.cn
 * Jun 30, 2016
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <gsl/gsl_rng.h>

#include "dnestvars.h"

/*! \file dnestpostprocess.c
 *  \brief post process the sample generated by dnest. 
 */ 

int cmp_sample(const void *pa, const void *pb);
typedef struct
{
  double logl, tiebreaker;
  int id;
}SampleType;

/*
 * This function calculates log(exp(x1) - exp(x2)).
 */
double logdiffexp(double x1, double x2)  // x1 is larger
{
  double biggest = x1;
  double xx1 = x1 - biggest, xx2 = x2 - biggest;
  return log(exp(xx1) - exp(xx2)) + biggest;
}
/*
 * This functions calculates log(exp(x1)+...+exp(xn)).
 */
double logsumexp(double *x, int n)
{
  int j;
  double sum, max;
  
  max = x[0];
  for(j = 0; j < n; j++)
  {
    max = fmax(max, x[j]);
  }
  sum = 0.0;
  for(j=0; j< n; j++)
  {
    sum += exp( x[j] - max);
  }
  return log(sum) + max;
}

void postprocess(double temperature)
{
  printf("# Starts postprocess.\n");
  FILE *fp;
  
  double **levels_orig, **sample_info, *logl;
  int *sandwhich;
  void *sample;
  int i, j;
  int num_levels, num_samples, num_params;
  char buf[BUF_MAX_LENGTH];
  int moreSample = 1;
  
  // read number of levels and samples
  fp = fopen(options.sampler_state_file, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "# Error: Cannot open file %s.\n", options.sampler_state_file);
    exit(0);
  }
  fscanf(fp, "%d %d\n", &num_levels, &num_samples);
  fclose(fp);
  
  // allocate memory for levels
  levels_orig = malloc(num_levels * sizeof(double *));
  for(i=0; i< num_levels; i++)
  {
    levels_orig[i] = malloc(3 * sizeof(double));
  }
  
  // allocate memory for sample_info
  sample_info = malloc(num_samples * sizeof(double *));
  for(i=0; i< num_samples; i++)
  {
    sample_info[i] = malloc(3 * sizeof(double));
  }
  
  // allocate memory for samples
  num_params = get_num_params();
  sample = (void *)malloc(num_samples * size_of_modeltype);
  logl = (void *)malloc(num_samples * sizeof(double));
  sandwhich = malloc(num_samples * sizeof(int));
  
  // read levels
  fp = fopen(options.levels_file, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "# Error: Cannot open file %s.\n", options.levels_file);
    exit(0);
  }
  fgets(buf, BUF_MAX_LENGTH, fp);
  for(i=0; i < num_levels; i++)
  {
    fgets(buf, BUF_MAX_LENGTH, fp);
    
    if(sscanf(buf, "%lf %lf %lf", &levels_orig[i][0], &levels_orig[i][1], &levels_orig[i][2]) < 3)
    {
      fprintf(stderr, "# Error: Cannot read file %s.\n", options.levels_file);
      exit(0);
    }
  }
  fclose(fp);

  // read sample
  double *psample;
  fp = fopen(options.sample_file, "r");
  if(fp == NULL)
  {
    fprintf(stderr, "# Error: Cannot open file %s.\n", options.sample_file);
    exit(0);
  }
  fgets(buf, BUF_MAX_LENGTH, fp);
  for(i=0; i < num_samples; i++)
  {
    for(j=0; j < num_params; j++)
    {
      psample = (double *)(sample+i*size_of_modeltype+ j*sizeof(double)) ;
      if(fscanf(fp, "%lf", psample) < 1)
      {
        printf("%f\n", *psample);
        fprintf(stderr, "# Error: Cannot read file %s.\n", options.sample_file);
        exit(0);
      }
    }
    //printf("%f %f %f\n", sample[i].params[0], sample[i].params[1], sample[i].params[2]);
  }
  fclose(fp);
  
  // read sample_info
  if(flag_sample_info == 0) //no need to recalculate
  {
    fp = fopen(options.sample_info_file, "r");
    if(fp == NULL)
    {
      fprintf(stderr, "# Error: Cannot open file %s.\n", options.sample_info_file);
      exit(0);
    }
    fgets(buf, BUF_MAX_LENGTH, fp);
    for(i=0; i < num_samples; i++)
    {
      fgets(buf, BUF_MAX_LENGTH, fp);
      if(sscanf(buf, "%lf %lf %lf", &sample_info[i][0], &sample_info[i][1], &sample_info[i][2]) < 3)
      {
        fprintf(stderr, "# Error: Cannot read file %s.\n", options.sample_info_file);
        exit(0);
      }
    }
    fclose(fp);
  }
  else   //sample_info file doest not exist, need to recalculate.
  {
    fp = fopen(options.sample_info_file, "w");
    if(fp == NULL)
    {
      fprintf(stderr, "# Error: Cannot open file %s.\n", options.sample_info_file);
      exit(0);
    }
    printf("# Dnest starts to recalculate the sample info.\n");
    fprintf(fp, "# level assignment, log likelihood, tiebreaker, ID.\n");

    for(i=0; i < num_samples; i++)
    {
      sample_info[i][1] = log_likelihoods_cal_initial(sample+i*size_of_modeltype);
      sample_info[i][2] = dnest_rand();

      for(j=0; j<num_levels; j++)
      {
        if(sample_info[i][1] < levels_orig[j][1])
          break;
      }

      /*j=num_levels-1;  // find out the highest allowed level
      while( (sample_info[i][1] < levels_orig[j][1]) && (j>=0) )
      {
        j--;
      }*/

      sample_info[i][0] = (double)dnest_rand_int(j); // randomly assign a level [0, j-1]

      fprintf(fp, "%d %e %f %d\n", (int)sample_info[i][0], sample_info[i][1], sample_info[i][2], 1);
    }
    fclose(fp);
  }
  //tempering with a temperature
  for(i=0; i<num_samples; i++)
    logl[i] = sample_info[i][1] / temperature;

  // finding sandwhiching levels for each samples
  for(i=0; i<num_samples; i++)
  {
    sandwhich[i] = (int)sample_info[i][0];
    
    for(j=sandwhich[i]; j < num_levels; j++)
    {
      if( sample_info[i][1] > levels_orig[j][1] )
        sandwhich[i] = j;
    }
    //printf("%f %d\n", logl[i], sandwhich[i]);
  }
  
  double *logx_samples, *logp_samples, *logP_samples;
  double logx_min, logx_max, Umin, U;
  int num_samples_thisLevel;
  double *logx_samples_thisLevel;
  SampleType *logl_samples_thisLevel;
  
  double left, right;
  
  logx_samples = malloc(num_samples * sizeof(double));
  logp_samples = malloc(num_samples * sizeof(double));
  logP_samples = malloc(num_samples * sizeof(double));
  
  logx_samples_thisLevel = malloc(num_samples * sizeof(double));
  logl_samples_thisLevel = malloc(num_samples * sizeof(SampleType));
  
  for(i=0; i<num_levels; i++)
  {
    logx_max = levels_orig[i][0];
    if(i == num_levels - 1)
      logx_min = -1.0E300;
    else
      logx_min = levels_orig[i+1][0];
    
    Umin = exp( logx_min - logx_max);
    
    // finding the samples sandwhiched by this levels
    num_samples_thisLevel = 0;
    for(j=0; j<num_samples; j++)
      if( sandwhich[j] == i )
      {
        logl_samples_thisLevel[num_samples_thisLevel].logl = sample_info[j][1]; // logl
        logl_samples_thisLevel[num_samples_thisLevel].tiebreaker = sample_info[j][2]; // tiebreaker
        logl_samples_thisLevel[num_samples_thisLevel].id = j; // id
        
        num_samples_thisLevel++;
      }
    
    //printf("%d\n", num_samples_thisLevel);
    
    for(j=0; j<num_samples_thisLevel; j++)
    {
      U = Umin + (1.0 - Umin) * ( 1.0/(1.0 + num_samples_thisLevel) 
           + ( 1.0 - 2.0/(1.0 + num_samples_thisLevel) ) * (num_samples_thisLevel-1 - j)/(num_samples_thisLevel - 1.0) );
      logx_samples_thisLevel[j] = logx_max + log(U);
    }
    
    qsort(logl_samples_thisLevel, num_samples_thisLevel, sizeof(SampleType), cmp_sample);
    
    //printf("%f %f %d %f\n", logl_samples_thisLevel[0].logl, logl_samples_thisLevel[0].tiebreaker, logl_samples_thisLevel[0].id, logx_samples_thisLevel[0]);
    //printf("%f %f %d %f\n", logl_samples_thisLevel[1].logl, logl_samples_thisLevel[1].tiebreaker, logl_samples_thisLevel[1].id, logx_samples_thisLevel[1]);
    
    
    for(j = 0; j<num_samples_thisLevel; j++)
    {
      if(j != num_samples_thisLevel - 1)
        left = logx_samples_thisLevel[j+1];
      else if (i == num_levels - 1)
        left = -1.0E300;
      else
        left = levels_orig[i+1][0];
        
      if( j!= 0)
        right = logx_samples_thisLevel[j-1];
      else
        right = levels_orig[i][0];
      
      //printf("%e %e %e\n", right, left, logdiffexp(right, left));
      
      logx_samples[logl_samples_thisLevel[j].id] = logx_samples_thisLevel[j];
      logp_samples[logl_samples_thisLevel[j].id] = log(0.5)  + logdiffexp(right, left);
    }
  }
  
  double sum, max, logz_estimates, H_estimates, ESS;
  
  
  sum = logsumexp(logp_samples, num_samples);
  for(j = 0; j < num_samples; j++)
  {
    logp_samples[j] -= sum;
    //logP_samples[j] = logp_samples[j] + sample_info[j][1];
    logP_samples[j] = logp_samples[j] + logl[j];
  }
  
  logz_estimates = logsumexp(logP_samples, num_samples);
  
  H_estimates = -logz_estimates;
  ESS = 0.0;
  for(j=0; j<num_samples; j++)
  {
    logP_samples[j] -= logz_estimates; 
    //H_estimates += exp(logP_samples[j]) * sample_info[j][1];
    H_estimates += exp(logP_samples[j]) * logl[j];
    ESS += -logP_samples[j]*exp(logP_samples[j]);
  }
  ESS = exp(ESS);
    
  printf("log(Z) = %f\n", logz_estimates);
  printf("H = %f\n", H_estimates);
  printf("Effective sample size = %f\n", ESS);
  
  // resample to uniform weight
  
  int num_ps = moreSample*ESS;
  void *posterior_sample;
  double *posterior_sample_info;
  int which;

  const gsl_rng_type * dnest_post_gsl_T;
  gsl_rng * dnest_post_gsl_r;

  dnest_post_gsl_T = (gsl_rng_type *) gsl_rng_default;
  dnest_post_gsl_r = gsl_rng_alloc (dnest_post_gsl_T);
#ifndef Debug
  gsl_rng_set(dnest_post_gsl_r, time(NULL) + thistask);
#else
  gsl_rng_set(dnest_post_gsl_r, 8888 + thistask);
  printf("# debugging, random seed %d\n", 8888 + thistask);
#endif  

  posterior_sample = malloc(num_ps * size_of_modeltype);
  posterior_sample_info = malloc(num_ps * sizeof(double));
  
  max = logP_samples[0];
  for(j=0; j<num_samples; j++)
    max = fmax(logP_samples[j], max);
  for(j=0; j<num_samples; j++)
    logP_samples[j] -= max;
  
  for(j=0; j<num_ps; j++)
  {
    while(true)
    {
      which =  gsl_rng_uniform_int(dnest_post_gsl_r, num_samples);
      if(log(gsl_rng_uniform(dnest_post_gsl_r)) < logP_samples[which])
      {
        posterior_sample_info[j] = logl[j]; // add back the subtracted value.
        copy_model(posterior_sample+j*size_of_modeltype, sample+which*size_of_modeltype);
        break;
      }
    }
  }
  
  //save posterior sample
  fp = fopen(options.posterior_sample_file, "w");
  if(fp == NULL)
  {
    fprintf(stderr, "# Error: Cannot open file %s.\n", options.posterior_sample_file);
    exit(0);
  }
  fprintf(fp, "# %d\n", num_ps);
  for(i=0; i<num_ps; i++)
  {
    print_particle(fp, posterior_sample + i*size_of_modeltype);
  }
  fclose(fp);

  fp = fopen(options.posterior_sample_info_file, "w");
  if(fp == NULL)
  {
    fprintf(stderr, "# Error: Cannot open file %s.\n", options.posterior_sample_info_file);
    exit(0);
  }
  fprintf(fp, "# %d\n", num_ps);
  for(i=0; i<num_ps; i++)
  {
    fprintf(fp, "%e\n", posterior_sample_info[i]);
  }
  fclose(fp);
  
  for(i=0; i<num_levels; i++)
   free(levels_orig[i]);
  free(levels_orig);

  for(i=0; i<num_samples; i++)
    free(sample_info[i]);
  free(sample_info);
  free(logl);

  free(logx_samples);
  free(logx_samples_thisLevel);
  free(logp_samples);
  free(logP_samples);
  free(logl_samples_thisLevel);
  free(sandwhich);
  free(sample);
  free(posterior_sample);
  free(posterior_sample_info);

  gsl_rng_free(dnest_post_gsl_r);

  printf("# Ends dnest postprocess.\n");
}

int cmp_sample(const void *pa, const void *pb)
{
  SampleType *a = (SampleType *)pa;
  SampleType *b = (SampleType *)pb;

  // in acesending order
  if(a->logl > b->logl)
    return true;
  if( a->logl == b->logl && a->tiebreaker > b->tiebreaker)
    return true;
  
  return false;
}
