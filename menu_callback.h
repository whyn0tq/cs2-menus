#pragma once

#include "include/menus.h"

inline void InvokeMenu(const MenuCallbackFunc& callback, std::string value, std::string label, int item, int slot)
{
    auto run = callback;
    if (run)
        run(value.c_str(), label.c_str(), item, slot);
}
