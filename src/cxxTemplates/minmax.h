
//
// simple inline template functions to replace the min() and max()
// macros
//

//
// ??? g++ 2.7.2 -Winline is unable to in line these tiny functions ???
//

template <class T> 
inline const T &max(const T &a, const T &b)
{
	return (a>b) ? a : b;
}
 
template <class T> 
inline const T &min(const T &a, const T &b)
{
	return (a<b) ? a : b;
}

