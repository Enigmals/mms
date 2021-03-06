#pragma once

#include "../IMouseAlgorithm.h"

namespace manual {

class Manual : public IMouseAlgorithm {

public:
    std::string mouseFile() const;
    std::string interfaceType() const;
    void solve(
        int mazeWidth, int mazeHeight, bool isOfficialMaze,
        char initialDirection, mms::MouseInterface* mouse);

};

} // namespace manual
