/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_MATHS_VEC2I_H
#define PESTO_MATHS_VEC2I_H

namespace pesto {

namespace maths {

/**
 * A class that represent a 2d vector in an integer space
 */
class Vec2i {
  public:
	long x, y;

	Vec2i() : x(0), y(0) {}
	Vec2i(long x_, long y_) : x(x_), y(y_) {}

	Vec2i operator+(const Vec2i &other) const { return Vec2i(x + other.x, y + other.y); }

	Vec2i operator-(const Vec2i &other) const { return Vec2i(x - other.x, y - other.y); }

	Vec2i &operator+=(const Vec2i &other) {
		x += other.x;
		y += other.y;
		return *this;
	}

	Vec2i &operator-=(const Vec2i &other) {
		x -= other.x;
		y -= other.y;
		return *this;
	}

	bool operator==(const Vec2i &other) const { return x == other.x && y == other.y; }

	bool operator!=(const Vec2i &other) const { return !(*this == other); }

	long dot(const Vec2i &other) const { return x * other.x + y * other.y; }

	long lengthSquared() const { return x * x + y * y; }

	void set(long x_, long y_) {
		x = x_;
		y = y_;
	}

	long cross(const Vec2i &other) const { return x * other.y - y * other.x; }
};

} // namespace maths

} // namespace pesto

#endif // PESTO_MATHS_VEC2I_H