int isnan(double d)
{
        union { long l[2]; double d; } u;
        u.d = d;
        if ((u.l[0] & 0x7ff00000) != 0x7ff00000) return(0);
        if (u.l[1] || (u.l[0] & 0x000fffff)) return(1);
        return(0);
}
