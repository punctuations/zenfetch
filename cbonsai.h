#ifndef CBONSAI_H
#define CBONSAI_H

/*
 * cbonsai library interface
 * Allows other programs to run cbonsai directly without fork/exec
 */

/*
 * Run cbonsai with the given arguments
 *
 * argc: argument count (including program name)
 * argv: argument array (argv[0] should be "cbonsai")
 *
 * Returns: 0 on success, non-zero on error
 */
int cbonsai_run(int argc, char *argv[]);

#endif /* CBONSAI_H */
