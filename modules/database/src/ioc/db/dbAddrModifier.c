long wrapArrayIndices(long *start, const long increment,
    long *end, const long no_elements)
{
    if (*start < 0) *start = no_elements + *start;
    if (*start < 0) *start = 0;
    if (*start > no_elements) *start = no_elements;

    if (*end < 0) *end = no_elements + *end;
    if (*end < 0) *end = 0;
    if (*end >= no_elements) *end = no_elements - 1;

    if (*end - *start >= 0)
        return 1 + (*end - *start) / increment;
    else
        return 0;
}
