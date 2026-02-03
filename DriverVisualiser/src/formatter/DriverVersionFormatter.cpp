#include "DriverVersionFormatter.h"

QString DriverVersionFormatter::versionToString(const DriverVersion& version)
{
    if (!version.hasVersion) {
        return "Unknown";
    }

    return QString("%1.%2.%3.%4")
        .arg(version.major)
        .arg(version.minor)
        .arg(version.build)
        .arg(version.revision);
}
