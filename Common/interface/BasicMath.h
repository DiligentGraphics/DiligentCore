/*     Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

#include "../../Platforms/Basic/interface/DebugUtilities.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include "HashUtils.h"

#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable : 4201) // nonstandard extension used: nameless struct/union
#endif

// Common Constants

#define PI_F 3.1415927f

namespace Diligent
{

// Template Vector & Matrix Classes
template <class T> struct Matrix4x4;
template <class T> struct Vector4;

template <class T> struct Vector2
{
    union
    {
        struct
        {
            T x;
            T y;
        };
        struct
        {
            T r;
            T g;
        };
        struct
        {
            T u;
            T v;
        };
    };


    Vector2 operator-(const Vector2<T> &right)const
    {
        return Vector2(x - right.x, y - right.y);
    }

    Vector2& operator-=(const Vector2<T> &right)
    {
        x -= right.x;
        y -= right.y;
        return *this;
    }

    Vector2 operator-()const
    {
        return Vector2(-x, -y);
    }

    Vector2 operator+(const Vector2<T> &right)const
    {
        return Vector2(x + right.x, y + right.y);
    }

    Vector2& operator+=(const Vector2<T> &right)
    {
        x += right.x;
        y += right.y;
        return *this;
    }

    Vector2 operator*(T s)const
    {
        return Vector2(x * s, y * s);
    }

    Vector2 operator*(const Vector2 &right)const
    {
        return Vector2(x * right.x, y * right.y);
    }

    Vector2& operator*=( const Vector2 &right)
    {
        x *= right.x;
        y *= right.y;
        return *this;
    }

    Vector2& operator*=( T s)
    {
        x *= s;
        y *= s;
        return *this;
    }

    Vector2 operator/(const Vector2 &right)const
    {
        return Vector2(x / right.x, y / right.y);
    }

    Vector2& operator/=( const Vector2 &right)
    {
        x /= right.x;
        y /= right.y;
        return *this;
    }

    Vector2 operator/(T s)const
    {
        return Vector2(x / s, y / s);
    }

    Vector2& operator/=( T s)
    {
        x /= s;
        y /= s;
        return *this;
    }
    
    bool operator == (const Vector2 &right)const
    {
        return x == right.x && y == right.y;
    }
    
    bool operator != (const Vector2 &right)const
    {
        return !(*this == right);
    }

	Vector2 operator < ( const Vector2 &right )const
	{
		return Vector2(x < right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y < right.y ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector2 operator > ( const Vector2 &right )const
	{
		return Vector2(x > right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y > right.y ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector2 operator <= ( const Vector2 &right )const
	{
		return Vector2(x <= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y <= right.y ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector2 operator >= ( const Vector2 &right )const
	{
		return Vector2(x >= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y >= right.y ? static_cast<T>(1) : static_cast<T>(0));
	}

    T& operator[](size_t index)
    {
        return reinterpret_cast<T*>(this)[index];
    }

    const T& operator[](size_t index)const
    {
        return reinterpret_cast<const T*>(this)[index];
    }

    Vector2() : x(0), y(0) { }
    Vector2(T _x, T _y) : x(_x), y(_y) { }
};

template <class T>
Vector2<T> operator*(T s, const Vector2<T> &a)
{
    return a * s;
}


template <class T> struct Vector3
{
    union
    {
        struct
        {
            T x;
            T y;
            T z;
        };
        struct
        {
            T r;
            T g;
            T b;
        };
        struct
        {
            T u;
            T v;
            T w;
        };
    };

        
    Vector3 operator-( const Vector3 &right )const
    {
        return Vector3(x - right.x, y - right.y, z - right.z);
    }

    Vector3 operator-()const
    {
        return Vector3(-x, -y, -z);
    }
    
    Vector3& operator-=(const Vector3<T> &right)
    {
        x -= right.x;
        y -= right.y;
        z -= right.z;
        return *this;
    }

    Vector3 operator+( const Vector3 &right )const
    {
        return Vector3(x + right.x, y + right.y, z + right.z);
    }
    
    Vector3& operator+=(const Vector3<T> &right)
    {
        x += right.x;
        y += right.y;
        z += right.z;
        return *this;
    }

    Vector3 operator*( T s )const
    {
        return Vector3(x * s, y * s, z * s);
    }

    Vector3& operator*=( T s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    Vector3 operator*( const Vector3 &right )const
    {
        return Vector3(x * right.x, y * right.y, z * right.z);
    }
    
    Vector3 operator* (const Matrix4x4<T>& m)const
    {
        Vector4<T> out4 = Vector4<T>(x, y, z, 1) * m;
        return Vector3(out4.x / out4.w, out4.y / out4.w, out4.z / out4.w) ;
    }

    Vector3& operator*=( const Vector3 &right)
    {
        x *= right.x;
        y *= right.y;
        z *= right.z;
        return *this;
    }

    Vector3 operator/ ( T  s)const
    {
        return Vector3(x / s, y / s, z / s);
    }

    Vector3& operator/=( T s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }

    Vector3 operator/( const Vector3 &right )const
    {
        return Vector3(x / right.x, y / right.y, z / right.z);
    }

    Vector3& operator/=( const Vector3 &right)
    {
        x /= right.x;
        y /= right.y;
        z /= right.z;
        return *this;
    }

    bool operator == (const Vector3 &right)const
    {
        return x == right.x && y == right.y && z == right.z;
    }
    
    bool operator != (const Vector3 &right)const
    {
        return !(*this == right);
    }

	Vector3 operator < ( const Vector3 &right )const
	{
		return Vector3(x < right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y < right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z < right.z ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector3 operator > ( const Vector3 &right )const
	{
		return Vector3(x > right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y > right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z > right.z ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector3 operator <= ( const Vector3 &right )const
	{
		return Vector3(x <= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y <= right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z <= right.z ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector3 operator >= ( const Vector3 &right )const
	{
		return Vector3(x >= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y >= right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z >= right.z ? static_cast<T>(1) : static_cast<T>(0));
	}

    T& operator[](size_t index)
    {
        return reinterpret_cast<T*>(this)[index];
    }

    const T& operator[](size_t index)const
    {
        return reinterpret_cast<const T*>(this)[index];
    }

    Vector3() : x(0), y(0), z(0) {}
    Vector3(T _x, T _y, T _z) : x(_x), y(_y), z(_z) { }

    operator Vector2<T>()const{return Vector2<T>(x,y);}
};

template <class T>
Vector3<T> operator*(T s, const Vector3<T> &a)
{
    return a * s;
}


template <class T> struct Vector4
{
    union
    {
        struct
        {
            T x;
            T y;
            T z;
            T w;
        };
        struct
        {
            T r;
            T g;
            T b;
            T a;
        };
    };

    Vector4 operator-( const Vector4 &right)const
    {
        return Vector4(x - right.x, y - right.y, z - right.z, w - right.w);
    }

    Vector4 operator-()const
    {
        return Vector4(-x, -y, -z, -w);
    }

    Vector4& operator-=(const Vector4<T> &right)
    {
        x -= right.x;
        y -= right.y;
        z -= right.z;
        w -= right.w;
        return *this;
    }

    Vector4 operator+( const Vector4 &right)const
    {
        return Vector4(x + right.x, y + right.y, z + right.z, w + right.w);
    }

    Vector4& operator+=(const Vector4<T> &right)
    {
        x += right.x;
        y += right.y;
        z += right.z;
        w += right.w;
        return *this;
    }

    Vector4 operator*( T s)const
    {
        return Vector4(x * s, y * s, z * s, w * s);
    }

    Vector4& operator*=( T s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }

    Vector4 operator*( const Vector4 &right)const
    {
        return Vector4(x * right.x, y * right.y, z * right.z, w * right.w);
    }

    Vector4& operator*=( const Vector4 &right)
    {
        x *= right.x;
        y *= right.y;
        z *= right.z;
        w *= right.w;
        return *this;
    }

    Vector4 operator/( T s)const
    {
        return Vector4(x / s, y / s, z / s, w / s);
    }

    Vector4& operator/=( T s)
    {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }

    Vector4 operator/( const Vector4 &right)const
    {
        return Vector4(x / right.x, y / right.y, z / right.z, w / right.w);
    }

    Vector4& operator/=( const Vector4 &right)
    {
        x /= right.x;
        y /= right.y;
        z /= right.z;
        w /= right.w;
        return *this;
    }

    bool operator == (const Vector4 &right)const
    {
        return x == right.x && y == right.y && z == right.z && w == right.w;
    }
    
    bool operator != (const Vector4 &right)const
    {
        return !(*this == right);
    }

    Vector4 operator*(const Matrix4x4<T>& m)const
    {
        Vector4 out;
        out[0] = x * m[0][0] + y * m[1][0] + z * m[2][0] + w * m[3][0];
        out[1] = x * m[0][1] + y * m[1][1] + z * m[2][1] + w * m[3][1];
        out[2] = x * m[0][2] + y * m[1][2] + z * m[2][2] + w * m[3][2];
        out[3] = x * m[0][3] + y * m[1][3] + z * m[2][3] + w * m[3][3];
        return out;
    }

    Vector4& operator = (const Vector3<T> &v3)
    {
        x = v3.x;
        y = v3.y;
        z = v3.z;
        w = 1;
        return *this;
    }
    Vector4& operator = (const Vector4 &) = default;

	Vector4 operator < ( const Vector4 &right )const
	{
		return Vector4(x < right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y < right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z < right.z ? static_cast<T>(1) : static_cast<T>(0), 
			           w < right.w ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector4 operator > ( const Vector4 &right )const
	{
		return Vector4(x > right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y > right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z > right.z ? static_cast<T>(1) : static_cast<T>(0), 
			           w > right.w ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector4 operator <= ( const Vector4 &right )const
	{
		return Vector4(x <= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y <= right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z <= right.z ? static_cast<T>(1) : static_cast<T>(0), 
			           w <= right.w ? static_cast<T>(1) : static_cast<T>(0));
	}

	Vector4 operator >= ( const Vector4 &right )const
	{
		return Vector4(x >= right.x ? static_cast<T>(1) : static_cast<T>(0), 
				       y >= right.y ? static_cast<T>(1) : static_cast<T>(0), 
			           z >= right.z ? static_cast<T>(1) : static_cast<T>(0), 
			           w >= right.w ? static_cast<T>(1) : static_cast<T>(0));
	}

    T& operator[](size_t index)
    {
        return reinterpret_cast<T*>(this)[index];
    }

    const T& operator[](size_t index)const
    {
        return reinterpret_cast<const T*>(this)[index];
    }

    Vector4() : x(0), y(0), z(0), w(0) { }
    Vector4(T _x, T _y, T _z, T _w) : x(_x), y(_y), z(_z), w(_w) { }
    Vector4(const Vector3<T>& v3, T _w) :  x(v3.x), y(v3.y), z(v3.z), w(_w) { }

    operator Vector3<T>() const
    {
        return Vector3<T>(x, y, z);
    }
};


template <class T>
Vector4<T> operator*(T s, const Vector4<T> &a)
{
    return a * s;
}


template <class T> struct Matrix2x2
{
    union
    {
        struct
        {
            T _11; T _12;
            T _21; T _22;
        };
        struct
        {
            T _m00; T _m01;
            T _m10; T _m11;
        };
    };

    explicit
    Matrix2x2(T value)
    {
        _11 = _12 = value;
        _21 = _22 = value;
    }

    Matrix2x2() : Matrix2x2(0) {}

    Matrix2x2(
            T i11, T i12,
            T i21, T i22)
    {
        _11 = i11; _12 = i12;
        _21 = i21; _22 = i22;
    }

    bool operator == (const Matrix2x2 &r)const
    {
        for (int i = 0; i < 2; ++i)
            for (int j = 0; i < 2; ++i)
                if ((*this)[i][j] != r[i][j])
                    return false;

        return true;
    }

    bool operator != (const Matrix2x2 &r)const
    {
        return !(*this == r);
    }

    T* operator[](size_t index)
    {
        return &(reinterpret_cast<T*>(this)[index * 2]);
    }

    const T* operator[](size_t index)const
    {
        return &(reinterpret_cast<const T*>(this)[index * 2]);
    }

    Matrix2x2& operator *=(T s)
    {
        for (int i = 0; i < 4; ++i)
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }
};


template <class T> struct Matrix3x3
{
    union
    {
        struct
        {
            T _11; T _12; T _13;
            T _21; T _22; T _23;
            T _31; T _32; T _33;
        };
        struct
        {
            T _m00; T _m01; T _m02;
            T _m10; T _m11; T _m12;
            T _m20; T _m21; T _m22;
        };
    };

    explicit
    Matrix3x3(T value)
    {
        _11 = _12 = _13 =value;
        _21 = _22 = _23 =value;
        _31 = _32 = _33 =value;
    }

    Matrix3x3() : Matrix3x3(0) {}

    Matrix3x3(
        T i11, T i12, T i13,
        T i21, T i22, T i23,
        T i31, T i32, T i33 )
    {
        _11 = i11; _12 = i12; _13 = i13;
        _21 = i21; _22 = i22; _23 = i23;
        _31 = i31; _32 = i32; _33 = i33;
    }

    bool operator == (const Matrix3x3 &r)const
    {
        for( int i = 0; i < 3; ++i )
            for( int j = 0; i < 3; ++i )
                if( (*this)[i][j] != r[i][j] )
                    return false;

        return true;
    }

    bool operator != (const Matrix3x3 &r)const
    {
        return !(*this == r);
    }

    T* operator[](size_t index)
    {
        return &(reinterpret_cast<T*>(this)[index*3]);
    }

    const T* operator[](size_t index)const
    {
        return &(reinterpret_cast<const T*>(this)[index*3]);
    }

    Matrix3x3& operator *=(T s)
    {
        for( int i = 0; i < 9; ++i )
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }
};

template <class T> struct Matrix4x4
{
    union
    {
        struct
        {
            T _11; T _12; T _13; T _14;
            T _21; T _22; T _23; T _24;
            T _31; T _32; T _33; T _34;
            T _41; T _42; T _43; T _44;
        };
        struct
        {
            T _m00; T _m01; T _m02; T _m03;
            T _m10; T _m11; T _m12; T _m13;
            T _m20; T _m21; T _m22; T _m23;
            T _m30; T _m31; T _m32; T _m33;
        };
    };

    explicit
    Matrix4x4(T value)
    {
        _11 = _12 = _13 = _14 = value;
        _21 = _22 = _23 = _24 = value;
        _31 = _32 = _33 = _34 = value;
        _41 = _42 = _43 = _44 = value;
    }

    Matrix4x4() : Matrix4x4(0) {}

    Matrix4x4(
        T i11, T i12, T i13, T i14,
        T i21, T i22, T i23, T i24,
        T i31, T i32, T i33, T i34,
        T i41, T i42, T i43, T i44
        )
    {
        _11 = i11; _12 = i12; _13 = i13; _14 = i14;
        _21 = i21; _22 = i22; _23 = i23; _24 = i24;
        _31 = i31; _32 = i32; _33 = i33; _34 = i34;
        _41 = i41; _42 = i42; _43 = i43; _44 = i44;
    }

    bool operator == (const Matrix4x4 &r)const
    {
        for( int i = 0; i < 4; ++i )
            for( int j = 0; i < 4; ++i )
                if( (*this)[i][j] != r[i][j] )
                    return false;

        return true;
    }

    bool operator != (const Matrix4x4 &r)const
    {
        return !(*this == r);
    }

    T* operator[](size_t index)
    {
        return &(reinterpret_cast<T*>(this)[index*4]);
    }

    const T* operator[](size_t index)const
    {
        return &(reinterpret_cast<const T*>(this)[index*4]);
    }

    Matrix4x4& operator *=(T s)
    {
        for( int i = 0; i < 16; ++i )
            (reinterpret_cast<T*>(this))[i] *= s;

        return *this;
    }
};

// Template Vector Operations


template <class T>
T dot(const Vector2<T> &a, const Vector2<T> &b)
{
    return a.x * b.x + a.y * b.y;
}

template <class T>
T dot(const Vector3<T> &a, const Vector3<T> &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <class T>
T dot(const Vector4<T> &a, const Vector4<T> &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

template <class VectorType>
auto length(const VectorType &a)->decltype(dot(a,a))
{
    return sqrt( dot(a,a) );
}


template <class T>
Vector3<T> min(const Vector3<T> &a, const Vector3<T> &b)
{
    return Vector3<T>( std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) );
}

template <class T>
Vector4<T> min(const Vector4<T> &a, const Vector4<T> &b)
{
    return Vector4<T>( std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w) );
}

template <class T>
Vector3<T> max(const Vector3<T> &a, const Vector3<T> &b)
{
    return Vector3<T>( std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) );
}

template <class T>
Vector4<T> max(const Vector4<T> &a, const Vector4<T> &b)
{
    return Vector4<T>( std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w) );
}

template <class T>
Vector2<T> abs(const Vector2<T> &a)
{
	// WARNING: abs() on gcc is for integers only!
    return Vector2<T>( a.x < 0 ? -a.x : a.x, 
					   a.y < 0 ? -a.y : a.y);
}

template <class T>
Vector3<T> abs(const Vector3<T> &a)
{
	// WARNING: abs() on gcc is for integers only!
    return Vector3<T>( a.x < 0 ? -a.x : a.x, 
					   a.y < 0 ? -a.y : a.y,
					   a.z < 0 ? -a.z : a.z);
}

template <class T>
Vector4<T> abs(const Vector4<T> &a)
{
	// WARNING: abs() on gcc is for integers only!
    return Vector4<T>( a.x < 0 ? -a.x : a.x, 
					   a.y < 0 ? -a.y : a.y,
					   a.z < 0 ? -a.z : a.z,
					   a.w < 0 ? -a.w : a.w);
}


template<typename T>
T clamp(T val, T _min, T _max)
{
    return val < _min ? _min : (val > _max ? _max : val);
}

template <class T>
Vector2<T> clamp(const Vector2<T> &a, const Vector2<T> &_min, const Vector2<T> &_max)
{
    return Vector2<T>( clamp(a.x, _min.x, _max.x),
                       clamp(a.y, _min.y, _max.y));
}

template <class T>
Vector3<T> clamp(const Vector3<T> &a, const Vector3<T> &_min, const Vector3<T> &_max)
{
    return Vector3<T>( clamp(a.x, _min.x, _max.x),
                       clamp(a.y, _min.y, _max.y),
                       clamp(a.z, _min.z, _max.z));
}

template <class T>
Vector4<T> clamp(const Vector4<T> &a, const Vector4<T> &_min, const Vector4<T> &_max)
{
    return Vector4<T>( clamp(a.x, _min.x, _max.x),
                       clamp(a.y, _min.y, _max.y),
                       clamp(a.z, _min.z, _max.z),
                       clamp(a.w, _min.w, _max.w));
}


template <class T>
Vector3<T> cross(const Vector3<T> &a, const Vector3<T> &b)
{
    // |   i    j    k   |
    // |  a.x  a.y  a.z  |
    // |  b.x  b.y  b.z  |
    return Vector3<T>((a.y*b.z)-(a.z*b.y), (a.z*b.x)-(a.x*b.z), (a.x*b.y)-(a.y*b.x));
}

template <class VectorType>
VectorType normalize(const VectorType &a)
{
    auto len = length(a);
    return a / len;
}


// Template Matrix Operations

template <class T>
Matrix4x4<T> transposeMatrix(const Matrix4x4<T> &m)
{
    return Matrix4x4<T>(
        m._11, m._21, m._31, m._41,
        m._12, m._22, m._32, m._42,
        m._13, m._23, m._33, m._43,
        m._14, m._24, m._34, m._44
        );
}

template <class T>
Matrix4x4<T> mul(const Matrix4x4<T> &m1, const Matrix4x4<T> &m2)
{
    Matrix4x4<T> mOut;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            for (int k = 0; k < 4; k++)
            {
                mOut[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }

    return mOut;
}

template <class T>
Matrix4x4<T> operator* (const Matrix4x4<T> &m1, const Matrix4x4<T> &m2)
{
    return mul( m1, m2 );
}



template <class T>
Matrix3x3<T> transposeMatrix(const Matrix3x3<T> &m)
{
    return Matrix3x3<T>(
        m._11, m._21, m._31,
        m._12, m._22, m._32,
        m._13, m._23, m._33
        );
}

template <class T>
Matrix3x3<T> mul(const Matrix3x3<T> &m1, const Matrix3x3<T> &m2)
{
    Matrix3x3<T> mOut;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                mOut[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }

    return mOut;
}

template <class T>
Matrix3x3<T> operator* (const Matrix3x3<T> &m1, const Matrix3x3<T> &m2)
{
    return mul( m1, m2 );
}



template <class T>
Matrix2x2<T> transposeMatrix(const Matrix2x2<T> &m)
{
    return Matrix2x2<T>(
        m._11, m._21,
        m._12, m._22
        );
}

template <class T>
Matrix2x2<T> mul(const Matrix2x2<T> &m1, const Matrix2x2<T> &m2)
{
    Matrix2x2<T> mOut;

    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 2; k++)
            {
                mOut[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }

    return mOut;
}

template <class T>
Matrix2x2<T> operator* (const Matrix2x2<T> &m1, const Matrix2x2<T> &m2)
{
    return mul(m1, m2);
}
// Common HLSL-compatible vector typedefs

using uint  = uint32_t;
using uint2 = Vector2<uint>;
using uint3 = Vector3<uint>;
using uint4 = Vector4<uint>;

using int2 = Vector2<int32_t>;
using int3 = Vector3<int32_t>;
using int4 = Vector4<int32_t>;

using float2 = Vector2<float>;
using float3 = Vector3<float>;
using float4 = Vector4<float>;

using float4x4 = Matrix4x4<float>;
using float3x3 = Matrix3x3<float>;
using float2x2 = Matrix2x2<float>;

// Standard Matrix Intializers

inline float4x4 identityMatrix()
{
    return float4x4(1, 0, 0, 0,
                    0, 1, 0, 0,
                    0, 0, 1, 0,
                    0, 0, 0, 1);
}

inline float4x4 translationMatrix(float x, float y, float z)
{
    return float4x4 (1, 0, 0, 0,
                     0, 1, 0, 0,
                     0, 0, 1, 0,
                     x, y, z, 1);
}

inline float4x4 translationMatrix( const float3 &v )
{
    return translationMatrix( v.x, v.y, v.z );
}


inline float4x4 scaleMatrix(float x, float y, float z)
{
    return float4x4(x, 0, 0, 0,
                    0, y, 0, 0,
                    0, 0, z, 0, 
                    0, 0, 0, 1);
}

inline float4x4 rotationX(float angleInRadians)
{
    float sinAngle = sinf(angleInRadians);
    float cosAngle = cosf(angleInRadians);

    float4x4 mOut;

    mOut._11 = 1.0f; mOut._12 = 0.0f;     mOut._13 = 0.0f;      mOut._14 = 0.0f;
    mOut._21 = 0.0f; mOut._22 = cosAngle; mOut._23 = -sinAngle; mOut._24 = 0.0f;
    mOut._31 = 0.0f; mOut._32 = sinAngle; mOut._33 = cosAngle;  mOut._34 = 0.0f;
    mOut._41 = 0.0f; mOut._42 = 0.0f;     mOut._43 = 0.0f;      mOut._44 = 1.0f;

    return mOut;
}

inline float4x4 rotationY(float angleInRadians)
{
    float sinAngle = sinf(angleInRadians);
    float cosAngle = cosf(angleInRadians);

    float4x4 mOut;

    mOut._11 = cosAngle;  mOut._12 = 0.0f; mOut._13 = sinAngle; mOut._14 = 0.0f;
    mOut._21 = 0.0f;      mOut._22 = 1.0f; mOut._23 = 0.0f;     mOut._24 = 0.0f;
    mOut._31 = -sinAngle; mOut._32 = 0.0f; mOut._33 = cosAngle; mOut._34 = 0.0f;
    mOut._41 = 0.0f;      mOut._42 = 0.0f; mOut._43 = 0.0f;     mOut._44 = 1.0f;

    return mOut;
}

inline float4x4 rotationZ(float angleInRadians)
{
    float sinAngle = sinf(angleInRadians);
    float cosAngle = cosf(angleInRadians);

    float4x4 mOut;

    mOut._11 = cosAngle; mOut._12 = -sinAngle; mOut._13 = 0.0f; mOut._14 = 0.0f;
    mOut._21 = sinAngle; mOut._22 = cosAngle;  mOut._23 = 0.0f; mOut._24 = 0.0f;
    mOut._31 = 0.0f;     mOut._32 = 0.0f;      mOut._33 = 1.0f; mOut._34 = 0.0f;
    mOut._41 = 0.0f;     mOut._42 = 0.0f;      mOut._43 = 0.0f; mOut._44 = 1.0f;

    return mOut;
}

// 3D Rotation matrix for an arbitrary axis specified by x, y and z
inline float4x4 rotationArbitrary(float3 axis, float degree)
{
    UNSUPPORTED("This function is not tested, it might be incorrect");

    axis = normalize(axis);

    float angleInRadians = degree * (PI_F / 180.0f);

    float sinAngle = sinf(angleInRadians);
    float cosAngle = cosf(angleInRadians);
    float oneMinusCosAngle = 1 - cosAngle;

    float4x4 mOut;

    mOut._11 = 1.0f + oneMinusCosAngle * (axis.x * axis.x - 1.0f);
    mOut._12 = axis.z * sinAngle + oneMinusCosAngle * axis.x * axis.y;
    mOut._13 = -axis.y * sinAngle + oneMinusCosAngle * axis.x * axis.z;
    mOut._41 = 0.0f;

    mOut._21 = -axis.z * sinAngle + oneMinusCosAngle * axis.y * axis.x;
    mOut._22 = 1.0f + oneMinusCosAngle * (axis.y * axis.y - 1.0f);
    mOut._23 = axis.x * sinAngle + oneMinusCosAngle * axis.y * axis.z;
    mOut._24 = 0.0f;

    mOut._31 = axis.y * sinAngle + oneMinusCosAngle * axis.z * axis.x;
    mOut._32 = -axis.x * sinAngle + oneMinusCosAngle * axis.z * axis.y;
    mOut._33 = 1.0f + oneMinusCosAngle * (axis.z * axis.z - 1.0f);
    mOut._34 = 0.0f;

    mOut._41 = 0.0f;
    mOut._42 = 0.0f;
    mOut._43 = 0.0f;
    mOut._44 = 1.0f;

    return mOut;
}

inline float4x4 ViewMatrixFromBasis( const float3 &f3X, const float3 &f3Y, const float3 &f3Z )
{
    return float4x4( f3X.x, f3Y.x, f3Z.x, 0,
                     f3X.y, f3Y.y, f3Z.y, 0,
                     f3X.z, f3Y.z, f3Z.z, 0,
                         0,     0,     0, 1);
}

inline void SetNearFarClipPlanes( float4x4 &ProjMatrix, float zNear, float zFar, bool bIsGL )
{
    if( bIsGL )
    {
        // https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml
        // http://www.terathon.com/gdc07_lengyel.pdf
        // Note that OpenGL uses right-handed coordinate system, where
        // camera is looking in negative z direction:
        //   OO
        //  |__|<--------------------
        //         -z             +z
        // Consequently, OpenGL projection matrix given by these two
        // references inverts z axis.

        // We do not need to do this, because we use DX coordinate
        // system for the camera space. Thus we need to invert the 
        // sign of the values in the third column in the matrix 
        // from the references:

        ProjMatrix._33 = -(-(zFar + zNear) / (zFar - zNear));
        ProjMatrix._43 = -2.0f * zNear * zFar / (zFar - zNear);
        ProjMatrix._34 = -(-1);
    }
    else
    {
        ProjMatrix._33 = zFar / (zFar - zNear);
        ProjMatrix._43 = -zNear * zFar / (zFar - zNear);
        ProjMatrix._34 = 1;
    }
}

inline void GetNearFarPlaneFromProjMatrix( const float4x4 &ProjMatrix, float &zNear, float &zFar, bool bIsGL )
{
    if( bIsGL )
    {
        zNear = ProjMatrix._43 / (-1.f - ProjMatrix._33);
        zFar  = ProjMatrix._43 / (+1.f - ProjMatrix._33);
    }
    else
    {
        zNear = -ProjMatrix._43 / ProjMatrix._33;
        zFar = ProjMatrix._33 / (ProjMatrix._33 - 1) * zNear;
    }
}

inline float4x4 Projection(float fov, float aspectRatio, float zNear, float zFar, bool bIsGL ) // Left-handed projection
{
    float4x4 mOut;
    float yScale = 1.0f / tan(fov / 2.0f);
    float xScale = yScale / aspectRatio;
    mOut._11 = xScale;
    mOut._22 = yScale;

    SetNearFarClipPlanes( mOut, zNear, zFar, bIsGL );
  
    return mOut;
}

inline float4x4 OrthoOffCenter(float left, float right, float bottom, float top, float zNear, float zFar, bool bIsGL ) // Left-handed ortho projection
{
    float _22 = (bIsGL ? 2.f          : 1.f   ) / (zFar - zNear);
    float _32 = (bIsGL ? zNear + zFar : zNear ) / (zNear - zFar);
    return float4x4 (
                 2.f / (right - left),                             0.f,  0.f, 0.f,
                                  0.f,              2.f/(top - bottom),  0.f, 0.f,
                                  0.f,                             0.f,  _22, 0.f,                
        (left + right)/(left - right), (top + bottom) / (bottom - top),  _32, 1.f
    );
}

inline float4x4 Ortho(float width, float height, float zNear, float zFar, bool bIsGL ) // Left-handed ortho projection
{
    return OrthoOffCenter(-width * 0.5f, +width * 0.5f, -height * 0.5f, +height * 0.5f, zNear, zFar, bIsGL);
}

struct Quaternion
{
    float q[4];
};

inline Quaternion RotationFromAxisAngle(const float3& axis, float angle)
{
    Quaternion out;
    float norm = length(axis);
    float sina2 = sin(0.5f * angle);
    out.q[0] = sina2 * axis[0] / norm;
    out.q[1] = sina2 * axis[1] / norm;
    out.q[2] = sina2 * axis[2] / norm;
    out.q[3] = cos(0.5f * angle);
    return out;
}

inline void AxisAngleFromRotation(float3& outAxis, float& outAngle, const Quaternion& quat)
{
    float sina2 = sqrt(quat.q[0]*quat.q[0] + quat.q[1]*quat.q[1] + quat.q[2]*quat.q[2]);
    outAngle = 2.0f * atan2(sina2, quat.q[3]);
    float r = (sina2 > 0) ? (1.0f / sina2) : 0;
    outAxis[0] = r * quat.q[0];
    outAxis[1] = r * quat.q[1];
    outAxis[2] = r * quat.q[2]; 
}

inline float4x4 QuaternionToMatrix(const Quaternion& quat)
{
    float4x4 out;
    float yy2 = 2.0f * quat.q[1] * quat.q[1];
    float xy2 = 2.0f * quat.q[0] * quat.q[1];
    float xz2 = 2.0f * quat.q[0] * quat.q[2];
    float yz2 = 2.0f * quat.q[1] * quat.q[2];
    float zz2 = 2.0f * quat.q[2] * quat.q[2];
    float wz2 = 2.0f * quat.q[3] * quat.q[2];
    float wy2 = 2.0f * quat.q[3] * quat.q[1];
    float wx2 = 2.0f * quat.q[3] * quat.q[0];
    float xx2 = 2.0f * quat.q[0] * quat.q[0];
    out[0][0] = - yy2 - zz2 + 1.0f;
    out[0][1] = xy2 + wz2;
    out[0][2] = xz2 - wy2;
    out[0][3] = 0;
    out[1][0] = xy2 - wz2;
    out[1][1] = - xx2 - zz2 + 1.0f;
    out[1][2] = yz2 + wx2;
    out[1][3] = 0;
    out[2][0] = xz2 + wy2;
    out[2][1] = yz2 - wx2;
    out[2][2] = - xx2 - yy2 + 1.0f;
    out[2][3] = 0;
    out[3][0] = out[3][1] = out[3][2] = 0;
    out[3][3] = 1;
    return out;
}

inline float determinant( const float3x3& m )
{
    float det = 0.f;
    det += m._11 * (m._22*m._33 - m._32*m._23);
    det -= m._12 * (m._21*m._33 - m._31*m._23);
    det += m._13 * (m._21*m._32 - m._31*m._22);
    return det;
}


inline float determinant( const float4x4& m )
{
    float det = 0.f;

    det += m._11 * determinant(      
            float3x3 ( m._22,m._23,m._24,
                       m._32,m._33,m._34,
                       m._42,m._43,m._44) );

    det -= m._12 * determinant(
            float3x3( m._21,m._23,m._24,
                      m._31,m._33,m._34,
                      m._41,m._43,m._44) );

    det += m._13 * determinant(
            float3x3( m._21,m._22,m._24,
                      m._31,m._32,m._34,
                      m._41,m._42,m._44) );

    det -= m._14 * determinant(
            float3x3( m._21,m._22,m._23,
                      m._31,m._32,m._33,
                      m._41,m._42,m._43) );

    return det;
}

inline float4x4 inverseMatrix(const float4x4& m)
{
    float4x4 inv;

    // row 1
    inv._11 = determinant( 
        float3x3( m._22, m._23, m._24,
                  m._32, m._33, m._34,
                  m._42, m._43, m._44 ) );

    inv._12 = -determinant(
        float3x3( m._21, m._23, m._24,
                  m._31, m._33, m._34,
                  m._41, m._43, m._44) );

    inv._13 = determinant(
        float3x3 ( m._21, m._22, m._24,
                   m._31, m._32, m._34,
                   m._41, m._42, m._44) );

    inv._14 = -determinant(
        float3x3 ( m._21, m._22, m._23,
                   m._31, m._32, m._33,
                   m._41, m._42, m._43) );


    // row 2
    inv._21 = -determinant(
        float3x3( m._12, m._13, m._14,
                  m._32, m._33, m._34,
                  m._42, m._43, m._44) );

    inv._22 = determinant(
        float3x3( m._11, m._13, m._14,
                  m._31, m._33, m._34,
                  m._41, m._43, m._44) );

    inv._23 = -determinant(
        float3x3( m._11, m._12, m._14,
                  m._31, m._32, m._34,
                  m._41, m._42, m._44) );

    inv._24 = determinant(
        float3x3( m._11, m._12, m._13,
                  m._31, m._32, m._33,
                  m._41, m._42, m._43) );


    // row 3
    inv._31 = determinant(
        float3x3( m._12,m._13,m._14,
                  m._22,m._23,m._24,
                  m._42,m._43,m._44) );

    inv._32 = -determinant(
        float3x3( m._11,m._13,m._14,
                  m._21,m._23,m._24,
                  m._41,m._43,m._44) );

    inv._33 = determinant(
        float3x3( m._11,m._12,m._14,
                  m._21,m._22,m._24,
                  m._41,m._42,m._44) );

    inv._34 = -determinant(
        float3x3( m._11,m._12,m._13,
                  m._21,m._22,m._23,
                  m._41,m._42,m._43) );

    // row 4
    inv._41 = -determinant(
        float3x3( m._12, m._13, m._14,
                  m._22, m._23, m._24,
                  m._32, m._33, m._34) );

    inv._42 = determinant(
        float3x3( m._11, m._13, m._14,
                  m._21, m._23, m._24,
                  m._31, m._33, m._34) );

    inv._43 = -determinant(
        float3x3( m._11, m._12, m._14,
                  m._21, m._22, m._24,
                  m._31, m._32, m._34) );

    inv._44 = determinant(
        float3x3( m._11, m._12, m._13,
                  m._21, m._22, m._23,
                  m._31, m._32, m._33) );
    
    auto det = m._11 * inv._11 + m._12 * inv._12 + m._13 * inv._13 + m._14 * inv._14;
    inv = transposeMatrix(inv);
    inv *= 1.0f/det;

    return inv;
}

} // namespace Diligent

namespace std
{
    template<typename T>
    Diligent::Vector2<T> max( const Diligent::Vector2<T> &Left, const Diligent::Vector2<T> &Right )
    {
        return Diligent::Vector2<T>( 
            std::max( Left.x, Right.x ), 
            std::max( Left.y, Right.y ) 
            );
    }

    template<typename T>
    Diligent::Vector3<T> max( const Diligent::Vector3<T> &Left, const Diligent::Vector3<T> &Right )
    {
        return Diligent::Vector3<T>( 
            std::max( Left.x, Right.x ), 
            std::max( Left.y, Right.y ),
            std::max( Left.z, Right.z )
            );
    }

    template<typename T>
    Diligent::Vector4<T> max( const Diligent::Vector4<T> &Left, const Diligent::Vector4<T> &Right )
    {
        return Diligent::Vector4<T>( 
            std::max( Left.x, Right.x ), 
            std::max( Left.y, Right.y ),
            std::max( Left.z, Right.z ),
            std::max( Left.w, Right.w )
            );
    }


    template<typename T>
    Diligent::Vector2<T> min( const Diligent::Vector2<T> &Left, const Diligent::Vector2<T> &Right )
    {
        return Diligent::Vector2<T>( 
            std::min( Left.x, Right.x ), 
            std::min( Left.y, Right.y ) 
            );
    }

    template<typename T>
    Diligent::Vector3<T> min( const Diligent::Vector3<T> &Left, const Diligent::Vector3<T> &Right )
    {
        return Diligent::Vector3<T>( 
            std::min( Left.x, Right.x ), 
            std::min( Left.y, Right.y ),
            std::min( Left.z, Right.z )
            );
    }

    template<typename T>
    Diligent::Vector4<T> min( const Diligent::Vector4<T> &Left, const Diligent::Vector4<T> &Right )
    {
        return Diligent::Vector4<T>( 
            std::min( Left.x, Right.x ), 
            std::min( Left.y, Right.y ),
            std::min( Left.z, Right.z ),
            std::min( Left.w, Right.w )
            );
    }


    template<typename T>
    struct hash<Diligent::Vector2<T>>
    {
        size_t operator()( const Diligent::Vector2<T> &v2 ) const
        {
            return Diligent::ComputeHash(v2.x, v2.y);
        }
    };

    template<typename T>
    struct hash<Diligent::Vector3<T>>
    {
        size_t operator()( const Diligent::Vector3<T> &v3 ) const
        {
            return Diligent::ComputeHash(v3.x, v3.y, v3.z);
        }
    };

    template<typename T>
    struct hash<Diligent::Vector4<T>>
    {
        size_t operator()( const Diligent::Vector4<T> &v4 ) const
        {
            return Diligent::ComputeHash(v4.x, v4.y, v4.z, v4.w);
        }
    };

    template<typename T>
    struct hash<Diligent::Matrix2x2<T>>
    {
        size_t operator()(const Diligent::Matrix2x2<T> &m) const
        {
            return Diligent::ComputeHash(
                m._m00, m._m01,
                m._m10, m._m11
            );
        }
    };

    template<typename T>
    struct hash<Diligent::Matrix3x3<T>>
    {
        size_t operator()( const Diligent::Matrix3x3<T> &m ) const
        {
            return Diligent::ComputeHash(            
                m._m00,  m._m01,  m._m02,
                m._m10,  m._m11,  m._m12,
                m._m20,  m._m21,  m._m22
            );
        }
    };

    template<typename T>
    struct hash<Diligent::Matrix4x4<T>>
    {
        size_t operator()( const Diligent::Matrix4x4<T> &m ) const
        {
            return Diligent::ComputeHash(            
                m._m00,  m._m01,  m._m02,  m._m03,
                m._m10,  m._m11,  m._m12,  m._m13,
                m._m20,  m._m21,  m._m22,  m._m23,
                m._m30,  m._m31,  m._m32,  m._m33
            );
        }
    };
} // namespace std

#ifdef _MSC_VER
#   pragma warning(pop)
#endif
