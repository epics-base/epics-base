

#ifndef osiWireFormat
#define osiWireFormat

#include "epicsTypes.h"

inline void osiConvertToWireFormat ( const epicsFloat32 &value, unsigned char *pWire )
{
    cvt$convert_float ( &value, CVT$K_VAX_F , pWire, CVT$K_IEEE_S, CVT$M_BIG_ENDIAN );
}

inline void osiConvertToWireFormat ( const epicsFloat64 &value, unsigned char *pWire )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( &value, CVT$K_VAX_G , pWire, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( &value, CVT$K_VAX_D , pWire, CVT$K_IEEE_T, CVT$M_BIG_ENDIAN );
#   endif
}

inline void osiConvertFromWireFormat ( epicsFloat32 &value, epicsUInt8 *pWire )
{
    cvt$convert_float ( &value, CVT$K_IEEE_S, pWire, CVT$K_VAX_F, CVT$M_BIG_ENDIAN );
}

inline void osiConvertFromWireFormat ( epicsFloat64 &value, epicsUInt8 *pWire )
{
#   if defined ( __G_FLOAT ) && ( __G_FLOAT == 1 )
        cvt$convert_float ( pWire, CVT$K_IEEE_T, &value, CVT$K_VAX_G, CVT$M_BIG_ENDIAN );
#   else
        cvt$convert_float ( pWire, CVT$K_IEEE_T, &value, CVT$K_VAX_D, CVT$M_BIG_ENDIAN );
#   endif
}

#endif // osiWireFormat