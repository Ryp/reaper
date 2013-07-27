///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2013 G-Truc Creation (www.g-truc.net)
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref core	
/// @file glm/core/type_mat2x2.hpp
/// @date 2005-01-27 / 2011-06-15
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

#ifndef glm_core_type_mat2x2
#define glm_core_type_mat2x2

#include "../fwd.hpp"
#include "type_mat.hpp"

namespace glm{
namespace detail
{
	template <typename T, precision P>
	struct tmat2x2
	{
		// Implementation detail
		enum ctor{_null};
		typedef T value_type;
		typedef std::size_t size_type;
		typedef tvec2<T, P> col_type;
		typedef tvec2<T, P> row_type;
		typedef tmat2x2<T, P> type;
		typedef tmat2x2<T, P> transpose_type;

		static GLM_FUNC_DECL size_type col_size();
		static GLM_FUNC_DECL size_type row_size();

		GLM_FUNC_DECL GLM_CONSTEXPR size_type length() const;

	public:
		// Implementation detail
		GLM_FUNC_DECL tmat2x2<T, P> _inverse() const;

	private:
		//////////////////////////////////////
		// Implementation detail
		col_type value[2];

	public:
		//////////////////////////////////////
		// Constructors
		GLM_FUNC_DECL tmat2x2();
		GLM_FUNC_DECL tmat2x2(
			tmat2x2 const & m);

		GLM_FUNC_DECL explicit tmat2x2(
			ctor Null);
		GLM_FUNC_DECL explicit tmat2x2(
			value_type const & x);
		GLM_FUNC_DECL explicit tmat2x2(
			value_type const & x1, value_type const & y1,
			value_type const & x2, value_type const & y2);
		GLM_FUNC_DECL explicit tmat2x2(
			col_type const & v1,
			col_type const & v2);

		//////////////////////////////////////
		// Conversions
		template <typename U> 
		GLM_FUNC_DECL explicit tmat2x2(
			U const & x);

		template <typename U, typename V, typename M, typename N>
		GLM_FUNC_DECL explicit tmat2x2(
			U const & x1, V const & y1,
			M const & x2, N const & y2);
			
		template <typename U, typename V>
		GLM_FUNC_DECL explicit tmat2x2(
			tvec2<U, P> const & v1,
			tvec2<V, P> const & v2);

		//////////////////////////////////////
		// Matrix conversions
		template <typename U, precision Q>
		GLM_FUNC_DECL explicit tmat2x2(tmat2x2<U, Q> const & m);

		GLM_FUNC_DECL explicit tmat2x2(tmat3x3<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat4x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat2x3<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat3x2<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat2x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat4x2<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat3x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat2x2(tmat4x3<T, P> const & x);

		//////////////////////////////////////
		// Accesses

		GLM_FUNC_DECL col_type & operator[](size_type i);
		GLM_FUNC_DECL col_type const & operator[](size_type i) const;

		// Unary updatable operators
		GLM_FUNC_DECL tmat2x2<T, P> & operator=(tmat2x2<T, P> const & m);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator=(tmat2x2<U, P> const & m);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator+=(U const & s);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator+=(tmat2x2<U, P> const & m);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator-=(U const & s);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator-=(tmat2x2<U, P> const & m);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator*=(U const & s);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator*=(tmat2x2<U, P> const & m);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator/=(U const & s);
		template <typename U> 
		GLM_FUNC_DECL tmat2x2<T, P> & operator/=(tmat2x2<U, P> const & m);

		//////////////////////////////////////
		// Increment and decrement operators

		GLM_FUNC_DECL tmat2x2<T, P> & operator++ ();
		GLM_FUNC_DECL tmat2x2<T, P> & operator-- ();
		GLM_FUNC_DECL tmat2x2<T, P> operator++(int);
		GLM_FUNC_DECL tmat2x2<T, P> operator--(int);
	};

	// Binary operators

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator+ (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator+ (
		typename tmat2x2<T, P>::value_type const & s,
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator+ (
		tmat2x2<T, P> const & m1,
		tmat2x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator- (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator- (
		typename tmat2x2<T, P>::value_type const & s,
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator- (
		tmat2x2<T, P> const & m1,
		tmat2x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator* (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator* (
		typename tmat2x2<T, P>::value_type const & s, 
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat2x2<T, P>::col_type operator* (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat2x2<T, P>::row_type operator* (
		typename tmat2x2<T, P>::col_type const & v,
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator* (
		tmat2x2<T, P> const & m1,
		tmat2x2<T, P> const & m2);
		
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x2<T, P> operator* (
		tmat2x2<T, P> const & m1,
		tmat3x2<T, P> const & m2);
		
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x2<T, P> operator* (
		tmat2x2<T, P> const & m1,
		tmat4x2<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator/ (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator/ (
		typename tmat2x2<T, P>::value_type const & s,
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat2x2<T, P>::col_type operator/ (
		tmat2x2<T, P> const & m,
		typename tmat2x2<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat2x2<T, P>::row_type operator/ (
		typename tmat2x2<T, P>::col_type const & v,
		tmat2x2<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x2<T, P> operator/ (
		tmat2x2<T, P> const & m1,
		tmat2x2<T, P> const & m2);

	// Unary constant operators
	template <typename T, precision P> 
	GLM_FUNC_DECL tmat2x2<T, P> const operator-(
		tmat2x2<T, P> const & m);
} //namespace detail
} //namespace glm

#ifndef GLM_EXTERNAL_TEMPLATE
#include "type_mat2x2.inl"
#endif

#endif //glm_core_type_mat2x2
