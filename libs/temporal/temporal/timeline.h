/*
    Copyright (C) 2020 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __libtemporal_timeline_h__
#define __libtemporal_timeline_h__

#include <ostream>
#include <exception>
#include <string>
#include <cassert>
#include <limits>

#include "pbd/enumwriter.h"

#include "temporal/types.h"
#include "temporal/beats.h"
#include "temporal/bbt_time.h"
#include "temporal/visibility.h"

namespace Temporal {

class timecnt_t;

/* 62 bit time value.
 * 63rd bit: indicates music or audio time value
 * 64th bit: sign bit
 */

class timepos_t {
 private:
	int64_t mark_beats (int64_t n) { return n|(1LL<62); }
  public:
	timepos_t ();
	timepos_t (superclock_t s) {
#ifdef SUPERSAFE_TIMEPOS
		v = s&~(1LL<<62);
#else
		v = s;
#endif
	}
	explicit timepos_t (timecnt_t const &); /* will throw() if val is negative */
	explicit timepos_t (Temporal::Beats const & b) { v = mark_beats (b.to_ticks()); }

	bool is_beats() const { return v & (1LL<<62); }
	bool is_superclock() const { return !is_beats (); }

	Temporal::TimeDomain time_domain () const { if (is_beats()) return Temporal::BeatTime; return Temporal::AudioTime; }

	superclock_t superclocks() const { assert (is_superclock()); return v; }
	int64_t ticks() const { assert (is_beats()); return v; }
	Beats beats() const { assert (is_beats()); int64_t b = v/ticks_per_beat; return Beats (b, v - (b * ticks_per_beat)); }

	/* return a timepos_t that is the next (later) possible position given
	 * this one
	 */

	timepos_t increment () const {
		return timepos_t (v + 1);
	}

	/* return a timepos_t that is the previous (earlier) possible position given
	 * this one
	 */
	timepos_t decrement () const {
		if (is_beats()) {
			return timepos_t (v - 1); /* beats can go negative */
		}
		return timepos_t (v > 0 ? v - 1 : v); /* samples cannot go negative */
	}

	timepos_t & operator= (timecnt_t const & t); /* will throw() if val is negative */
	timepos_t & operator= (superclock_t s) { v = s; return *this; }
	timepos_t & operator= (Temporal::Beats const & b) { v = mark_beats (b.to_ticks()); return *this; }

	bool operator== (timepos_t const & other) const { return v == other.v; }
	bool operator!= (timepos_t const & other) const { return v != other.v; }
#if 0
	bool operator<  (timecnt_t const & other) const { if (is_beats() == other.is_beats()) return v < other.v; return expensive_lt (other); }
	bool operator>  (timecnt_t const & other) const { if (is_beats() == other.is_beats()) return v > other.v; return expensive_gt (other); }
	bool operator<= (timecnt_t const & other) const { if (is_beats() == other.is_beats()) return v <= other.v; return expensive_lte (other); }
	bool operator>= (timecnt_t const & other) const { if (is_beats() == other.is_beats()) return v >= other.v; return expensive_gte (other); }
#endif
	bool operator<  (timepos_t const & other) const { if (is_beats() == other.is_beats()) return v < other.v; return expensive_lt (other); }
	bool operator>  (timepos_t const & other) const { if (is_beats() == other.is_beats()) return v > other.v; return expensive_gt (other); }
	bool operator<= (timepos_t const & other) const { if (is_beats() == other.is_beats()) return v <= other.v; return expensive_lte (other); }
	bool operator>= (timepos_t const & other) const { if (is_beats() == other.is_beats()) return v >= other.v; return expensive_gte (other); }

	timepos_t operator+(timecnt_t const & d) const;
	timepos_t operator+(timepos_t const & d) const { if (is_beats() == d.is_beats()) return timepos_t (v + d.ticks()); return expensive_add (d.superclocks()); }
	timepos_t operator+(superclock_t s) const { if (is_superclock()) return timepos_t (v + s); return expensive_add (s); }
	timepos_t operator+(Temporal::Beats const &b ) const { if (is_beats()) { return timepos_t (ticks() + b.to_ticks()); return expensive_add (b); } }

	/* operator-() poses severe and thorny problems for a class that represents position on a timeline.

	   If the value of the class is a simple scalar, then subtraction can be used for both:

	     1) movement backwards along the timeline
	     2) computing the distance between two positions

	   But timepos_t is not a simple scalar, and neither is timecnt_t, and these two operations are quite different.

	     1) movement backwards along the timeline should result in another timepos_t
             2) the distance between two positions is a timecnt_t

           so already we have a hint that we would need at least:

              timepos_t operator- (timecnt_t const &); ... compute new position
              timecnt_t operator- (timepos_t const &); ... compute distance

            But what happens we try to use more explicit types. What does this expression mean:

              timepos_t pos;
              pos - Beats (3);

            is this computing a new position 3 beats earlier than pos? or is it computing the distance between
            pos and the 3rd beat?

            For this reason, we do not provide any operator-() methods, but instead require the use of
            explicit methods with clear semantics.
	*/

	/* computes the distance between this timepos_t and @param p
	   such that: this + distance = p

	   This means that if @param p is later than this, distance is positive;
	   if @param p is earlier than this, distance is negative.

	   Note that the return value is a timecnt_t whose position member
	   is equal to the value of this. That means if the distance uses
	   musical time value, the distance may not have constant value
	   at other positions on the timeline.
	*/


	timecnt_t distance (timecnt_t const & p) const;
	timecnt_t distance (timepos_t const & p) const; // { if (is_beats() == p.is_beats()) return timecnt_t (v - p.val()); return expensive_distance (p); }
	timecnt_t distance (superclock_t s) const; // { if (is_samples()) return timecnt_t (v, v - s); }
	timecnt_t distance (Temporal::Beats const & b) const; // { if (is_beats()) return timecnt_t (v, ....); return expensive_distance (b); }


	/* computes a new position value that is @param d earlier than this */

	timepos_t earlier (timepos_t const & d) const; /* treat d as distance measured from timeline origin */
	timepos_t earlier (timecnt_t const & d) const;
	timepos_t earlier (samplepos_t d) const;
	timepos_t earlier (Beats const & d) const;
	timepos_t earlier (BBT_Offset const & d) const;

	/* like ::earlier() but changes this. loosely equivalent to operator-= */

	timepos_t & shift_earlier (timecnt_t const & d);
	timepos_t & shift_earlier (samplepos_t);
	timepos_t & shift_earlier (Temporal::Beats const &);
	timepos_t & shift_earlier (Temporal::BBT_Offset const &);

	timepos_t operator/(double) const;
	timepos_t operator*(double) const;

	timepos_t & operator*=(double);
	timepos_t & operator/=(double);

	timepos_t & operator+=(timecnt_t const & d);
	timepos_t & operator+=(samplepos_t);
	timepos_t & operator+=(Temporal::Beats const &);
	timepos_t & operator+=(Temporal::BBT_Offset const &);

	timepos_t   operator% (timecnt_t const &) const;
	timepos_t & operator%=(timecnt_t const &);

	bool operator<  (superclock_t s) { return v < s; }
	bool operator<  (Temporal::Beats const & b) { return beats() < b; }
	bool operator<= (superclock_t s) { return v <= s; }
	bool operator<= (Temporal::Beats const & b) { return beats() <= b; }
	bool operator>  (superclock_t s) { return v > s; }
	bool operator>  (Temporal::Beats const & b) { return beats() > b; }
	bool operator>= (superclock_t s) { return v >= s; }
	bool operator>= (Temporal::Beats const & b) { return beats() >= b; }
	bool operator== (superclock_t s) { return v == s; }
	bool operator== (Temporal::Beats const & b) { return beats() == b; }
	bool operator!= (superclock_t s) { return v != s; }
	bool operator!= (Temporal::Beats const & b) { return beats() != b; }

	void set_sample (samplepos_t s);
	void set_beat (Temporal::Beats const &);
	void set_bbt (Temporal::BBT_Time const &);

	bool string_to (std::string const & str);
	std::string to_string () const;

	static timepos_t const & max() { return _max_timepos; }

  private:
	/* these are mutable because we may need to update them at arbitrary
	   times, even within contexts that are otherwise const. For example, an
	   audio-locked position whose _beats value is out of date. The audio time
	   is canonical and will not change, but beats() needs to be callable, and
	   we'd prefer to claim const-ness for it than not.
	*/

	mutable superclock_t v;

	/* special constructor for max_timepos */
	//timepos_t (TimeDomainStatus);

	static timepos_t _max_timepos;

	bool expensive_lt (timepos_t const &) const;
	bool expensive_lte (timepos_t const &) const;
	bool expensive_gt (timepos_t const &) const;
	bool expensive_gte(timepos_t const &) const;

	bool expensive_lt (timecnt_t const &) const;
	bool expensive_lte (timecnt_t const &) const;
	bool expensive_gt (timecnt_t const &) const;
	bool expensive_gte(timecnt_t const &) const;

	timepos_t expensive_add (Temporal::Beats const &) const;
	timepos_t expensive_add (superclock_t s) const;
};


/**
 * a timecnt_t measures a duration in a specified time domain and starting at a
 * specific position.
 *
 * It can be freely converted between time domains, as well as used as the
 * subject of most arithmetic operations.
 *
 * An important distinction between timepos_t and timecnt_t can be thought of
 * this way: a timepos_t ALWAYS refers to a position relative to the origin of
 * the timeline (technically, the origin in the tempo map used to translate
 * between audio and musical domains). By contrast, a timecnt_t refers to a
 * certain distance beyond some arbitrary (specified) origin. So, a timepos_t
 * of "3 beats" always means "3 beats measured from the timeline origin". A
 * timecnt_t of "3 beats" always come with a position, and so is really "3
 * beats after <position>".
 *
 * The ambiguity surrounding operator-() that affects timepos_t does not exist
 * for timecnt_t: all uses of operator-() are intended to compute the result of
 * subtracting one timecnt_t from another which will always result in another
 * timecnt_t of lesser value than the first operand.
 */

class LIBTEMPORAL_API timecnt_t {

   public:
	timecnt_t () : _distance (0), _position (0) {}
	timecnt_t (timepos_t const & d, timepos_t const & p) : _distance (d), _position (p) { assert (p.is_beats() == d.is_beats()); }
	timecnt_t (timecnt_t const &, timepos_t const & pos);
	timecnt_t (superclock_t s, timepos_t const & pos) : _distance (s), _position (pos) { assert (_distance.is_beats() == _position.is_beats()); }
	explicit timecnt_t (Temporal::Beats const & b, timepos_t const & pos) :  _distance (b), _position (pos) { assert ( _distance.is_beats() == _position.is_beats()); }

	timepos_t const & distance() const { return _distance; }
	timepos_t const & position() const { return _position; }
	void set_position (timepos_t const &pos);

	/* provides a more compact form and faster execution for "timecnt_t > 0 */
	bool positive() const { return _distance > 0; }

	/* provides a more compact form and faster execution for "timecnt_t < 0 */
	bool negative() const {return _distance < 0; }

	/* provides a more compact form and faster execution for "timecnt_t == 0 */
	bool zero() const { return _distance == 0; }


	static timecnt_t const & max() { return _max_timecnt; }

	timecnt_t abs() const;

	Temporal::TimeDomain time_domain () const { return _position.time_domain (); }

	superclock_t     superclocks() const { if (_position.is_superclock()) return _distance.superclocks(); return compute_superclocks(); }
	Temporal::Beats beats  () const { if (_position.is_beats()) return _distance.beats(); return compute_beats(); }
	int64_t         ticks  () const { if (_position.is_beats()) return _distance.ticks(); return compute_ticks(); }

	timecnt_t & operator= (superclock_t s) { _distance = s; return *this; }
	timecnt_t & operator= (Temporal::Beats const & b) { _distance = b; return *this; }
	timecnt_t & operator= (timepos_t const & s)  { _distance = s; return *this; }

	/* return a timecnt_t that is the next/previous (earlier/later) possible position given
	 * this one
	 */
	timecnt_t operator++ () { _distance = _distance + 1; return *this; }
	timecnt_t operator-- () { _distance = _distance.earlier (1); return *this; }

	timecnt_t operator*(double) const;
	timecnt_t operator/(double n) const;

	timecnt_t operator-() const { return timecnt_t (_distance.is_beats() ? -_distance.ticks() : -_distance.superclocks(), _position); }
	timecnt_t operator- (timecnt_t const & t) const { return timecnt_t (_distance.earlier (t.distance()), _position); }
	timecnt_t operator+ (timecnt_t const & t) const { return timecnt_t (_distance + t.distance(), _position); }

	timecnt_t & operator-= (timecnt_t const & t)  { _distance.earlier (t.distance()); return *this; }
	timecnt_t & operator+= (timecnt_t const & t)  { _distance = _distance + t.distance(); return *this; }

	//timecnt_t operator- (timepos_t const & t) const;
	//timecnt_t operator+ (timepos_t const & t) const;
	//timecnt_t & operator-= (timepos_t);
	//timecnt_t & operator+= (timepos_t);

	bool operator> (timecnt_t const & other) const { return _distance > other.distance (); }
	bool operator>= (timecnt_t const & other) const { return _distance >= other.distance(); }
	bool operator< (timecnt_t const & other) const { return _distance < other.distance(); }
	bool operator<= (timecnt_t const & other) const { return _distance <= other.distance(); }
	timecnt_t & operator=(timecnt_t const & other) {
		if (this != &other) {
			if (_distance.is_beats() == other.distance().is_beats()) {
				_distance = other.distance();
			} else {
				/* unclear what to do here but we cannot allow
				   inconsistent timecnt_t to be created
				*/
			}
		}
		return *this;
	}

	bool operator!= (timecnt_t const & other) const { return _distance != other.distance(); }
	bool operator== (timecnt_t const & other) const { return _distance == other.distance(); }

	/* test for numerical equivalence with a timepos_T. This tests ONLY the
	   duration in the given domain, NOT position.
	*/
	bool operator== (timepos_t const & other) const { return _distance == other; }

	bool operator< (superclock_t s) { if (_distance.is_superclock()) return _distance < s; return false; }
	bool operator< (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance < b; return false; }
	bool operator<= (superclock_t s) { if (_distance.is_superclock()) return _distance <= s; return false; }
	bool operator<= (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance <= b; return false; }
	bool operator> (superclock_t s) { if (_distance.is_superclock()) return _distance > s; return false; }
	bool operator> (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance > b; return false; }
	bool operator>= (superclock_t s) { if (_distance.is_superclock()) return _distance >= s; return false; }
	bool operator>= (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance >= b; return false; }
	bool operator== (superclock_t s) { if (_distance.is_superclock()) return _distance == s; return false; }
	bool operator== (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance == b; return false; }
	bool operator!= (superclock_t s) { if (_distance.is_superclock()) return _distance != s; return false; }
	bool operator!= (Temporal::Beats const & b) { if (_distance.is_beats()) return _distance != b; return false; }

	timecnt_t   operator% (timecnt_t const &) const;
	timecnt_t & operator%=(timecnt_t const &);

	bool string_to (std::string const & str);
	std::string to_string () const;

  private:
	timepos_t _distance; /* aka "duration" */
	timepos_t _position; /* aka "origin */

	static timecnt_t _max_timecnt;

	superclock_t compute_superclocks () const;
	Beats compute_beats () const;
	int64_t compute_ticks () const;
};

} /* end namespace Temporal */


/* because timepos_t is just a typedef here, C++ won't let this be redefined
 * (numeric_limits<uint64_t> are already implicitly defined.
 */

namespace std {
	template<>
	struct numeric_limits<Temporal::timepos_t> {
		static Temporal::timepos_t min() {
			return Temporal::timepos_t (0);
		}
		static Temporal::timepos_t max() {
			return Temporal::timepos_t (4611686018427387904); /* pow (2,62) */
		}
	};
}



#endif /* __libtemporal_timeline_h__ */