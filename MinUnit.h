#ifndef MINUNIT_H
#define MINUNIT_H

/* file: minunit.h */
// see https://jera.com/techinfo/jtns/jtn002

#define mu_assert(message, test) do { if (!(test)) return message; } while (0)
#define mu_run_test(test) do { const char *message = test(); tests_run++; \
                            if (message) return message; } while (0)
extern int tests_run;

#endif
