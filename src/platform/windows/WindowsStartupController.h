#pragma once

class QString;

class WindowsStartupController final
{
  public:
    [[nodiscard]] static bool setEnabled(bool enabled, QString *errorMessage = nullptr);
};
