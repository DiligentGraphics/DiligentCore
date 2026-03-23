#ifndef _FSR_STRUCTURES_FXH_
#define _FSR_STRUCTURES_FXH_

#ifndef __cplusplus
#    ifndef DEFAULT_VALUE
#        define DEFAULT_VALUE(x)
#    endif
#elif !defined(DEFAULT_VALUE)
#    define DEFAULT_VALUE(x) = x
#endif

#ifndef __cplusplus
#    ifndef CHECK_STRUCT_ALIGNMENT
#        define CHECK_STRUCT_ALIGNMENT(s)
#    endif
#endif

struct FSRAttribs
{
    uint4  EASUConstants0;
    uint4  EASUConstants1;
    uint4  EASUConstants2;
    uint4  EASUConstants3;
    uint4  RCASConstants;
    float4 SourceSize;
};

#ifdef CHECK_STRUCT_ALIGNMENT
CHECK_STRUCT_ALIGNMENT(FSRAttribs)
#endif


#endif //_FSR_STRUCTURES_FXH_
