#include "macArith.h"

#include <epicsStdio.h>
#include <epicsStdlib.h>
#include <epicsString.h>

/* Skip spaces and tabs at the current parse position. */
static void skipBlankSpaces(const char *&p)
{
    while (*p && (*p == ' ' || *p == '\t'))
        ++p;
}

static int parseAddSub(const char *&p, long &out);

/* Parse an integer literal from the current parse position using strtol() base auto-detection. */
static int parseNumber(const char *&p, long &out)
{
    char *end = NULL;
    long value;

    skipBlankSpaces(p);
    value = strtol(p, &end, 0);

    if (end == p)
        return 0;

    out = value;
    p = end;
    return 1;
}

/* Parse a unary operand: optional '+'/'-', a number, or a parenthesized expression. */
static int parseUnary(const char *&p, long &out)
{
    int negate = 0;

    skipBlankSpaces(p);

    if (*p == '+') {
        ++p;
        skipBlankSpaces(p);
    } else if (*p == '-') {
        ++p;
        negate = 1;
        skipBlankSpaces(p);
    }

    if (*p == '(') {
        ++p;

        if (!parseAddSub(p, out))
            return 0;

        skipBlankSpaces(p);
        if (*p != ')')
            return 0;

        ++p;
    } else {
        if (!parseNumber(p, out))
            return 0;
    }

    if (negate)
        out = -out;

    return 1;
}

/* Parse multiplication, division, and modulo operations. */
static int parseMulDivMod(const char *&p, long &out)
{
    long rhs;
    char op;

    if (!parseUnary(p, out))
        return 0;

    for (;;) {
        skipBlankSpaces(p);
        op = *p;

        if (op != '*' && op != '/' && op != '%')
            break;

        ++p;

        if (!parseUnary(p, rhs))
            return 0;

        if (op == '*') {
            out = out * rhs;
        } else if (op == '/') {
            if (rhs == 0)
                return 0;
            out = out / rhs;
        } else {
            if (rhs == 0)
                return 0;
            out = out % rhs;
        }
    }

    return 1;
}

/* Parse addition and subtraction operations. */
static int parseAddSub(const char *&p, long &out)
{
    long rhs;
    char op;

    if (!parseMulDivMod(p, out))
        return 0;

    for (;;) {
        skipBlankSpaces(p);
        op = *p;

        if (op != '+' && op != '-')
            break;

        ++p;

        if (!parseMulDivMod(p, rhs))
            return 0;

        if (op == '+')
            out = out + rhs;
        else
            out = out - rhs;
    }

    return 1;
}

/* Evaluate an arithmetic expression in the given character range. */
static int evalMacroExpression(const char * const begin, const char * const end, long &result)
{
    const char *p = begin;

    if (!parseAddSub(p, result))
        return 0;

    skipBlankSpaces(p);
    return p == end;
}

/* Replace $(...) arithmetic expressions in the buffer with their evaluated values. */
char *macArithPostprocess(char *buf, const size_t capacity)
{
    const char *r;
    char *w;

    if (!buf || capacity == 0)
        return buf;

    char * const srcCopy = epicsStrDup(buf);
    char * const wend = buf + capacity - 1;
    r = srcCopy;
    w = buf;

    while (*r && w < wend) {
        if (r[0] == '$' && r[1] == '(') {
            const char * const exprBegin = r + 2;
            const char *p = exprBegin;
            int depth = 1;
            long value;

            while (*p && depth > 0) {
                if (*p == '(')
                    ++depth;
                else if (*p == ')')
                    --depth;
                ++p;
            }

            if (depth == 0) {
                const char * const exprEnd = p - 1;

                if (evalMacroExpression(exprBegin, exprEnd, value)) {
                    int n = epicsSnprintf(w, (size_t)(wend - w + 1), "%ld", value);

                    if (n < 0 || w + n > wend)
                        break;

                    w += n;
                    r = p;
                    continue;
                }
            }
        }

        *w++ = *r++;
    }

    *w = '\0';
    free(srcCopy);
    return w;
}
