#ifndef INCmacArithH
#define INCmacArithH

#include <dbDefs.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Replace valid $(...) arithmetic expressions in a string buffer.
 * \return Pointer to the terminating NUL character in the output buffer.
 *
 * Scans the buffer for fragments of the form $(...) and evaluates each
 * fragment as an integer arithmetic expression. Supported syntax includes
 * whitespace, parentheses, leading '+' and '-', and the binary operators
 * '+', '-', '*', '/', and '%'.
 *
 * Invalid, incomplete, or non-evaluable expressions are copied unchanged.
 * Processing is done in-place in \a buf using an internal copy of the
 * original input.
 *
 * \param buf Input/output string buffer to postprocess.
 * \param capacity Total size of \a buf in bytes, including the terminating
 *                 NUL character.
 */
char *macArithPostprocess(char *buf, const size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* INCmacArithH */
