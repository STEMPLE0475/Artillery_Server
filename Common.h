#pragma once

#include <cstdint>

constexpr float PROJECTILE_GRAVITY = -100.0f;
constexpr float POWER_SCALAR = 220.0f;
constexpr float EXPLOSION_RADIUS_SQ = 150.0f * 150.0f;

enum class TEAM_TYPE : uint8_t
{
	TEAM_A = 0,
	TEAM_B = 1,
};

struct Vec2D
{
	float X = 0.0f;
	float Y = 0.0f;
};