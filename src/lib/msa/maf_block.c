 /***************************************************************************
 * PHAST: PHylogenetic Analysis with Space/Time models
 * Copyright (c) 2002-2005 University of California, 2006-2009 Cornell 
 * University.  All rights reserved.
 *
 * This source code is distributed under a BSD-style license.  See the
 * file LICENSE.txt for details.
 ***************************************************************************/

/* $Id: maf_block.c,v 1.29 2009-01-23 21:11:47 mt269 Exp $ */

/** \file maf_block.c
    Reading of individual blocks from MAF ("Multiple Alignment Format")
    files, as produced by MULTIZ and TBA.  See maf.h for details.
    \ingroup msa
*/

#include <sufficient_stats.h>
#include <msa.h>
#include <maf_block.h>
#include <hashtable.h>
#include <ctype.h>


MafBlock *mafBlock_new() {
  MafBlock *block = smalloc(sizeof(MafBlock));
  block->aLine = NULL;
  block->specMap = hsh_new(100);
  block->seqlen = -1;
  block->data = lst_new_ptr(20);
  block->prev = block->next = NULL;
  return block;
}

//allocates new subBlock and initializes
MafSubBlock *mafBlock_new_subBlock()  {
  MafSubBlock *sub = smalloc(sizeof(MafSubBlock));
  sub->seq = NULL;
  sub->quality = NULL;
  sub->src = NULL;
  sub->specName = NULL;
  sub->numLine = 0;
  return sub;
}

//parses a line from maf block starting with 'e' or 's' and returns a new MafSubBlock 
//object. 
MafSubBlock *mafBlock_get_subBlock(String *line) {
  int i;
  List *l = lst_new_ptr(7);
  String *str;
  MafSubBlock *sub;

  if (7 != str_split(line, NULL, l)) 
    die("Error: mafBlock_get_subBlock expected seven fields in MAF line starting "
	"with %s\n",
	((String*)lst_get_ptr(l, 0))->chars);
  
  sub = mafBlock_new_subBlock();
  
  //field 0: should be 's' or 'e'
  str = (String*)lst_get_ptr(l, 0);
  if (str_compare_charstr(str, "s")==0)
    sub->lineType[0]='s';
  else if (str_compare_charstr(str, "e")==0)
    sub->lineType[0]='e';
  else die("ERROR: mafBlock_get_subBlock expected first field 's' or 'e' (got %s)\n",
	   str->chars);

  //field 1: should be src.  Also set specName
  sub->src = (String*)lst_get_ptr(l, 1);
  sub->specName = str_new_charstr(sub->src->chars);
  str_shortest_root(sub->specName, '.');

  //field 2: should be start
  sub->start = atol(((String*)lst_get_ptr(l, 2))->chars);
  
  //field 3: should be length
  sub->size = atoi(((String*)lst_get_ptr(l, 3))->chars);

  //field 4: should be strand
  str = (String*)lst_get_ptr(l, 4);
  if (str_compare_charstr(str, "+")==0)
    sub->strand = '+';
  else if (str_compare_charstr(str, "-")==0)
    sub->strand = '-';
  else die("ERROR: got strand %s\n", str->chars);
  
  //field 5: should be srcSize
  sub->srcSize = atol(((String*)lst_get_ptr(l, 5))->chars);

  //field 6: sequence if sLine, eStatus if eLine.
  str = (String*)lst_get_ptr(l, 6);
  if (sub->lineType[0]=='s')
    sub->seq = str;
  else {
    assert(sub->lineType[0]=='e');
    if (str->length != 1)
      die("ERROR: e-Line with status %s in MAF block\n", str->chars);
    sub->eStatus = str->chars[0];
    if (sub->eStatus != 'C' && sub->eStatus != 'I' && sub->eStatus != 'M' &&
	sub->eStatus != 'n')
      die("ERROR: e-Line has illegal status %c\n", sub->eStatus);
  }
  sub->numLine = 1;
  //free all strings except field 1 and field 6 when lineType=='s'
  for (i=0; i<6; i++)
    if (i!=1 && (i!=6 || sub->lineType[0]!='s')) 
      str_free((String*)lst_get_ptr(l, i));
  lst_free(l);
  return sub;
}

void mafBlock_add_iLine(String *line, MafSubBlock *sub) {
  List *l = lst_new_ptr(6);
  String *str;
  int i;

  if (sub->numLine<1 || sub->lineType[0]!='s') 
    die("ERROR: got i-Line without preceding s-Line in MAF block\n");
  
  if (6 != str_split(line, NULL, l))
    die("ERROR: expected six fields in MAF line starting with 'i' (got %i)\n",
	lst_size(l));

  //field[0] should be 'i'
  assert(str_compare_charstr((String*)lst_get_ptr(l, 0), "i")==0);

  //field[1] should be src, and should match src already set in sub
  if (str_compare((String*)lst_get_ptr(l, 1), sub->src) != 0)
    die("iLine sourceName does not match preceding s-Line (%s, %s)\n", 
	((String*)lst_get_ptr(l, 1))->chars, sub->src->chars);

  for (i=0; i<2; i++) {

    //field[2,4] should be leftStatus, rightStauts
    str = (String*)lst_get_ptr(l, i*2+2);
    if (str->length != 1) die("ERROR: i-Line got illegal %sStatus = %s\n",
			      i==0 ? "left": "right", str->chars);
    sub->iStatus[i] = str->chars[0];
    if (sub->iStatus[i] != 'C' && sub->iStatus[i] != 'I' &&
	sub->iStatus[i] != 'N' && sub->iStatus[i] != 'n' &&
	sub->iStatus[i] != 'M' && sub->iStatus[i] != 'T')
      die("ERROR: i-Line got illegal %sStatus = '%c'\n",
	  i==0 ? "left" : "right", sub->iStatus[i]);

    //field 3,5 should be leftCount, rightCount
    str = (String*)lst_get_ptr(l, i*2+3);
    sub->iCount[i] = atoi(str->chars);
  }
  
  for (i=0; i<6; i++) str_free((String*)lst_get_ptr(l, i));
  lst_free(l);
  sub->lineType[sub->numLine++] = 'i';
}


void mafBlock_add_qLine(String *line, MafSubBlock *sub) {
  List *l = lst_new_ptr(3);
  String *str;
  int i;

  if (sub->numLine<1 || sub->lineType[0]!='s') 
    die("ERROR: got q-Line without preceding s-Line in MAF block\n");

  if (3 != str_split(line, NULL, l))
    die("ERROR: expected three fields in q-Line of maf file, got %i\n", lst_size(l));
  
  //field[0] should be 'q'
  assert(str_compare_charstr((String*)lst_get_ptr(l, 0), "q")==0);
  
  //field[1] should be src, and should match src already set in sub
  if (str_compare((String*)lst_get_ptr(l, 1), sub->src) != 0)
    die("iLine sourceName does not match preceding s-Line (%s, %s)\n", 
	((String*)lst_get_ptr(l, 1))->chars, sub->src->chars);

  //field[2] should be quality
  assert(sub->seq != NULL);
  str = (String*)lst_get_ptr(l, 2);
  if (sub->seq->length != str->length) 
    die("ERROR: length of q-line does not match sequence length\n");
  sub->quality = str;
  for (i=0; i<sub->quality->length; i++) {
    if (sub->seq->chars[i] == '-') {
      if (sub->quality->chars[i] != '-') 
	die("ERROR: got quality score where alignment char is gap\n");
    } else {
      if (sub->quality->chars[i] != 'F' && sub->quality->chars[i] < '0' &&
	  sub->quality->chars[i] > '9')
	die("ERROR: Illegal quality score '%c' in MAF block\n", 
	    sub->quality->chars[i]);
    }
  }
   
  for (i=0; i<2; i++) str_free((String*)lst_get_ptr(l, i));
  lst_free(l);
  sub->lineType[sub->numLine++] = 'q';
}


//read next block in mfile and return MafBlock object or NULL if EOF.
//specHash and numSpec are not used, but they should both either be NULL or not-NULL.
//if not-NULL, they should be initialized, and any new species encountered will
//be added to the hash, with numspec increased accordingly
MafBlock *mafBlock_read_next(FILE *mfile, int *numSpec, Hashtable *specHash) {
  int i;
  char firstchar;
  String *currLine = str_new(1000);
  MafBlock *block=NULL;
  MafSubBlock *sub=NULL;

  if ((specHash==NULL && numSpec!=NULL) ||
      (specHash!=NULL && numSpec==NULL)) 
    die("ERROR: mafblock_read_next expects numSpec and specHash both to be non-NULL,"
	"or both to be NULL\n");

  while (EOF != str_readline_alloc(currLine, mfile)) {
    str_trim(currLine);
    if (currLine->length==0) {  //if blank line, it is either first or last line
      if (block == NULL) continue;
      else break;
    }
    firstchar = currLine->chars[0];
    if (firstchar == '#') continue;  //ignore comments
    if (block == NULL) {
      if (firstchar != 'a') 
	die("ERROR: first line of MAF block should start with 'a'\n");
      block = mafBlock_new();
      block->aLine = str_new_charstr(currLine->chars);
    }
    //if 's' or 'e', then this is first line of data for this species
    else if (firstchar == 's' || firstchar == 'e') {
      sub = mafBlock_get_subBlock(currLine);
      if (ptr_to_int(hsh_get(block->specMap, sub->src->chars)) != -1) 
	die("ERROR: mafBlock has two alignments with same srcName (%s)\n", 
	    sub->src->chars);
      hsh_put(block->specMap, sub->src->chars, int_to_ptr(lst_size(block->data)));
      hsh_put(block->specMap, sub->specName->chars, int_to_ptr(lst_size(block->data)));
      lst_push_ptr(block->data, (void*)sub);
      if (specHash != NULL) {
	if (-1 == ptr_to_int(hsh_get(specHash, sub->specName->chars))) {
	  hsh_put(specHash, sub->specName->chars, int_to_ptr(*numSpec));
	  (*numSpec)++;
	}
      }
    }
    else {
      if (firstchar == 'i')
	mafBlock_add_iLine(currLine, sub);
      else if (firstchar == 'q')
	mafBlock_add_qLine(currLine, sub);
      else die("ERROR: found line in MAF block starting with '%c'\n", firstchar);
    }
  }
  free(currLine);
  if (block == NULL) return NULL;

  //set seqlen and make sure all seq arrays agree
  for (i=0; i<lst_size(block->data); i++) {
    sub = (MafSubBlock*)lst_get_ptr(block->data, i);
    if (sub->lineType[0]=='e') continue;
    if (block->seqlen == -1) block->seqlen = sub->seq->length;
    else if (sub->seq->length != block->seqlen) {
      die("ERROR: lengths of sequences in MAF block do not agree (%i, %i)\n",
	  block->seqlen, sub->seq->length);
    }
  }
  return block;
}

//returns 1 if 
int mafBlock_all_gaps(MafBlock *block) {
  MafSubBlock *sub;
  int i, j;
  for (i=0; i<lst_size(block->data); i++) {
    sub = (MafSubBlock*)lst_get_ptr(block->data, i);
    if (sub->lineType[0]=='e') continue;
    for (j=0; j<block->seqlen; j++)
      if (sub->seq->chars[j] != '-') return 0;
  }
  return 1;
}


//sets fieldSize[i] to maximum length of field i in MAF, so that block
//can be printed with nice formatting
void mafBlock_get_fieldSizes(MafBlock *block, int fieldSize[6]) {
  int i;
  MafSubBlock *sub;
  char tempstr[1000];
  for (i=0; i<6; i++) fieldSize[i] = 0;

  fieldSize[0] = 1;  //this is always one character
  fieldSize[4] = 1;  //this is always one character (strand)
  for (i=0; i<lst_size(block->data); i++) {
    sub = (MafSubBlock*)lst_get_ptr(block->data, i);

    //field[1] is src
    if (sub->src->length > fieldSize[1])
      fieldSize[1] = sub->src->length;

    //field[2] is start
    sprintf(tempstr, "%i", sub->start);
    if (strlen(tempstr) > fieldSize[2])
      fieldSize[2] = strlen(tempstr);

    //field[3] is size
    sprintf(tempstr, "%i", sub->size);
    if (strlen(tempstr) > fieldSize[3])
      fieldSize[3] = strlen(tempstr);
    
    //field[4] is strand... skip
    
    //field[5] is srcSize
    sprintf(tempstr, "%i", sub->srcSize);
    if (strlen(tempstr) > fieldSize[5])
      fieldSize[5] = strlen(tempstr);

    //don't worry about size of lastField since it just goes to end-of-line
  }
}


void mafBlock_print(FILE *outfile, MafBlock *block) {
  int i, j, k, numSpace;
  int fieldSize[6];  //maximum # of characters in the first 6 fields of block
  MafSubBlock *sub;
  char firstChar, formatstr[1000];

  //if processing has reduced the number of species with data to zero, or has
  //reduced the block to all gaps, don't print
  if (lst_size(block->data) == 0 ||
      mafBlock_all_gaps(block)) return;
  mafBlock_get_fieldSizes(block, fieldSize);

  fprintf(outfile, "%s\n", block->aLine->chars);
  for (i=0; i<lst_size(block->data); i++) {
    sub = (MafSubBlock*)lst_get_ptr(block->data, i);
    for (j=0; j<sub->numLine; j++) {
      firstChar = sub->lineType[j];
      if (firstChar == 's' || firstChar == 'e') {
	sprintf(formatstr, "%%c %%-%is %%%ii %%%ii %%c %%%ii ",
		fieldSize[1], fieldSize[2], fieldSize[3], fieldSize[5]);
	fprintf(outfile, formatstr, firstChar, sub->src->chars,
		sub->start, sub->size, sub->strand, sub->srcSize);
	if (firstChar == 's') fprintf(outfile, "%s\n", sub->seq->chars);
	else fprintf(outfile, "%c\n", sub->eStatus);
      } else if (firstChar=='i') {
	sprintf(formatstr, "i %%-%is %%c %%i %%c %%i\\n",
		fieldSize[1]);
	fprintf(outfile, formatstr, sub->src->chars,
		sub->iStatus[0], sub->iCount[0],
		sub->iStatus[1], sub->iCount[1]);
      } else {
	assert(firstChar=='q');
	sprintf(formatstr, "q %%-%is", fieldSize[1]);
	fprintf(outfile, formatstr, sub->src->chars);
	numSpace = 6 + fieldSize[2] + fieldSize[3] + fieldSize[5];
	for (k=0; k<numSpace; k++) fputc(' ', outfile);
	fprintf(outfile, "%s\n", sub->quality->chars);
      }
    }
  }
  fputc('\n', outfile);  //blank line to mark end of block
  fflush(outfile);
}

void mafSubBlock_free(MafSubBlock *sub) {
  if (sub->seq != NULL) {
    str_free(sub->seq);
    sub->seq = NULL;
  }
  if (sub->src != NULL) {
    str_free(sub->src);
    sub->src = NULL;
  }
  if (sub->specName != NULL) {
    str_free(sub->specName);
    sub->specName = NULL;
  }
  if (sub->quality != NULL) {
    str_free(sub->quality);
    sub->quality = NULL;
  }
  free(sub);
}

//if exclude==0, removes all species not in list.
//if exclude==1, removes all species in list
void mafBlock_subSpec(MafBlock *block, List *specNameList, int include) {
  String *str;
  MafSubBlock *sub, *testSub;
  int i, idx, *keep, oldSize = lst_size(block->data), newSize=0;

  keep = malloc(oldSize*sizeof(int));
  for (i=0; i<oldSize; i++) keep[i]=(include==0);

  for (i=0; i<lst_size(specNameList); i++) {
    str = (String*)lst_get_ptr(specNameList, i);
    idx = ptr_to_int(hsh_get(block->specMap, str->chars));
    if (idx != -1) keep[idx] = !(include==0);
  }
  for (i=0; i<oldSize; i++) {
    if (keep[i]) {
      if (i != newSize) {
	sub = (MafSubBlock*)lst_get_ptr(block->data, i);
	hsh_reset(block->specMap, sub->src->chars, int_to_ptr(newSize));
	hsh_reset(block->specMap, sub->specName->chars, int_to_ptr(newSize));
	testSub = (MafSubBlock*)lst_get_ptr(block->data, newSize);
	assert(testSub == NULL);
	lst_set_ptr(block->data, newSize, (void*)sub);
	lst_set_ptr(block->data, i, NULL);
      }
      newSize++;
    } else {
      sub = (MafSubBlock*)lst_get_ptr(block->data, i);
      hsh_reset(block->specMap, sub->src->chars, int_to_ptr(-1));
      hsh_reset(block->specMap, sub->specName->chars, int_to_ptr(-1));
      mafSubBlock_free(sub);
      lst_set_ptr(block->data, i, NULL);
    }
  }
  for (i=oldSize-1; i>=newSize; i--)
    lst_delete_idx(block->data, i);
  free(keep);
  return;
}


void mafBlock_reorder(MafBlock *block, List *specNameOrder) {
  String *str;
  MafSubBlock *sub;
  List *newData;
  Hashtable *newSpecMap;
  int i, idx, *found, oldSize = lst_size(block->data), newSize = lst_size(specNameOrder);

  found = malloc(oldSize*sizeof(int));
  for (i=0; i<oldSize; i++) found[i]=0;

  newData = lst_new_ptr(oldSize);
  newSpecMap = hsh_new(100);

  for (i=0; i<newSize; i++) {
    str = (String*)lst_get_ptr(specNameOrder, i);
    idx = ptr_to_int(hsh_get(block->specMap, str->chars));
    if (idx != -1) {
      if (found[idx]==1) die("ERROR: species %s appears twice in reorder list\n", 
			     str->chars);
      sub = (MafSubBlock*)lst_get_ptr(block->data, idx);
      hsh_put(newSpecMap, sub->src->chars, int_to_ptr(lst_size(newData)));
      hsh_put(newSpecMap, sub->specName->chars, int_to_ptr(lst_size(newData)));
      lst_push_ptr(newData, (void*)sub);
      found[idx] = 1;
    }
  }
  for (i=0; i<oldSize; i++) {
    if (found[i]==0) {
      sub = (MafSubBlock*)lst_get_ptr(block->data, i);
      mafSubBlock_free(sub);
    }
  }
  hsh_free(block->specMap);
  lst_free(block->data);
  block->specMap = newSpecMap;
  block->data = newData;
  free(found);
}


//frees all elements of block except prev and next pointers
void mafBlock_free(MafBlock *block) {
  MafSubBlock *sub;
  int i;
  if (block->aLine != NULL) {
    str_free(block->aLine);
    block->aLine = NULL;
  }
  if (block->specMap != NULL) {
    hsh_free(block->specMap);
    block->specMap = NULL;
  }
  if (block->data != NULL) {
    for (i=0; i<lst_size(block->data); i++) {
      sub = (MafSubBlock*)lst_get_ptr(block->data, i);
      mafSubBlock_free(sub);
    }
    lst_free(block->data);
  }
  free(block);
}

FILE *mafBlock_open_file(char *fn) {
  FILE *outfile;
  if (fn != NULL) 
    outfile = fopen_fname(fn, "w");
  else outfile = stdout;
  fprintf(outfile, "##maf version=1\n# maf_parse\n\n");
  return outfile;
}

void mafBlock_close_file(FILE *outfile) {
  fprintf(outfile, "#eof\n");
  if (outfile != stdout) fclose(outfile);
}

String *mafBlock_get_refSpec(MafBlock *block) {
  MafSubBlock *sub = (MafSubBlock*)lst_get_ptr(block->data, 0);
  if (sub==NULL) return NULL;
  return sub->specName;
}

int mafBlock_get_start(MafBlock *block, String *specName) {
  int idx=0;
  if (specName != NULL) 
    idx = ptr_to_int(hsh_get(block->specMap, specName->chars));
  if (idx == -1 || idx >= lst_size(block->data)) return -1;
  return ((MafSubBlock*)lst_get_ptr(block->data, idx))->start;
}

int mafBlock_get_size(MafBlock *block, String *specName) {
  int idx=0;
  MafSubBlock *sub;
  if (specName == NULL) return block->seqlen;
    idx = ptr_to_int(hsh_get(block->specMap, specName->chars));
  if (idx == -1 || idx >= lst_size(block->data)) return -1;
  sub = (MafSubBlock*)lst_get_ptr(block->data, idx);
  if (sub->lineType[0]=='s') return sub->size;
  assert(sub->lineType[0]=='e');
  return 0;
}

int mafBlock_numSpec(MafBlock *block) {
  return lst_size(block->data);
}

void mafBlock_subAlign(MafBlock *block, int start, int end) {
  int i, j, k, oldSeqlen = block->seqlen;
  MafSubBlock *sub;
  String *str;
  if (start > end || start <= 0 || start > oldSeqlen ||
      end <= 0 || end > oldSeqlen) 
    die("ERROR: mafBlock_subAlign got start=%i, end=%i, seqlen=%i\n", 
	start, end, oldSeqlen);
  if (end==oldSeqlen && start==1) return;  //nothing to do

  start--; //convert to zero-based coords
  block->seqlen = end-start;

  for (i=0; i<lst_size(block->data); i++) {
    sub = (MafSubBlock*)lst_get_ptr(block->data, i);
    if (sub->lineType[0]=='e') continue;  //e-lines remain unchanged

    assert(sub->lineType[0]=='s');
    for (j=0; j<start; j++)
      if (sub->seq->chars[j]!='-') sub->start++;
    sub->size = 0;
    for (j=start; j<end; j++)
      if (sub->seq->chars[j] != '-') sub->size++;

    //get rid of i-line if exists
    for (j=1; j<sub->numLine; j++) 
      if (sub->lineType[i]=='i') break;
    if (j < sub->numLine) {
      for (k=j+1; k<sub->numLine; k++) {
	sub->lineType[k-1] = sub->lineType[k];
	assert(sub->lineType[k] != 'i');
      }
      sub->numLine--;
    }

    //trim seq and quality scores
    str = str_new(end-start);
    str_substring(str, sub->seq, start, end-start);
    str_free(sub->seq);
    sub->seq = str;

    if (sub->quality != NULL) {
      str = str_new(end-start);
      str_substring(str, sub->quality, start, end-start);
      str_free(sub->quality);
      sub->quality = str;
    }
  }
}


int mafBlock_trim(MafBlock *block, int startcol, int endcol, String *refseq,
		  int offset) {
  MafSubBlock *sub=NULL;
  int i, specIdx, first=-1, last=-1, keep, startIdx, lastIdx, length, idx;
  if (block->seqlen == 0) return 0;
  if (refseq == NULL) {
    startIdx = 1;
    length = block->seqlen;
  }
  else {
    specIdx = ptr_to_int(hsh_get(block->specMap, refseq->chars));
    assert(specIdx != -1);
    sub = (MafSubBlock*)lst_get_ptr(block->data, specIdx);
    startIdx = sub->start + 1;
    length = sub->size;
  }
  startIdx += offset;
  lastIdx = startIdx + length - 1;
  idx = startIdx;
  if (refseq != NULL && sub->seq->chars[0]=='-') startIdx--;

  if (startcol != 1 && endcol != -1 && startcol > endcol) 
    die("ERROR: startcol > endcol\n");
  if (startcol > lastIdx) return 0;
  if (endcol  != -1 && endcol   < startIdx) return 0;
  if (startcol <= startIdx && 
      (endcol   == -1 || endcol  >= lastIdx)) return 1;
  
  //now we know we have to do some trimming
  for (i=0; i<block->seqlen; i++) {
    if (refseq != NULL && sub->seq->chars[i]=='-') idx--;

    keep = (idx >= startcol &&
	    (idx <= endcol   || endcol == -1));
    if (first == -1 && keep) first = i+1;
    if (keep) last=i+1;
    idx++;
  }
  mafBlock_subAlign(block, first, last);
  return 1;
}
