/* getopt.h */

#ifndef INC_getopt_H
#define INC_getopt_H

extern int opterr;
extern int optind;
extern int optopt;
extern int optopt;
extern int optreset;
extern char *optarg;

extern int getopt(int argc, char * const *argv, const char *ostr);

#endif
