
//
// simple type safe inline template functions to replace 
// the min() and max() macros
//

template <class T> 
inline const T & tsMax (const T &a, const T &b)
{
	return (a>b) ? a : b;
}
 
template <class T> 
inline const T & tsMin (const T &a, const T &b)
{
	return (a<b) ? a : b;
}
