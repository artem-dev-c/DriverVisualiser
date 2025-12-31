#pragma once

#include <map>
#include <vector>
#include "DriverScanner.h"

/**
 * @class DriverGrouper
 * @brief Groups drivers based on their DriverInfo.class.
 * 
 * This class provides static methods to group a list of DriverInfo objects
 * into categories for easier analysis and display.
 */
class DriverGrouper {
public:

    using DriverGroupMap = std::map<std::wstring, std::vector<DriverInfo>>;

    static DriverGroupMap groupByClass(const std::vector<DriverInfo>& drivers);
};

//===========================================================================
//                         NOT USED DURING RECENT EDITS
//===========================================================================