/*!
 * A better (templated) ND vector class which derives from std::array.
 */
#pragma once

#include <cmath>
using std::abs;
using std::sqrt;
using std::cos;
using std::sin;

#include <array>
using std::array;

#include <iostream>
using std::cout;
using std::endl;
using std::ostream;

#include <string>
using std::string;
#include <sstream>
using std::stringstream;

#include <type_traits>
using std::enable_if;
using std::enable_if_t;
using std::is_integral;
using std::is_scalar;

#include "Random.h"
using morph::RandUniformReal;
using morph::RandUniformInt;

namespace morph {

    // Forward declaration of templates for stream operator overloading
    // Example adapted from https://stackoverflow.com/questions/4660123
    template <typename Flt, size_t N> struct Vector;
    template <typename Flt, size_t N> ostream& operator<< (ostream&, const Vector<Flt, N>&);

    template <typename Flt=float, size_t N=3>
    struct Vector : public array<Flt, N>
    {
        /*!
         * Named component functions for up to 4D vectors
         */
        //@{
        template <size_t _N = N, enable_if_t<(_N>0), int> = 0>
        Flt x (void) const {
            return this->at(0);
        }
        template <size_t _N = N, enable_if_t<(_N>1), int> = 0>
        Flt y (void) const {
            return this->at(1);
        }
        template <size_t _N = N, enable_if_t<(_N>2), int> = 0>
        Flt z (void) const {
            return this->at(2);
        }
        template <size_t _N = N, enable_if_t<(_N>3), int> = 0>
        Flt w (void) const {
            return this->at(3);
        }
        //@}

        /*!
         * The threshold outside of which the vector is no longer
         * considered to be a unit vector.
         */
        const Flt unitThresh = 0.001;

#if 0
        // AVOID CODING ANY CONSTRUCTORS if possible
        // But I would like:
        Vector (const array<Flt, N> arr) {}

        // and:
        //! Construct from an array of *four* floats, assuming that this is a 4d vector
        //! of the sort used with transformation matrix and that v[3] is 'w', and can be
        //! discarded.
        Vector (const array<Flt, (N+1)> v) {
            // copy N elements from v
        }
        // Problem to solve: Including these buggers up init with curly braces.
#endif
        //! Return the vector as an array // no need
        //array<Flt, 3> asArray (void) const {}

        // Output the vector to stdout
        void output (void) const {
            cout << "Vector" << this->asString();
        }

        // Create a string representation of the vector
        string asString (void) const {
            stringstream ss;
            auto i = this->begin();
            ss << "(";
            bool first = true;
            while (i != this->end()) {
                if (first) {
                    ss << *i++;
                    first = false;
                } else {
                    ss << "," << *i++;
                }
            }
            ss << ")";
            return ss.str();
        }

        /*!
         * Renormalize the vector to 1.
         */
        void renormalize (void) {
            Flt denom = static_cast<Flt>(0);
            auto i = this->begin();
            while (i != this->end()) {
                denom += ((*i) * (*i));
                ++i;
            }
            denom = sqrt(denom);
            if (denom != static_cast<Flt>(0.0)) {
                Flt oneovermag = static_cast<Flt>(1.0) / denom;
                i = this->begin();
                while (i != this->end()) {
                    *i++ *= oneovermag;
                }
            }
        }

        /*!
         * Randomize the vector. Note that I need a real or int implementation here,
         * depending on the type of Flt. This allows me to use the correct type of
         * randomizer.
         *
         * Note, if you omit the second template arg from enable_if_t (or enable_if)
         * then the type defaults to void.
         */
        //@{
        template <typename F=Flt, enable_if_t<!is_integral<F>::value, int> = 0 >
        void randomize (void) {
            RandUniformReal<F> ruf (static_cast<F>(0), static_cast<F>(1));
            auto i = this->begin();
            while (i != this->end()) {
                *i++ = ruf.get();
            }
        }
        // Note: Here, if F is integral, then enable_if_t's type is '0' and the function
        // is included
        template <typename F=Flt, enable_if_t<is_integral<F>::value, int> = 0 >
        void randomize (void) {
            RandUniformInt<F> rui (static_cast<F>(0), static_cast<F>(255));
            auto i = this->begin();
            while (i != this->end()) {
                *i++ = rui.get();
            }
        }
        //@}

        /*!
         * Test to see if this vector is a unit vector (it doesn't *have* to be)
         */
        bool checkunit (void) {
            bool rtn = true;
            Flt metric = 1.0;
            auto i = this->begin();
            while (i != this->end()) {
                metric -= ((*i) * (*i));
                ++i;
            }
            if (abs(metric) > morph::Vector<Flt, N>::unitThresh) {
                rtn = false;
            }
            return rtn;
        }

        //! Return the length of the vector
        Flt length (void) const {
            Flt sos = static_cast<Flt>(0);
            auto i = this->begin();
            while (i != this->end()) {
                sos += ((*i) * (*i));
                ++i;
            }
            return sqrt(sos);
        }

        //! Unary negate operator
        Vector<Flt, N> operator- (void) const {
            Vector<Flt, N> rtn;
            auto i = this->begin();
            auto j = rtn.begin();
            while (i != this->end()) {
                *j++ = -(*i++);
            }
            return rtn;
        }

        //! Unary not
        bool operator! (void) const {
            return (this->length() == static_cast<Flt>(0.0)) ? true : false;
        }

        //! Vector multiply. Cross product of this with another vector v2 (if N==3). In
        //! higher dimensions, its more complicated to define what the cross product is,
        //! and I'm unlikely to need anything other than the plain old 3D cross product.
        template <size_t _N = N, enable_if_t<(_N==3), int> = 0>
        Vector<Flt, N> operator* (const Vector<Flt, _N>& v2) const {
            Vector<Flt, _N> v;
            v[0] = this->y() * v2.z() - this->z() * v2.y();
            v[1] = this->z() * v2.x() - this->x() * v2.z();
            v[2] = this->x() * v2.y() - this->y() * v2.x();
            return v;
        }
        template <size_t _N = N, enable_if_t<(_N==3), int> = 0>
        void operator*= (const Vector<Flt, _N>& v2) {
            Vector<Flt, _N> v;
            v[0] = this->y() * v2.z() - this->z() * v2.y();
            v[1] = this->z() * v2.x() - this->x() * v2.z();
            v[2] = this->x() * v2.y() - this->y() * v2.x();
            this->at(0) = v.x();
            this->at(1) = v.y();
            this->at(2) = v.z();
        }

        //! Scalar product of this with another vector, v2.
        Flt dot (const Vector<Flt, N>& v2) const {
            Flt rtn = static_cast<Flt>(0);
            auto i = this->begin();
            auto j = v2.begin();
            while (i != this->end()) {
                rtn += ((*i++) * (*j++));
            }
            return rtn;
        }

        //! Scalar multiply. Define for scalar S as the multiplier of the Vector<Flt, N>
        //@{
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        Vector<Flt, N> operator* (const S& s) const {
            Vector<Flt, N> rtn;
            auto val = this->begin();
            auto rval = rtn.begin();
            // Here's a way to iterate through which the compiler should be able to
            // autovectorise; it knows what i is on each loop:
            for (size_t i = 0; i < N; ++i) {
                *(rval+i) = *(val+i) * static_cast<Flt>(s);
            }
            return rtn;
        }

        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        void operator*= (const S& s) {
            auto val = this->begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) *= static_cast<Flt>(s);
            }
        }
        //@}

        //! Scalar division.
        //@{
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        Vector<Flt, N> operator/ (const S& s) const {
            Vector<Flt, N> rtn;
            auto val = this->begin();
            auto rval = rtn.begin();
            // Here's a way to iterate through which the compiler should be able to
            // autovectorise; it knows what i is on each loop:
            for (size_t i = 0; i < N; ++i) {
                *(rval+i) = *(val+i) / static_cast<Flt>(s);
            }
            return rtn;
        }
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        void operator/= (const S& s) {
            auto val = this->begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) /= static_cast<Flt>(s);
            }
        }
        //@}

        //! Vector addition
        //@{
        Vector<Flt, N> operator+ (const Vector<Flt, N>& v2) const {
            Vector<Flt, N> v;
            auto val = this->begin();
            auto val2 = v2.begin();
            for (size_t i = 0; i < N; ++i) {
                v[i] = *(val+i) + *(val2+i);
            }
            return v;
        }
        void operator+= (const Vector<Flt, N>& v2) {
            auto val = this->begin();
            auto val2 = v2.begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) += *(val2+i);
            }
        }
        //@}

        //! Vector subtraction
        //@{
        Vector<Flt, N> operator- (const Vector<Flt, N>& v2) const {
            Vector<Flt, N> v;
            auto val = this->begin();
            auto val2 = v2.begin();
            for (size_t i = 0; i < N; ++i) {
                v[i] = *(val+i) - *(val2+i);
            }
            return v;
        }
        void operator-= (const Vector<Flt, N>& v2) {
            auto val = this->begin();
            auto val2 = v2.begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) -= *(val2+i);
            }
        }
        //@}

        //! Scalar addition
        //@{
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        Vector<Flt, N> operator+ (const S& s) const {
            Vector<Flt, N> rtn;
            auto val = this->begin();
            auto rval = rtn.begin();
            // Here's a way to iterate through which the compiler should be able to
            // autovectorise; it knows what i is on each loop:
            for (size_t i = 0; i < N; ++i) {
                *(rval+i) = *(val+i) + static_cast<Flt>(s);
            }
            return rtn;
        }
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        void operator+= (const S& s) {
            auto val = this->begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) += static_cast<Flt>(s);
            }
        }
        //@}

        //! Scalar subtraction
        //@{
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        Vector<Flt, N> operator- (const S& s) const {
            Vector<Flt, N> rtn;
            auto val = this->begin();
            auto rval = rtn.begin();
            // Here's a way to iterate through which the compiler should be able to
            // autovectorise; it knows what i is on each loop:
            for (size_t i = 0; i < N; ++i) {
                *(rval+i) = *(val+i) - static_cast<Flt>(s);
            }
            return rtn;
        }
        template <typename S=Flt, enable_if_t<is_scalar<S>::value, int> = 0 >
        void operator-= (const S& s) {
            auto val = this->begin();
            for (size_t i = 0; i < N; ++i) {
                *(val+i) -= static_cast<Flt>(s);
            }
        }
        //@}

        //! Overload the stream output operator
        friend ostream& operator<< <Flt, N> (ostream& os, const Vector<Flt, N>& v);
    };

    // Template friendly mechanism to overload stream operator.
    template <typename Flt=float, size_t N=3>
    ostream& operator<< (ostream& os, const Vector<Flt, N>& v)
    {
        os << v.asString();
        return os;
    }

} // namespace morph