/******************************************************************************
IrstLM: IRST Language Model Toolkit
Copyright (C) 2006 Marcello Federico, ITC-irst Trento, Italy

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

******************************************************************************/
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cassert>
#include "math.h"
#include "mempool.h"
#include "htable.h"
#include "ngramcache.h"
#include "dictionary.h"
#include "n_gram.h"
#include "lmtable.h"
#include "util.h"

#define DEBUG 0

//special value for pruned iprobs
#define NOPROB -2

using namespace std;

inline void error(char* message){
  std::cerr << message << "\n";
  throw std::runtime_error(message);
}


//instantiate an empty lm table
lmtable::lmtable(){

  configure(1, false);

  dict=new dictionary((char *)NULL, 1000000, (char*)NULL, (char*)NULL);

  memset(cursize, 0, sizeof(cursize));
    memset(tbltype, 0, sizeof(tbltype));
    memset(maxsize, 0, sizeof(maxsize));
    memset(info, 0, sizeof(info));
    memset(NumCenters, 0, sizeof(NumCenters));

  max_cache_lev=0;
  for (int i=0;i<=LMTMAXLEV+1;i++) lmtcache[i]=NULL;

  probcache=NULL;
  statecache=NULL;

  memmap=0;

  //statistics
  for (int i=0;i<=LMTMAXLEV+1;i++) totget[i]=totbsearch[i]=0;

  logOOVpenalty=0.0; //penalty for OOV words (default 0)

  // by default, it is a standard LM, i.e. queried for score
  setOrderQuery(false);
};

void lmtable::init_probcache(){
    assert(probcache==NULL);
    probcache=new ngramcache(maxlev, sizeof(double), 400000);
#ifdef TRACE_CACHE
    cacheout=new std::fstream("/tmp/tracecache", std::ios::out);
    sentence_id=0;
#endif
}

void lmtable::init_statecache(){
    assert(statecache==NULL);
    statecache=new ngramcache(maxlev-1, sizeof(char *), 200000);
}

void lmtable::init_lmtcaches(int uptolev){
    max_cache_lev=uptolev;
    for (int i=2;i<=max_cache_lev;i++){
    assert(lmtcache[i]==NULL);
    lmtcache[i]=new ngramcache(i, sizeof(char *), 200000);
    }
}

void lmtable::check_cache_levels(){
    if (probcache && probcache->isfull()) probcache->reset(probcache->cursize());
    if (statecache && statecache->isfull()) statecache->reset(statecache->cursize());
    for (int i=2;i<=max_cache_lev;i++)
      if (lmtcache[i]->isfull()) lmtcache[i]->reset(lmtcache[i]->cursize());
}

void lmtable::reset_caches(){
      if (probcache) probcache->reset(MAX(probcache->cursize(), probcache->maxsize()));
      if (statecache) statecache->reset(MAX(statecache->cursize(), statecache->maxsize()));
      for (int i=2;i<=max_cache_lev;i++)
        lmtcache[i]->reset(MAX(lmtcache[i]->cursize(), lmtcache[i]->maxsize()));
}

void lmtable::configure(int n, bool quantized){
  maxlev=n;
  if (n==1)
    tbltype[1]=(quantized?QLEAF:LEAF);
  else{
    for (int i=1;i<n;i++) tbltype[i]=(quantized?QINTERNAL:INTERNAL);
       tbltype[n]=(quantized?QLEAF:LEAF);
  }
}



//loadstd::istream& inp a lmtable from a lm file

void lmtable::load(istream& inp, const char* filename, const char* outfilename, int keep_on_disk, OUTFILE_TYPE outtype){

#ifdef WIN32
  if (keep_on_disk>0){
    std::cerr << "lmtable::load memory mapping not yet available under WIN32\n";
        keep_on_disk = 0;
  }
#endif

  //give a look at the header to select loading method
  char header[MAX_LINE];
  inp >> header;
  //cerr << header << "\n";

  if (strncmp(header, "Qblmt", 5)==0 || strncmp(header, "blmt", 4)==0){
    if (outtype==BINARY) {
      cerr << "Error: nothing to do. Passed input file: binary. Specified output format: binary.\n";
      exit(0);
    }
    loadbin(inp, header, filename, keep_on_disk);
  }
  else{
    if (strncmp(header, "ARPA", 4)==0 && outtype==TEXT) {
      cerr << "Error: nothing to do. Passed input file: textual. Specified output format: textual.\n";
      exit(0);
    }
    loadtxt(inp, header, outfilename, keep_on_disk);
  }

  //cerr << "OOV code is " << lmtable::getDict()->oovcode() << "\n";


}



int parseWords(char *sentence, char **words, int max)
{
  char *word;
  int i = 0;

  char *const wordSeparators = " \t\r\n";

  for (word = strtok(sentence, wordSeparators);
       i < max && word != 0;
       i++, word = strtok(0, wordSeparators))
  {
    words[i] = word;
  }

  if (i < max){words[i] = 0;}

  return i;
}



//Load a LM as a text file. LM could have been generated either with the
//IRST LM toolkit or with the SRILM Toolkit. In the latter we are not
//sure that n-grams are lexically ordered (according to the 1-grams).
//However, we make the following assumption:
//"all successors of any prefix are sorted and written in contiguous lines!"
//This method also loads files processed with the quantization
//tool: qlm

int parseline(istream& inp, int Order, ngram& ng, float& prob, float& bow){

  char* words[1+ LMTMAXLEV + 1 + 1];
  int howmany;
  char line[MAX_LINE];

  inp.getline(line, MAX_LINE);
  if (strlen(line)==MAX_LINE-1){
      cerr << "parseline: input line exceed MAXLINE ("
      << MAX_LINE << ") chars " << line << "\n";
    exit(1);
  }

  howmany = parseWords(line, words, Order + 3);

  if (!(howmany == (Order+ 1) || howmany == (Order + 2)))
    cerr << "line=<" << line << ">\n";
  assert(howmany == (Order+ 1) || howmany == (Order + 2));

  //read words
  ng.size=0;
  for (int i=1;i<=Order;i++)
    ng.pushw(strcmp(words[i], "<unk>")?words[i]:ng.dict->OOV());

  //read logprob/code and logbow/code
  assert(sscanf(words[0], "%f", &prob));
  if (howmany==(Order+2))
    assert(sscanf(words[Order+1], "%f", &bow));
  else
    bow=0.0; //this is log10prob=0 for implicit backoff

  /**
  if (Order>1) {
    cout << prob << "\n";
    for (int i=1;i<=Order;i++)
      cout <<  words[i] << " ";
    cout << "\n" << bow  << "\n\n";
  }
  **/
  return 1;
}


void lmtable::loadcenters(istream& inp, int Order){
  char line[MAX_LINE];

  //first read the coodebook
  cerr << Order << " read code book ";
  inp >> NumCenters[Order];
  Pcenters[Order]=new float[NumCenters[Order]];
  Bcenters[Order]=(Order<maxlev?new float[NumCenters[Order]]:NULL);

  for (int c=0;c<NumCenters[Order];c++){
    inp >> Pcenters[Order][c];
    if (Order<maxlev) inp >> Bcenters[Order][c];
  };
  //empty the last line
  inp.getline((char*)line, MAX_LINE);

}

void lmtable::loadtxt(istream& inp, const char* header, const char* outfilename, int mmap){
    if (mmap>0)
      loadtxtmmap(inp, header, outfilename);
    else {
      loadtxt(inp, header);
      lmtable::getDict()->genoovcode();
    }
}

void lmtable::loadtxtmmap(istream& inp, const char* header, const char* outfilename){

  char nameNgrams[BUFSIZ];
  char nameHeader[BUFSIZ];

  FILE *fd = NULL;
  long long filesize=0;

  int Order, n;

  int maxlevel_h;
  //char *SepString = " \t\n"; unused

  //open input stream and prepare an input string
  char line[MAX_LINE];

  //prepare word dictionary
  //dict=(dictionary*) new dictionary(NULL, 1000000, NULL, NULL);
  lmtable::getDict()->incflag(1);

  //put here ngrams, log10 probabilities or their codes
  ngram ng(lmtable::getDict());
  float pb, bow;;

  //check the header to decide if the LM is quantized or not
  isQtable=(strncmp(header, "qARPA", 5)==0?true:false);

  //check the header to decide if the LM table is incomplete
  isItable=(strncmp(header, "iARPA", 5)==0?true:false);

  if (isQtable){
    //check if header contains other infos
    inp >> line;
    if (!(maxlevel_h=atoi(line))){
      cerr << "loadtxt with mmap requires new qARPA header. Please regenerate the file.\n";
      exit(1);
    }

    for (n=1;n<=maxlevel_h;n++){
      inp >> line;
      if (!(NumCenters[n]=atoi(line))){
        cerr << "loadtxt with mmap requires new qARPA header. Please regenerate the file.\n";
        exit(0);
      }
    }
  }

  //we will configure the table later we we know the maxlev;
  bool yetconfigured=false;

  cerr << "loadtxtmmap()\n";

  // READ ARPA Header

  while (inp.getline(line, MAX_LINE)){

    if (strlen(line)==MAX_LINE-1){
      cerr << "lmtable::loadtxtmmap: input line exceed MAXLINE ("
      << MAX_LINE << ") chars " << line << "\n";
      exit(1);
    }

    bool backslash = (line[0] == '\\');

    if (sscanf(line, "ngram %d=%d", &Order, &n) == 2) {
      maxsize[Order] = n; maxlev=Order; //upadte Order
      cerr << "size[" << Order << "]=" << maxsize[Order] << "\n";
    }

    if (backslash && sscanf(line, "\\%d-grams", &Order) == 1) {

      //at this point we are sure about the size of the LM
      if (!yetconfigured){
        configure(maxlev, isQtable);
        yetconfigured=true;

        //opening output file
        strcpy(nameNgrams, outfilename);
        strcat(nameNgrams, "-ngrams");

        cerr << "saving ngrams probs in " << nameNgrams << "\n";

        fd = fopen(nameNgrams, "w+");

        // compute the size of file (only for tables and - possibly - centroids; no header nor dictionary)
        for (int l=1;l<=maxlev;l++)
          if (l<maxlev)
            filesize +=  (long long)maxsize[l] * nodesize(tbltype[l]) + 2 * NumCenters[l] * sizeof(float);
          else
            filesize +=  (long long)maxsize[l] * nodesize(tbltype[l]) + NumCenters[l] * sizeof(float);
        cerr << "filesize = " << filesize << "\n";

        // set the file to the proper size:
        ftruncate(fileno(fd), filesize);
        table[0]=(char *)(MMap(fileno(fd), PROT_READ|PROT_WRITE, 0, filesize, &tableGaps[0]));

        //allocate space for tables into the file through mmap:

        if (maxlev>1)
          table[1]=table[0] + 2 * NumCenters[1] * sizeof(float);
        else
          table[1]=table[0] + NumCenters[1] * sizeof(float);

        for (int l=2;l<=maxlev;l++)
          if (l<maxlev)
            table[l]=(char *)(table[l-1] + (long long)maxsize[l-1]*nodesize(tbltype[l-1]) +
                              2 * NumCenters[l] * sizeof(float));
          else
            table[l]=(char *)(table[l-1] + (long long)maxsize[l-1]*nodesize(tbltype[l-1]) +
                              NumCenters[l] * sizeof(float));

        for (int l=2;l<=maxlev;l++)
          cerr << "table[" << l << "]-table[" << l-1 << "]="
            << table[l]-table[l-1] << " (nodesize=" << nodesize(tbltype[l-1]) << ")\n";

      }


      cerr << Order << "-grams: reading ";
      if (isQtable) {
        loadcenters(inp, Order);
        // writing centroids on disk
        if (Order<maxlev){
          memcpy(table[Order] - 2 * NumCenters[Order] * sizeof(float),
                 Pcenters[Order],
                 NumCenters[Order] * sizeof(float));
          memcpy(table[Order] - NumCenters[Order] * sizeof(float),
                 Bcenters[Order],
                 NumCenters[Order] * sizeof(float));
        } else
          memcpy(table[Order] - NumCenters[Order] * sizeof(float),
                 Pcenters[Order],
                 NumCenters[Order] * sizeof(float));
      }

      //allocate support vector to manage badly ordered n-grams
      if (maxlev>1 && Order<maxlev) {
        startpos[Order]=new int[maxsize[Order]];
        for (int c=0;c<maxsize[Order];c++) startpos[Order][c]=-1;
      }
      //prepare to read the n-grams entries
      cerr << maxsize[Order] << " entries\n";

      //WE ASSUME A WELL STRUCTURED FILE!!!
      for (int c=0;c<maxsize[Order];c++){

        if (parseline(inp, Order, ng, pb, bow)){
          //if table is in incomplete ARPA format pb is just the
          //discounted frequency, so we need to add bow * Pr(n-1 gram)
          if (isItable && Order>1){
            //get bow of lower of context
            get(ng, ng.size, ng.size-1);
            float rbow=0.0;
            if (ng.lev==ng.size-1){ //found context
              int ibow=ng.bow; rbow=*((float *)&ibow);
            }

            int tmp=maxlev;
            maxlev=Order-1;
            //cerr << ng << "rbow: " << rbow << "prob: " << pb << "low-prob: " << lprob(ng) << "\n";
            pb= log(exp((double)pb * M_LN10) +  exp(((double)rbow + lprob(ng)) * M_LN10))/M_LN10;
            maxlev=tmp;
          }
          add(ng,
              (int)(isQtable?pb:*((int *)&pb)),
              (int)(isQtable?bow:*((int *)&bow)));
        }
      }
      // To avoid huge memory write concentrated at the end of the program
      msync(table[0], filesize, MS_SYNC);

      // now we can fix table at level Order -1
      // (not required if the input LM is in lexicographical order)
      if (maxlev>1 && Order>1) checkbounds(Order-1);
    }
  }

  cerr << "closing output file: " << nameNgrams << "\n";
  for (int i=1;i<=maxlev;i++)
    if (maxsize[i] != cursize[i]) {
      for (int l=1;l<=maxlev;l++)
    cerr << "Level " << l << ": starting ngrams=" << maxsize[l] << " - actual stored ngrams=" << cursize[l] << "\n";
      break;
    }

  Munmap(table[0], filesize, MS_SYNC);
  for (int l=1;l<=maxlev;l++)
    table[l]=0; // to avoid wrong free in ~lmtable()
  cerr << "running fclose...\n";
  fclose(fd);
  cerr << "done\n";

  lmtable::getDict()->incflag(0);
  lmtable::getDict()->genoovcode();

  // saving header + dictionary

  strcpy(nameHeader, outfilename);
  strcat(nameHeader, "-header");
  cerr << "saving header+dictionary in " << nameHeader << "\n";
  fstream out(nameHeader, ios::out);

  // print header
  if (isQtable){
    out << "Qblmt " << maxlev;
    for (int i=1;i<=maxlev;i++) out << " " << maxsize[i];  // not cursize[i] because the file was already allocated
    out << "\nNumCenters";
    for (int i=1;i<=maxlev;i++)  out << " " << NumCenters[i];
    out << "\n";

  }else{
    //out << "blmt " << maxlev;
    for (int i=1;i<=maxlev;i++) out << " " << maxsize[i];  // not cursize[i] because the file was already allocated
    out << "\n";
  }

  lmtable::getDict()->save(out);

  out.close();
  cerr << "done\n";

  // cat header+dictionary and n-grams files:

  char cmd[MAX_LINE];
  sprintf(cmd, "cat %s >> %s", nameNgrams, nameHeader);
  cerr << "run cmd <" << cmd << ">\n";
  system(cmd);

  sprintf(cmd, "mv %s %s", nameHeader, outfilename);
  cerr << "run cmd <" << cmd << ">\n";
  system(cmd);

  sprintf(cmd, "rm %s", nameNgrams);
  cerr << "run cmd <" << cmd << ">\n";
  system(cmd);

  return;
}

void lmtable::loadtxt(istream& inp, const char* header){


  //open input stream and prepare an input string
  char line[MAX_LINE];

  //prepare word dictionary
  //dict=(dictionary*) new dictionary(NULL, 1000000, NULL, NULL);
  lmtable::getDict()->incflag(1);

  //put here ngrams, log10 probabilities or their codes
  ngram ng(lmtable::getDict());
  float prob, bow;

  //check the header to decide if the LM is quantized or not
  isQtable=(strncmp(header, "qARPA", 5)==0?true:false);

  //check the header to decide if the LM table is incomplete
  isItable=(strncmp(header, "iARPA", 5)==0?true:false);

  //we will configure the table later we we know the maxlev;
  bool yetconfigured=false;

  cerr << "loadtxt()\n";

  // READ ARPA Header
  int Order, n;

  while (inp.getline(line, MAX_LINE)){

    if (strlen(line)==MAX_LINE-1){
      cerr << "lmtable::loadtxt: input line exceed MAXLINE ("
      << MAX_LINE << ") chars " << line << "\n";
      exit(1);
    }

    bool backslash = (line[0] == '\\');

    if (sscanf(line, "ngram %d=%d", &Order, &n) == 2) {
      maxsize[Order] = n; maxlev=Order; //upadte Order

    }

    if (backslash && sscanf(line, "\\%d-grams", &Order) == 1) {

      //at this point we are sure about the size of the LM
      if (!yetconfigured){
        configure(maxlev, isQtable);yetconfigured=true;
        //allocate space for loading the table of this level
        for (int i=1;i<=maxlev;i++)
          table[i]= new char[maxsize[i] * nodesize(tbltype[i])];
      }

      cerr << Order << "-grams: reading ";

      if (isQtable) loadcenters(inp, Order);

      //allocate support vector to manage badly ordered n-grams
      if (maxlev>1 && Order<maxlev) {
        startpos[Order]=new int[maxsize[Order]];
        for (int c=0;c<maxsize[Order];c++) startpos[Order][c]=-1;
      }

      //prepare to read the n-grams entries
      cerr << maxsize[Order] << " entries\n";

      //WE ASSUME A WELL STRUCTURED FILE!!!

      for (int c=0;c<maxsize[Order];c++){

        if (parseline(inp, Order, ng, prob, bow)){

          //if table is in incomplete ARPA format prob is just the
          //discounted frequency, so we need to add bow * Pr(n-1 gram)
          if (isItable && Order>1) {
            //get bow of lower of context
            get(ng, ng.size, ng.size-1);
            float rbow=0.0;
            if (ng.lev==ng.size-1){ //found context
              int ibow=ng.bow; rbow=*((float *)&ibow);
            }

            int tmp=maxlev;
            maxlev=Order-1;
            //cerr << ng << "rbow: " << rbow << "prob: " << prob << "low-prob: " << lprob(ng) << "\n";
            prob= log(exp((double)prob * M_LN10) +  exp(((double)rbow + lprob(ng)) * M_LN10))/M_LN10;
            //cerr << "new prob: " << prob << "\n";

            maxlev=tmp;
          }

          add(ng,
              (int)(isQtable?prob:*((int *)&prob)),
              (int)(isQtable?bow:*((int *)&bow)));
        }
      }
      // now we can fix table at level Order -1
      if (maxlev>1 && Order>1) checkbounds(Order-1);
    }
  }

  lmtable::getDict()->incflag(0);
  cerr << "done\n";

}

void lmtable::printTable(int level) {
  char*  tbl=table[level];
  LMT_TYPE ndt=tbltype[level];
  int ndsz=nodesize(ndt);
  int printEntryN=1000;
  if (cursize[level]>0)
    printEntryN=(printEntryN<cursize[level])?printEntryN:cursize[level];

  cout << "level = " << level << "\n";
  int p;
  for (int c=0;c<printEntryN;c++){
    p=prob(tbl, ndt);
    cout << *(float *)&p << " "
     << word(tbl) << "\n";
    tbl+=ndsz;
  }
  return;
}

//Checkbound with sorting of n-gram table on disk

void lmtable::checkbounds(int level){

  char*  tbl=table[level];
  char*  succtbl=table[level+1];

  LMT_TYPE ndt=tbltype[level], succndt=tbltype[level+1];
  int ndsz=nodesize(ndt), succndsz=nodesize(succndt);

  //re-order table at level+1 on disk
  //generate random filename to avoid collisions
  ofstream out;string filePath;
  createtempfile(out, filePath, ios::out|ios::binary);

  int start, end, newstart;

  //re-order table at level l+1
  newstart=0;
  for (int c=0;c<cursize[level];c++){
    start=startpos[level][c]; end=bound(tbl+(long long)c*ndsz, ndt);
    //is start==-1 there are no successors for this entry and end==-2
    if (end==-2) end=start;
    assert(start<=end);
    assert(newstart+(end-start)<=cursize[level+1]);
    assert(end<=cursize[level+1]);

    if (start<end){
      out.write((char*)(succtbl + (long long)start * succndsz), (end-start) * succndsz);
      if (!out.good()){
        std::cerr << " Something went wrong while writing temporary file " << filePath
        << " Maybe there is not enough space on this filesystem\n";

        out.close();
        removefile(filePath);
      }
    }

    bound(tbl+(long long)c*ndsz, ndt, newstart+(end-start));
    newstart+=(end-start);
  }

  out.close();

  fstream inp(filePath.c_str(), ios::in|ios::binary);

  inp.read(succtbl, cursize[level+1]*succndsz);
  inp.close();
  removefile(filePath);

}

//Add method inserts n-grams in the table structure. It is ONLY used during
//loading of LMs in text format. It searches for the prefix, then it adds the
//suffix to the last level and updates the start-end positions.

int lmtable::add(ngram& ng, int iprob, int ibow){

  char *found; LMT_TYPE ndt; int ndsz;
  static int no_more_msg = 0;

  if (ng.size>1){

    // find the prefix starting from the first level
    int start=0, end=cursize[1];

    for (int l=1;l<ng.size;l++){

      ndt=tbltype[l]; ndsz=nodesize(ndt);

      if (search(l, start, (end-start), ndsz,
                 ng.wordp(ng.size-l+1), LMT_FIND, &found)){

        //update start-end positions for next step
        if (l< (ng.size-1)){
          //set start position
          if (found==table[l]) start=0; //first pos in table
          else start=bound(found - ndsz, ndt); //end of previous entry

          //set end position
          end=bound(found, ndt);
        }
      }
      else {
    if (!no_more_msg)
      cerr << "warning: missing back-off for ngram " << ng << " (and possibly for others)\n";

    no_more_msg++;
    if (!(no_more_msg % 5000000))
      cerr << "!";

    return 0;
      }
    }

    // update book keeping information about level ng-size -1.
    // if this is the first successor update start position
    int position=(int)((found-table[ng.size-1])/(long long)ndsz);

    if (startpos[ng.size-1][position]==-1)
      startpos[ng.size-1][position]=cursize[ng.size];

    //always update ending position
    bound(found, ndt, cursize[ng.size]+1);

  }

  // just add at the end of table[ng.size]

  assert(cursize[ng.size]< maxsize[ng.size]); // is there enough space?
  ndt=tbltype[ng.size];ndsz=nodesize(ndt);

  found=table[ng.size] + ((long long)cursize[ng.size] * ndsz);
  word(found, *ng.wordp(1));
  prob(found, ndt, iprob);
  if (ng.size<maxlev){bow(found, ndt, ibow);bound(found, ndt, -2);}

  cursize[ng.size]++;

  if (!(cursize[ng.size]%5000000))
    cerr << ".";

  return 1;

}


void *lmtable::search(int lev,
                      int offs,
                      int n,
                      int sz,
                      int *ngp,
                      LMT_ACTION action,
                      char **found){

/***
  if (lev>=2)
    cout << "searching entry for codeword: " << ngp[0] << "...";
***/

  //assume 1-grams is a 1-1 map of the vocabulary
  if (lev==1) return *found=(*ngp <n ? table[1] + *ngp * sz:NULL);

  //prepare table to be searched with mybserach
  char* tb;
  tb=table[lev]+((long long)sz * offs);
  //prepare search pattern
  char w[LMTCODESIZE];putmem(w, ngp[0], 0, LMTCODESIZE);

  int idx=0; // index returned by mybsearch
  *found=NULL;  //initialize output variable

  totbsearch[lev]++;

  switch (action){
    case LMT_FIND:
      if (!tb || !mybsearch(tb, n, sz, (unsigned char *)w, &idx)) return NULL;
      else
        return *found=tb + ((long long)idx * sz);
    default:
      error("lmtable::search: this option is available");
  };

  return NULL;
}


int lmtable::mybsearch(char *ar, int n, int size,
                       unsigned char *key, int *idx)
{

  register int low, high;
  register unsigned char *p;
  register long long result=0;
  register int i;

  /* return idx with the first position equal or greater than key */

  /*   Warning("start bsearch \n"); */

  low = 0;high = n; *idx=0;
  while (low < high)
  {
    *idx = (low + high) / 2;
    p = (unsigned char *) (ar + (*idx * (long long)size));

    //comparison
    for (i=(LMTCODESIZE-1);i>=0;i--){
      result=key[i]-p[i];
      if (result) break;
    }

    if (result < 0)
      high = *idx;
    else if (result > 0)
      low = *idx + 1;
    else
      return 1;
  }

  *idx=low;

  return 0;

}


// saves a LM table in text format

void lmtable::savetxt(const char *filename){

  fstream out(filename, ios::out);
  int       cnt[1+MAX_NGRAM];
  int l;

  out.precision(7);

  if (isQtable){
    out << "qARPA " << maxlev;
    for (l=1;l<=maxlev;l++)
      out << " " << NumCenters[l];
    out << endl;
  }

  ngram ng(lmtable::getDict(), 0);

  cerr << "savetxt: " << filename << "\n";

  //check size of table by considering pruned n-grams
  ngcnt(cnt);
  out << "\n\\data\\\n";
  for (l=1;l<=maxlev;l++){
    out << "ngram " << l << "= " << cnt[l] << "\n";
  }

  for (l=1;l<=maxlev;l++){

    out << "\n\\" << l << "-grams:\n";
    cerr << "save: " << cnt[l] << " " << l << "-grams\n";
    if (isQtable){
      out << NumCenters[l] << "\n";
      for (int c=0;c<NumCenters[l];c++){
        out << Pcenters[l][c];
        if (l<maxlev) out << " " << Bcenters[l][c];
        out << "\n";
      }
    }

    ng.size=0;
    dumplm(out, ng, 1, l, 0, cursize[1]);

  }

  out << "\\end\\\n";
  cerr << "done\n";
}


void lmtable::savebin(const char *filename){

  fstream out(filename, ios::out);
  cerr << "savebin: " << filename << "\n";

  // print header
  if (isQtable){
    out << "Qblmt " << maxlev;
    for (int i=1;i<=maxlev;i++) out << " " << cursize[i];
    out << "\nNumCenters";
    for (int i=1;i<=maxlev;i++)  out << " " << NumCenters[i];
    out << "\n";

  }else{
    //out << "blmt " << maxlev;
    for (int i=1;i<=maxlev;i++) out << " " << cursize[i] ;
    out << "\n";
  }

  lmtable::getDict()->save(out);

  for (int i=1;i<=maxlev;i++){
    cerr << "saving " << cursize[i] << " " << i << "-grams\n";
    if (isQtable){
      out.write((char*)Pcenters[i], NumCenters[i] * sizeof(float));
      if (i<maxlev)
        out.write((char *)Bcenters[i], NumCenters[i] * sizeof(float));
    }
    out.write(table[i], cursize[i]*nodesize(tbltype[i]));
  }

  cerr << "done\n";
}


//manages the long header of a bin file
//and allocates table for each n-gram level

void lmtable::loadbinheader(istream& inp, const char* header){

  // read rest of header
  inp >> maxlev;

  if (strncmp(header, "Qblmt", 5)==0) isQtable=1;
  else if (strncmp(header, "blmt", 4)==0) isQtable=0;
  else error("loadbin: LM file is not in binary format");

  configure(maxlev, isQtable);

  for (int l=1;l<=maxlev;l++){
    inp >> cursize[l]; maxsize[l]=cursize[l];
  }

  if (isQtable){
    char header2[100];
    inp >> header2;
    for (int i=1;i<=maxlev;i++){
      inp >> NumCenters[i];
      cerr << "reading  " << NumCenters[i] << " centers\n";
    }
  }
}

//load codebook of level l

void lmtable::loadbincodebook(istream& inp, int l){

  Pcenters[l]=new float [NumCenters[l]];
  inp.read((char*)Pcenters[l], NumCenters[l] * sizeof(float));
  if (l<maxlev){
    Bcenters[l]=new float [NumCenters[l]];
    inp.read((char *)Bcenters[l], NumCenters[l]*sizeof(float));
  }

}


//load a binary lmfile

void lmtable::loadbin(istream& inp, const char* header, const char* filename, int mmap){

  //cerr << "loadbin()\n";
  loadbinheader(inp, header);
  lmtable::getDict()->load(inp);

  //if MMAP is used, then open the file
  if (filename && mmap>0){

#ifdef WIN32
    error("lmtable::loadbin mmap facility not yet supported under WIN32\n");
#else

    if (mmap <= maxlev) memmap=mmap;
    else error("keep_on_disk value is out of range\n");

    if ((diskid=open(filename, O_RDONLY))<0){
      std::cerr << "cannot open " << filename << "\n";
      error("dying");
    }

    //check that the LM is uncompressed
    char miniheader[4];
    read(diskid, miniheader, 4);
    if (strncmp(miniheader, "Qblm", 4) && strncmp(miniheader, "blmt", 4))
      error("mmap functionality does not work with compressed binary LMs\n");
#endif
  }

  for (int l=1;l<=maxlev;l++){
    if (isQtable) loadbincodebook(inp, l);
    if ((memmap == 0) || (l < memmap)){
      //cerr << "loading " << cursize[l] << " " << l << "-grams\n";
      table[l]=new char[(long long)cursize[l] * nodesize(tbltype[l])];
      inp.read(table[l], (long long)cursize[l] * nodesize(tbltype[l]));
    }
    else{

#ifdef WIN32
      error("mmap not available under WIN32\n");
#else
      //cerr << "mapping " << cursize[l] << " " << l << "-grams\n";
      tableOffs[l]=inp.tellg();
      table[l]=(char *)MMap(diskid, PROT_READ,
                            tableOffs[l], (long long)cursize[l]*nodesize(tbltype[l]),
                    &tableGaps[l]);
      table[l]+=tableGaps[l];
      inp.seekg((long long)cursize[l]*nodesize(tbltype[l]), ios_base::cur);
#endif

    }
  };

  //cerr << "done\n";

}



int lmtable::get(ngram& ng, int n, int lev){

/***
  cout << "cerco:" << ng << "\n";
***/
  totget[lev]++;

  if (lev > maxlev) error("get: lev exceeds maxlevel");
  if (n < lev) error("get: ngram is too small");

  //set boudaries for 1-gram
  int offset=0, limit=cursize[1];

  //information of table entries
  int hit;char* found; LMT_TYPE ndt;
  ng.link=NULL;
  ng.lev=0;

  for (int l=1;l<=lev;l++){

    //initialize entry information
    hit = 0 ; found = NULL; ndt=tbltype[l];

    if (lmtcache[l] && lmtcache[l]->get(ng.wordp(n), (char *)&found))
      hit=1;
    else
      search(l,
             offset,
             (limit-offset),
             nodesize(ndt),
             ng.wordp(n-l+1),
             LMT_FIND,
             &found);

    //insert both found and not found items!!!
    if (lmtcache[l] && hit==0)
      lmtcache[l]->add(ng.wordp(n), (char *)&found);

    if (!found) return 0;
    if (prob(found, ndt)==NOPROB) return 0; //pruned n-gram

    ng.bow=(l<maxlev?bow(found, ndt):0);
    ng.prob=prob(found, ndt);
    ng.link=found;
    ng.info=ndt;
    ng.lev=l;

    if (l<maxlev){ //set start/end point for next search

      //if current offset is at the bottom also that of successors will be
      if (offset+1==cursize[l]) limit=cursize[l+1];
      else limit=bound(found, ndt);

      //if current start is at the begin, then also that of successors will be
      if (found==table[l]) offset=0;
      else offset=bound((found - nodesize(ndt)), ndt);

      assert(offset!=-1); assert(limit!=-1);
    }
  }

  //put information inside ng
  ng.size=n;  ng.freq=0;
  ng.succ=(lev<maxlev?limit-offset:0);

#ifdef TRACE_CACHE
  if (ng.size==maxlev && sentence_id>0){
    *cacheout << sentence_id << " miss " << ng << " " << (unsigned int) ng.link << "\n";
  }
#endif

  return 1;
}


//recursively prints the language model table

void lmtable::dumplm(fstream& out, ngram ng, int ilev, int elev, int ipos, int epos){

  LMT_TYPE ndt=tbltype[ilev];
  int ndsz=nodesize(ndt);

  assert(ng.size==ilev-1);
  assert(ipos>=0 && epos<=cursize[ilev] && ipos<epos);
  ng.pushc(0);

  for (int i=ipos;i<epos;i++){
    *ng.wordp(1)=word(table[ilev]+(long long)i*ndsz);
    int ipr=prob(table[ilev]+(long long)i*ndsz, ndt);

    //skip pruned n-grams
    if (ipr==NOPROB) continue;

    if (ilev<elev){
      //get first and last successor position
      int isucc=(i>0?bound(table[ilev]+(long long)(i-1)*ndsz, ndt):0);
      int esucc=bound(table[ilev]+(long long)i*ndsz, ndt);
      if (isucc < esucc) //there are successors!
        dumplm(out, ng, ilev+1, elev, isucc, esucc);
      //else
      //cout << "no successors for " << ng << "\n";
    }
    else{
      //out << i << " "; //this was just to count printed n-grams
      out << (isQtable?ipr:*(float *)&ipr) <<"\t";
      for (int k=ng.size;k>=1;k--){
        if (k<ng.size) out << " ";
        out << lmtable::getDict()->decode(*ng.wordp(k));
      }

      if (ilev<maxlev){
        int ibo=bow(table[ilev]+ (long long)i * ndsz, ndt);
        if (isQtable) out << "\t" << ibo;
        else
          if (*((float *)&ibo)!=0.0)
            out << "\t" << *((float *)&ibo);

      }
      out << "\n";
    }
  }
}

//succscan iteratively returns all successors of an ngram h for which
//get(h, h.size, h.size) returned true.


int lmtable::succscan(ngram& h, ngram& ng, LMT_ACTION action, int lev){
  assert(lev==h.lev+1 && h.size==lev && lev<=maxlev);

  LMT_TYPE ndt=tbltype[h.lev];
  int ndsz=nodesize(ndt);

  switch (action){

    case LMT_INIT:
      //reset ngram local indexes

      ng.size=lev;
      ng.trans(h);
      ng.midx[lev]=(h.link>table[h.lev]?bound(h.link-ndsz, ndt):0);

      return 1;

    case LMT_CONT:

      if (ng.midx[lev]<bound(h.link, ndt))
      {
        //put current word into ng
        *ng.wordp(1)=word(table[lev]+ng.midx[lev]*nodesize(tbltype[lev]));
        ng.midx[lev]++;
        return 1;
      }
      else
        return 0;

    default:
      cerr << "succscan: only permitted options are LMT_INIT and LMT_CONT\n";
      exit(0);
  }

}

//maxsuffptr returns the largest suffix of an n-gram that is contained
//in the LM table. This can be used as a compact representation of the
//(n-1)-gram state of a n-gram LM. if the input k-gram has k>=n then it
//is trimmed to its n-1 suffix.

const char *lmtable::maxsuffptr(ngram ong){
//cerr << "lmtable::maxsuffptr\n";
//cerr << "ong: " << ong
//  << " -> ong.size: " << ong.size << "\n";

  if (ong.size==0) return (char*) NULL;
  if (ong.size>=maxlev) ong.size=maxlev-1;

  ngram ng=ong;
  //ngram ng(lmtable::getDict()); //eventually use the <unk> word
  //ng.trans(ong);

  if (get(ng, ng.size, ng.size))
    return ng.link;
  else{
    ong.size--;
    return maxsuffptr(ong);
  }
}


const char *lmtable::cmaxsuffptr(ngram ong){
//cerr << "lmtable::CMAXsuffptr\n";
//cerr << "ong: " << ong
//  << " -> ong.size: " << ong.size << "\n";

  if (ong.size==0) return (char*) NULL;
  if (ong.size>=maxlev) ong.size=maxlev-1;

  char* found;

  if (statecache && (ong.size==maxlev-1) && statecache->get(ong.wordp(maxlev-1), (char *)&found))
    return found;

  found=(char *)maxsuffptr(ong);

  if (statecache && ong.size==maxlev-1){
    //if (statecache->isfull()) statecache->reset();
    statecache->add(ong.wordp(maxlev-1), (char *)&found);
  };

  return found;
}



//returns log10prob of n-gram
//bow: backoff weight
//bol: backoff level

double lmtable::lprob(ngram ong, double* bow, int* bol, int internalcall){
//double lmtable::lprob(ngram ong){

  if (ong.size==0) return 0.0;
  if (ong.size>maxlev) ong.size=maxlev;

  if (internalcall==0){ //first call to lprob
    if (bow) *bow=0;
    if (bol) *bol=0;
  }

  ngram ng=ong;
  //ngram ng(lmtable::getDict()); //avoid dictionary transfer
  //ng.trans(ong);

  double rbow, lpr=0;
  int ibow, iprob;

  if (get(ng, ng.size, ng.size)){
    iprob=ng.prob;
    lpr = (double)(isQtable?Pcenters[ng.size][iprob]:*((float *)&iprob));
    if (*ng.wordp(1)==dict->oovcode()) lpr-=logOOVpenalty;
    return (double)lpr;
  }
  else{
    if (ng.size==1) //means an OOV word
      return -log(UNIGRAM_RESOLUTION)/M_LN10;
    else{ //compute backoff
          //set backoff state, shift n-gram, set default bow prob
      rbow=0.0;
            if (bol) (*bol)++; //increase backoff level
      if ((ng.lev==(ng.size-1)) && (*ng.wordp(2)!=dict->oovcode())){
        //found history in table: use its bo weight
        //avoid wrong quantization of bow of <unk>
        ibow=ng.bow;
        rbow= (double) (isQtable?Bcenters[ng.lev][ibow]:*((float *)&ibow));
      }

      if (bow) (*bow)+=rbow;

      //prepare recursion step
      ong.size--;
      return rbow + lmtable::lprob(ong, bow, bol, internalcall=1);
    }
  }
}

//return log10 probsL use cache memory

double lmtable::clprob(ngram ong){

  if (ong.size==0) return 0.0;

  if (ong.size>maxlev) ong.size=maxlev;

  double logpr;

#ifdef TRACE_CACHE
  if (probcache && ong.size==maxlev && sentence_id>0){
   *cacheout << sentence_id << " " << ong << "\n";
  }
#endif

  //cache hit
  if (probcache && ong.size==maxlev && probcache->get(ong.wordp(maxlev), (char *)&logpr)){
    return logpr;
  }

  //cache miss
  logpr=lmtable::lprob(ong);

  if (probcache && ong.size==maxlev){
     probcache->add(ong.wordp(maxlev), (char *)&logpr);
  };

  return logpr;

};



void lmtable::stat(int level){
  int totmem=0, memory;
  float mega=1024 * 1024;

  cout.precision(2);

  cout << "lmtable class statistics\n";

  cout << "levels " << maxlev << "\n";
  for (int l=1;l<=maxlev;l++){
    memory=cursize[l] * nodesize(tbltype[l]);
    cout << "lev " << l
      << " entries "<< cursize[l]
      << " used mem " << memory/mega << "Mb\n";
    totmem+=memory;
  }

  cout << "total allocated mem " << totmem/mega << "Mb\n";

  cout << "total number of get and binary search calls\n";
  for (int l=1;l<=maxlev;l++){
    cout << "level " << l << " get: " << totget[l] << " bsearch: " << totbsearch[l] << "\n";
  }

  if (level >1 ) lmtable::getDict()->stat();

}

void lmtable::reset_mmap(){
#ifndef WIN32
  if (memmap>0 and memmap<=maxlev)
    for (int l=memmap;l<=maxlev;l++){
      //std::cerr << "resetting mmap at level:" << l << "\n";
      Munmap(table[l]-tableGaps[l], (long long)cursize[l]*nodesize(tbltype[l])+tableGaps[l], 0);
      table[l]=(char *)MMap(diskid, PROT_READ,
                            tableOffs[l], (long long)cursize[l]*nodesize(tbltype[l]),
                            &tableGaps[l]);
      table[l]+=tableGaps[l];
    }
#endif
}

// ng: input n-gram

// *lk: prob of n-(*bol) gram
// *boff: backoff weight vector
// *bol:  backoff level

double lmtable::lprobx(ngram    ong,
                       double   *lkp,
                       double   *bop,
                       int  *bol)
{
    double      bo,
    lbo,
    pr;
    int     ipr;
    ngram       ng(dict),
            ctx(dict);

    if (bol) *bol=0;
    if (ong.size==0) {
        if (lkp) *lkp=0;
        return 0;   // lprob ritorna 0, prima lprobx usava LOGZERO
    }
    if (ong.size>maxlev) ong.size=maxlev;
    ctx = ng = ong;
    bo=0;
    ctx.shift();
    while (!get(ng)) { // back-off

        //OOV not included in dictionary
        if (ng.size==1) {
            pr = -log(UNIGRAM_RESOLUTION)/M_LN10;
            if (lkp) *lkp=pr; // this is the innermost probability
            pr += bo; //add all the accumulated back-off probability
            return pr;
        }
        // backoff-probability
        lbo = 0.0; //local back-off: default is logprob 0
        if (get(ctx)){ //this can be replaced with (ng.lev==(ng.size-1))
            ipr = ctx.bow;
            lbo = isQtable?Bcenters[ng.size][ipr]:*(float*)&ipr;
        }
        if (bop) *bop++=lbo;
        if (bol) ++*bol;
        bo += lbo;
        ng.size--;
        ctx.size--;
    }
    ipr = ng.prob;
    pr = isQtable?Pcenters[ng.size][ipr]:*((float*)&ipr);
    if (lkp) *lkp=pr;
    pr += bo;
    return pr;
}


// FABIO
int lmtable::wdprune(float  *thr)
{
    int l;
    ngram   ng(lmtable::getDict(), 0);

    ng.size=0;
    for (l=2; l<=maxlev; l++) wdprune(thr, ng, 1, l, 0, cursize[1]);
    return 0;
}

// FABIO: LM pruning method

int lmtable::wdprune(float  *thr, ngram ng, int ilev, int   elev, int   ipos, int   epos, double    tlk,
                     double bo, double  *ts, double *tbs)
{
    LMT_TYPE    ndt=tbltype[ilev];
    int        ndsz=nodesize(ndt);
    char         *ndp;
    float        lk;
    int i, ipr, ibo, k, nk;

    assert(ng.size==ilev-1);
    assert(ipos>=0 && epos<=cursize[ilev] && ipos<epos);

    ng.pushc(0); //increase size of n-gram

    for (i=ipos, nk=0; i<epos; i++) {

        //scan table at next level ilev from position ipos
        ndp = table[ilev]+(long long)i*ndsz;
        *ng.wordp(1) = word(ndp);

        //get probability
        ipr = prob(ndp, ndt);
        if (ipr==NOPROB) continue;   // Has it been already pruned ??
        lk = *(float*)&ipr;

        if (ilev<elev) { //there is an higher order

            //get backoff-weight for next level
            ibo = bow(ndp, ndt);
            bo = *(float*)&ibo;

            //get table boundaries for next level
            int isucc = i>0 ? bound(ndp-ndsz, ndt) : 0;
            int esucc = bound(ndp, ndt);
            if (isucc>=esucc) continue; // no successors

            //look for n-grams to be pruned with this context (see
            //back-off weight)
        prune:  double ts=0, tbs=0;
            k = wdprune(thr, ng, ilev+1, elev, isucc, esucc,
                    tlk+lk, bo, &ts, &tbs);
            //k  is the number of pruned n-grams with this context
            if (ilev!=elev-1) continue;
            if (ts>=1 || tbs>=1) {
                cerr << "ng: " << ng
                    <<" ts=" << ts
                    <<" tbs=" << tbs
                    <<" k=" << k
                    <<" ns=" << esucc-isucc
                    << "\n";
                if (ts>=1) {
                    pscale(ilev+1, isucc, esucc,
                           0.999999/ts);
                    goto prune;
                }
            }
            // adjusts backoff:
            // 1-sum_succ(pr(w|ng)) / 1-sum_succ(pr(w|bng))
            bo = log((1-ts)/(1-tbs))/M_LN10;
            *(float*)&ibo=bo;
            bow(ndp, ndt, ibo);
        } else { //we are at the highest level

            //get probability of lower order n-gram
            ngram   bng = ng; --bng.size;
            double blk = lprob(bng);

            double wd = pow(10., tlk+lk) * (lk-bo-blk);
            if (wd > thr[elev-1]) {  // kept
                *ts += pow(10., lk);
                *tbs += pow(10., blk);
            } else {        // discarded
                ++nk;
                prob(ndp, ndt, NOPROB);
            }
        }
    }
    return nk;
}

int lmtable::pscale(int lev, int        ipos, int       epos, double s)
{
    LMT_TYPE        ndt=tbltype[lev];
    int             ndsz=nodesize(ndt);
    char            *ndp;
    int             i, ipr;

    s=log(s)/M_LN10;
    ndp = table[lev]+(long long)ipos*ndsz;
    for (i=ipos; i<epos; ndp+=ndsz, i++) {
        ipr = prob(ndp, ndt);
        if (ipr==NOPROB) continue;
        *(float*)&ipr+=s;
        prob(ndp, ndt, ipr);
    }
    return 0;
}

//recompute table size by excluding pruned n-grams
int lmtable::ngcnt(int      *cnt)
{
    ngram   ng(lmtable::getDict(), 0);
    memset(cnt, 0, (maxlev+1)*sizeof(*cnt));
    ngcnt(cnt, ng, 1, 0, cursize[1]);
    return 0;
}

//recursively compute size
int lmtable::ngcnt(int      *cnt, ngram ng, int     l, int      ipos, int       epos){

    int     i, ipr, isucc, esucc;
    char        *ndp;
    LMT_TYPE    ndt=tbltype[l];
    int     ndsz=nodesize(ndt);

    ng.pushc(0);
    for (i=ipos; i<epos; i++) {
        ndp = table[l]+(long long)i*ndsz;
        *ng.wordp(1)=word(ndp);
        ipr=prob(ndp, ndt);
        if (ipr==NOPROB) continue;
        ++cnt[l];
        if (l==maxlev) continue;
        isucc = (i>0)?bound(ndp-ndsz, ndt):0;
        esucc = bound(ndp, ndt);
        if (isucc < esucc) ngcnt(cnt, ng, l+1, isucc, esucc);
    }
    return 0;
}
