#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <misc.h>
#include <maf.h>
#include <sufficient_stats.h>
#include <tree_likelihoods.h>
#include <phylo_hmm.h>
#include <indel_history.h>
#include <dmotif_indel_mod.h>
#include <subst_distrib.h>
#include <dmotif_phmm.h>
#include <pssm.h>
#include "dmsimulate.help"

#define DEFAULT_RHO 0.3
#define DEFAULT_PHI 0.5
#define DEFAULT_MU 0.01
#define DEFAULT_NU 0.01
#define DEFAULT_ZETA 0.001
#define DEFAULT_MSA_LEN 1000000;

int main(int argc, char *argv[]) {
  char c;
  int opt_idx, *path;
/*   int i; */
  MSA *msa;
  List *tmpl;
  DMotifPhyloHmm *dm;
  GFF_Set *motifs;
  PSSM *motif;

  struct option long_opts[] = {
    {"refseq", 1, 0, 'M'},
    {"msa-format", 1, 0, 'i'},
    {"refidx", 1, 0, 'r'},
    {"rho", 1, 0, 'R'},
    {"phi", 1, 0, 'p'},
    {"zeta", 1, 0, 'z'},
    {"transitions", 1, 0, 't'},    
    {"expected-length", 1, 0, 'E'},
    {"target-coverage", 1, 0, 'C'},
    {"seqname", 1, 0, 'N'},
    {"idpref", 1, 0, 'P'},
    {"indel-model", 1, 0, 'I'},
    {"msa-length", 1, 0, 'L'},
    {"help", 0, 0, 'h'},
    {0, 0, 0, 0}
  };

  /* arguments and defaults for options */
  FILE *refseq_f = NULL, *msa_f = NULL, *motif_f = NULL;
  msa_format_type msa_format = FASTA;
  TreeModel *source_mod;
  double rho = DEFAULT_RHO, mu = DEFAULT_MU, nu = DEFAULT_NU, 
    phi = DEFAULT_PHI, zeta = DEFAULT_ZETA, gamma = -1, omega = -1, 
    alpha_c = -1, beta_c = -1, tau_c = -1, epsilon_c = -1,
    alpha_n = -1, beta_n = -1, tau_n = -1, epsilon_n = -1;
  int set_transitions = FALSE, refidx = 1, msa_len = DEFAULT_MSA_LEN;
  char *seqname = NULL, *idpref = NULL;
  
  while ((c = getopt_long(argc, argv, "R:t:p:z:E:C:r:M:i:N:P:I:L:h", 
			  long_opts, &opt_idx)) != -1) {
    switch (c) {
    case 'R':
      rho = get_arg_dbl_bounds(optarg, 0, 1);
      break;
    case 't':
      set_transitions = TRUE;
      tmpl = get_arg_list_dbl(optarg);
      if (lst_size(tmpl) != 2) 
        die("ERROR: bad argument to --transitions.\n");
      mu = lst_get_dbl(tmpl, 0);
      nu = lst_get_dbl(tmpl, 1);
      if (mu <= 0 || mu >= 1 || nu <= 0 || nu >= 1)
        die("ERROR: bad argument to --transitions.\n");
      lst_free(tmpl);
      break;
    case 'p':
      phi = get_arg_dbl_bounds(optarg, 0, 1);
      break;
    case 'z':
      zeta = get_arg_dbl_bounds(optarg, 0, 1);
      break;
    case 'E':
      omega = get_arg_dbl_bounds(optarg, 1, INFTY);
      mu = 1/omega;
      break;
    case 'C':
      gamma = get_arg_dbl_bounds(optarg, 0, 1);
      break;
    case 'r':
      refidx = get_arg_int_bounds(optarg, 0, INFTY);
      break;
    case 'M':
      refseq_f = fopen_fname(optarg, "r");
      break;
    case 'i':
      msa_format = msa_str_to_format(optarg);
      if (msa_format == -1)
        die("ERROR: unrecognized alignment format.\n");
      break;
    case 'N':
      seqname = optarg;
      break;
    case 'P':
      idpref = optarg;
      break;
    case 'I':
      tmpl = get_arg_list_dbl(optarg);
      if (lst_size(tmpl) != 3 && lst_size(tmpl) != 6)
        die("ERROR: bad argument to --indel-model.\n");
      alpha_n = lst_get_dbl(tmpl, 0);
      beta_n = lst_get_dbl(tmpl, 1);
      tau_n = lst_get_dbl(tmpl, 2);
      if (lst_size(tmpl) == 6) {
        alpha_c = lst_get_dbl(tmpl, 3);
        beta_c = lst_get_dbl(tmpl, 4);
        tau_c = lst_get_dbl(tmpl, 5);
      }
      else {
        alpha_c = alpha_n; beta_c = beta_n; tau_c = tau_n;
      }
      if (alpha_c <= 0 || alpha_c >= 1 || beta_c <= 0 || beta_c >= 1 || 
          tau_c <= 0 || tau_c >= 1 || alpha_n <= 0 || alpha_n >= 1 || 
          beta_n <= 0 || beta_n >= 1 || tau_n <= 0 || tau_n >= 1)
        die("ERROR: bad argument to --indel-model.\n");
      break;
    case 'L':
      msa_len = get_arg_int_bounds(optarg, 0, INFTY);
      break;
    case 'h':
      printf(HELP);
      exit(0);
    case '?':
      die("Bad argument.  Try 'dmsimulate -h'.\n");
    }
  }

  if (optind != argc - 3) 
    die("Three arguments required.  Try 'dmsimulate -h'.\n");

  if (set_transitions && (gamma != -1 || omega != -1))
    die("ERROR: --transitions and --target-coverage/--expected-length cannot be used together.\n");

  if ((gamma != -1 && omega == -1) || (gamma == -1 && omega != -1))
    die("ERROR: --target-coverage and --expecteed-length must be used together.\n");

  if (gamma != -1)
    nu = gamma/(1-gamma) * mu;

  fprintf(stderr, "Reading tree model from %s...\n", argv[optind]);
  source_mod = tm_new_from_file(fopen_fname(argv[optind], "r"));

  fprintf(stderr, "Reading motif model from %s...\n", argv[optind+1]);
  motif_f = fopen_fname(argv[optind+1], "r");
  motif = mot_read(motif_f);

  if (source_mod->nratecats > 1) 
    die("ERROR: rate variation not currently supported.\n");

  if (source_mod->order > 0)
    die("ERROR: only single nucleotide models are currently supported.\n");

  if (!tm_is_reversible(source_mod->subst_mod))
    fprintf(stderr, "WARNING: p-value computation assumes reversibility and your model is non-reversible.\n");

  /* Name ancestral nodes */
  tr_name_ancestors(source_mod->tree);

  /* Instantiate the dmotif phmm */
  dm = dm_new(source_mod, motif, rho, mu, nu, phi, zeta, alpha_c, beta_c, 
              tau_c, epsilon_c, alpha_n, beta_n, tau_n, epsilon_n, FALSE,
	      FALSE, FALSE, FALSE);

  /* set seqname and idpref, if necessary */
  if (seqname == NULL || idpref == NULL) {
    /* derive default from msa file name root */
    String *tmp = str_new_charstr(argv[optind+2]);
    if (!str_equals_charstr(tmp, "-")) {
      str_remove_path(tmp);
      str_root(tmp, '.');
      if (idpref == NULL) idpref = strdup(tmp->chars);
      if (seqname == NULL) seqname = tmp->chars;    
    }
    else if (seqname == NULL) seqname = "refseq";
  }

  /* Simulate the alignment */
  fprintf(stderr, "Simulating multiple sequence alignment of length %d...\n",
	  msa_len);
  path = smalloc(msa_len * sizeof(int));
  msa = tm_generate_msa(msa_len, dm->phmm->hmm, dm->phmm->mods, path);

/*   for (i = 0; i < msa_len; i++) */
/*     fprintf(stderr, "%d", path[i]); */
/*   fprintf(stderr, "\n"); */

  /* Build the motif features GFF */
  motifs = dm_labeling_as_gff(dm->phmm->cm, path, msa_len, 
			      dm->m->width, dm->phmm->state_to_cat,
			      dm->state_to_motifpos, dm->phmm->reverse_compl,
			      seqname, "DMSIMULATE", NULL, NULL, idpref);

  /* Print the alignment to the msa file */
  fprintf(stderr, "Writing multiple sequence alignment to %s in %s format...\n",
	  argv[optind+2], msa_suffix_for_format(msa_format));
  msa_f = fopen_fname(argv[optind+2], "w");
  msa_print(msa_f, msa, msa_format, 0);

  /* Print motif features to stdout */
  fprintf(stderr, "Writing GFF to stdout...\n");
  gff_print_set(stdout, motifs);

  free(path);
  fprintf(stderr, "Done.\n");
  
  return 0;
}

