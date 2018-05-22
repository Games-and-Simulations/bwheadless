#pragma once

#include <optional>
#include <string>
#include <vector>

class GameSpeed {
public:
	GameSpeed() = delete;
	GameSpeed(const int value, const std::string &name);

	int intValue() const noexcept { return _value; }
	std::string getName() const noexcept { return _name; }

	static const GameSpeed SLOWEST;
	static const GameSpeed SLOWER;
	static const GameSpeed SLOW;
	static const GameSpeed NORMAL;
	static const GameSpeed FAST;
	static const GameSpeed FASTER;
	static const GameSpeed FASTEST;
	static std::vector<GameSpeed> getAllGameSpeeds();

	static std::optional<int> parse(const std::string &name, const bool caseSensitive = false);
	static std::optional<std::string> parse(const int value);

private:
	const int _value;
	const std::string _name;

	static std::string to_upper(const std::string &str);
};
