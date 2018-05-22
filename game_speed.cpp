#include "game_speed.h"

GameSpeed::GameSpeed(const int value, const std::string &name)
	: _value(value)
	, _name(name)
{}

std::optional<int> GameSpeed::parse(const std::string &name, const bool caseSensitive) {
	auto nameCopy = name;

	if (!caseSensitive) {
		nameCopy = to_upper(name);
	}

	for (const auto &type : getAllGameSpeeds()) {
		auto typeNameCopy = type.getName();

		if (!caseSensitive) {
			typeNameCopy = to_upper(typeNameCopy);
		}

		if (typeNameCopy == nameCopy) {
			return type.intValue();
		}
	}
	return {};
}

std::optional<std::string> GameSpeed::parse(const int value) {
	for (const auto &type : getAllGameSpeeds()) {
		if (type.intValue() == value) {
			return type.getName();
		}
	}
	return {};
}

std::string GameSpeed::to_upper(const std::string &str) {
	std::string upper("");

	const int uoffset = 'a' - 'A';

	for (size_t i = 0; i < str.length(); ++i) {
		char ch = str[i];
		if (ch >= 'a' && ch <= 'z') {
			ch -= uoffset;
		}
		upper += ch;
	}

	return upper;
}

const GameSpeed GameSpeed::SLOWEST(0, "Slowest");
const GameSpeed GameSpeed::SLOWER(1, "Slower");
const GameSpeed GameSpeed::SLOW(2, "Slow");
const GameSpeed GameSpeed::NORMAL(3, "Normal");
const GameSpeed GameSpeed::FAST(4, "Fast");
const GameSpeed GameSpeed::FASTER(5, "Faster");
const GameSpeed GameSpeed::FASTEST(6, "Fastest");
std::vector<GameSpeed> GameSpeed::getAllGameSpeeds() {
	const auto &all = {
		GameSpeed::SLOWEST,
		GameSpeed::SLOWER,
		GameSpeed::SLOW,
		GameSpeed::NORMAL,
		GameSpeed::FAST,
		GameSpeed::FASTER,
		GameSpeed::FASTEST
	};
	return all;
}

